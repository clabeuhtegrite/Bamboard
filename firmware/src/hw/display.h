// Display + touch HAL for the Guition JC4827W543.
//
// Wraps LovyanGFX (which drives the 480×272 RGB-parallel IPS panel via the
// ESP32-S3's LCD_CAM peripheral and reads the GT911 capacitive touch
// controller over I²C), and hooks both into LVGL: a display flush callback
// pushes draw buffers to the panel, and a pointer input device feeds touch
// coordinates into LVGL's event system.
//
// Higher layers (ui/) talk to LVGL directly; they don't see LovyanGFX at
// all. The only thing they call here is set_backlight() to drive the
// auto-dim behaviour.

#pragma once

#include <Arduino.h>

#include "../config.h"

namespace hw {

class Display {
   public:
    // Bring up the panel, register the LVGL display + touch input devices,
    // and set the backlight to full. Returns false if the panel init failed.
    bool begin();

    // Call once per UI loop iteration: dispatches LVGL timers (which run
    // both the display flush and the touch input read).
    void tick();

    // PWM the backlight between 0 and 255. The auto-dim logic in main.cpp
    // calls this; nothing else should.
    void set_backlight(uint8_t value);

    // Current backlight value (cached locally to avoid PWM read-back).
    uint8_t backlight() const { return backlight_; }

    // User-facing brightness level (1..5). Stored centrally so both the
    // Settings UI and the auto-dim wake path agree on the same "full"
    // value. set_brightness_level() persists nothing on its own — the
    // caller is responsible for writing it to NVS.
    uint8_t brightness_level() const { return level_; }
    void    set_brightness_level(uint8_t level);   // applies PWM immediately

    // True when the touch driver reported any contact since the last call.
    // Resets to false after each read — used by main.cpp's auto-dim timer
    // to wake the screen on any user interaction.
    bool consume_touch_activity();

   private:
    uint8_t backlight_ = ::display::BL_FULL;
    uint8_t level_     = ::display::BL_LEVEL_DEFAULT;
};

extern Display g_display;

}  // namespace hw
