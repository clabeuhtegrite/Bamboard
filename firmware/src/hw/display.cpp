// Display + touch HAL for the Guition JC4827W543.
//
// The 4.3" 480×272 IPS panel is an NV3041A driven over a QSPI (quad-SPI) bus —
// NOT an RGB-parallel panel (that's the 800×480 JC8048W550 sibling). We use
// Arduino_GFX (moononournation) for the panel and TouchLib (mmMicky) for the
// GT911 capacitive touch, then hook both into LVGL: a flush callback pushes
// draw buffers to the panel and a pointer input device feeds touch coordinates.
//
// NOTE: this file currently carries BOOT DIAGNOSTICS (see BAMBOARD_DISPLAY_DIAG)
// for first-hardware bring-up — backlight-blink milestones + an RGB panel
// self-test so the failing init step is visible without a serial console. Strip
// them once the panel is confirmed.

#include "display.h"

// TouchLib picks its controller backend by macro. It's also defined globally
// via -D TOUCH_MODULES_GT911 (platformio.ini) so the library's own translation
// units compile the GT911 path; define it here too as a guard.
#ifndef TOUCH_MODULES_GT911
#define TOUCH_MODULES_GT911
#endif

#include <Arduino_GFX_Library.h>
#include <TouchLib.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "../config.h"

// Boot-bring-up diagnostics: 1 = instrument display.begin() with a serial-attach
// wait, per-step Serial logs, backlight-blink milestones and an RGB self-test.
#ifndef BAMBOARD_DISPLAY_DIAG
#define BAMBOARD_DISPLAY_DIAG 1
#endif

namespace hw {

Display g_display;

static Arduino_DataBus* s_bus   = nullptr;
static Arduino_GFX*     s_gfx   = nullptr;
static TouchLib*        s_touch = nullptr;

// Backlight PWM uses one LEDC channel; 8-bit duty maps straight onto the
// 0..255 values set_backlight() takes.
static constexpr uint8_t BL_CHANNEL = 7;

// ---------- LVGL plumbing --------------------------------------------------

static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t      s_disp_drv;
static lv_indev_drv_t     s_indev_drv;
static volatile bool      s_touch_activity = false;

static void flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* buf) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    s_gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)buf, w, h);
    lv_disp_flush_ready(drv);
}

static void touch_read_cb(lv_indev_drv_t*, lv_indev_data_t* data) {
    if (s_touch && s_touch->read()) {
        TP_Point t = s_touch->getPoint(0);
        data->state   = LV_INDEV_STATE_PRESSED;
        data->point.x = t.x;
        data->point.y = t.y;
        s_touch_activity = true;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

#if BAMBOARD_DISPLAY_DIAG
// Blink the backlight n times — a milestone marker visible with no serial line.
static void diag_blink(int n) {
    for (int i = 0; i < n; i++) {
        ledcWrite(BL_CHANNEL, 220); delay(160);
        ledcWrite(BL_CHANNEL, 0);   delay(220);
    }
    delay(500);
}
#define DIAG(...) do { Serial.printf(__VA_ARGS__); Serial.print('\n'); Serial.flush(); } while (0)
#else
#define DIAG(...) do {} while (0)
#endif

// ---------- Public API -----------------------------------------------------

bool Display::begin() {
#if BAMBOARD_DISPLAY_DIAG
    // Hold ~5 s before any panel I/O so a host can attach to the USB-CDC port and
    // catch the per-step logs (the CDC enumerates ~1 s after boot, so an early
    // crash is otherwise invisible). Also slows a reset loop, easing capture.
    for (int i = 0; i < 25; i++) { DIAG("[diag] boot-wait %d/25 — attach serial now", i); delay(200); }
    DIAG("[diag] === display.begin() ===");
#endif

    // Backlight PWM up first, held dark (no pre-render flash in normal boot).
    ledcSetup(BL_CHANNEL, pins::BL_FREQ, 8);
    ledcAttachPin(pins::BL_PIN, BL_CHANNEL);
    ledcWrite(BL_CHANNEL, 0);
    DIAG("[diag] M1: backlight PWM ready on GPIO %d", (int)pins::BL_PIN);
    diag_blink(1);   // 1 blink = reached here AND GPIO1 really is the backlight

    // --- Panel: NV3041A over QSPI ---
    s_bus = new Arduino_ESP32QSPI(pins::LCD_CS, pins::LCD_SCK,
                                  pins::LCD_D0, pins::LCD_D1,
                                  pins::LCD_D2, pins::LCD_D3);
    s_gfx = new Arduino_NV3041A(s_bus, pins::LCD_RST, 0 /* rotation */, true /* IPS */);
    DIAG("[diag] M2: calling gfx->begin() (NV3041A/QSPI) ...");
    bool gok = s_gfx->begin();
    DIAG("[diag] M2: gfx->begin() returned %d", (int)gok);
    diag_blink(2);   // 2 blinks = survived gfx->begin()

#if BAMBOARD_DISPLAY_DIAG
    // Panel self-test: full-screen red / green / blue, backlight on. If the user
    // sees these, the panel + QSPI + colour order all work.
    ledcWrite(BL_CHANNEL, 200);
    s_gfx->fillScreen(0xF800); DIAG("[diag] fill RED");   delay(1000);
    s_gfx->fillScreen(0x07E0); DIAG("[diag] fill GREEN"); delay(1000);
    s_gfx->fillScreen(0x001F); DIAG("[diag] fill BLUE");  delay(1000);
    ledcWrite(BL_CHANNEL, 0);
#endif
    s_gfx->fillScreen(0x0000);   // black

    // --- Touch: GT911 over I²C (TouchLib) ---
    // Probe the controller on the bus FIRST: a wrong pin map / missing GT911
    // must not be able to hang boot inside a blocking TouchLib::init().
    Wire.begin(pins::GT911_SDA, pins::GT911_SCL);
    Wire.setClock(pins::GT911_FREQ);
    pinMode(pins::GT911_RST, OUTPUT);
    digitalWrite(pins::GT911_RST, 0);
    delay(20);
    digitalWrite(pins::GT911_RST, 1);
    delay(50);
    Wire.beginTransmission(pins::GT911_ADDR);
    bool touch_present = (Wire.endTransmission() == 0);
    DIAG("[diag] M3: GT911 @0x%02X present=%d (SDA=%d SCL=%d)",
         (int)pins::GT911_ADDR, (int)touch_present,
         (int)pins::GT911_SDA, (int)pins::GT911_SCL);
    if (touch_present) {
        s_touch = new TouchLib(Wire, pins::GT911_SDA, pins::GT911_SCL, pins::GT911_ADDR);
        s_touch->init();
    }
    diag_blink(3);   // 3 blinks = survived touch bring-up

    // --- LVGL ---
    lv_init();

    // Single draw buffer in INTERNAL RAM (~38 KB at 480×40). Arduino_GFX copies
    // the buffer out over QSPI, so it needs no DMA/PSRAM region.
    size_t pixels = ::display::WIDTH * ::display::DRAW_BUF_LINES;
    size_t bytes  = pixels * sizeof(lv_color_t);
    lv_color_t* buf1 =
        (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buf1) {
        DIAG("[diag] LVGL draw-buffer alloc FAILED");
        log_e("Failed to allocate LVGL draw buffer");
        return false;
    }
    lv_disp_draw_buf_init(&s_draw_buf, buf1, nullptr, pixels);

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

    DIAG("[diag] M4: LVGL ready — display.begin() DONE");
    diag_blink(4);   // 4 blinks = full success; boot continues to provisioning
    return true;
}

void Display::tick() {
    lv_timer_handler();
}

void Display::set_backlight(uint8_t v) {
    backlight_ = v;
    ledcWrite(BL_CHANNEL, v);
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
