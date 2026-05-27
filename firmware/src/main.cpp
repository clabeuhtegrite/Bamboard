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
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <lvgl.h>

#include "config.h"
#include "hw/buttons.h"
#include "hw/display.h"
#include "hw/led.h"
#include "net/bambuddy_client.h"
#include "ui/screens.h"
#include "ui/ui.h"

// Runtime config (read once from NVS at boot, written back when the captive
// portal saves a new value).
String g_cfg_bambuddy_url;
String g_cfg_api_key;

// ---------------------------------------------------------------------------
// Persisted settings
// ---------------------------------------------------------------------------

static Preferences s_prefs;

static void load_prefs() {
    s_prefs.begin("bamboard", true);
    g_cfg_bambuddy_url = s_prefs.getString("url", "");
    g_cfg_api_key      = s_prefs.getString("key", "");
    s_prefs.end();
}

static void save_prefs() {
    s_prefs.begin("bamboard", false);
    s_prefs.putString("url", g_cfg_bambuddy_url);
    s_prefs.putString("key", g_cfg_api_key);
    s_prefs.end();
}

static void clear_all_prefs() {
    s_prefs.begin("bamboard", false);
    s_prefs.clear();
    s_prefs.end();
    WiFi.disconnect(true, true);
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
            next_status_ms = now + bambuddy::POLL_DASHBOARD_MS;
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
    uint32_t last_refresh = 0;
    for (;;) {
        hw::g_buttons.update();
        ui::g_ui.handle_input();
        hw::g_display.tick();

        if (millis() - last_refresh > 250) {
            ui::g_ui.refresh();
            last_refresh = millis();
        }

        // LED reflects the currently-selected printer.
        {
            bambuddy::Printer ps[8]; uint8_t n = 0;
            bambuddy::g_client.snapshot_printers(ps, n);
            int id = ui::g_ui.selected_printer_id();
            bambuddy::PrinterState st = bambuddy::PrinterState::Unknown;
            bool hms_err = false;
            for (uint8_t i = 0; i < n; ++i)
                if (ps[i].id == id) {
                    st = ps[i].state;
                    hms_err = ps[i].hms.length() && ps[i].hms != "ok";
                }
            hw::g_led.tick(st, hms_err);
        }

        // Auto-dim
        if (millis() - hw::g_buttons.last_activity_ms() > display::DIM_AFTER_MS) {
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

    hw::g_buttons.begin();
    hw::g_led.begin();
    if (!hw::g_display.begin()) {
        // Fall back to a minimal serial-only mode; the user will see no
        // output but we don't want to brick the device.
        log_e("Display init failed");
    }
    ui::g_ui.begin();
    lv_timer_handler();

    // If PREV is held during boot, wipe NVS and force re-provisioning.
    if (digitalRead(pins::BTN_PREV) == LOW) {
        log_w("PREV held — clearing settings");
        clear_all_prefs();
        delay(500);
    }

    load_prefs();

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

    // Tasks: net on core 0, UI on core 1.
    xTaskCreatePinnedToCore(net_task, "net", 8192, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(ui_task,  "ui",  6144, nullptr, 2, nullptr, 1);
}

void loop() {
    // All work happens in tasks; idle the Arduino loop.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
