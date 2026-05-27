#include "display.h"

#include <TFT_eSPI.h>
#include <esp_heap_caps.h>

#include "../config.h"

namespace hw {

Display g_display;

static TFT_eSPI s_tft;
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t      s_disp_drv;

void Display::flush_cb_(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* buf) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    s_tft.startWrite();
    s_tft.setAddrWindow(area->x1, area->y1, w, h);
    s_tft.pushColors((uint16_t*)buf, w * h, true);
    s_tft.endWrite();
    lv_disp_flush_ready(drv);
}

bool Display::begin() {
    // Backlight PWM
    ledcSetup(pins::BL_CHANNEL, pins::BL_FREQ, pins::BL_RES);
    ledcAttachPin(TFT_BL, pins::BL_CHANNEL);
    set_backlight(0);   // stay dark until LVGL has drawn something

    s_tft.begin();
    s_tft.setRotation(1);   // landscape, USB on the left
    s_tft.fillScreen(TFT_BLACK);

    lv_init();

    // Draw buffer in PSRAM
    size_t pixels = display::WIDTH * display::DRAW_BUF_LINES;
    size_t bytes  = pixels * sizeof(lv_color_t);
    lv_color_t* buf1 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_color_t* buf2 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf1 || !buf2) {
        log_e("Failed to allocate LVGL draw buffers (have PSRAM?)");
        return false;
    }
    lv_disp_draw_buf_init(&s_draw_buf, buf1, buf2, pixels);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res  = display::WIDTH;
    s_disp_drv.ver_res  = display::HEIGHT;
    s_disp_drv.flush_cb = flush_cb_;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);

    set_backlight(display::BL_FULL);
    return true;
}

void Display::set_backlight(uint8_t v) {
    current_bl_ = v;
    ledcWrite(pins::BL_CHANNEL, v);
}

void Display::tick() {
    lv_timer_handler();
}

}  // namespace hw
