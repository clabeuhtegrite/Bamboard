// Stub for hw::Display so ui.cpp can call hw::g_display.set_backlight()
// etc. without dragging in LovyanGFX. The real display init lives in
// sim/main.cpp (SDL2 + LVGL); these methods just print what would have
// happened to the backlight PWM on the hardware.

#include "../../firmware/src/hw/display.h"

#include <cstdio>

namespace hw {

Display g_display;

bool Display::begin() {
    // The real init runs from sim/main.cpp before LVGL starts; here we
    // just acknowledge the call.
    return true;
}

void Display::tick() {
    // The real tick is `lv_timer_handler()`, which the sim drives itself
    // from the SDL event loop — leave this a no-op.
}

void Display::set_backlight(uint8_t v) {
    if (v != backlight_) {
        std::fprintf(stderr, "[sim] backlight -> %u\n", (unsigned)v);
    }
    backlight_ = v;
}

void Display::set_brightness_level(uint8_t level) {
    if (level < 1) level = 1;
    if (level > 5) level = 5;
    level_ = level;
    static const uint8_t kPwm[5] = {32, 72, 128, 180, 225};
    set_backlight(kPwm[level - 1]);
}

bool Display::consume_touch_activity() {
    // The sim never auto-dims; the SDL window is always lit.
    return false;
}

}  // namespace hw
