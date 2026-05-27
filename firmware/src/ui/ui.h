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

    // What screen are we on?
    Screen current() const { return current_; }
    int selected_printer_id() const { return selected_printer_id_; }

   private:
    void go_to(Screen s);
    void show_toast(const char* text, lv_color_t colour);

    Screen current_ = Screen::Dashboard;
    int    selected_printer_id_ = -1;
    int    selected_index_      = 0;
    lv_obj_t* nav_indicator_ = nullptr;
};

extern Manager g_ui;

}  // namespace ui
