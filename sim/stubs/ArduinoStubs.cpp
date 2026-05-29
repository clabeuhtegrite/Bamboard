// Out-of-line bits of the Arduino shim (see stubs/Arduino.h).

#include "Arduino.h"

#include <SDL.h>

#include <chrono>
#include <thread>

// LVGL's lv_tick.c calls millis() with C linkage; the firmware sources
// call it from C++. Single `extern "C"` definition keeps both happy.
extern "C" {

uint32_t millis(void) {
    // SDL_GetTicks is monotonic, starts at 0, matches Arduino's millis()
    // contract closely enough for everything firmware/src/ui/ uses.
    return SDL_GetTicks();
}

void delay(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

}  // extern "C"

// Single global the firmware references.
WiFiClass WiFi;

// firmware/src/ui/screens.cpp pokes `g_cfg_bambuddy_url` (declared
// `extern` near the bottom of update_settings()). Provide it so the
// Settings screen has something to render.
String g_cfg_bambuddy_url("http://192.168.1.42:8000");

// Stub for the firmware's factory reset hook, called by the Settings
// screen's two-tap confirm button. On the device this wipes NVS and
// reboots; on the sim we just print a notice — the window stays open.
void factory_reset() {
    std::fprintf(stderr, "[sim] factory_reset() — would wipe NVS and reboot on device\n");
}
