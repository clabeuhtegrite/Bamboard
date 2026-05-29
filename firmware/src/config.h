// Bamboard - compile-time configuration
//
// Pin mapping, timings and UI defaults live here. Anything secret (Bambuddy
// URL, API key, Wi-Fi credentials) is *not* stored here — it's provisioned at
// runtime via the WiFiManager captive portal and saved in NVS (Preferences).
//
// v1.0+ targets the Guition JC4827W543 all-in-one board (ESP32-S3-WROOM-1
// with a 4.3" RGB-parallel IPS panel + GT911 capacitive touch on the same
// PCB). The display + touch pinout lives in `hw/display.cpp` because it's
// wide enough to deserve its own file.

#pragma once

#include <Arduino.h>

// ---------- Hardware ----------------------------------------------------

namespace pins {

// The only physical button still in the firmware: the side-mounted BOOT
// switch on the Guition PCB. We sample it once at boot to detect a
// factory-reset gesture; LVGL touch handles every other interaction.
constexpr uint8_t BOOT_BUTTON = 0;

// Display backlight PWM (LovyanGFX drives this internally, but we keep
// the constants here so other firmware code can reference them if needed).
constexpr uint8_t BL_PIN     = 2;
constexpr uint32_t BL_FREQ   = 5000;
constexpr uint8_t  BL_RES    = 8;

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
// the hard-coded BL_FULL. Level 3 (≈ 70 %) is the default first-boot
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

// Poll cadence per screen (the active screen polls more aggressively than
// background screens). Bambuddy's read rate-limit is 100/min so we stay well
// under it.
constexpr uint32_t POLL_DASHBOARD_MS = 2000;     // active dashboard, no WS
constexpr uint32_t POLL_LIST_MS      = 5000;
constexpr uint32_t POLL_STATS_MS     = 30000;
constexpr uint32_t POLL_HEALTH_MS    = 15000;

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

// ---------- ArduinoOTA --------------------------------------------------

namespace ota {

constexpr const char* HOSTNAME = "bamboard";

#ifdef OTA_PASSWORD_OVERRIDE
constexpr const char* PASSWORD = OTA_PASSWORD_OVERRIDE;
#else
constexpr const char* PASSWORD = "bamboard";
#endif

}  // namespace ota

// ---------- Wi-Fi provisioning ------------------------------------------

namespace provision {

constexpr const char* AP_SSID     = "Bamboard-setup";
constexpr const char* AP_PASSWORD = "bamboard";
constexpr uint32_t    PORTAL_TIMEOUT_S = 300;

}  // namespace provision
