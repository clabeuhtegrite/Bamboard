// Bamboard - compile-time configuration
//
// Pin mapping, timings and UI defaults live here. Anything secret (Bambuddy
// URL, API key, Wi-Fi credentials) is *not* stored here — it's provisioned at
// runtime via the WiFiManager captive portal and saved in NVS (Preferences).
//
// If you fork this project for a different display or MCU, this is the only
// file you should normally need to touch, alongside include/User_Setup.h.

#pragma once

#include <Arduino.h>

// ---------- Hardware ----------------------------------------------------

namespace pins {

// Buttons (active-low, internal pull-up)
constexpr uint8_t BTN_PREV = 4;
constexpr uint8_t BTN_OK   = 5;
constexpr uint8_t BTN_NEXT = 6;

// WS2812 status LED module (single LED, plug-in module: VCC / GND / DIN)
constexpr uint8_t LED_DIN  = 7;
constexpr uint8_t LED_COUNT = 1;

// Display backlight PWM channel (LEDC)
constexpr uint8_t BL_CHANNEL = 0;
constexpr uint32_t BL_FREQ   = 5000;   // Hz
constexpr uint8_t BL_RES     = 8;      // bits

}  // namespace pins

// ---------- Display ------------------------------------------------------

namespace display {

constexpr uint16_t WIDTH  = 480;
constexpr uint16_t HEIGHT = 320;

// LVGL draw buffer height in lines. ~40 lines * 480 px * 2 B/px ≈ 38 KB,
// which we allocate from PSRAM on ESP32-S3.
constexpr uint16_t DRAW_BUF_LINES = 40;

// Auto-dim
constexpr uint32_t DIM_AFTER_MS = 60UL * 1000UL;   // dim after 1 min idle
constexpr uint8_t  BL_FULL = 255;
constexpr uint8_t  BL_DIM  = 40;

}  // namespace display

// ---------- Buttons ------------------------------------------------------

namespace buttons {

constexpr uint16_t DEBOUNCE_MS     = 25;
constexpr uint16_t LONG_PRESS_MS   = 600;
constexpr uint16_t REPEAT_MS       = 180;
constexpr uint16_t REPEAT_AFTER_MS = 700;
// Max gap between two releases that still counts as a double-click. Short
// enough to feel intentional, long enough to be reachable with the
// chunky tactiles on the case.
constexpr uint16_t DOUBLE_CLICK_MS = 350;

}  // namespace buttons

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
constexpr uint32_t POLL_DASHBOARD_WS_MS = 30000; // WS push is primary

// Max printers we keep in RAM (more than enough for desk-side monitoring).
constexpr uint8_t  MAX_PRINTERS = 8;

// Max recent archives we display.
constexpr uint8_t  MAX_RECENT_ARCHIVES = 10;

}  // namespace bambuddy

// ---------- UI ----------------------------------------------------------

namespace ui {

// Bamboard accent colours (chosen to read well on the IPS panel).
constexpr uint32_t C_BG        = 0x101418;
constexpr uint32_t C_PANEL     = 0x1B2027;
constexpr uint32_t C_PANEL_HI  = 0x232A33;
constexpr uint32_t C_ACCENT    = 0x00B7C3;   // teal
constexpr uint32_t C_OK        = 0x39D98A;
constexpr uint32_t C_WARN      = 0xF5A524;
constexpr uint32_t C_ERR       = 0xE5484D;
constexpr uint32_t C_TEXT      = 0xE9EEF3;
constexpr uint32_t C_TEXT_DIM  = 0x8A95A4;

}  // namespace ui

// ---------- Wi-Fi provisioning ------------------------------------------

namespace provision {

constexpr const char* AP_SSID     = "Bamboard-setup";
constexpr const char* AP_PASSWORD = "bamboard";   // change on first boot
constexpr uint32_t    PORTAL_TIMEOUT_S = 300;

}  // namespace provision
