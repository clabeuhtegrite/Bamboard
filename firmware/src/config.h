// Bamboard - compile-time configuration
//
// Pin mapping, timings and UI defaults live here. Anything secret (Bambuddy
// URL, API key, Wi-Fi credentials) is *not* stored here — it's provisioned at
// runtime via the WiFiManager captive portal and saved in NVS (Preferences).
//
// v0.3+ targets the Guition JC4827W543 all-in-one board (ESP32-S3-WROOM-1
// with a 4.3" RGB-parallel IPS panel + GT911 capacitive touch on the same
// PCB). The display + touch pinout lives in `hw/display.cpp` because it's
// wide enough to deserve its own file.

#pragma once

#include <Arduino.h>

// ---------- Firmware version --------------------------------------------
//
// The build injects the version as a *bare* token, e.g. -DBAMBOARD_FW_VERSION=1.2.0
// (no quotes — a single pp-number, which the stringify macro turns into a
// string literal). CI derives it from the pushed git tag. A local build with
// no tag falls back to the dev sentinel below, which disables the boot-time
// auto-update so a USB-flashed work-in-progress isn't immediately pulled back
// to the latest published release.

#define BB_STRINGIFY2(x) #x
#define BB_STRINGIFY(x)  BB_STRINGIFY2(x)

#ifdef BAMBOARD_FW_VERSION
#  define BAMBOARD_VERSION        BB_STRINGIFY(BAMBOARD_FW_VERSION)
#  define BAMBOARD_VERSION_IS_DEV 0
#else
#  define BAMBOARD_VERSION        "0.0.0-dev"
#  define BAMBOARD_VERSION_IS_DEV 1
#endif

// ---------- Hardware ----------------------------------------------------

namespace pins {

// The only physical button still in the firmware: the side-mounted BOOT
// switch on the Guition PCB. We sample it once at boot to detect a
// factory-reset gesture; LVGL touch handles every other interaction.
constexpr uint8_t BOOT_BUTTON = 0;

// Display backlight PWM. LovyanGFX configures the channel itself; these
// constants are the source of truth that hw/display.cpp pulls from when
// building its Light_PWM config.
constexpr uint8_t  BL_PIN     = 2;
constexpr uint32_t BL_FREQ    = 5000;
constexpr uint8_t  BL_RES     = 8;

// GT911 capacitive touch controller on the Guition JC4827W543. The board
// routes the touch IC onto a dedicated I²C bus that doesn't clash with the
// native USB lines (S3 USB lives on the same physical USB-C connector as
// power). INT is not wired on this revision; RST is on GPIO 38.
constexpr int      GT911_SDA  = 19;
constexpr int      GT911_SCL  = 20;
constexpr int      GT911_INT  = -1;    // GPIO_NUM_NC equivalent
constexpr int      GT911_RST  = 38;
constexpr uint32_t GT911_FREQ = 400000;
constexpr uint8_t  GT911_ADDR = 0x5D;  // primary; 0x14 fallback
constexpr uint8_t  GT911_PORT = 0;

}  // namespace pins

// ---------- Display ------------------------------------------------------

namespace display {

constexpr uint16_t WIDTH  = 480;
constexpr uint16_t HEIGHT = 272;

// LVGL draw buffer height in lines. ~40 lines * 480 px * 2 B/px ≈ 38 KB,
// which we allocate from PSRAM on ESP32-S3.
constexpr uint16_t DRAW_BUF_LINES = 40;

// Auto-dim: backlight drops after this many ms of no touch activity.
constexpr uint32_t DIM_AFTER_MS = 60UL * 1000UL;
constexpr uint8_t  BL_FULL = 255;
constexpr uint8_t  BL_DIM  = 40;

// User-facing brightness levels (1..5). The Settings screen lets the user
// pick one; the chosen level is stored in NVS as `bl_level`, applied at
// boot, and used as the "wake" target by the auto-dim logic instead of
// the hard-coded BL_FULL. Level 3 (≈ 50 %) is the default first-boot
// value — comfortable on a desk in normal light.
constexpr uint8_t  BL_LEVEL_DEFAULT = 3;
constexpr uint8_t  BL_LEVEL_MIN     = 1;
constexpr uint8_t  BL_LEVEL_MAX     = 5;

// PWM duty for each level. Hand-tuned so step 1 is still readable in a
// dark room and step 5 is full whack for sunny offices. Anything brighter
// than ~225 starts to wash out the IPS contrast on the JC4827W543 panel,
// so we cap there rather than running at 255.
constexpr uint8_t  BL_LEVELS[5] = { 32, 72, 128, 180, 225 };

inline uint8_t bl_pwm_for_level(uint8_t level) {
    if (level < BL_LEVEL_MIN) level = BL_LEVEL_MIN;
    if (level > BL_LEVEL_MAX) level = BL_LEVEL_MAX;
    return BL_LEVELS[level - 1];
}

}  // namespace display

// ---------- Bambuddy client ---------------------------------------------

namespace bambuddy {

// HTTP timeouts
constexpr uint32_t CONNECT_TIMEOUT_MS = 4000;
constexpr uint32_t READ_TIMEOUT_MS    = 6000;

// Camera stream-token reuse window. Bambuddy issues a snapshot/stream token
// valid for 60 min; we refresh a little early (50 min) and also re-fetch on a
// 401. Reusing it avoids a POST before every ~5 s snapshot.
constexpr uint32_t CAMERA_TOKEN_TTL_MS = 50UL * 60UL * 1000UL;

// Poll cadence per screen (the active screen polls more aggressively than
// background screens). Bambuddy's read rate-limit is 100/min so we stay well
// under it.
constexpr uint32_t POLL_DASHBOARD_MS = 2000;     // active dashboard, no WS
constexpr uint32_t POLL_LIST_MS      = 5000;
constexpr uint32_t POLL_STATS_MS     = 30000;
constexpr uint32_t POLL_HEALTH_MS    = 15000;
constexpr uint32_t POLL_QUEUE_MS     = 15000;    // print-queue refresh
constexpr uint32_t POLL_SYSINFO_MS   = 60000;    // Bambuddy version / uptime

// When the WebSocket is connected the server pushes printer_status frames
// on every MQTT delta, so REST polling becomes a low-cadence safety net.
constexpr uint32_t POLL_DASHBOARD_WS_MS = 30000;

// Full-screen HMS-error flash: how long each pulse is shown, and how long
// to stay quiet between pulses while the error persists.
constexpr uint32_t HMS_FLASH_VISIBLE_MS  = 5000;
constexpr uint32_t HMS_FLASH_COOLDOWN_MS = 30000;

// Max printers we keep in RAM (more than enough for desk-side monitoring).
constexpr uint8_t  MAX_PRINTERS = 8;

// Max recent archives we display.
constexpr uint8_t  MAX_RECENT_ARCHIVES = 10;

// Max pending print-queue items we keep / show.
constexpr uint8_t  MAX_QUEUE_ITEMS = 10;

}  // namespace bambuddy

// ---------- UI ----------------------------------------------------------

namespace ui {

// Bamboard accent colours (chosen to read well on the IPS panel).
constexpr uint32_t C_BG          = 0x0E1116;
constexpr uint32_t C_PANEL       = 0x1A1F26;
constexpr uint32_t C_PANEL_HI    = 0x252B34;
constexpr uint32_t C_PANEL_LINE  = 0x2F3742;   // hairline borders / dividers
constexpr uint32_t C_ACCENT      = 0x00B7C3;   // teal — primary action / focus
constexpr uint32_t C_ACCENT_DARK = 0x017782;
constexpr uint32_t C_OK          = 0x39D98A;
constexpr uint32_t C_WARN        = 0xF5A524;
constexpr uint32_t C_ERR         = 0xE5484D;
constexpr uint32_t C_TEXT        = 0xE9EEF3;
constexpr uint32_t C_TEXT_DIM    = 0x8A95A4;
constexpr uint32_t C_TEXT_INV    = 0x0E1116;   // text on top of accent fill

// v0.10 visual refresh — depth + accent tokens.
// Cards get a subtle top-to-bottom gradient (TOP lighter than BOT) plus a
// crisp hairline for elevation; we avoid heavy blur shadows to protect the
// ESP32 frame rate. The accent gradient (C_ACCENT -> C_ACCENT_DARK) fills
// the active chip / primary button. C_TINT is a dark-teal wash used behind
// the active tab cell and the dashboard state pill.
constexpr uint32_t C_PANEL_GRAD_TOP = 0x2B313B;
constexpr uint32_t C_PANEL_GRAD_BOT = 0x222831;
constexpr uint32_t C_TINT           = 0x0E2A2E;   // dark-teal accent wash
constexpr uint32_t C_ACCENT_HI      = 0x2BE0EC;   // bright accent (pill text)

// Shared radii so every panel / button / pill agrees with the others.
// Bumping these in one place reshapes the whole UI consistently.
constexpr uint8_t  R_PANEL      = 12;
constexpr uint8_t  R_BUTTON     = 14;
constexpr uint8_t  R_PILL       = 18;
constexpr uint8_t  R_CHIP       =  8;

// Standard touch-target heights — keep ≥ 40 px so fat fingers never miss.
constexpr uint8_t  H_BTN        = 44;
constexpr uint8_t  H_CHIP       = 30;

// Tab bar (bottom of every screen): a row of icons + labels, fixed
// at 44 px high. The active tab is highlighted in C_ACCENT.
constexpr uint16_t TAB_BAR_H = 44;
constexpr uint16_t HEADER_H  = 36;

}  // namespace ui

// ---------- OTA (GitHub Releases) ---------------------------------------
//
// Updates are pulled straight from this repo's GitHub Releases. At boot the
// device fetches a small manifest from the stable "latest release" redirect,
// compares its version against BAMBOARD_VERSION, and — if a newer release
// exists — downloads and flashes the firmware .bin before normal operation
// begins (the update is mandatory). The CI workflow in
// .github/workflows/release.yml builds the .bin and publishes the manifest
// on every pushed `v*` tag.

namespace ota {

// Stable URL that always resolves to the newest release's manifest asset.
// GitHub 302-redirects /releases/latest/download/<asset> to the versioned
// asset, so this never has to change between releases.
constexpr const char* MANIFEST_URL =
    "https://github.com/clabeuhtegrite/Bamboard/releases/latest/download/manifest.json";

// Give up the check after this long. An offline device — or a GitHub outage —
// must still fall through to normal operation instead of hanging at boot.
constexpr uint32_t CHECK_TIMEOUT_MS = 8000;

// Master switch for the boot-time check. Shipped firmware leaves it on; a dev
// build can pass -DBAMBOARD_OTA_AUTOCHECK=0 to opt out. Dev-sentinel builds
// skip the check regardless (see BAMBOARD_VERSION_IS_DEV).
#ifndef BAMBOARD_OTA_AUTOCHECK
#  define BAMBOARD_OTA_AUTOCHECK 1
#endif

}  // namespace ota

// ---------- Scheduled daily reboot --------------------------------------
//
// The device reboots itself once a day so the boot-time GitHub OTA check
// (see the `ota` namespace) runs unattended overnight — a new release lands
// without anyone power-cycling the device or tapping a prompt. The wall clock
// comes from SNTP; the reboot fires at local DAILY_REBOOT_HOUR:00.

namespace schedule {

// DAILY_REBOOT_ENABLED / DAILY_REBOOT_HOUR / TZ below are first-boot DEFAULTS.
// The captive portal lets the user override the timezone and reboot hour at
// runtime (NVS keys `tz` / `reboot_h`); those take precedence over these.
constexpr bool     DAILY_REBOOT_ENABLED = true;
constexpr uint8_t  DAILY_REBOOT_HOUR    = 0;     // 0 = local midnight
constexpr uint8_t  REBOOT_DISABLED      = 255;   // NVS `reboot_h` value = off

// Ignore the reboot window until the device has been up this long, so a
// reboot that lands inside the 00:00 minute can't immediately re-trigger
// (anti-loop guard — the 00:00 minute is gone by the time this elapses).
constexpr uint32_t MIN_UPTIME_MS = 120UL * 1000UL;

// How often the net task compares the wall clock against the reboot window.
constexpr uint32_t CHECK_INTERVAL_MS = 20UL * 1000UL;

// POSIX TZ string for "local" time. Default Europe/Paris (CET/CEST, EU DST).
// Change for your locale, e.g.:
//   "GMT0BST,M3.5.0/1,M10.5.0"   London     "EST5EDT,M3.2.0,M11.1.0"  US Eastern
//   "CST6CDT,M3.2.0,M11.1.0"     US Central "UTC0"                    plain UTC
constexpr const char* TZ          = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr const char* NTP_SERVER1 = "pool.ntp.org";
constexpr const char* NTP_SERVER2 = "time.nist.gov";

}  // namespace schedule

// ---------- Wi-Fi provisioning ------------------------------------------

namespace provision {

constexpr const char* AP_SSID     = "Bamboard-setup";
constexpr const char* AP_PASSWORD = "bamboard";
constexpr uint32_t    PORTAL_TIMEOUT_S = 300;

}  // namespace provision
