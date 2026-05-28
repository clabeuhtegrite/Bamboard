// Bamboard UI — LVGL screen manager.
//
// Four screens connected as a horizontal carousel. The three buttons
// behave as follows:
//
//   On any screen:
//     PREV (short) ↔ NEXT (short) : switch screen
//     OK   (long)                 : open contextual actions for the current screen
//
//   On the dashboard:
//     PREV (long) : previous printer
//     NEXT (long) : next printer
//     OK   (short): trigger refresh
//
// The UI never blocks: it reads snapshots produced by the network task and
// re-renders. Animations are kept simple (LVGL transitions) to remain
// snappy on the S3.

#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "../hw/buttons.h"

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

    // Pull events from the buttons module and dispatch them.
    void handle_input();

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
