// Bamboard UI — LVGL screen manager.
//
// Five screens stacked under a bottom tab bar (Live / AMS / Printers /
// History / Settings). Tap a tab to switch; the follow-up commit will
// also wire a swipe gesture across the screen body.
//
// The UI never blocks: it reads snapshots produced by the network task
// and re-renders. Touch events come straight from the LVGL pointer
// input device registered by hw/display.cpp, so the screen manager
// only needs to publish the screens themselves — no manual event poll.

#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace ui {

enum class Screen : uint8_t {
    Dashboard = 0,
    Ams,
    Printers,
    History,
    Settings,
    _Count,
};

class Manager {
   public:
    void begin();

    // Periodic UI refresh (pull latest snapshots into widgets).
    void refresh();

    int selected_printer_id() const { return selected_printer_id_; }

   private:
    void go_to(Screen s);
    void show_toast(const char* text, lv_color_t colour);

    Screen current_ = Screen::Dashboard;
    int    selected_printer_id_ = -1;
    int    selected_index_      = 0;

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
