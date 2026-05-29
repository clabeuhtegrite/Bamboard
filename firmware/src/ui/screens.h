// Per-screen builders. Each screen returns its root container; the UI
// manager wires them into a tab-bar-driven stack on the 480 × 272 panel.

#pragma once

#include <lvgl.h>

#include "../net/bambuddy_client.h"

namespace ui::screens {

lv_obj_t* build_dashboard(lv_obj_t* parent);
lv_obj_t* build_ams      (lv_obj_t* parent);
lv_obj_t* build_printers (lv_obj_t* parent);
lv_obj_t* build_history  (lv_obj_t* parent);
lv_obj_t* build_settings (lv_obj_t* parent);

// Per-tick updaters — copy the latest Bambuddy snapshot into each
// screen's widgets. Called from ui::Manager::refresh().
void update_dashboard(int printer_id);
void update_ams      (int printer_id);
void update_printers (int focused_printer_id);
void update_history  ();
void update_settings ();

// Cycle visible AMS unit when the selected printer has more than one.
void ams_cycle_unit(int dir);

// Toast / message overlay used by the manager.
lv_obj_t* build_toast(lv_obj_t* parent);
void      show_toast(const char* msg, lv_color_t bg);

// Full-screen HMS-error flash. The UI manager drives a periodic show /
// hide cycle while an HMS error is active; consumers just call these.
// Touch anywhere on the overlay dismisses it immediately; the manager
// re-arms it after the cooldown if the error persists.
lv_obj_t* build_hms_flash(lv_obj_t* parent);
void      hms_flash_show      (const char* msg);
void      hms_flash_hide      ();
void      hms_flash_update_msg(const char* msg);
bool      hms_flash_is_visible();

// Full-screen OTA progress overlay. Same shape as before — the setters
// are safe to call from any task, ota_apply() is called from the UI
// task once per refresh tick to sync the shared state into the widget.
lv_obj_t* build_ota_overlay(lv_obj_t* parent);
void      ota_set_active  (bool active);
void      ota_set_progress(uint8_t pct);
void      ota_set_error   (const char* msg);
void      ota_apply       ();
bool      ota_is_active   ();

// Shared header (title + connectivity indicator). Used by the carousel.
lv_obj_t* build_header(lv_obj_t* parent);
void      header_set_title(const char* title);
void      header_set_online(bool online, uint32_t latency_ms);
void      header_set_printer_name(const char* name);

// Bottom tab bar. Permanent — sits above every screen and routes taps
// to ui::g_ui.go_to(Screen). tab_bar_set_active() highlights the tab
// matching the currently visible screen.
lv_obj_t* build_tab_bar(lv_obj_t* parent);
void      tab_bar_set_active(uint8_t idx);

}  // namespace ui::screens
