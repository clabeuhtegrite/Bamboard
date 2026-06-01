// Display + touch HAL for the Guition JC4827W543.
//
// The pin map below targets the JC4827W543**C** (capacitive) revision and
// is taken from Guition's reference Arduino sketch + the well-known
// rzeldent/esp32-smartdisplay board configs. If your specific board boots
// to a black screen or shows colour banding, double-check the silkscreen
// next to the FPC connector — earlier (`R` / resistive) revisions and the
// 800×480 sibling (JC8048W550) use a different mapping.

#include "display.h"

// LGFX_USE_V1 is also set in platformio.ini build_flags; guard the define so
// this translation unit doesn't trigger a "macro redefined" warning.
#ifndef LGFX_USE_V1
#define LGFX_USE_V1
#endif
#include <LovyanGFX.hpp>

// The ESP32-S3 RGB-parallel bus and its framebuffer panel are NOT pulled in by
// <LovyanGFX.hpp>: device.hpp only wires the SPI / I2C / Parallel8 / Parallel16
// buses for the S3, so lgfx::Bus_RGB / lgfx::Panel_RGB are otherwise undefined.
// Including them explicitly is the canonical LovyanGFX pattern for custom RGB
// panels (the JC4827W543 drives the ST7262 over a 16-bit RGB bus).
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

#include <esp_heap_caps.h>
#include <lvgl.h>

#include "../config.h"

namespace hw {

Display g_display;

// ---------- LovyanGFX board class ------------------------------------------

class LGFX_JC4827W543 : public lgfx::LGFX_Device {
   public:
    lgfx::Bus_RGB     bus_;
    lgfx::Panel_RGB   panel_;
    lgfx::Light_PWM   light_;
    lgfx::Touch_GT911 touch_;

    LGFX_JC4827W543() {
        {  // ---- RGB bus pin map ----
            auto cfg = bus_.config();
            cfg.panel = &panel_;

            // Sync / data-enable / pixel clock
            cfg.pin_d0  = GPIO_NUM_8;   // B0
            cfg.pin_d1  = GPIO_NUM_3;   // B1
            cfg.pin_d2  = GPIO_NUM_46;  // B2
            cfg.pin_d3  = GPIO_NUM_9;   // B3
            cfg.pin_d4  = GPIO_NUM_1;   // B4

            cfg.pin_d5  = GPIO_NUM_5;   // G0
            cfg.pin_d6  = GPIO_NUM_6;   // G1
            cfg.pin_d7  = GPIO_NUM_7;   // G2
            cfg.pin_d8  = GPIO_NUM_15;  // G3
            cfg.pin_d9  = GPIO_NUM_16;  // G4
            cfg.pin_d10 = GPIO_NUM_4;   // G5

            cfg.pin_d11 = GPIO_NUM_45;  // R0
            cfg.pin_d12 = GPIO_NUM_48;  // R1
            cfg.pin_d13 = GPIO_NUM_47;  // R2
            cfg.pin_d14 = GPIO_NUM_21;  // R3
            cfg.pin_d15 = GPIO_NUM_14;  // R4

            cfg.pin_henable = GPIO_NUM_40;
            cfg.pin_vsync   = GPIO_NUM_41;
            cfg.pin_hsync   = GPIO_NUM_39;
            cfg.pin_pclk    = GPIO_NUM_42;

            // Timing — values from the ST7262 datasheet, validated by the
            // Guition example sketch.
            cfg.freq_write  = 14000000;
            cfg.hsync_polarity = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4;
            cfg.hsync_back_porch  = 43;
            cfg.vsync_polarity = 0;
            cfg.vsync_front_porch = 8;
            cfg.vsync_pulse_width = 4;
            cfg.vsync_back_porch  = 12;
            cfg.pclk_active_neg = 1;
            cfg.de_idle_high    = 0;
            cfg.pclk_idle_high  = 0;
            bus_.config(cfg);
        }

        {  // ---- Panel geometry ----
            auto cfg = panel_.config();
            cfg.memory_width  = ::display::WIDTH;
            cfg.memory_height = ::display::HEIGHT;
            cfg.panel_width   = ::display::WIDTH;
            cfg.panel_height  = ::display::HEIGHT;
            cfg.offset_x      = 0;
            cfg.offset_y      = 0;
            panel_.config(cfg);
        }

        {  // ---- Backlight (PWM on GPIO 2) ----
            auto cfg = light_.config();
            cfg.pin_bl   = pins::BL_PIN;
            cfg.invert   = false;
            cfg.freq     = pins::BL_FREQ;
            cfg.pwm_channel = 7;
            light_.config(cfg);
            panel_.setLight(&light_);
        }

        {  // ---- GT911 capacitive touch (I²C) ----
            // The JC4827W543C routes the GT911 onto a dedicated I²C bus that
            // doesn't conflict with native USB. Pin map lives in config.h's
            // `pins::` namespace so a fork targeting a different board only
            // edits one file.
            auto cfg = touch_.config();
            cfg.x_min = 0;
            cfg.x_max = ::display::WIDTH  - 1;
            cfg.y_min = 0;
            cfg.y_max = ::display::HEIGHT - 1;
            cfg.pin_sda  = (gpio_num_t)pins::GT911_SDA;
            cfg.pin_scl  = (gpio_num_t)pins::GT911_SCL;
            cfg.pin_int  = (gpio_num_t)(pins::GT911_INT < 0 ? GPIO_NUM_NC
                                                            : pins::GT911_INT);
            cfg.pin_rst  = (gpio_num_t)pins::GT911_RST;
            cfg.freq     = pins::GT911_FREQ;
            cfg.i2c_addr = pins::GT911_ADDR;
            cfg.i2c_port = pins::GT911_PORT;
            touch_.config(cfg);
            panel_.setTouch(&touch_);
        }

        setPanel(&panel_);
    }
};

static LGFX_JC4827W543 s_tft;

// ---------- LVGL plumbing --------------------------------------------------

static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t      s_disp_drv;
static lv_indev_drv_t     s_indev_drv;
static volatile bool      s_touch_activity = false;

static void flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* buf) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    s_tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t*)buf);
    lv_disp_flush_ready(drv);
}

static void touch_read_cb(lv_indev_drv_t*, lv_indev_data_t* data) {
    uint16_t x, y;
    if (s_tft.getTouch(&x, &y)) {
        data->state   = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
        s_touch_activity = true;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ---------- Public API -----------------------------------------------------

bool Display::begin() {
    if (!s_tft.init()) {
        log_e("LovyanGFX init failed");
        return false;
    }
    s_tft.setRotation(0);
    s_tft.fillScreen(0x0000);   // black
    set_backlight(0);     // stay dark until LVGL has drawn at least one frame

    lv_init();

    // Two DMA-capable draw buffers in PSRAM — about 38 KB each at 480 × 40.
    size_t pixels = ::display::WIDTH * ::display::DRAW_BUF_LINES;
    size_t bytes  = pixels * sizeof(lv_color_t);
    lv_color_t* buf1 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    lv_color_t* buf2 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    if (!buf1 || !buf2) {
        log_e("Failed to allocate LVGL draw buffers (have PSRAM?)");
        return false;
    }
    lv_disp_draw_buf_init(&s_draw_buf, buf1, buf2, pixels);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res  = ::display::WIDTH;
    s_disp_drv.ver_res  = ::display::HEIGHT;
    s_disp_drv.flush_cb = flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);

    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&s_indev_drv);

    set_backlight(::display::BL_FULL);
    return true;
}

void Display::tick() {
    lv_timer_handler();
}

void Display::set_backlight(uint8_t v) {
    backlight_ = v;
    s_tft.setBrightness(v);
}

void Display::set_brightness_level(uint8_t level) {
    if (level < ::display::BL_LEVEL_MIN) level = ::display::BL_LEVEL_MIN;
    if (level > ::display::BL_LEVEL_MAX) level = ::display::BL_LEVEL_MAX;
    level_ = level;
    set_backlight(::display::bl_pwm_for_level(level));
}

bool Display::consume_touch_activity() {
    bool was = s_touch_activity;
    s_touch_activity = false;
    return was;
}

}  // namespace hw
