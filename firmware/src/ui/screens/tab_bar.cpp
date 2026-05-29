// Bamboard screens — bottom tab bar.
//
//   y = 228 .. 272 (44 px), permanent on every screen.
//
// 5 buttons of equal width. Each carries (icon over label) plus a tiny
// 3-px accent strip at the top edge that fades in on the active tab —
// makes the selected screen obvious from across the room without
// reading the labels.

#include "theme.h"

namespace ui::screens {

static lv_obj_t* s_tab_bar = nullptr;
static lv_obj_t* s_tab_btns   [(uint8_t)Screen::_Count] = {};
static lv_obj_t* s_tab_icons  [(uint8_t)Screen::_Count] = {};
static lv_obj_t* s_tab_labels [(uint8_t)Screen::_Count] = {};
static lv_obj_t* s_tab_indicat[(uint8_t)Screen::_Count] = {};
static uint8_t   s_tab_active = 0;

static const char* k_tab_labels[(uint8_t)Screen::_Count] = {
    "Live", "AMS", "Printers", "History", "Settings",
};
static const char* k_tab_icons[(uint8_t)Screen::_Count] = {
    LV_SYMBOL_HOME, LV_SYMBOL_TINT, LV_SYMBOL_LIST, LV_SYMBOL_LOOP, LV_SYMBOL_SETTINGS,
};

static void tab_clicked_cb(lv_event_t* e) {
    uint8_t idx = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    ::ui::g_ui.go_to((::ui::Screen)idx);
}

lv_obj_t* build_tab_bar(lv_obj_t* parent) {
    ensure_styles();
    s_tab_bar = lv_obj_create(parent);
    lv_obj_remove_style_all(s_tab_bar);
    lv_obj_set_size(s_tab_bar, LV_HOR_RES, ::ui::TAB_BAR_H);
    lv_obj_align(s_tab_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_tab_bar, lv_color_hex(::ui::C_PANEL), 0);
    lv_obj_set_style_bg_opa(s_tab_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_tab_bar, 0, 0);
    // Hairline above the tab bar mirroring the one under the header.
    lv_obj_set_style_border_side(s_tab_bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(s_tab_bar, 1, 0);
    lv_obj_set_style_border_color(s_tab_bar, lv_color_hex(::ui::C_PANEL_LINE), 0);
    lv_obj_set_style_border_opa(s_tab_bar, LV_OPA_70, 0);
    lv_obj_clear_flag(s_tab_bar, LV_OBJ_FLAG_SCROLLABLE);

    // 5 buttons, equal width, sliced edge-to-edge.
    int tab_w = LV_HOR_RES / (int)Screen::_Count;
    for (uint8_t i = 0; i < (uint8_t)Screen::_Count; ++i) {
        lv_obj_t* btn = lv_btn_create(s_tab_bar);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, tab_w, ::ui::TAB_BAR_H);
        lv_obj_set_pos(btn, i * tab_w, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_add_event_cb(btn, tab_clicked_cb, LV_EVENT_CLICKED,
                            (void*)(intptr_t)i);

        // Top-edge indicator: 3 px accent strip that fades in on the
        // active tab.
        lv_obj_t* ind = lv_obj_create(btn);
        lv_obj_remove_style_all(ind);
        lv_obj_set_size(ind, tab_w - 24, 3);
        lv_obj_align(ind, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(ind, lv_color_hex(::ui::C_ACCENT), 0);
        lv_obj_set_style_bg_opa(ind, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(ind, 2, 0);
        lv_obj_clear_flag(ind, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* icon = lv_label_create(btn);
        lv_label_set_text(icon, k_tab_icons[i]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(::ui::C_TEXT_DIM), 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 6);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, k_tab_labels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(::ui::C_TEXT_DIM), 0);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -3);

        s_tab_btns   [i] = btn;
        s_tab_icons  [i] = icon;
        s_tab_labels [i] = lbl;
        s_tab_indicat[i] = ind;
    }

    tab_bar_set_active(0);
    return s_tab_bar;
}

void tab_bar_set_active(uint8_t idx) {
    if (idx >= (uint8_t)Screen::_Count) return;
    s_tab_active = idx;
    for (uint8_t i = 0; i < (uint8_t)Screen::_Count; ++i) {
        bool on = (i == idx);
        uint32_t col = on ? ::ui::C_ACCENT : ::ui::C_TEXT_DIM;
        lv_obj_set_style_text_color(s_tab_icons [i], lv_color_hex(col), 0);
        lv_obj_set_style_text_color(s_tab_labels[i], lv_color_hex(col), 0);
        lv_obj_set_style_bg_opa(s_tab_indicat[i],
                                on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}

}  // namespace ui::screens
