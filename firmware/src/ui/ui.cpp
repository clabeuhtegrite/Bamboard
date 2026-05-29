#include "ui.h"

#include <lvgl.h>

#include "../config.h"
#include "../hw/display.h"
#include "../net/bambuddy_client.h"
#include "screens.h"

namespace ui {

Manager g_ui;

// --- LVGL style helpers ----------------------------------------------------

static lv_style_t s_style_bg;
static lv_style_t s_style_panel;

static void init_theme() {
    lv_style_init(&s_style_bg);
    lv_style_set_bg_color(&s_style_bg, lv_color_hex(::ui::C_BG));
    lv_style_set_bg_opa(&s_style_bg, LV_OPA_COVER);
    lv_style_set_border_width(&s_style_bg, 0);
    lv_style_set_pad_all(&s_style_bg, 0);

    lv_style_init(&s_style_panel);
    lv_style_set_bg_color(&s_style_panel, lv_color_hex(::ui::C_PANEL));
    lv_style_set_bg_opa(&s_style_panel, LV_OPA_COVER);
    lv_style_set_border_width(&s_style_panel, 0);
    lv_style_set_radius(&s_style_panel, 10);
    lv_style_set_pad_all(&s_style_panel, 10);
}

// --- screen carousel -------------------------------------------------------

// Each screen lives in its own lv_obj_t* and we toggle visibility via a
// horizontal slide animation, which is cheap on the S3.

static lv_obj_t* s_root        = nullptr;
static lv_obj_t* s_screens[(uint8_t)Screen::_Count] = {};
static lv_obj_t* s_header      = nullptr;
static lv_obj_t* s_nav         = nullptr;
static lv_obj_t* s_nav_dots[(uint8_t)Screen::_Count] = {};

static const char* screen_titles[(uint8_t)Screen::_Count] = {
    "Live",
    "AMS",
    "Printers",
    "History",
    "Settings",
};

void Manager::begin() {
    init_theme();

    s_root = lv_scr_act();
    lv_obj_add_style(s_root, &s_style_bg, 0);

    s_header = screens::build_header(s_root);

    // Bottom nav indicator (small dots)
    s_nav = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_nav);
    lv_obj_set_size(s_nav, LV_HOR_RES, 12);
    lv_obj_align(s_nav, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_flex_flow(s_nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_nav, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_nav, 8, 0);
    for (uint8_t i = 0; i < (uint8_t)Screen::_Count; ++i) {
        s_nav_dots[i] = lv_obj_create(s_nav);
        lv_obj_remove_style_all(s_nav_dots[i]);
        lv_obj_set_size(s_nav_dots[i], 8, 8);
        lv_obj_set_style_radius(s_nav_dots[i], 4, 0);
        lv_obj_set_style_bg_opa(s_nav_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s_nav_dots[i],
                                   lv_color_hex(::ui::C_TEXT_DIM), 0);
    }

    // Build screens
    s_screens[(uint8_t)Screen::Dashboard] = screens::build_dashboard(s_root);
    s_screens[(uint8_t)Screen::Ams]       = screens::build_ams(s_root);
    s_screens[(uint8_t)Screen::Printers]  = screens::build_printers(s_root);
    s_screens[(uint8_t)Screen::History]   = screens::build_history(s_root);
    s_screens[(uint8_t)Screen::Settings]  = screens::build_settings(s_root);

    for (uint8_t i = 0; i < (uint8_t)Screen::_Count; ++i) {
        lv_obj_add_flag(s_screens[i], LV_OBJ_FLAG_HIDDEN);
    }
    screens::build_toast(s_root);
    screens::build_actions(s_root);
    screens::build_hms_flash(s_root);
    screens::build_ota_overlay(s_root);

    go_to(Screen::Dashboard);
}

void Manager::go_to(Screen s) {
    if (s == current_) return;
    lv_obj_add_flag(s_screens[(uint8_t)current_], LV_OBJ_FLAG_HIDDEN);
    current_ = s;
    lv_obj_clear_flag(s_screens[(uint8_t)current_], LV_OBJ_FLAG_HIDDEN);
    screens::header_set_title(screen_titles[(uint8_t)current_]);
    // Update nav dots
    for (uint8_t i = 0; i < (uint8_t)Screen::_Count; ++i) {
        lv_color_t c = (i == (uint8_t)current_)
                           ? lv_color_hex(::ui::C_ACCENT)
                           : lv_color_hex(::ui::C_TEXT_DIM);
        lv_obj_set_style_bg_color(s_nav_dots[i], c, 0);
        uint16_t sz = (i == (uint8_t)current_) ? 10 : 6;
        lv_obj_set_size(s_nav_dots[i], sz, sz);
        lv_obj_set_style_radius(s_nav_dots[i], sz / 2, 0);
    }
}

void Manager::refresh() {
    // OTA gets right-of-way: while an update is in flight, the rest of the
    // UI is irrelevant and the overlay is the only thing the user should
    // see. We still tick through the other screens so they're in a sane
    // state when the device reboots back to the same UI on success.
    screens::ota_apply();

    // Auto-pick the first printer the first time we know about one.
    ::bambuddy::Printer ps[8];
    uint8_t n = 0;
    ::bambuddy::g_client.snapshot_printers(ps, n);
    if (selected_printer_id_ < 0 && n > 0) {
        selected_index_ = 0;
        selected_printer_id_ = ps[0].id;
    }

    switch (current_) {
        case Screen::Dashboard: screens::update_dashboard(selected_printer_id_); break;
        case Screen::Ams:       screens::update_ams(selected_printer_id_);       break;
        case Screen::Printers:  screens::update_printers(selected_index_);       break;
        case Screen::History:   screens::update_history();                       break;
        case Screen::Settings:  screens::update_settings();                      break;
        default: break;
    }

    // --- HMS full-screen flash state machine -------------------------------
    // While OTA owns the screen we suppress every other overlay; popping a
    // red HMS flash on top of an upload progress bar would be useless and
    // confusing.
    if (screens::ota_is_active()) {
        if (screens::hms_flash_is_visible()) screens::hms_flash_hide();
        return;
    }

    String hms_now;
    for (uint8_t i = 0; i < n; ++i)
        if (ps[i].id == selected_printer_id_) hms_now = ps[i].hms;
    bool active = hms_now.length() && hms_now != "ok";
    uint32_t now = millis();

    if (!active) {
        if (screens::hms_flash_is_visible()) screens::hms_flash_hide();
        hms_was_active_ = false;
        hms_last_msg_   = "";
        return;
    }

    // Pop the overlay immediately on the edge from ok → error so the user
    // gets the alert without waiting for the next periodic tick.
    if (!hms_was_active_) {
        screens::hms_flash_show(hms_now.c_str());
        hms_hide_at_ms_   = now + ::bambuddy::HMS_FLASH_VISIBLE_MS;
        hms_next_show_ms_ = now + ::bambuddy::HMS_FLASH_COOLDOWN_MS;
        hms_was_active_   = true;
        hms_last_msg_     = hms_now;
        return;
    }

    if (screens::hms_flash_is_visible()) {
        if (hms_now != hms_last_msg_) {
            screens::hms_flash_update_msg(hms_now.c_str());
            hms_last_msg_ = hms_now;
        }
        if (now >= hms_hide_at_ms_) screens::hms_flash_hide();
    } else if (now >= hms_next_show_ms_) {
        screens::hms_flash_show(hms_now.c_str());
        hms_hide_at_ms_   = now + ::bambuddy::HMS_FLASH_VISIBLE_MS;
        hms_next_show_ms_ = now + ::bambuddy::HMS_FLASH_COOLDOWN_MS;
        hms_last_msg_     = hms_now;
    }
}

void Manager::show_toast(const char* text, lv_color_t colour) {
    screens::show_toast(text, colour);
}

// --- input handling --------------------------------------------------------

static void cycle_printer(int dir, int& selected_index, int& selected_id) {
    ::bambuddy::Printer ps[8];
    uint8_t n = 0;
    ::bambuddy::g_client.snapshot_printers(ps, n);
    if (n == 0) return;
    selected_index = ((selected_index + dir) % (int)n + (int)n) % (int)n;
    selected_id = ps[selected_index].id;
}

}  // namespace ui
