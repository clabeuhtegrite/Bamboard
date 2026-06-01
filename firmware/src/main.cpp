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
#include <esp_heap_caps.h> // free the PSRAM camera-JPEG buffer
#include <esp_system.h>   // esp_reset_reason() for boot diagnostics
#include <esp_task_wdt.h> // task watchdog on the UI task
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
// Optional Cloudflare Access service token — only used (and only collected by
// the captive portal) when the Bambuddy URL is https://. Empty on a LAN/http box.
String  g_cfg_cf_id;
String  g_cfg_cf_secret;

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
    g_cfg_cf_id     = s_prefs.getString("cf_id", "");
    g_cfg_cf_secret = s_prefs.getString("cf_secret", "");
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
    s_prefs.putString("cf_id",     g_cfg_cf_id);
    s_prefs.putString("cf_secret", g_cfg_cf_secret);
    s_prefs.end();
}

void save_brightness_level(uint8_t level) {
    if (level < ::display::BL_LEVEL_MIN) level = ::display::BL_LEVEL_MIN;
    if (level > ::display::BL_LEVEL_MAX) level = ::display::BL_LEVEL_MAX;
    // Always apply the backlight, but only touch NVS when the level actually
    // changed — re-tapping the current level (or the per-tick re-sync in the
    // Settings screen) shouldn't burn a flash write.
    hw::g_display.set_brightness_level(level);
    if (level == g_cfg_brightness_level) return;
    g_cfg_brightness_level = level;
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

// --- Bambuddy host validation ----------------------------------------------
// http  → must be a private IPv4 or an mDNS *.local name (LAN only; the API key
//         travels in clear). https → any IPv4 or a hostname/domain. An optional
//         :port is allowed either way. Enforced server-side after the portal saves.

static bool digits_only(const String& s) {
    if (s.length() == 0) return false;
    for (size_t i = 0; i < s.length(); ++i)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

static bool parse_ipv4(const String& h, int oct[4]) {
    int parts = 0, start = 0;
    const int n = h.length();
    for (int i = 0; i <= n; ++i) {
        if (i == n || h[i] == '.') {
            String p = h.substring(start, i);
            if (parts >= 4 || !digits_only(p) || p.length() > 3) return false;
            int v = p.toInt();
            if (v < 0 || v > 255) return false;
            oct[parts++] = v;
            start = i + 1;
        } else if (h[i] < '0' || h[i] > '9') {
            return false;
        }
    }
    return parts == 4;
}

static bool is_ipv4(const String& h) { int o[4]; return parse_ipv4(h, o); }

static bool is_private_ipv4(const String& h) {
    int o[4];
    if (!parse_ipv4(h, o)) return false;
    if (o[0] == 10) return true;                              // 10.0.0.0/8
    if (o[0] == 192 && o[1] == 168) return true;              // 192.168.0.0/16
    if (o[0] == 172 && o[1] >= 16 && o[1] <= 31) return true; // 172.16.0.0/12
    if (o[0] == 127) return true;                             // loopback
    return false;
}

static bool is_mdns_local(const String& h) {
    String l = h; l.toLowerCase();
    return l.length() > 6 && l.endsWith(".local");
}

// RFC-1123-ish hostname: dot-separated labels of [A-Za-z0-9-], 1..63 chars each,
// not starting/ending with '-'.
static bool is_hostname(const String& h) {
    if (h.length() == 0 || h.length() > 253) return false;
    int start = 0;
    const int n = h.length();
    for (int i = 0; i <= n; ++i) {
        if (i == n || h[i] == '.') {
            int len = i - start;
            if (len == 0 || len > 63) return false;
            if (h[start] == '-' || h[i - 1] == '-') return false;
            start = i + 1;
        } else {
            char c = h[i];
            bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') || c == '-';
            if (!ok) return false;
        }
    }
    return true;
}

// Split "host[:port]" → host + port ("" when absent). IPv6 literals unsupported.
static void split_host_port(const String& in, String& host, String& port) {
    int c = in.indexOf(':');
    if (c >= 0 && in.indexOf(':', c + 1) < 0) {   // exactly one ':'
        host = in.substring(0, c);
        port = in.substring(c + 1);
    } else {
        host = in;
        port = "";
    }
}

static bool host_valid_for_scheme(const String& host, bool https) {
    if (https) return is_ipv4(host) || is_hostname(host);
    return is_private_ipv4(host) || is_mdns_local(host);
}

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

    // Pre-fill: split the stored full URL back into scheme (checkbox) + host.
    bool pre_https = g_cfg_bambuddy_url.startsWith("https://");
    String pre_host = g_cfg_bambuddy_url;
    if      (pre_host.startsWith("https://")) pre_host = pre_host.substring(8);
    else if (pre_host.startsWith("http://"))  pre_host = pre_host.substring(7);
    while (pre_host.endsWith("/")) pre_host.remove(pre_host.length() - 1);

    WiFiManagerParameter p_host_hint(
        "<br/><p><b>Bambuddy host.</b> HTTP (LAN): a private IP "
        "(<code>192.168.x.y</code>, <code>10.x</code>, <code>172.16-31.x</code>) "
        "or an <code>*.local</code> name. HTTPS: a domain or IP. Optional "
        "<code>:port</code>; don't include <code>http(s)://</code>.</p>");
    WiFiManagerParameter p_host("host", "Bambuddy host (IP / .local / domain)",
        pre_host.c_str(), 120);
    WiFiManagerParameter p_https("https",
        "Use HTTPS (enables a domain + Cloudflare Access)", "T", 2,
        pre_https ? "type=\"checkbox\" checked" : "type=\"checkbox\"");
    WiFiManagerParameter p_key("key", "Bambuddy API key",
        g_cfg_api_key.c_str(), 79);

    // Cloudflare Access service token — only meaningful (and only saved) when
    // HTTPS is on; the script below reveals these two rows only then.
    WiFiManagerParameter p_cf_hint(
        "<p id=\"cf_hint\"><b>Cloudflare Access</b> (optional, HTTPS only): paste a "
        "service token to reach a Bambuddy published behind CF Access. Leave blank "
        "for a plain HTTPS endpoint.</p>");
    WiFiManagerParameter p_cf_id("cf_id", "CF-Access-Client-Id",
        g_cfg_cf_id.c_str(), 80);
    WiFiManagerParameter p_cf_secret("cf_secret", "CF-Access-Client-Secret",
        g_cfg_cf_secret.c_str(), 96, "type=\"password\"");

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

    // Show/hide the Cloudflare rows from the HTTPS checkbox. Added last so every
    // referenced input already exists when this inline script runs.
    WiFiManagerParameter p_js(
        "<script>(function(){"
        "function row(id,on){var e=document.getElementById(id);if(!e)return;"
        "e.style.display=on?'':'none';"
        "var l=document.querySelector('label[for=\"'+id+'\"]');if(l)l.style.display=on?'':'none';}"
        "function sync(){var c=document.getElementById('https');var on=!!(c&&c.checked);"
        "row('cf_id',on);row('cf_secret',on);"
        "var h=document.getElementById('cf_hint');if(h)h.style.display=on?'':'none';}"
        "var c=document.getElementById('https');if(c)c.addEventListener('change',sync);"
        "sync();})();</script>");

    s_wm.addParameter(&p_host_hint);
    s_wm.addParameter(&p_host);
    s_wm.addParameter(&p_https);
    s_wm.addParameter(&p_key);
    s_wm.addParameter(&p_cf_hint);
    s_wm.addParameter(&p_cf_id);
    s_wm.addParameter(&p_cf_secret);
    s_wm.addParameter(&p_tz_hint);
    s_wm.addParameter(&p_tz);
    s_wm.addParameter(&p_rbh);
    s_wm.addParameter(&p_lang_hint);
    s_wm.addParameter(&p_lang);
    s_wm.addParameter(&p_js);

    s_wm.setConfigPortalTimeout(provision::PORTAL_TIMEOUT_S);
    s_wm.setBreakAfterConfig(true);

    if (!s_wm.startConfigPortal(provision::AP_SSID, provision::AP_PASSWORD)) {
        log_w("Captive portal timed out; rebooting");
        delay(500);
        ESP.restart();
    }

    // --- read back + validate -------------------------------------------------
    bool use_https = p_https.getValue()[0] != '\0';

    String raw = p_host.getValue();
    raw.trim();
    if      (raw.startsWith("https://")) raw = raw.substring(8);
    else if (raw.startsWith("http://"))  raw = raw.substring(7);
    while (raw.endsWith("/")) raw.remove(raw.length() - 1);

    String vhost, vport;
    split_host_port(raw, vhost, vport);
    bool host_ok = host_valid_for_scheme(vhost, use_https) &&
                   (vport.length() == 0 ||
                    (digits_only(vport) && vport.toInt() >= 1 && vport.toInt() <= 65535));
    if (!host_ok) {
        log_w("Bambuddy host '%s' invalid for %s; reopening portal",
              raw.c_str(), use_https ? "https" : "http");
        delay(500);
        ESP.restart();   // nothing saved → the portal runs again on reboot
    }

    g_cfg_bambuddy_url = String(use_https ? "https://" : "http://") + raw;
    g_cfg_api_key      = p_key.getValue();

    // The CF service token only applies to https; drop it on http so a stale
    // token can't linger and leak onto a plain-text connection.
    if (use_https) {
        g_cfg_cf_id     = p_cf_id.getValue();
        g_cfg_cf_secret = p_cf_secret.getValue();
        g_cfg_cf_id.trim();
        g_cfg_cf_secret.trim();
    } else {
        g_cfg_cf_id     = "";
        g_cfg_cf_secret = "";
    }

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

    bambuddy::g_client.set_credentials(g_cfg_bambuddy_url, g_cfg_api_key,
                                       g_cfg_cf_id, g_cfg_cf_secret);
    bambuddy::g_ws.set_credentials    (g_cfg_bambuddy_url,
                                       g_cfg_cf_id, g_cfg_cf_secret);
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
    uint32_t next_cam_ms          = 0;

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
            // Health-cadence device diagnostics over serial (heap is the one to
            // watch for leaks/fragmentation; ws=0 means we're on the REST
            // safety net rather than live push).
            log_i("diag: heap=%uK psram=%uK ws=%d rssi=%d online=%d lat=%ums",
                  (unsigned)(ESP.getFreeHeap()  / 1024),
                  (unsigned)(ESP.getFreePsram() / 1024),
                  (int)bambuddy::g_ws.is_connected(), (int)WiFi.RSSI(),
                  (int)ok, (unsigned)lat);
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

        // Camera: while the full-screen viewer is open, OR the Live screen is up
        // (to feed the inline thumbnail). One snapshot every 5 s — gentle for a
        // desk monitor. Fetch + decode are heavy (HTTP + JPEG) so they live here
        // on the net task; the UI task just blits the frame. A run of failures
        // (Bambuddy with no camera) backs the thumbnail polling off; opening the
        // full-screen viewer resets it and always retries.
        static uint8_t cam_fail = 0;
        if (ui::screens::camera_overlay_is_open()) cam_fail = 0;
        bool want_cam = ui::screens::camera_overlay_is_open() ||
                        (ui::g_ui.current() == ui::Screen::Dashboard && cam_fail < 3);
        if (want_cam && now >= next_cam_ms) {
            int id = ui::g_ui.selected_printer_id();
            if (id >= 0) {
                uint8_t* jpeg = nullptr;
                size_t   jlen = 0;
                if (bambuddy::g_client.fetch_camera_jpeg(id, &jpeg, &jlen)) {
                    ui::screens::camera_decode_frame(jpeg, jlen);
                    heap_caps_free(jpeg);
                    cam_fail = 0;
                } else if (cam_fail < 255) {
                    cam_fail++;
                }
            }
            next_cam_ms = now + 5000;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void ui_task(void*) {
    uint32_t last_refresh       = 0;
    uint32_t last_touch_seen_ms = millis();
    // Watch the UI task: a frozen screen is the worst failure for a desk
    // monitor, and this loop never blocks (LVGL tick + an 8 ms delay), so it
    // can't false-trip the way the HTTP-bound net task would. If LVGL ever
    // deadlocks, the task WDT panics and the device reboots into a fresh UI.
    esp_task_wdt_add(nullptr);
    for (;;) {
        esp_task_wdt_reset();
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

// Human-readable reset cause — logged at boot so a field unit's reboot history
// is diagnosable over serial (panic / brownout / watchdog vs a clean restart).
static const char* reset_reason_str(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:  return "power-on";
        case ESP_RST_EXT:      return "external";
        case ESP_RST_SW:       return "software";       // our ESP.restart()
        case ESP_RST_PANIC:    return "panic";
        case ESP_RST_INT_WDT:  return "int-wdt";
        case ESP_RST_TASK_WDT: return "task-wdt";
        case ESP_RST_WDT:      return "other-wdt";
        case ESP_RST_DEEPSLEEP:return "deep-sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO:     return "sdio";
        default:               return "unknown";
    }
}

void setup() {
    Serial.begin(115200);
    delay(50);
    log_i("Bamboard %s booting — reset cause: %s",
          BAMBOARD_VERSION, reset_reason_str(esp_reset_reason()));

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
        // Creds exist but the AP is unreachable (router reboot / ISP outage).
        // Don't loop the captive portal + reboot — boot into the normal UI in an
        // offline state and let auto-reconnect + net_task recover when Wi-Fi
        // returns. Hold BOOT at power-up to force re-provisioning if the stored
        // creds themselves are wrong.
        log_w("Wi-Fi connect failed; booting offline (auto-reconnect armed). "
              "Hold BOOT at power-up to re-provision.");
        WiFi.setAutoReconnect(true);
    } else {
        log_i("Wi-Fi up: %s", WiFi.localIP().toString().c_str());
    }

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

    bambuddy::g_client.begin(g_cfg_bambuddy_url, g_cfg_api_key,
                             g_cfg_cf_id, g_cfg_cf_secret);
    bambuddy::g_ws.begin    (g_cfg_bambuddy_url, g_cfg_cf_id, g_cfg_cf_secret);

    // Arm the task watchdog (10 s, panic+reboot on timeout) before the UI task
    // subscribes itself. Best-effort: if the Arduino runtime already initialised
    // the TWDT this is a no-op and the existing timeout stands — the UI loop
    // feeds it every few ms either way, so it only fires on a real hang. The
    // net task is intentionally NOT watched (its HTTP calls block for seconds
    // by design).
    esp_task_wdt_init(10, true);

    // Tasks: net on core 0, UI on core 1. The net task needs a generous stack:
    // a single call frame can stack the mbedTLS handshake (several KB) + an
    // ArduinoJson parse + the TJpgDec JPEG decode for camera snapshots
    // (≈3.5 KB workspace). 12 KB was tight on the camera-over-TLS path; 16 KB
    // buys margin (internal RAM is plentiful on the S3).
    xTaskCreatePinnedToCore(net_task, "net", 16384, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(ui_task,  "ui",  6144,  nullptr, 2, nullptr, 1);
}

void loop() {
    // All work happens in tasks; idle the Arduino loop.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
