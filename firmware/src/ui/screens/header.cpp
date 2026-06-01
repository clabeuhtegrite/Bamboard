// Bamboard screens — top header bar.
//
//   y =   0 .. 36
//   layout:  [ "Bamboard" accent ]   [ printer name centre ]   [ wifi / latency ]
//
// The connectivity readout colour-codes itself: green/teal when the last
// Bambuddy ping succeeded, red when offline. The setter is safe to call
// from the net task — LVGL widget mutation only happens on this TU's
// statics, which the UI task reads next refresh.

#include "theme.h"

namespace ui::screens {

static lv_obj_t* s_hdr_title   = nullptr;
static lv_obj_t* s_hdr_printer = nullptr;
static lv_obj_t* s_hdr_conn    = nullptr;

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

    s_hdr_title = lv_label_create(hdr);
    lv_label_set_text(s_hdr_title, "Bamboard");
    lv_obj_align(s_hdr_title, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_set_style_text_color(s_hdr_title, lv_color_hex(::ui::C_ACCENT), 0);
    lv_obj_set_style_text_font(s_hdr_title, &bb_font_20, 0);

    s_hdr_printer = lv_label_create(hdr);
    lv_label_set_text(s_hdr_printer, "");
    lv_obj_align(s_hdr_printer, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(s_hdr_printer, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_set_style_text_font(s_hdr_printer, &bb_font_16, 0);

    s_hdr_conn = lv_label_create(hdr);
    lv_label_set_text(s_hdr_conn, LV_SYMBOL_WIFI " --");
    lv_obj_align(s_hdr_conn, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_text_color(s_hdr_conn, lv_color_hex(::ui::C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_hdr_conn, &bb_font_14, 0);
    return hdr;
}

void header_set_title(const char* title) {
    if (s_hdr_title) lv_label_set_text(s_hdr_title, title);
}

void header_set_online(bool online, uint32_t latency_ms) {
    if (!s_hdr_conn) return;
    if (online) {
        char buf[24];
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %ums", (unsigned)latency_ms);
        lv_label_set_text(s_hdr_conn, buf);
        lv_obj_set_style_text_color(s_hdr_conn, lv_color_hex(::ui::C_OK), 0);
    } else {
        lv_label_set_text(s_hdr_conn,
                          (String(LV_SYMBOL_WARNING " ") +
                           i18n::tr(i18n::Str::OFFLINE_SHORT)).c_str());
        lv_obj_set_style_text_color(s_hdr_conn, lv_color_hex(::ui::C_ERR), 0);
    }
}

void header_set_printer_name(const char* name) {
    if (s_hdr_printer) lv_label_set_text(s_hdr_printer, name ? name : "");
}

}  // namespace ui::screens
