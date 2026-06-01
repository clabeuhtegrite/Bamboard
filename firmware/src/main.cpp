// Bamboard — main entry point.
//
// Boots the display, brings up Wi-Fi via the captive portal if needed,
// optionally self-updates from GitHub Releases, then launches two tasks:
//
//   - UI task   (core 1): drives LVGL, touch input, auto-dim backlight
//   - Net task  (core 0): polls Bambuddy + pumps the WebSocket
//
// Persistent settings (Wi-Fi creds, Bambuddy URL, API key, brightness) live
// in NVS via the Preferences API. Holding the side BOOT button at power-up
// wipes them and re-opens the captive portal.

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <lvgl.h>
#include <time.h>

#include "config.h"
#include "hw/display.h"
#include "net/bambuddy_client.h"
#include "net/bambuddy_ws.h"
#include "net/github_ota.h"
#include "ui/fonts.h"
#include "ui/i18n.h"
#include "ui/screens.h"
#include "ui/ui.h"

// Runtime config (read once from NVS at boot, written back when the captive
// portal saves a new value).
String  g_cfg_bambuddy_url;
String  g_cfg_api_key;
uint8_t g_cfg_brightness_level = ::display::BL_LEVEL_DEFAULT;
String  g_cfg_tz               = ::schedule::TZ;
// 0..23 = reboot daily at that local hour; schedule::REBOOT_DISABLED = off.
uint8_t g_cfg_reboot_hour      = ::schedule::DAILY_REBOOT_ENABLED
                                     ? ::schedule::DAILY_REBOOT_HOUR
                                     : ::schedule::REBOOT_DISABLED;
uint8_t g_cfg_lang             = (uint8_t)::i18n::Lang::EN;  // UI language index

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
    g_cfg_tz = s_prefs.getString("tz", ::schedule::TZ);
    if (g_cfg_tz.length() == 0) g_cfg_tz = ::schedule::TZ;
    g_cfg_reboot_hour = s_prefs.getUChar("reboot_h",
        ::schedule::DAILY_REBOOT_ENABLED ? ::schedule::DAILY_REBOOT_HOUR
                                         : ::schedule::REBOOT_DISABLED);
    if (g_cfg_reboot_hour > 23 &&
        g_cfg_reboot_hour != ::schedule::REBOOT_DISABLED) {
        g_cfg_reboot_hour = ::schedule::REBOOT_DISABLED;
    }
    g_cfg_lang = s_prefs.getUChar("lang", (uint8_t)::i18n::Lang::EN);
    if (g_cfg_lang >= (uint8_t)::i18n::Lang::COUNT) {
        g_cfg_lang = (uint8_t)::i18n::Lang::EN;
    }
    s_prefs.end();
}

static void save_prefs() {
    s_prefs.begin("bamboard", false);
    s_prefs.putString("url",      g_cfg_bambuddy_url);
    s_prefs.putString("key",      g_cfg_api_key);
    s_prefs.putUChar ("bl_level", g_cfg_brightness_level);
    s_prefs.putString("tz",       g_cfg_tz);
    s_prefs.putUChar ("reboot_h", g_cfg_reboot_hour);
    s_prefs.putUChar ("lang",     g_cfg_lang);
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
// (declared `extern void factory_reset();` in screens.cpp). Wipes the entire
// NVS namespace the same way "hold BOOT at boot" does — Wi-Fi + Bambuddy
// creds, timezone, daily-reboot hour, brightness and language all go — then
// reboots, so the captive portal flow runs again on the next start.
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
    lv_obj_set_style_text_font(title, &bb_font_36, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t* sub = lv_label_create(scr);
    lv_label_set_text(sub,
        "Connect to Wi-Fi network:\n"
        "    Bamboard-setup\n"
        "Password: bamboard\n\n"
        "Then open http://192.168.4.1 in a browser\n"
        "and fill in Wi-Fi, Bambuddy + timezone details.");
    lv_obj_set_style_text_color(sub, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_set_style_text_font(sub, &bb_font_20, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 0);
    lv_timer_handler();

    WiFiManagerParameter p_url("url",
        "Bambuddy URL (e.g. http://192.168.1.42:8000)",
        g_cfg_bambuddy_url.c_str(), 120);
    WiFiManagerParameter p_key("key", "Bambuddy API key",
        g_cfg_api_key.c_str(), 79);

    // Timezone (POSIX TZ string — cryptic, so show ready-to-paste examples)
    // and the daily-reboot hour (0-23, or blank to turn the reboot off).
    WiFiManagerParameter p_tz_hint(
        "<br/><p>Timezone is a POSIX TZ string. Examples &mdash; "
        "Paris: <code>CET-1CEST,M3.5.0,M10.5.0/3</code> &middot; "
        "London: <code>GMT0BST,M3.5.0/1,M10.5.0</code> &middot; "
        "US&nbsp;East: <code>EST5EDT,M3.2.0,M11.1.0</code> &middot; "
        "UTC: <code>UTC0</code></p>");
    WiFiManagerParameter p_tz("tz", "Timezone (POSIX TZ string)",
        g_cfg_tz.c_str(), 48);

    char rbh_default[4] = "";
    if (g_cfg_reboot_hour <= 23)
        snprintf(rbh_default, sizeof(rbh_default), "%u",
                 (unsigned)g_cfg_reboot_hour);
    WiFiManagerParameter p_rbh("reboot_h",
        "Daily reboot hour 0-23 (blank = off)", rbh_default, 4);

    // UI language — a two-letter code; the hint lists the five supported ones.
    WiFiManagerParameter p_lang_hint(
        "<br/><p>Interface language &mdash; "
        "<code>en</code> English &middot; <code>es</code> Español &middot; "
        "<code>fr</code> Français &middot; <code>pt</code> Português &middot; "
        "<code>de</code> Deutsch</p>");
    WiFiManagerParameter p_lang("lang", "Language (en/es/fr/pt/de)",
        ::i18n::lang_code(g_cfg_lang), 4);

    s_wm.addParameter(&p_url);
    s_wm.addParameter(&p_key);
    s_wm.addParameter(&p_tz_hint);
    s_wm.addParameter(&p_tz);
    s_wm.addParameter(&p_rbh);
    s_wm.addParameter(&p_lang_hint);
    s_wm.addParameter(&p_lang);

    s_wm.setConfigPortalTimeout(provision::PORTAL_TIMEOUT_S);
    s_wm.setBreakAfterConfig(true);

    if (!s_wm.startConfigPortal(provision::AP_SSID, provision::AP_PASSWORD)) {
        log_w("Captive portal timed out; rebooting");
        delay(500);
        ESP.restart();
    }

    g_cfg_bambuddy_url = p_url.getValue();
    g_cfg_api_key      = p_key.getValue();

    g_cfg_tz = p_tz.getValue();
    g_cfg_tz.trim();
    if (g_cfg_tz.length() == 0) g_cfg_tz = ::schedule::TZ;

    // Reboot hour: blank / non-numeric / out of range all mean "disabled".
    String rbh = p_rbh.getValue();
    rbh.trim();
    bool numeric = rbh.length() > 0;
    for (size_t i = 0; i < rbh.length(); ++i)
        if (rbh[i] < '0' || rbh[i] > '9') numeric = false;
    if (numeric) {
        int h = rbh.toInt();
        g_cfg_reboot_hour = (h >= 0 && h <= 23) ? (uint8_t)h
                                                : ::schedule::REBOOT_DISABLED;
    } else {
        g_cfg_reboot_hour = ::schedule::REBOOT_DISABLED;
    }

    int li = ::i18n::lang_from_code(p_lang.getValue());
    if (li >= 0) g_cfg_lang = (uint8_t)li;   // unknown code → keep current
    ::i18n::set_language(g_cfg_lang);

    save_prefs();

    bambuddy::g_client.set_credentials(g_cfg_bambuddy_url, g_cfg_api_key);
    bambuddy::g_ws.set_credentials    (g_cfg_bambuddy_url);  // /ws is unauthenticated
    delay(300);
    ESP.restart();   // clean restart with the new config
}

// ---------------------------------------------------------------------------
// FreeRTOS tasks
// ---------------------------------------------------------------------------

static void net_task(void*) {
    uint32_t next_printers_ms     = 0;
    uint32_t next_status_ms       = 0;
    uint32_t next_stats_ms        = 0;
    uint32_t next_recent_ms       = 0;
    uint32_t next_health_ms       = 0;
    uint32_t next_reboot_check_ms = 0;

    for (;;) {
        uint32_t now = millis();
        if (WiFi.status() != WL_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Drive the WS event loop on every iteration so push frames land
        // without waiting for the next poll tick.
        bambuddy::g_ws.loop();

        // Daily scheduled reboot: at local DAILY_REBOOT_HOUR:00 the device
        // restarts so the boot-time OTA check applies any new firmware
        // unattended. getLocalTime() returns false until SNTP has synced, so
        // we simply don't reboot until the clock is valid. The MIN_UPTIME
        // guard stops a reboot that lands in the 00:00 minute from looping.
        if (g_cfg_reboot_hour <= 23 && now >= next_reboot_check_ms) {
            next_reboot_check_ms = now + schedule::CHECK_INTERVAL_MS;
            struct tm lt;
            if (now > schedule::MIN_UPTIME_MS && getLocalTime(&lt, 0) &&
                lt.tm_hour == (int)g_cfg_reboot_hour && lt.tm_min == 0) {
                log_w("Scheduled daily reboot at %02d:%02d local time",
                      lt.tm_hour, lt.tm_min);
                delay(200);
                ESP.restart();
            }
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
            bool ws = bambuddy::g_ws.is_connected();
            if (ws) {
                // WS is feeding `printer_status` for every printer in real
                // time, so REST polling is purely a safety net — refresh
                // only the focused printer at a slow cadence so a silently
                // broken WS still surfaces stale data eventually.
                if (id >= 0) bambuddy::g_client.fetch_printer_status(id);
                next_status_ms = now + bambuddy::POLL_DASHBOARD_WS_MS;
            } else {
                // No WS push → poll every printer ourselves at the snappy
                // cadence so the Printers screen stays responsive.
                if (id >= 0) bambuddy::g_client.fetch_printer_status(id);
                bambuddy::Printer ps[8]; uint8_t n = 0;
                bambuddy::g_client.snapshot_printers(ps, n);
                for (uint8_t i = 0; i < n; ++i) {
                    if (ps[i].id != id)
                        bambuddy::g_client.fetch_printer_status(ps[i].id);
                }
                next_status_ms = now + bambuddy::POLL_DASHBOARD_MS;
            }
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
// Boot-time firmware update (GitHub Releases)
// ---------------------------------------------------------------------------

// Runs once in setup(), after Wi-Fi is up and before the UI / net tasks start.
// No FreeRTOS tasks are running yet, so we own LVGL outright here and can pump
// lv_timer_handler() straight from the OTA callbacks to drive the on-screen
// progress overlay. On success the device reboots into the new image inside
// check_and_update() and never returns; otherwise we clear the overlay and
// fall through to normal operation on the current build.
static void run_boot_update() {
    ota::CheckResult r = ota::check_and_update(
        /* on_start */ []() {
            ui::screens::ota_set_active(true);
            ui::screens::ota_set_progress(0);
            ui::screens::ota_apply();
            lv_timer_handler();
        },
        /* on_progress */ [](uint8_t pct) {
            ui::screens::ota_set_progress(pct);
            ui::screens::ota_apply();
            lv_timer_handler();
        });

    // A download that started but failed leaves the overlay up — surface the
    // reason briefly so a wall-mounted device shows something other than a
    // frozen progress bar, then continue booting the current firmware.
    if (r == ota::CheckResult::Failed) {
        ui::screens::ota_set_error("Update failed - continuing");
        ui::screens::ota_apply();
        for (int i = 0; i < 30; ++i) {
            lv_timer_handler();
            delay(100);
        }
    }

    ui::screens::ota_set_active(false);
    ui::screens::ota_apply();
    lv_timer_handler();
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
    // Factory reset: the BOOT button on the side of the Guition PCB is the
    // only physical input left. Holding it during power-up wipes NVS and
    // re-opens the captive portal (used to be PREV-at-boot on v0.x).
    pinMode(pins::BOOT_BUTTON, INPUT_PULLUP);
    if (digitalRead(pins::BOOT_BUTTON) == LOW) {
        log_w("BOOT held — clearing settings");
        clear_all_prefs();
        delay(500);
    }

    // Load persisted settings and apply the UI language BEFORE building the
    // screens, so the tab bar and titles come up already translated.
    load_prefs();
    ::i18n::set_language(g_cfg_lang);

    ui::g_ui.begin();
    lv_timer_handler();

    // Apply the user's saved brightness now that we have the value. The
    // display starts off dimmed inside hw::Display::begin() to avoid a
    // bright flash before the first frame is drawn.
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

    // Start SNTP so the daily scheduled reboot (net_task) can tell when it's
    // local midnight. Non-blocking: the first sync lands a few seconds later,
    // and the reboot check tolerates "clock not set yet".
    configTzTime(g_cfg_tz.c_str(), schedule::NTP_SERVER1, schedule::NTP_SERVER2);

    // Boot-time firmware update. The device pulls the latest release straight
    // from GitHub; if it's newer than what we're running, the update is
    // mandatory and applied before anything else starts. Offline / already
    // current / GitHub down all fall through quickly so the device still
    // boots. Dev-sentinel builds skip it so a USB-flashed work-in-progress
    // isn't immediately pulled back to the published release.
#if BAMBOARD_OTA_AUTOCHECK
    if (!BAMBOARD_VERSION_IS_DEV) {
        run_boot_update();
    } else {
        log_i("OTA: dev build (%s) — skipping boot update check", BAMBOARD_VERSION);
    }
#endif

    bambuddy::g_client.begin(g_cfg_bambuddy_url, g_cfg_api_key);
    bambuddy::g_ws.begin    (g_cfg_bambuddy_url);  // /ws is unauthenticated

    // Tasks: net on core 0, UI on core 1.
    xTaskCreatePinnedToCore(net_task, "net", 8192, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(ui_task,  "ui",  6144, nullptr, 2, nullptr, 1);
}

void loop() {
    // All work happens in tasks; idle the Arduino loop.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
