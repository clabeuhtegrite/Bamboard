// Bamboard screens — overlays.
//
// Three independent full-screen / floating overlays live here:
//
//   - toast       : ephemeral confirmation strip at the bottom of the body
//   - hms_flash   : full-screen red pulse when the focused printer has an
//                   HMS error; tap anywhere to dismiss
//   - ota_overlay : opaque progress UI while the boot-time GitHub OTA is
//                   downloading + flashing a firmware image
//
// All three are siblings under lv_scr_act() and z-ordered by creation
// time. Overlays that should sit on top of every screen are built last
// in ui::Manager::begin() — see firmware/src/ui/ui.cpp.

#include "theme.h"

#include <TJpg_Decoder.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace ui::screens {

// =============================================================================
// TOAST
// =============================================================================

static lv_obj_t* s_toast       = nullptr;
static lv_obj_t* s_toast_label = nullptr;
static uint32_t  s_toast_hide_at = 0;

lv_obj_t* build_toast(lv_obj_t* parent) {
    ensure_styles();
    s_toast = lv_obj_create(parent);
    lv_obj_remove_style_all(s_toast);
    lv_obj_set_size(s_toast, 280, 36);
    // Sit just above the tab bar — y offset = tab_bar_h + 8 px gap.
    lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0,
                 -(int)::ui::TAB_BAR_H - 8);
    lv_obj_set_style_bg_color(s_toast, lv_color_hex(::ui::C_ACCENT), 0);
    lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_toast, 8, 0);
    lv_obj_clear_flag(s_toast, LV_OBJ_FLAG_SCROLLABLE);

    s_toast_label = lv_label_create(s_toast);
    lv_label_set_text(s_toast_label, "");
    lv_obj_center(s_toast_label);
    lv_obj_set_style_text_color(s_toast_label, lv_color_hex(::ui::C_TEXT_INV), 0);
    lv_obj_set_style_text_font(s_toast_label, &bb_font_16, 0);

    lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    return s_toast;
}

void show_toast(const char* msg, lv_color_t bg) {
    if (!s_toast) return;
    lv_label_set_text(s_toast_label, msg);
    lv_obj_set_style_bg_color(s_toast, bg, 0);
    lv_obj_clear_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_toast);
    s_toast_hide_at = lv_tick_get() + 1800;
}

// Called from each per-screen update_*() so stale toasts disappear even
// when the user doesn't touch anything.
void maybe_hide_toast() {
    if (!s_toast || lv_obj_has_flag(s_toast, LV_OBJ_FLAG_HIDDEN)) return;
    if (s_toast_hide_at && lv_tick_get() > s_toast_hide_at) {
        lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    }
}

// =============================================================================
// HMS FULL-SCREEN FLASH
// =============================================================================

static lv_obj_t* s_hms_overlay = nullptr;
static lv_obj_t* s_hms_msg     = nullptr;
static bool      s_hms_visible = false;

static void hms_pulse_cb(void* var, int32_t v) {
    const uint8_t r0 = 0x80, g0 = 0x1F, b0 = 0x22;
    const uint8_t r1 = (::ui::C_ERR >> 16) & 0xFF;
    const uint8_t g1 = (::ui::C_ERR >>  8) & 0xFF;
    const uint8_t b1 = (::ui::C_ERR      ) & 0xFF;
    uint8_t r = r0 + (uint8_t)(((int32_t)(r1 - r0) * v) / 100);
    uint8_t g = g0 + (uint8_t)(((int32_t)(g1 - g0) * v) / 100);
    uint8_t b = b0 + (uint8_t)(((int32_t)(b1 - b0) * v) / 100);
    lv_obj_set_style_bg_color((lv_obj_t*)var, lv_color_make(r, g, b), 0);
}

static void hms_overlay_clicked(lv_event_t*) {
    hms_flash_hide();
}

lv_obj_t* build_hms_flash(lv_obj_t* parent) {
    ensure_styles();
    s_hms_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_hms_overlay);
    lv_obj_set_size(s_hms_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(s_hms_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_hms_overlay, lv_color_hex(::ui::C_ERR), 0);
    lv_obj_set_style_bg_opa(s_hms_overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_hms_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_hms_overlay, hms_overlay_clicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* icon = lv_label_create(s_hms_overlay);
    lv_label_set_text(icon, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(icon, &bb_font_36, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -66);

    lv_obj_t* title = lv_label_create(s_hms_overlay);
    lv_label_set_text(title, i18n::tr(i18n::Str::HMS_ERROR));
    lv_obj_set_style_text_font(title, &bb_font_28, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -22);

    s_hms_msg = lv_label_create(s_hms_overlay);
    lv_label_set_text(s_hms_msg, "");
    lv_obj_set_width(s_hms_msg, LV_HOR_RES - 60);
    lv_label_set_long_mode(s_hms_msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_hms_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_hms_msg, &bb_font_16, 0);
    lv_obj_set_style_text_color(s_hms_msg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_hms_msg, LV_ALIGN_CENTER, 0, 28);

    lv_obj_t* hint = lv_label_create(s_hms_overlay);
    lv_label_set_text(hint, i18n::tr(i18n::Str::TAP_DISMISS));
    lv_obj_set_style_text_font(hint, &bb_font_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xFFE0E0), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -14);

    lv_obj_add_flag(s_hms_overlay, LV_OBJ_FLAG_HIDDEN);
    return s_hms_overlay;
}

void hms_flash_show(const char* msg) {
    if (!s_hms_overlay) return;
    lv_label_set_text(s_hms_msg, (msg && *msg) ? msg
                                               : i18n::tr(i18n::Str::CHECK_PRINTER));
    lv_obj_clear_flag(s_hms_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_hms_overlay);
    s_hms_visible = true;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_hms_overlay);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 700);
    lv_anim_set_playback_time(&a, 700);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, hms_pulse_cb);
    lv_anim_start(&a);
}

void hms_flash_hide() {
    if (!s_hms_overlay) return;
    lv_anim_del(s_hms_overlay, hms_pulse_cb);
    lv_obj_add_flag(s_hms_overlay, LV_OBJ_FLAG_HIDDEN);
    s_hms_visible = false;
}

void hms_flash_update_msg(const char* msg) {
    if (s_hms_msg && msg) lv_label_set_text(s_hms_msg, msg);
}

bool hms_flash_is_visible() { return s_hms_visible; }

// =============================================================================
// OTA PROGRESS OVERLAY
// =============================================================================
//
// OTA callbacks fire on net_task (core 0) and the UI task runs on core 1;
// the setters poke a tiny volatile state and ota_apply() (called from
// ui::Manager::refresh()) syncs it into the widget on the right core.

static lv_obj_t* s_ota_overlay = nullptr;
static lv_obj_t* s_ota_bar     = nullptr;
static lv_obj_t* s_ota_pct_lbl = nullptr;
static lv_obj_t* s_ota_status  = nullptr;

static volatile bool    s_ota_active        = false;
static volatile bool    s_ota_dirty         = true;
static volatile uint8_t s_ota_pct           = 0;
static volatile bool    s_ota_error_flag    = false;
static char             s_ota_err_msg[80]   = {};
static bool             s_ota_visible_cache = false;
static uint8_t          s_ota_pct_cache     = 0;
static bool             s_ota_error_cache   = false;

lv_obj_t* build_ota_overlay(lv_obj_t* parent) {
    ensure_styles();
    s_ota_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_ota_overlay);
    lv_obj_set_size(s_ota_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(s_ota_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_ota_overlay, lv_color_hex(::ui::C_BG), 0);
    lv_obj_set_style_bg_opa(s_ota_overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_ota_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* icon = lv_label_create(s_ota_overlay);
    lv_label_set_text(icon, LV_SYMBOL_DOWNLOAD);
    lv_obj_set_style_text_font(icon, &bb_font_36, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(::ui::C_ACCENT), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -66);

    lv_obj_t* title = lv_label_create(s_ota_overlay);
    lv_label_set_text(title, i18n::tr(i18n::Str::UPDATING_FW));
    lv_obj_set_style_text_font(title, &bb_font_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -22);

    s_ota_bar = lv_bar_create(s_ota_overlay);
    lv_obj_set_size(s_ota_bar, 320, 16);
    lv_obj_align(s_ota_bar, LV_ALIGN_CENTER, 0, 20);
    lv_bar_set_range(s_ota_bar, 0, 100);
    lv_bar_set_value(s_ota_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_ota_bar, lv_color_hex(::ui::C_PANEL_HI), LV_PART_MAIN);
    lv_obj_set_style_bg_opa  (s_ota_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius  (s_ota_bar, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ota_bar, lv_color_hex(::ui::C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa  (s_ota_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius  (s_ota_bar, 8, LV_PART_INDICATOR);

    s_ota_pct_lbl = lv_label_create(s_ota_overlay);
    lv_label_set_text(s_ota_pct_lbl, "0%");
    lv_obj_set_style_text_font(s_ota_pct_lbl, &bb_font_16, 0);
    lv_obj_set_style_text_color(s_ota_pct_lbl, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_align(s_ota_pct_lbl, LV_ALIGN_CENTER, 0, 50);

    s_ota_status = lv_label_create(s_ota_overlay);
    lv_label_set_text(s_ota_status, i18n::tr(i18n::Str::DONT_POWER_OFF));
    lv_obj_set_style_text_font(s_ota_status, &bb_font_14, 0);
    lv_obj_set_style_text_color(s_ota_status, lv_color_hex(::ui::C_TEXT_DIM), 0);
    lv_obj_align(s_ota_status, LV_ALIGN_BOTTOM_MID, 0, -14);

    lv_obj_add_flag(s_ota_overlay, LV_OBJ_FLAG_HIDDEN);
    return s_ota_overlay;
}

void ota_set_active(bool active) {
    if (active != s_ota_active) { s_ota_active = active; s_ota_dirty = true; }
}
void ota_set_progress(uint8_t pct) {
    if (pct > 100) pct = 100;
    if (pct != s_ota_pct) { s_ota_pct = pct; s_ota_dirty = true; }
}
void ota_set_error(const char* msg) {
    if (!msg) msg = i18n::tr(i18n::Str::UPDATE_FAILED);
    strncpy(s_ota_err_msg, msg, sizeof(s_ota_err_msg) - 1);
    s_ota_err_msg[sizeof(s_ota_err_msg) - 1] = '\0';
    s_ota_error_flag = true;
    s_ota_dirty      = true;
}
bool ota_is_active() { return s_ota_active; }

void ota_apply() {
    if (!s_ota_dirty) return;
    s_ota_dirty = false;
    bool    active = s_ota_active;
    uint8_t pct    = s_ota_pct;
    bool    err    = s_ota_error_flag;

    if (active != s_ota_visible_cache) {
        if (active) {
            lv_obj_clear_flag(s_ota_overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(s_ota_overlay);
        } else {
            lv_obj_add_flag(s_ota_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        s_ota_visible_cache = active;
    }
    if (pct != s_ota_pct_cache) {
        lv_bar_set_value(s_ota_bar, pct, LV_ANIM_OFF);
        char buf[8];
        snprintf(buf, sizeof(buf), "%u%%", (unsigned)pct);
        lv_label_set_text(s_ota_pct_lbl, buf);
        s_ota_pct_cache = pct;
    }
    if (err && !s_ota_error_cache) {
        lv_label_set_text(s_ota_status, s_ota_err_msg);
        lv_obj_set_style_text_color(s_ota_status, lv_color_hex(::ui::C_ERR), 0);
        s_ota_error_cache = true;
    }
}

// =============================================================================
// CAMERA SNAPSHOT OVERLAY
// =============================================================================
//
// Full-screen JPEG snapshot viewer. The net task fetches + decodes frames
// (camera_decode_frame) into a PSRAM RGB565 buffer under a mutex; the UI task
// points an lv_img at it (camera_apply); a tap dismisses the overlay. lv_img
// is used rather than lv_canvas because LV_USE_CANVAS is off.
//
// HARDWARE-TUNING NOTE: the byte-swap (setSwapBytes) / colour order and the
// decode scale are the things most likely to need adjusting once a real panel
// + camera stream are in hand — they can't be eyeballed in CI.

static lv_obj_t*  s_cam_overlay = nullptr;
static lv_obj_t*  s_cam_img     = nullptr;
static lv_obj_t*  s_cam_hint    = nullptr;
static lv_img_dsc_t s_cam_dsc   = {};
static uint8_t*   s_cam_pix      = nullptr;   // PSRAM RGB565, CAM_W_MAX*CAM_H_MAX
static SemaphoreHandle_t s_cam_mtx = nullptr;
static volatile bool s_cam_open  = false;
static volatile bool s_cam_dirty = false;
static uint16_t s_cam_w = 0, s_cam_h = 0;
// Optional inline thumbnail (the Live dashboard registers one). It shows the
// same decoded frame, contain-fit into a small box. s_cam_have_frame gates the
// dashboard's reveal so the box stays hidden until a real frame has arrived
// (e.g. a Bambuddy with no camera never shows an empty box).
static lv_obj_t* s_cam_thumb       = nullptr;
static uint16_t  s_cam_thumb_w     = 0;
static uint16_t  s_cam_thumb_h     = 0;
static volatile bool s_cam_have_frame = false;

static const uint16_t CAM_W_MAX = 480;
static const uint16_t CAM_H_MAX = 232;   // body height between header and tab bar

// TJpgDec block callback — runs on the net task during decode. Writes the
// decoded RGB565 block into the shared buffer, clipped to its bounds.
static bool cam_tjpg_cb(int16_t x, int16_t y, uint16_t bw, uint16_t bh, uint16_t* bmp) {
    if (!s_cam_pix) return false;
    uint16_t* dst = (uint16_t*)s_cam_pix;
    for (uint16_t row = 0; row < bh; ++row) {
        int yy = y + row;
        if (yy < 0 || yy >= (int)s_cam_h) continue;
        for (uint16_t col = 0; col < bw; ++col) {
            int xx = x + col;
            if (xx < 0 || xx >= (int)s_cam_w) continue;
            dst[(uint32_t)yy * s_cam_w + xx] = bmp[(uint32_t)row * bw + col];
        }
    }
    return true;
}

static void cam_overlay_clicked(lv_event_t*) { camera_overlay_close(); }

lv_obj_t* build_camera_overlay(lv_obj_t* parent) {
    ensure_styles();
    s_cam_mtx = xSemaphoreCreateMutex();
    s_cam_pix = (uint8_t*)heap_caps_malloc((size_t)CAM_W_MAX * CAM_H_MAX * 2,
                                           MALLOC_CAP_SPIRAM);
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);   // match the LVGL RGB565 buffer byte order
    TJpgDec.setCallback(cam_tjpg_cb);

    s_cam_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_cam_overlay);
    lv_obj_set_size(s_cam_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(s_cam_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_cam_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_cam_overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_cam_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_cam_overlay, cam_overlay_clicked, LV_EVENT_CLICKED, nullptr);

    s_cam_img = lv_img_create(s_cam_overlay);
    lv_obj_center(s_cam_img);
    lv_obj_clear_flag(s_cam_img, LV_OBJ_FLAG_CLICKABLE);

    s_cam_hint = lv_label_create(s_cam_overlay);
    lv_label_set_text(s_cam_hint, i18n::tr(i18n::Str::TAP_DISMISS));
    lv_obj_set_style_text_font(s_cam_hint, &bb_font_14, 0);
    lv_obj_set_style_text_color(s_cam_hint, lv_color_hex(::ui::C_TEXT_DIM), 0);
    lv_obj_align(s_cam_hint, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_add_flag(s_cam_overlay, LV_OBJ_FLAG_HIDDEN);
    return s_cam_overlay;
}

void camera_overlay_open() {
    if (!s_cam_overlay || !s_cam_pix) return;   // no PSRAM buffer → feature off
    s_cam_dirty = false;
    s_cam_open  = true;
    lv_obj_clear_flag(s_cam_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_cam_overlay);
}

void camera_overlay_close() {
    if (!s_cam_overlay) return;
    s_cam_open = false;
    lv_obj_add_flag(s_cam_overlay, LV_OBJ_FLAG_HIDDEN);
}

bool camera_overlay_is_open() { return s_cam_open; }

// Net task: decode a freshly-fetched JPEG into the shared PSRAM buffer.
void camera_decode_frame(const uint8_t* jpeg, size_t len) {
    if (!s_cam_pix || !s_cam_mtx || !jpeg || !len) return;
    uint16_t w = 0, h = 0;
    if (TJpgDec.getJpgSize(&w, &h, jpeg, len) != JDR_OK || w == 0 || h == 0) return;
    // Native power-of-two downscale until it fits the on-screen area.
    uint8_t scale = 1;
    while ((w / scale > CAM_W_MAX || h / scale > CAM_H_MAX) && scale < 8) scale *= 2;
    uint16_t dw = w / scale, dh = h / scale;
    if (dw > CAM_W_MAX) dw = CAM_W_MAX;
    if (dh > CAM_H_MAX) dh = CAM_H_MAX;

    xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
    s_cam_w = dw;
    s_cam_h = dh;
    memset(s_cam_pix, 0, (size_t)dw * dh * 2);
    TJpgDec.setJpgScale(scale);
    TJpgDec.drawJpg(0, 0, jpeg, len);   // → cam_tjpg_cb fills s_cam_pix
    s_cam_dirty = true;
    s_cam_have_frame = true;
    xSemaphoreGive(s_cam_mtx);
}

// UI task: publish the latest decoded frame to the on-screen image.
void camera_apply() {
    if (!s_cam_overlay || !s_cam_dirty) return;
    xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
    s_cam_dirty = false;
    uint16_t w = s_cam_w, h = s_cam_h;
    s_cam_dsc.header.always_zero = 0;
    s_cam_dsc.header.cf   = LV_IMG_CF_TRUE_COLOR;
    s_cam_dsc.header.w    = w;
    s_cam_dsc.header.h    = h;
    s_cam_dsc.data        = s_cam_pix;
    s_cam_dsc.data_size   = (uint32_t)w * h * 2;
    xSemaphoreGive(s_cam_mtx);
    if (w == 0 || h == 0) return;
    lv_img_set_src(s_cam_img, &s_cam_dsc);
    lv_obj_set_size(s_cam_img, w, h);
    lv_obj_center(s_cam_img);
    lv_obj_invalidate(s_cam_img);

    // Inline dashboard thumbnail (if attached): same frame, contain-fit + centred.
    if (s_cam_thumb && s_cam_thumb_w && s_cam_thumb_h) {
        uint16_t zw = (uint32_t)s_cam_thumb_w * 256u / w;
        uint16_t zh = (uint32_t)s_cam_thumb_h * 256u / h;
        uint16_t z  = zw < zh ? zw : zh;        // contain — no crop
        if (z == 0) z = 1;
        lv_img_set_src(s_cam_thumb, &s_cam_dsc);
        lv_img_set_pivot(s_cam_thumb, 0, 0);
        lv_img_set_zoom(s_cam_thumb, z);
        lv_obj_set_size(s_cam_thumb, (uint32_t)w * z / 256u, (uint32_t)h * z / 256u);
        lv_obj_center(s_cam_thumb);
        lv_obj_invalidate(s_cam_thumb);
    }
}

// Register an lv_img (owned by another screen) to receive the decoded frame as
// a contain-fit thumbnail of at most w x h. camera_apply() updates it.
void camera_attach_thumbnail(lv_obj_t* img, uint16_t w, uint16_t h) {
    s_cam_thumb   = img;
    s_cam_thumb_w = w;
    s_cam_thumb_h = h;
}

// True once at least one camera frame has been decoded — the dashboard uses
// this to reveal its thumbnail only when the camera actually works.
bool camera_has_frame() { return s_cam_have_frame; }

}  // namespace ui::screens
