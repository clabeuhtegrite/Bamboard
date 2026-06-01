// Bamboard screens — top header bar.
//
//   y =   0 .. 36
//   layout:  [ "Bamboard" accent ]   [ printer name centre ]   [ wifi / latency ]
//
// The connectivity readout colour-codes itself: green/teal when the last
// Bambuddy ping succeeded, red when offline. header_set_online() is the only
// hook called from the net task; it parks the new state in a volatile cell
// and the UI task syncs it into the widget via header_apply() (called from
// ui::Manager::refresh()). header_set_title() / header_set_printer_name()
// only fire from the UI task and mutate LVGL widgets directly.

#include "theme.h"

namespace ui::screens {

static lv_obj_t* s_hdr_title   = nullptr;
static lv_obj_t* s_hdr_printer = nullptr;
static lv_obj_t* s_hdr_conn    = nullptr;

// Net-task → UI-task hand-off for the connectivity readout. Volatile so the
// reader sees the latest store; we accept a torn read on latency_ms because
// the worst case is a single stale frame.
static volatile bool     s_conn_dirty      = false;
static volatile bool     s_conn_online     = false;
static volatile uint32_t s_conn_latency_ms = 0;

lv_obj_t* build_header(lv_obj_t* parent) {
    ensure_styles();
    lv_obj_t* hdr = lv_obj_create(parent);
    lv_obj_remove_style_all(hdr);
    lv_obj_set_size(hdr, LV_HOR_RES, ::ui::HEADER_H);
    lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(::ui::C_PANEL), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    // Hairline under the header to separate it from the body.
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(hdr, 1, 0);
    lv_obj_set_style_border_color(hdr, lv_color_hex(::ui::C_PANEL_LINE), 0);
    lv_obj_set_style_border_opa(hdr, LV_OPA_70, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    // Small accent square "logo" mark at the far left.
    lv_obj_t* mark = lv_obj_create(hdr);
    lv_obj_remove_style_all(mark);
    lv_obj_set_size(mark, 10, 10);
    lv_obj_align(mark, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_set_style_radius(mark, 3, 0);
    lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(mark, lv_color_hex(::ui::C_ACCENT), 0);

    // Brand wordmark — "board" recoloured in accent. The header shows the
    // brand, NOT the screen name: the bottom tab bar already marks the active
    // screen, so a screen title up here would just duplicate it.
    s_hdr_title = lv_label_create(hdr);
    lv_label_set_recolor(s_hdr_title, true);
    lv_label_set_text(s_hdr_title, "Bam#00B7C3 board#");
    lv_obj_align(s_hdr_title, LV_ALIGN_LEFT_MID, 29, 0);
    lv_obj_set_style_text_color(s_hdr_title, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_set_style_text_font(s_hdr_title, &bb_font_20, 0);

    s_hdr_printer = lv_label_create(hdr);
    lv_label_set_text(s_hdr_printer, "");
    lv_obj_align(s_hdr_printer, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(s_hdr_printer, lv_color_hex(::ui::C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_hdr_printer, &bb_font_14, 0);

    // Connectivity readout styled as a rounded badge.
    s_hdr_conn = lv_label_create(hdr);
    lv_label_set_text(s_hdr_conn, LV_SYMBOL_WIFI " --");
    lv_obj_align(s_hdr_conn, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_text_color(s_hdr_conn, lv_color_hex(::ui::C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_hdr_conn, &bb_font_14, 0);
    lv_obj_set_style_bg_color(s_hdr_conn, lv_color_hex(::ui::C_TINT), 0);
    lv_obj_set_style_bg_opa(s_hdr_conn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_hdr_conn, ::ui::R_PILL, 0);
    lv_obj_set_style_pad_hor(s_hdr_conn, 8, 0);
    lv_obj_set_style_pad_ver(s_hdr_conn, 2, 0);
    return hdr;
}

void header_set_title(const char* title) {
    if (s_hdr_title) lv_label_set_text(s_hdr_title, title);
}

// Net-task safe: just parks the state. The UI task picks it up next refresh
// via header_apply().
void header_set_online(bool online, uint32_t latency_ms) {
    s_conn_online     = online;
    s_conn_latency_ms = latency_ms;
    s_conn_dirty      = true;
}

void header_apply() {
    if (!s_hdr_conn || !s_conn_dirty) return;
    s_conn_dirty = false;
    bool     online  = s_conn_online;
    uint32_t latency = s_conn_latency_ms;
    if (online) {
        char buf[24];
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %ums", (unsigned)latency);
        lv_label_set_text(s_hdr_conn, buf);
        lv_obj_set_style_text_color(s_hdr_conn, lv_color_hex(::ui::C_OK), 0);
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " %s",
                 i18n::tr(i18n::Str::OFFLINE_SHORT));
        lv_label_set_text(s_hdr_conn, buf);
        lv_obj_set_style_text_color(s_hdr_conn, lv_color_hex(::ui::C_ERR), 0);
    }
}

void header_set_printer_name(const char* name) {
    if (s_hdr_printer) lv_label_set_text(s_hdr_printer, name ? name : "");
}

}  // namespace ui::screens
