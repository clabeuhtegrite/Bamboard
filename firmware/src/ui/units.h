// Bamboard display-unit helpers — temperature unit (°C/°F) and clock format
// (24-hour / 12-hour). The preference is set in the captive portal and stored
// in NVS, mirrored into the two globals below at boot; the UI reads them
// through the thin wrappers here so every temperature / clock site agrees.
//
// The pure cores (to_fahrenheit / format_clock_core) take their inputs
// explicitly so they're unit-testable on the host with no global state.

#pragma once

#include <cstddef>
#include <cstdio>

// Set once at boot from NVS (firmware: main.cpp; host sim: sim/shim/shims.cpp).
extern bool g_cfg_temp_f;     // render temperatures in °F when true, else °C
extern bool g_cfg_clock_24h;  // 24-hour clock when true, else 12-hour AM/PM

namespace ui {

// --- pure cores (unit-tested) ----------------------------------------------
inline float to_fahrenheit(float celsius) { return celsius * 9.0f / 5.0f + 32.0f; }

// Write "HH:MM" (24h) or "H:MM AM/PM" (12h) into buf. hour is 0..23.
inline void format_clock_core(char* buf, size_t n, int hour, int min, bool clock24) {
    if (clock24) {
        snprintf(buf, n, "%02d:%02d", hour, min);
        return;
    }
    int h12 = hour % 12;
    if (h12 == 0) h12 = 12;                 // 0h and 12h both display as 12
    snprintf(buf, n, "%d:%02d %s", h12, min, hour < 12 ? "AM" : "PM");
}

// --- active-preference wrappers (read the globals) -------------------------
// Convert a Celsius measurement into the user's chosen unit for display.
inline float temp_value(float celsius) {
    return g_cfg_temp_f ? to_fahrenheit(celsius) : celsius;
}
// UTF-8 "°C" / "°F" suffix to pair with temp_value().
inline const char* temp_unit() { return g_cfg_temp_f ? "\xC2\xB0" "F" : "\xC2\xB0" "C"; }

// Upper bound for the temperature-graph Y axis in the active unit (the chart
// plots converted values, so the range has to follow the unit).
inline int temp_axis_max() { return g_cfg_temp_f ? 580 : 300; }

// Format a wall-clock time honouring the 24h/12h preference.
inline void format_clock(char* buf, size_t n, int hour, int min) {
    format_clock_core(buf, n, hour, min, g_cfg_clock_24h);
}

}  // namespace ui
