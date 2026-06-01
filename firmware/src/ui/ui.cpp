#include "ui.h"

#include <lvgl.h>

#include "../config.h"
#include "../hw/display.h"
#include "../net/bambuddy_client.h"
#include "i18n.h"
#include "screens.h"

namespace ui {

Manager g_ui;

// --- LVGL style helpers ----------------------------------------------------

static lv_style_t s_style_bg;

static void init_theme() {
    lv_style_init(&s_style_bg);
    lv_style_set_bg_color(&s_style_bg, lv_color_hex(::ui::C_BG));
    lv_style_set_bg_opa(&s_style_bg, LV_OPA_COVER);
    lv_style_set_border_width(&s_style_bg, 0);
    lv_style_set_pad_all(&s_style_bg, 0);
}

// --- screen carousel -------------------------------------------------------

static lv_obj_t* s_root        = nullptr;
static lv_obj_t* s_screens[(uint8_t)Screen::_Count] = {};
static lv_obj_t* s_header      = nullptr;

// Forward declaration — the swipe handler attached to each screen needs
// to call go_to_next / go_to_prev on the manager.
static void screen_gesture_cb(lv_event_t* e);

void Manager::begin() {
    init_theme();

    s_root = lv_scr_act();
    lv_obj_add_style(s_root, &s_style_bg, 0);

    s_header = screens::build_header(s_root);

    // Build screens.
    s_screens[(uint8_t)Screen::Dashboard] = screens::build_dashboard(s_root);
    s_screens[(uint8_t)Screen::Ams]       = screens::build_ams(s_root);
    s_screens[(uint8_t)Screen::Printers]  = screens::build_printers(s_root);
    s_screens[(uint8_t)Screen::History]   = screens::build_history(s_root);
    s_screens[(uint8_t)Screen::Settings]  = screens::build_settings(s_root);

    // Attach swipe-gesture detection on every screen container so the
    // user can flick left / right between them regardless of where they
    // touched inside the body.
    for (uint8_t i = 0; i < (uint8_t)Screen::_Count; ++i) {
        lv_obj_add_event_cb(s_screens[i], screen_gesture_cb,
                            LV_EVENT_GESTURE, nullptr);
        lv_obj_add_flag(s_screens[i], LV_OBJ_FLAG_HIDDEN);
    }

    screens::build_toast(s_root);
    screens::build_hms_flash(s_root);
    screens::build_ota_overlay(s_root);

    // The tab bar lives on top of every screen and stays clickable, so
    // build it last (LVGL z-orders by creation order within a parent).
    screens::build_tab_bar(s_root);

    go_to(Screen::Dashboard);
}

void Manager::go_to(Screen s) {
    if (s == current_) return;
    lv_obj_add_flag(s_screens[(uint8_t)current_], LV_OBJ_FLAG_HIDDEN);
    current_ = s;
    lv_obj_clear_flag(s_screens[(uint8_t)current_], LV_OBJ_FLAG_HIDDEN);
    // The header shows the brand wordmark (set once in build_header); the
    // active screen is indicated by the bottom tab bar, not a header title.
    screens::tab_bar_set_active((uint8_t)current_);
}

void Manager::go_to_next() {
    int next = ((int)current_ + 1) % (int)Screen::_Count;
    go_to((Screen)next);
}

void Manager::go_to_prev() {
    int prev = ((int)current_ - 1 + (int)Screen::_Count) % (int)Screen::_Count;
    go_to((Screen)prev);
}

void Manager::set_selected_printer(int id) {
    selected_printer_id_ = id;
}

void Manager::refresh() {
    // OTA gets right-of-way: while an update is in flight, the rest of the
    // UI is irrelevant and the overlay is the only thing the user should
    // see. We still tick through the other screens so they're in a sane
    // state when the device reboots back to the same UI on success.
    screens::ota_apply();

    // Sync any net-task-side header updates (connectivity readout) onto the
    // UI task, where the LVGL widget actually lives.
    screens::header_apply();

    // Auto-pick the first printer the first time we know about one.
    ::bambuddy::Printer ps[8];
    uint8_t n = 0;
    ::bambuddy::g_client.snapshot_printers(ps, n);
    if (selected_printer_id_ < 0 && n > 0) {
        selected_printer_id_ = ps[0].id;
    }

    switch (current_) {
        case Screen::Dashboard: screens::update_dashboard(selected_printer_id_); break;
        case Screen::Ams:       screens::update_ams(selected_printer_id_);       break;
        case Screen::Printers:  screens::update_printers(selected_printer_id_);  break;
        case Screen::History:   screens::update_history();                       break;
        case Screen::Settings:  screens::update_settings();                      break;
        default: break;
    }

    // --- HMS full-screen flash state machine -------------------------------
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

// --- swipe gesture ---------------------------------------------------------

static void screen_gesture_cb(lv_event_t* e) {
    if (screens::ota_is_active())       return;
    if (screens::hms_flash_is_visible()) return;

    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    // LVGL emits LV_EVENT_GESTURE on every event after a gesture is
    // detected; consume the indev so we don't fire the swipe twice for
    // the same flick.
    lv_indev_wait_release(indev);

    if (dir == LV_DIR_LEFT)       g_ui.go_to_next();
    else if (dir == LV_DIR_RIGHT) g_ui.go_to_prev();
}

}  // namespace ui
