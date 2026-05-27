// LVGL ⇄ TFT_eSPI bridge plus backlight handling.
//
// Encapsulates the boilerplate needed to wire LVGL to TFT_eSPI on the
// ILI9488. The draw buffer is allocated from PSRAM (ESP32-S3 octal PSRAM)
// so we have room for a roomy two-line buffer without blowing internal SRAM.

#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace hw {

class Display {
   public:
    bool begin();

    // Backlight 0..255. Used by the auto-dim logic.
    void set_backlight(uint8_t value);
    uint8_t backlight() const { return current_bl_; }

    // Call from the main loop or from the LVGL task at >= 60 Hz.
    void tick();

   private:
    static void flush_cb_(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* buf);

    uint8_t current_bl_ = 0;
};

extern Display g_display;

}  // namespace hw
