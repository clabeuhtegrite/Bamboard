// Bamboard — main entry point.
//
// Boots the hardware (display, buttons, LED), brings up Wi-Fi via captive
// portal if needed, and launches two tasks:
//
//   - UI task   (core 1): drives LVGL, button input, LED animation
//   - Net task  (core 0): polls Bambuddy on a schedule
//
// Persistent settings (Wi-Fi creds, Bambuddy URL, API key) live in NVS via
// the Preferences API. A long-press on PREV at boot wipes them and re-opens
// the captive portal.

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <lvgl.h>

#include "config.h"
#include "hw/display.h"
#include "net/bambuddy_client.h"
#include "net/bambuddy_ws.h"
#include "ui/screens.h"
#include "ui/ui.h"

// Runtime config (read once from NVS at boot, written back when the captive
// portal saves a new value).
String  g_cfg_bambuddy_url;
String  g_cfg_api_key;
uint8_t g_cfg_brightness_level = ::display::BL_LEVEL_DEFAULT;

// Public hook used by the Settings screen's brightness selector. Applies
// the new level immediately and persists it to NVS so the next boot picks
// the same brightness without flashing the previous one first.
void save_brightness_level(uint8_t level);

// ---------------------------------------------------------------------------
// Persisted settings
// ---------------------------------------------------------------------------

static Preferences s_prefs;

static void load_prefs() {
    s_prefs.begin("bamboard", true);
    g_cfg_bambuddy_url     = s_prefs.getString("url", "");
    g_cfg_api_key          = s_prefs.getString("key", "");
    g_cfg_brightness_level = s_prefs.getUChar("bl_level",
                                              ::display::BL_LEVEL_DEFAULT);
    if (g_cfg_brightness_level < ::display::BL_LEVEL_MIN ||
        g_cfg_brightness_level > ::display::BL_LEVEL_MAX) {
        g_cfg_brightness_level = ::display::BL_LEVEL_DEFAULT;
    }
    s_prefs.end();
}

static void save_prefs() {
    s_prefs.begin("bamboard", false);
    s_prefs.putString("url",      g_cfg_bambuddy_url);
    s_prefs.putString("key",      g_cfg_api_key);
    s_prefs.putUChar ("bl_level", g_cfg_brightness_level);
    s_prefs.end();
}

void save_brightness_level(uint8_t level) {
    if (level < ::display::BL_LEVEL_MIN) level = ::display::BL_LEVEL_MIN;
    if (level > ::display::BL_LEVEL_MAX) level = ::display::BL_LEVEL_MAX;
    g_cfg_brightness_level = level;
    hw::g_display.set_brightness_level(level);
    s_prefs.begin("bamboard", false);
    s_prefs.putUChar("bl_level", level);
    s_prefs.end();
}

static void clear_all_prefs() {
    s_prefs.begin("bamboard", false);
    s_prefs.clear();
    s_prefs.end();
    WiFi.disconnect(true, true);
}

// Public hook used by the Settings screen's "Factory reset" touch button
// (declared `extern void factory_reset();` in screens.cpp). Wipes NVS the
// same way "hold BOOT at boot" does, then reboots — on the next start,
// the captive portal flow runs again.
void factory_reset() {
    log_w("Factory reset requested from UI");
    clear_all_prefs();
    delay(300);
    ESP.restart();
}

// ---------------------------------------------------------------------------
// Wi-Fi provisioning
// ---------------------------------------------------------------------------

static WiFiManager s_wm;

static void start_provisioning() {
    // Show a friendly screen while we run the captive portal.
    lv_obj_t* scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(::ui::C_BG), 0);
    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "Bamboard setup");
    lv_obj_set_style_text_color(title, lv_color_hex(::ui::C_ACCENT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t* sub = lv_label_create(scr);
    lv_label_set_text(sub,
        "Connect to Wi-Fi network:\n"
        "    Bamboard-setup\n"
        "Password: bamboard\n\n"
        "Then open http://192.168.4.1 in a browser\n"
        "and fill in your Wi-Fi + Bambuddy details.");
    lv_obj_set_style_text_color(sub, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_20, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 0);
    lv_timer_handler();

    WiFiManagerParameter p_url("url",
        "Bambuddy URL (e.g. http://192.168.1.42:8000)",
        g_cfg_bambuddy_url.c_str(), 120);
    WiFiManagerParameter p_key("key", "Bambuddy API key",
        g_cfg_api_key.c_str(), 79);
    s_wm.addParameter(&p_url);
    s_wm.addParameter(&p_key);

    s_wm.setConfigPortalTimeout(provision::PORTAL_TIMEOUT_S);
    s_wm.setBreakAfterConfig(true);

    if (!s_wm.startConfigPortal(provision::AP_SSID, provision::AP_PASSWORD)) {
        log_w("Captive portal timed out; rebooting");
        delay(500);
        ESP.restart();
    }

    g_cfg_bambuddy_url = p_url.getValue();
    g_cfg_api_key      = p_key.getValue();
    save_prefs();

    bambuddy::g_client.set_credentials(g_cfg_bambuddy_url, g_cfg_api_key);
    bambuddy::g_ws.set_credentials    (g_cfg_bambuddy_url, g_cfg_api_key);
    delay(300);
    ESP.restart();   // clean restart with the new config
}

// ---------------------------------------------------------------------------
// FreeRTOS tasks
// ---------------------------------------------------------------------------

static void net_task(void*) {
    uint32_t next_printers_ms = 0;
    uint32_t next_status_ms   = 0;
    uint32_t next_stats_ms    = 0;
    uint32_t next_recent_ms   = 0;
    uint32_t next_health_ms   = 0;

    for (;;) {
        uint32_t now = millis();
        if (WiFi.status() != WL_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // OTA goes before any other network work: during an active upload
        // ArduinoOTA.handle() blocks on TCP receive in chunks, and we don't
        // want HTTP polling or the WS reconnect timer competing for the
        // socket while flash bytes are landing.
        ArduinoOTA.handle();

        // Drive the WS event loop on every iteration so push frames land
        // without waiting for the next poll tick.
        bambuddy::g_ws.loop();

        if (now >= next_health_ms) {
            uint32_t lat = 0;
            bool ok = bambuddy::g_client.ping_health(&lat);
            ui::screens::header_set_online(ok, lat);
            next_health_ms = now + bambuddy::POLL_HEALTH_MS;
        }

        if (now >= next_printers_ms) {
            bambuddy::g_client.fetch_printers();
            next_printers_ms = now + bambuddy::POLL_LIST_MS;
        }

        if (now >= next_status_ms) {
            int id = ui::g_ui.selected_printer_id();
            if (id >= 0) bambuddy::g_client.fetch_printer_status(id);
            // Always fetch every visible printer at a slower pace for the
            // Printers screen.
            bambuddy::Printer ps[8]; uint8_t n = 0;
            bambuddy::g_client.snapshot_printers(ps, n);
            for (uint8_t i = 0; i < n; ++i) {
                if (ps[i].id != id) bambuddy::g_client.fetch_printer_status(ps[i].id);
            }
            // When the WebSocket is feeding us live deltas, drop polling to
            // a 30 s safety net; otherwise stay on the snappy 2 s cadence.
            next_status_ms = now + (bambuddy::g_ws.is_connected()
                                        ? bambuddy::POLL_DASHBOARD_WS_MS
                                        : bambuddy::POLL_DASHBOARD_MS);
        }

        if (now >= next_stats_ms) {
            bambuddy::g_client.fetch_statistics();
            next_stats_ms = now + bambuddy::POLL_STATS_MS;
        }

        if (now >= next_recent_ms) {
            bambuddy::g_client.fetch_recent_archives(bambuddy::MAX_RECENT_ARCHIVES);
            next_recent_ms = now + bambuddy::POLL_STATS_MS;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void ui_task(void*) {
    uint32_t last_refresh       = 0;
    uint32_t last_touch_seen_ms = millis();
    for (;;) {
        // LVGL pumps both the display flush and the touch input device.
        // The touch driver bumps `consume_touch_activity()` whenever the
        // GT911 reports a press; we use that to reset the auto-dim timer.
        hw::g_display.tick();
        if (hw::g_display.consume_touch_activity()) {
            last_touch_seen_ms = millis();
            uint8_t wake = display::bl_pwm_for_level(
                hw::g_display.brightness_level());
            if (hw::g_display.backlight() < wake) {
                hw::g_display.set_backlight(wake);
            }
        }

        if (millis() - last_refresh > 250) {
            ui::g_ui.refresh();
            last_refresh = millis();
        }

        // Auto-dim — no more buttons, so the touch driver is the only thing
        // that can wake the screen back up.
        if (millis() - last_touch_seen_ms > display::DIM_AFTER_MS) {
            if (hw::g_display.backlight() > display::BL_DIM)
                hw::g_display.set_backlight(display::BL_DIM);
        }

        vTaskDelay(pdMS_TO_TICKS(8));
    }
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(50);
    log_i("Bamboard booting");

    if (!hw::g_display.begin()) {
        // Fall back to a minimal serial-only mode; the user will see no
        // output but we don't want to brick the device.
        log_e("Display init failed");
    }
    ui::g_ui.begin();
    lv_timer_handler();

    // Factory reset: the BOOT button on the side of the Guition PCB is the
    // only physical input left. Holding it during power-up wipes NVS and
    // re-opens the captive portal (used to be PREV-at-boot on v0.x).
    pinMode(pins::BOOT_BUTTON, INPUT_PULLUP);
    if (digitalRead(pins::BOOT_BUTTON) == LOW) {
        log_w("BOOT held — clearing settings");
        clear_all_prefs();
        delay(500);
    }

    load_prefs();

    // Apply the user's saved brightness as soon as we have the value. The
    // display starts off dimmed inside hw::Display::begin() to avoid a
    // bright flash before the first frame is drawn; now that LVGL has
    // ticked at least once we can ramp to the chosen level.
    hw::g_display.set_brightness_level(g_cfg_brightness_level);

    // Bring up Wi-Fi. If we have no saved credentials or Bambuddy details,
    // run the captive portal.
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("bamboard");

    if (g_cfg_bambuddy_url.length() == 0 || g_cfg_api_key.length() == 0) {
        start_provisioning();   // does not return (reboots after save)
    }

    // Try to connect using stored creds (WiFiManager saved them already).
    WiFi.begin();
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
        delay(200);
        lv_timer_handler();
    }
    if (WiFi.status() != WL_CONNECTED) {
        log_w("Wi-Fi connect failed; opening portal");
        start_provisioning();
    }
    log_i("Wi-Fi up: %s", WiFi.localIP().toString().c_str());

    bambuddy::g_client.begin(g_cfg_bambuddy_url, g_cfg_api_key);
    bambuddy::g_ws.begin    (g_cfg_bambuddy_url, g_cfg_api_key);

    // ArduinoOTA — flashing.md promises `pio run -t upload --upload-port
    // bamboard.local` works on the LAN. Callbacks fire on net_task (core
    // 0) and the UI task picks the new state up via ota_apply() on its
    // next tick (core 1) — no LVGL access from the wrong core.
    ArduinoOTA.setHostname(ota::HOSTNAME);
    ArduinoOTA.setPassword(ota::PASSWORD);
    ArduinoOTA.onStart([]() {
        log_i("OTA start");
        ui::screens::ota_set_active(true);
        ui::screens::ota_set_progress(0);
    });
    ArduinoOTA.onProgress([](unsigned int p, unsigned int total) {
        if (!total) return;
        uint8_t pct = (uint8_t)((uint64_t)p * 100ULL / total);
        ui::screens::ota_set_progress(pct);
    });
    ArduinoOTA.onEnd([]() {
        log_i("OTA end — rebooting");
        ui::screens::ota_set_progress(100);
        // ArduinoOTA reboots on its own once we return.
    });
    ArduinoOTA.onError([](ota_error_t err) {
        const char* msg = "Update failed.";
        switch (err) {
            case OTA_AUTH_ERROR:    msg = "Auth failed — wrong OTA password."; break;
            case OTA_BEGIN_ERROR:   msg = "Begin failed — out of space?";       break;
            case OTA_CONNECT_ERROR: msg = "Connection lost.";                   break;
            case OTA_RECEIVE_ERROR: msg = "Receive failed.";                    break;
            case OTA_END_ERROR:     msg = "End failed.";                        break;
            default: break;
        }
        log_e("OTA error: %s", msg);
        ui::screens::ota_set_error(msg);
    });
    ArduinoOTA.begin();
    log_i("OTA ready on %s.local", ota::HOSTNAME);

    // Tasks: net on core 0, UI on core 1.
    xTaskCreatePinnedToCore(net_task, "net", 8192, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(ui_task,  "ui",  6144, nullptr, 2, nullptr, 1);
}

void loop() {
    // All work happens in tasks; idle the Arduino loop.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
