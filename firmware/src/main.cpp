// Bamboard — main entry point.
//
// Boots the display, brings up Wi-Fi via the captive portal if needed,
// optionally self-updates from GitHub Releases, then launches two tasks:
//
//   - UI task   (core 1): drives LVGL, touch input, auto-dim backlight
//   - Net task  (core 0): polls Bambuddy over REST + runs queued control actions
//
// Persistent settings (Wi-Fi creds, Bambuddy URL, API key, brightness) live
// in NVS via the Preferences API. Holding the side BOOT button at power-up
// wipes them and re-opens the captive portal.

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_wifi.h>     // Wi-Fi country/region — allow 2.4 GHz channels 12/13 (EU)
#include <esp_heap_caps.h> // free the PSRAM camera-JPEG buffer
#include <esp_ota_ops.h>  // anti-brick: boot-partition rollback (set_boot_partition)
#include <esp_system.h>   // esp_reset_reason() for boot diagnostics
#include <esp_task_wdt.h> // task watchdog on the UI task
#include <freertos/queue.h>  // control-action command queue (UI → net task)
#include <lvgl.h>
#include <time.h>

#include "config.h"
#include "hw/display.h"
#include "net/bambuddy_client.h"
#include "net/github_ota.h"
#include "net/host_valid.h"
#include "ui/fonts.h"
#include "ui/i18n.h"
#include "ui/control.h"
#include "ui/screens.h"
#include "ui/ui.h"

// Larger loop-task stack for setup(): building the whole LVGL UI tree
// (ui::g_ui.begin) runs here on the Arduino loop task, whose default 8 KB stack
// is tight for that deep a call chain. 16 KB buys headroom (the LVGL object heap
// itself lives in PSRAM — see include/lv_conf.h).
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

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

// Display preferences set in the captive portal (NVS keys `tempf` / `clock24`),
// read by the UI through ui/units.h. Defaults match the EU-default timezone:
// Celsius and a 24-hour clock.
bool g_cfg_temp_f    = false;   // true → show temperatures in °F
bool g_cfg_clock_24h = true;    // false → 12-hour AM/PM clock

// Public hook used by the Settings screen's brightness selector. Applies
// the new level immediately and persists it to NVS so the next boot picks
// the same brightness without flashing the previous one first.
void save_brightness_level(uint8_t level);

// ---------------------------------------------------------------------------
// Persisted settings
// ---------------------------------------------------------------------------

// Human-readable cause of THIS boot (set in setup() from esp_reset_reason);
// surfaced on the Settings screen. A static string literal — safe to share.
const char* g_boot_reason = "?";

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
    g_cfg_temp_f    = s_prefs.getBool("tempf", false);
    g_cfg_clock_24h = s_prefs.getBool("clock24", true);
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
    s_prefs.putBool  ("tempf",     g_cfg_temp_f);
    s_prefs.putBool  ("clock24",   g_cfg_clock_24h);
    s_prefs.end();
}

// Deferred brightness persistence. save_brightness_level() runs inside the LVGL
// touch-event dispatch on the UI task; a synchronous NVS write (~tens of ms,
// occasionally more on a flash erase cycle) has no business stalling event
// handling. We apply the backlight instantly and defer the commit to
// brightness_flush() in the UI loop — same task (all NVS stays single-task, per
// the invariant below), and a quick level sweep collapses into one write.
static bool     s_bl_save_pending = false;
static uint32_t s_bl_save_at_ms   = 0;

void save_brightness_level(uint8_t level) {
    if (level < ::display::BL_LEVEL_MIN) level = ::display::BL_LEVEL_MIN;
    if (level > ::display::BL_LEVEL_MAX) level = ::display::BL_LEVEL_MAX;
    // Always apply the backlight, but only persist when the level actually
    // changed — re-tapping the current level (or the per-tick re-sync in the
    // Settings screen) shouldn't burn a flash write.
    hw::g_display.set_brightness_level(level);
    if (level == g_cfg_brightness_level) return;
    g_cfg_brightness_level = level;
    s_bl_save_pending = true;
    s_bl_save_at_ms   = millis();
}

// Drain a deferred brightness write once the level has settled. UI-task only
// (keeps every NVS write on one task). Called from the UI loop.
static void brightness_flush() {
    if (!s_bl_save_pending) return;
    if (millis() - s_bl_save_at_ms < 600) return;   // still settling — coalesce
    s_bl_save_pending = false;
    s_prefs.begin("bamboard", false);
    s_prefs.putUChar("bl_level", g_cfg_brightness_level);
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

// Public hook used by the Settings screen's "Wi-Fi setup" touch button
// (declared `extern void reconfigure_wifi();` in settings.cpp). Unlike
// factory_reset(), it does NOT wipe NVS: it sets a one-shot flag and reboots
// into the captive portal, which pre-fills the saved Bambuddy host / API key /
// Cloudflare token / timezone / language — so the user only re-picks their
// Wi-Fi network instead of re-entering everything. The flag is consumed early
// in setup().
void reconfigure_wifi() {
    log_w("Wi-Fi reconfigure requested from UI (config preserved)");
    s_prefs.begin("bamboard", false);
    s_prefs.putUChar("reprov", 1);
    s_prefs.end();
    delay(300);
    ESP.restart();
}

// ---------------------------------------------------------------------------
// OTA boot-verification / anti-brick rollback
// ---------------------------------------------------------------------------
//
// A new image can pass the OTA MD5 check yet still be a bad *build* — one that
// boots but crash-loops or hangs. The IDF bootloader's own rollback is off in
// the precompiled Arduino SDK, so we do it in the app: the OTA arms a
// "pending verify" flag (ota_arm_pending); each boot of the new image bumps a
// counter very early in setup (handle_ota_verify); if the image never runs long
// enough to confirm itself (ota_mark_confirmed, from the UI task) the counter
// hits VERIFY_MAX_BOOTS and we switch the boot partition back to the previous
// (known-good) slot and reboot. The bad version is remembered so the next OTA
// check skips it (no re-flash loop) until a newer release supersedes it.
// All NVS access here is on the setup/UI task only (the net task never writes NVS).

static void ota_arm_pending() {
    s_prefs.begin("bamboard", false);
    s_prefs.putUChar("ota_pend", 1);
    s_prefs.putUChar("ota_tries", 0);
    s_prefs.end();
}

static void ota_clear_pending() {
    s_prefs.begin("bamboard", false);
    if (s_prefs.getUChar("ota_pend", 0)) {
        s_prefs.putUChar("ota_pend", 0);
        s_prefs.putUChar("ota_tries", 0);
    }
    s_prefs.end();
}

// Called once the running image has proven healthy (steady uptime). Confirms it
// and clears any stale bad-version record — the fleet has moved past it.
static void ota_mark_confirmed() {
    s_prefs.begin("bamboard", false);
    if (s_prefs.getUChar("ota_pend", 0)) {
        s_prefs.putUChar("ota_pend", 0);
        s_prefs.putUChar("ota_tries", 0);
        s_prefs.remove("ota_bad_ver");
        log_i("OTA: running image confirmed healthy");
    }
    s_prefs.end();
}

// Run FIRST in setup(). Rolls the boot partition back to the previous slot (and
// reboots — never returns) if a freshly-OTA'd image keeps failing to confirm;
// otherwise returns and boot continues.
static void handle_ota_verify() {
    s_prefs.begin("bamboard", false);
    // Dev builds (USB-flashed) don't participate — clear any stale verify state.
    if (BAMBOARD_VERSION_IS_DEV) {
        s_prefs.putUChar("ota_pend", 0);
        s_prefs.putUChar("ota_tries", 0);
        s_prefs.end();
        return;
    }
    if (!s_prefs.getUChar("ota_pend", 0)) { s_prefs.end(); return; }  // normal boot
    uint8_t tries = s_prefs.getUChar("ota_tries", 0);
    if (tries >= ::ota::VERIFY_MAX_BOOTS) {
        // The new image never confirmed across VERIFY_MAX_BOOTS boots → roll back.
        s_prefs.putString("ota_bad_ver", BAMBOARD_VERSION);  // skip it next OTA check
        s_prefs.putUChar("ota_pend", 0);
        s_prefs.putUChar("ota_tries", 0);
        s_prefs.end();
        const esp_partition_t* prev = esp_ota_get_next_update_partition(nullptr);
        if (prev && esp_ota_set_boot_partition(prev) == ESP_OK) {
            log_e("OTA: image %s failed to confirm after %u boots — rolling back",
                  BAMBOARD_VERSION, (unsigned)tries);
            delay(300);
            esp_restart();   // boots the previous (good) slot; never returns
        }
        log_e("OTA: rollback wanted but set_boot_partition failed — running anyway");
        return;
    }
    s_prefs.putUChar("ota_tries", (uint8_t)(tries + 1));
    s_prefs.end();
    log_w("OTA: verifying new image %s (boot attempt %u/%u)", BAMBOARD_VERSION,
          (unsigned)(tries + 1), (unsigned)::ota::VERIFY_MAX_BOOTS);
}

// ---------------------------------------------------------------------------
// Wi-Fi provisioning
// ---------------------------------------------------------------------------

static WiFiManager s_wm;

// --- Bambuddy host validation ---
// The rules (private-IPv4 / *.local for http; any IPv4 / hostname for https)
// live in net/host_valid.h so they can be unit-tested on the host. Used below
// in start_provisioning() via the netcfg:: namespace.

// Minimal HTML-attribute escape for values pre-filled into the captive-portal
// form. The stored host is restricted to a safe charset by host_valid, but the
// API key and CF tokens are free-form: a stored value containing a double quote
// would otherwise break out of the value="..." attribute (a self-inflicted XSS
// in the AP-only portal). The browser decodes the entities back on submit, so a
// round-trip with no edit still yields the original secret.
static String html_attr_escape(const String& in) {
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); ++i) {
        char c = in[i];
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            default:   out += c;        break;
        }
    }
    return out;
}

static void start_provisioning() {
    // Per-device setup-AP password: derive it from the MAC so every unit differs
    // (see provision::AP_PASSWORD_PREFIX). The user reads the full value off the
    // on-screen prompt below, so a per-device secret costs them nothing.
    uint8_t mac[6] = {0};
    WiFi.macAddress(mac);
    char ap_pass[20];
    snprintf(ap_pass, sizeof(ap_pass), "%s%02X%02X",
             provision::AP_PASSWORD_PREFIX, mac[4], mac[5]);

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
    char sub_txt[256];
    snprintf(sub_txt, sizeof(sub_txt),
        "Connect to Wi-Fi network:\n"
        "    %s\n"
        "Password: %s\n\n"
        "Then open http://192.168.4.1 in a browser\n"
        "and fill in Wi-Fi, Bambuddy + timezone details.",
        provision::AP_SSID, ap_pass);
    lv_label_set_text(sub, sub_txt);
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
    String pre_key = html_attr_escape(g_cfg_api_key);
    WiFiManagerParameter p_key("key", "Bambuddy API key",
        pre_key.c_str(), 79);

    // Cloudflare Access service token — only meaningful (and only saved) when
    // HTTPS is on; the script below reveals these two rows only then.
    WiFiManagerParameter p_cf_hint(
        "<p id=\"cf_hint\"><b>Cloudflare Access</b> (optional, HTTPS only): paste a "
        "service token to reach a Bambuddy published behind CF Access. Leave blank "
        "for a plain HTTPS endpoint.</p>");
    String pre_cf_id     = html_attr_escape(g_cfg_cf_id);
    String pre_cf_secret = html_attr_escape(g_cfg_cf_secret);
    WiFiManagerParameter p_cf_id("cf_id", "CF-Access-Client-Id",
        pre_cf_id.c_str(), 80);
    WiFiManagerParameter p_cf_secret("cf_secret", "CF-Access-Client-Secret",
        pre_cf_secret.c_str(), 96, "type=\"password\"");

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

    // Display units. Checkboxes default to the EU norm (Celsius, 24-hour);
    // ticking flips each. p_clock12 is "12-hour" so an unchecked box = 24h.
    WiFiManagerParameter p_tempf("tempf",
        "Show temperatures in Fahrenheit (\xC2\xB0" "F)", "T", 2,
        g_cfg_temp_f ? "type=\"checkbox\" checked" : "type=\"checkbox\"");
    WiFiManagerParameter p_clock12("clock12",
        "12-hour clock (AM/PM)", "T", 2,
        g_cfg_clock_24h ? "type=\"checkbox\"" : "type=\"checkbox\" checked");

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
    s_wm.addParameter(&p_tempf);
    s_wm.addParameter(&p_clock12);
    s_wm.addParameter(&p_js);

    s_wm.setConfigPortalTimeout(provision::PORTAL_TIMEOUT_S);
    s_wm.setBreakAfterConfig(true);

    if (!s_wm.startConfigPortal(provision::AP_SSID, ap_pass)) {
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
    netcfg::split_host_port(raw, vhost, vport);
    bool host_ok = netcfg::host_valid_for_scheme(vhost, use_https) &&
                   (vport.length() == 0 ||
                    (netcfg::digits_only(vport) && vport.toInt() >= 1 && vport.toInt() <= 65535));
    if (!host_ok) {
        log_w("Bambuddy host '%s' invalid for %s; reopening portal",
              raw.c_str(), use_https ? "https" : "http");
        delay(500);
        ESP.restart();   // nothing saved → the portal runs again on reboot
    }

    g_cfg_bambuddy_url = String(use_https ? "https://" : "http://") + raw;
    g_cfg_api_key      = p_key.getValue();
    g_cfg_api_key.trim();
    // Strip CR/LF: the key is emitted verbatim as the X-API-Key request header
    // (bambuddy_client.cpp); a pasted trailing newline must not be able to
    // inject extra HTTP headers. Mirrors the CF-token sanitisation below.
    g_cfg_api_key.replace("\r", ""); g_cfg_api_key.replace("\n", "");

    // The CF service token only applies to https; drop it on http so a stale
    // token can't linger and leak onto a plain-text connection.
    if (use_https) {
        g_cfg_cf_id     = p_cf_id.getValue();
        g_cfg_cf_secret = p_cf_secret.getValue();
        g_cfg_cf_id.trim();
        g_cfg_cf_secret.trim();
        // Strip CR/LF: the token is emitted verbatim as a request header
        // (CF-Access-Client-Id / -Secret), so a pasted newline must not be able
        // to inject extra headers.
        g_cfg_cf_id.replace("\r", "");     g_cfg_cf_id.replace("\n", "");
        g_cfg_cf_secret.replace("\r", ""); g_cfg_cf_secret.replace("\n", "");
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

    // Display units: temperature scale + clock format (checkbox = non-empty value).
    g_cfg_temp_f    = p_tempf.getValue()[0] != '\0';
    g_cfg_clock_24h = !(p_clock12.getValue()[0] != '\0');

    save_prefs();

    bambuddy::g_client.set_credentials(g_cfg_bambuddy_url, g_cfg_api_key,
                                       g_cfg_cf_id, g_cfg_cf_secret);
    delay(300);
    ESP.restart();   // clean restart with the new config
}

// ---------------------------------------------------------------------------
// Control-action command queue
// ---------------------------------------------------------------------------
//
// Touch handlers enqueue() a control action (ui/control.h); net_task drains it
// via control_process() and runs the blocking POST there — off the UI task
// (which the TWDT guards and which must keep LVGL responsive) and on the single
// task that already owns the REST client, so there is no cross-task race on it.

namespace {
struct CtrlCmd {
    uint8_t  op;
    int      pid;
    uint8_t  a;
    uint8_t  b;
    uint16_t c;
};
QueueHandle_t s_ctrl_q = nullptr;
}  // namespace

namespace ui::ctrl {
void enqueue(Op op, int printer_id, uint8_t a, uint8_t b, uint16_t c) {
    if (!s_ctrl_q) return;
    CtrlCmd cmd{(uint8_t)op, printer_id, a, b, c};
    xQueueSend(s_ctrl_q, &cmd, 0);   // non-blocking; a full queue drops the tap
}
}  // namespace ui::ctrl

// Drain + run every queued control action on the net task. Each maps to one
// REST POST plus a toast: the success string where one exists, the failure
// string always (reported back to the UI via the park-and-apply request_toast).
static void control_process() {
    if (!s_ctrl_q) return;
    CtrlCmd c;
    while (xQueueReceive(s_ctrl_q, &c, 0) == pdTRUE) {
        auto& cl = bambuddy::g_client;
        bool ok = false;
        i18n::Str ok_str   = i18n::Str::_COUNT;        // _COUNT sentinel = no toast
        i18n::Str fail_str = i18n::Str::CONTROL_FAILED;
        switch ((ui::ctrl::Op)c.op) {
            case ui::ctrl::Speed:
                ok = cl.set_print_speed(c.pid, c.a);
                fail_str = i18n::Str::SPEED_CHANGE_FAILED; break;
            case ui::ctrl::ClearPlate:
                ok = cl.clear_plate(c.pid);
                ok_str = i18n::Str::PLATE_CLEARED;
                fail_str = i18n::Str::CLEAR_PLATE_FAILED; break;
            case ui::ctrl::ClearHms:
                ok = cl.clear_hms(c.pid);
                ok_str = i18n::Str::HMS_CLEARED;
                fail_str = i18n::Str::CLEAR_HMS_FAILED; break;
            case ui::ctrl::Pause:  ok = cl.pause_print(c.pid);  break;
            case ui::ctrl::Resume: ok = cl.resume_print(c.pid); break;
            case ui::ctrl::Stop:   ok = cl.stop_print(c.pid);   break;
            case ui::ctrl::Light:  ok = cl.set_chamber_light(c.pid, c.a != 0); break;
            case ui::ctrl::DryStart:
                ok = cl.start_ams_drying(c.pid, c.a, c.c, c.b);   // a=unit c=min b=°C
                ok_str = i18n::Str::DRYING_STARTED;
                fail_str = i18n::Str::START_DRYING_FAILED; break;
            case ui::ctrl::DryStop:
                ok = cl.stop_ams_drying(c.pid, c.a);
                ok_str = i18n::Str::DRYING_STOPPED;
                fail_str = i18n::Str::STOP_DRYING_FAILED; break;
            case ui::ctrl::QueueCancel:
                ok = cl.cancel_queue_item(c.pid);   // pid carries the queue-item id
                ok_str = i18n::Str::QUEUE_REMOVED;
                fail_str = i18n::Str::QUEUE_REMOVE_FAILED; break;
        }
        i18n::Str s = ok ? ok_str : fail_str;
        if (s != i18n::Str::_COUNT) {
            ui::screens::request_toast(i18n::tr(s), ok ? ::ui::C_OK : ::ui::C_ERR);
        }
    }
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
    uint32_t next_queue_ms        = 0;
    uint32_t next_sysinfo_ms      = 0;

    for (;;) {
        uint32_t now = millis();
        if (WiFi.status() != WL_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Run any control actions the UI queued (pause/stop/clear/speed/light/
        // dry) here — the blocking POST stays off the watchdog-guarded UI task.
        control_process();

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
            // watch for leaks/fragmentation).
            log_i("diag: heap=%uK psram=%uK rssi=%d online=%d lat=%ums",
                  (unsigned)(ESP.getFreeHeap()  / 1024),
                  (unsigned)(ESP.getFreePsram() / 1024),
                  (int)WiFi.RSSI(), (int)ok, (unsigned)lat);
            next_health_ms = now + bambuddy::POLL_HEALTH_MS;
        }

        if (now >= next_printers_ms) {
            bambuddy::g_client.fetch_printers();
            next_printers_ms = now + bambuddy::POLL_LIST_MS;
        }

        if (now >= next_status_ms) {
            // REST-only: poll every printer ourselves at the snappy cadence so
            // the Printers screen stays responsive (the focused one first).
            int id = ui::g_ui.selected_printer_id();
            if (id >= 0) bambuddy::g_client.fetch_printer_status(id);
            bambuddy::Printer ps[8]; uint8_t n = 0;
            bambuddy::g_client.snapshot_printers(ps, n);
            for (uint8_t i = 0; i < n; ++i) {
                if (ps[i].id != id)
                    bambuddy::g_client.fetch_printer_status(ps[i].id);
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

        if (now >= next_queue_ms) {
            bambuddy::g_client.fetch_queue();
            next_queue_ms = now + bambuddy::POLL_QUEUE_MS;
        }

        if (now >= next_sysinfo_ms) {
            bambuddy::g_client.fetch_system_info();
            next_sysinfo_ms = now + bambuddy::POLL_SYSINFO_MS;
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
            // Snappier while the full-screen viewer is open (more "live"),
            // gentle while it only feeds the Live thumbnail.
            next_cam_ms = now + (ui::screens::camera_overlay_is_open() ? 2000u : 5000u);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void ui_task(void*) {
    uint32_t last_refresh       = 0;
    uint32_t last_touch_seen_ms = millis();
    bool     ota_confirmed      = false;   // anti-brick: clear the OTA verify once steady
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
        // Show any toast the net task requested (control-action results).
        ui::screens::pump_toast_request();
        // Commit a settled brightness change (deferred off the touch callback).
        brightness_flush();

        // Anti-brick: once the UI has run steadily for a while, confirm the
        // running image so the boot-verify won't roll it back. Internal health
        // only (no Wi-Fi / Bambuddy dependency), so an offline router can never
        // trigger a false rollback.
        if (!ota_confirmed && millis() > ::ota::VERIFY_HEALTHY_MS) {
            ota_confirmed = true;
            ota_mark_confirmed();
            // First-hardware bring-up: 8 KB overflowed this task on-device, so we
            // run 16 KB now. The high-water mark is monotonic, so this one-shot log
            // reports the worst-case headroom seen since boot (incl. first render).
            log_i("UI task min free stack: %u bytes",
                  (unsigned)uxTaskGetStackHighWaterMark(nullptr));
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
static volatile bool s_ota_armed_this_boot = false;

static void run_boot_update() {
    // A version we rolled back as a bad build: tell the updater to skip it so it
    // can't re-flash-and-rebrick in a loop (cleared once a newer image confirms).
    s_prefs.begin("bamboard", true);
    String bad_ver = s_prefs.getString("ota_bad_ver", "");
    s_prefs.end();

    s_ota_armed_this_boot = false;
    ota::CheckResult r = ota::check_and_update(
        /* on_start */ []() {
            // A newer image is confirmed and about to flash → arm the boot-verify
            // so the new image must prove itself healthy or be rolled back.
            ota_arm_pending();
            s_ota_armed_this_boot = true;
            ui::screens::ota_set_active(true);
            ui::screens::ota_set_progress(0);
            ui::screens::ota_apply();
            lv_timer_handler();
        },
        /* on_progress */ [](uint8_t pct) {
            ui::screens::ota_set_progress(pct);
            ui::screens::ota_apply();
            lv_timer_handler();
        },
        /* skip_version */ bad_ver.length() ? bad_ver.c_str() : nullptr);

    // If on_start armed the verify but we're still running, the flash didn't
    // reboot us (it failed) → disarm: we're still on the current good image.
    if (s_ota_armed_this_boot) ota_clear_pending();

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
    g_boot_reason = reset_reason_str(esp_reset_reason());  // shown on the Settings screen

    // Anti-brick: before anything else, decide whether the last OTA's image has
    // proven itself. If it keeps failing to confirm, this rolls the boot
    // partition back to the previous slot and reboots (never returns here).
    handle_ota_verify();

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

    // One-shot "reopen the captive portal without wiping" flag, set by the
    // Settings "Wi-Fi setup" button (reconfigure_wifi()). Consume it now so it
    // fires exactly once; start_provisioning() below then pre-fills the
    // preserved Bambuddy creds and the user only re-picks their Wi-Fi.
    s_prefs.begin("bamboard", false);
    bool force_portal = s_prefs.getUChar("reprov", 0) != 0;
    if (force_portal) s_prefs.remove("reprov");
    s_prefs.end();

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

    // Allow 2.4 GHz channels 12/13. The default Wi-Fi region tops out at channel
    // 11 (US), so an EU access point on auto-channel (12/13 — common in France)
    // shows up in a scan but can't be joined. Base "FR" permits 1-13.
    {
        // { cc, start-channel, num-channels, max-tx-power(0.25 dBm), policy }
        wifi_country_t ctry = { "FR", 1, 13, 84, WIFI_COUNTRY_POLICY_MANUAL };
        esp_wifi_set_country(&ctry);
    }

    if (force_portal || g_cfg_bambuddy_url.length() == 0 ||
        g_cfg_api_key.length() == 0) {
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
    ui::screens::boot_overlay_set_status(::i18n::tr(::i18n::Str::BOOT_UPDATE));
    lv_timer_handler();
#if BAMBOARD_OTA_AUTOCHECK
    if (!BAMBOARD_VERSION_IS_DEV) {
        run_boot_update();
    } else {
        log_i("OTA: dev build (%s) — skipping boot update check", BAMBOARD_VERSION);
    }
#endif

    bambuddy::g_client.begin(g_cfg_bambuddy_url, g_cfg_api_key,
                             g_cfg_cf_id, g_cfg_cf_secret);

    // Final boot phase: the net task (started just below) reaches Bambuddy. Show
    // it on the splash; the UI manager dismisses the splash once data lands.
    ui::screens::boot_overlay_set_status(::i18n::tr(::i18n::Str::BOOT_CONNECT));
    lv_timer_handler();

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
    // Create the control-action queue before net_task starts draining it.
    s_ctrl_q = xQueueCreate(8, sizeof(CtrlCmd));
    xTaskCreatePinnedToCore(net_task, "net", 16384, nullptr, 1, nullptr, 0);
    // 16 KB for the UI task: lv_timer_handler() runs a deep call chain when
    // several widgets invalidate at once, and each per-screen refresh copies a
    // local Printer[MAX_PRINTERS] snapshot (~3 KB) onto this stack. 8 KB looked
    // fine in the host sim (effectively unbounded stack) but overflowed on real
    // hardware during the first dashboard render — the LVGL + NV3041A flush path
    // is deeper on-device. 16 KB matches the net task and clears it with margin
    // (configCHECK_FOR_STACK_OVERFLOW panics the task if it's ever breached).
    xTaskCreatePinnedToCore(ui_task,  "ui",  16384, nullptr, 2, nullptr, 1);
}

void loop() {
    // All work happens in tasks; idle the Arduino loop.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
