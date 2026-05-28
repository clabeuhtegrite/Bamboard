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
    // Auto-pick the first printer the first time we know about one.
    if (selected_printer_id_ < 0) {
        ::bambuddy::Printer ps[8];
        uint8_t n = 0;
        ::bambuddy::g_client.snapshot_printers(ps, n);
        if (n > 0) {
            selected_index_ = 0;
            selected_printer_id_ = ps[0].id;
        }
    }
    switch (current_) {
        case Screen::Dashboard: screens::update_dashboard(selected_printer_id_); break;
        case Screen::Ams:       screens::update_ams(selected_printer_id_);       break;
        case Screen::Printers:  screens::update_printers(selected_index_);       break;
        case Screen::History:   screens::update_history();                       break;
        case Screen::Settings:  screens::update_settings();                      break;
        default: break;
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

void Manager::handle_input() {
    using hw::Btn;
    using hw::BtnEvent;
    hw::ButtonEvent e;
    while (hw::g_buttons.next_event(e)) {
        // Any input wakes the screen up.
        hw::g_display.set_backlight(display::BL_FULL);

        if (e.btn == Btn::Prev && e.ev == BtnEvent::Press) {
            int next = ((int)current_ - 1 + (int)Screen::_Count) % (int)Screen::_Count;
            go_to((Screen)next);
        } else if (e.btn == Btn::Next && e.ev == BtnEvent::Press) {
            int next = ((int)current_ + 1) % (int)Screen::_Count;
            go_to((Screen)next);
        } else if (e.btn == Btn::Ok && e.ev == BtnEvent::Press) {
            // Dashboard: refresh; Printers: select printer.
            if (current_ == Screen::Dashboard) {
                if (selected_printer_id_ >= 0) {
                    ::bambuddy::g_client.refresh_status(selected_printer_id_);
                    show_toast("Refreshing\xE2\x80\xA6", lv_color_hex(::ui::C_ACCENT));
                }
            } else if (current_ == Screen::Printers) {
                ::bambuddy::Printer ps[8]; uint8_t n = 0;
                ::bambuddy::g_client.snapshot_printers(ps, n);
                if (selected_index_ >= 0 && selected_index_ < (int)n) {
                    selected_printer_id_ = ps[selected_index_].id;
                    go_to(Screen::Dashboard);
                }
            }
        } else if (e.btn == Btn::Prev && (e.ev == BtnEvent::LongPress || e.ev == BtnEvent::Repeat)) {
            if (current_ == Screen::Dashboard ||
                current_ == Screen::Ams       ||
                current_ == Screen::Printers) {
                cycle_printer(-1, selected_index_, selected_printer_id_);
            }
        } else if (e.btn == Btn::Next && (e.ev == BtnEvent::LongPress || e.ev == BtnEvent::Repeat)) {
            if (current_ == Screen::Dashboard ||
                current_ == Screen::Ams       ||
                current_ == Screen::Printers) {
                cycle_printer(+1, selected_index_, selected_printer_id_);
            }
        } else if (e.btn == Btn::Ok && e.ev == BtnEvent::LongPress) {
            if (current_ == Screen::Dashboard && selected_printer_id_ >= 0) {
                // Cycle print speed mode.
                static uint8_t mode = 2;
                mode = (uint8_t)((mode % 4) + 1);
                ::bambuddy::g_client.set_print_speed(selected_printer_id_, mode);
                const char* names[] = {"Silent", "Standard", "Sport", "Ludicrous"};
                String msg = String("Speed: ") + names[mode - 1];
                show_toast(msg.c_str(), lv_color_hex(::ui::C_OK));
            } else if (current_ == Screen::Ams) {
                // Cycle to the next AMS unit on multi-AMS setups.
                screens::ams_cycle_unit(+1);
            }
        }
    }
}

}  // namespace ui
