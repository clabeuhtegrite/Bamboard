// Bamboard UI — LVGL screen manager.
//
// Five screens stacked under a bottom tab bar (Live / AMS / Printers /
// History / Settings). Two ways to navigate:
//   - Tap a tab → go straight to that screen.
//   - Swipe left / right inside the screen body → next / previous screen.
//
// All touch input is wired through LVGL event callbacks set up at
// build time in screens.cpp; the manager only publishes go_to() and
// the selected printer id so the screens / tab bar can coordinate.
// There's no manual input pump — LVGL's pointer indev (fed by the
// GT911 in hw/display.cpp) does that for us.

#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace ui {

enum class Screen : uint8_t {
    Dashboard = 0,
    Ams,
    Printers,
    Queue,
    History,
    Settings,
    _Count,
};

class Manager {
   public:
    void begin();

    // Periodic UI refresh (pull latest snapshots into widgets).
    void refresh();

    // Public navigation API — called by tab-bar buttons, swipe-gesture
    // handlers and the Printers screen's "tap a row" callback.
    void go_to(Screen s);
    void go_to_next();
    void go_to_prev();

    // Currently focused printer id; -1 until the first snapshot arrives.
    int  selected_printer_id() const { return selected_printer_id_; }
    void set_selected_printer(int id);

    Screen current() const { return current_; }

   private:
    void show_toast(const char* text, lv_color_t colour);

    Screen current_ = Screen::Dashboard;
    int    selected_printer_id_ = -1;

    // HMS flash state machine (driven from refresh()). The overlay's own
    // visibility lives in screens::hms_flash_is_visible(); we only track
    // when to show/hide it next.
    bool     hms_was_active_   = false;
    uint32_t hms_next_show_ms_ = 0;
    uint32_t hms_hide_at_ms_   = 0;
    String   hms_last_msg_;
};

extern Manager g_ui;

}  // namespace ui
