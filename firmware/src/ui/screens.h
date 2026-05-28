// Per-screen builders. Each screen returns its root container; the UI
// manager wires them into a single carousel.

#pragma once

#include <lvgl.h>

#include "../net/bambuddy_client.h"

namespace ui::screens {

lv_obj_t* build_dashboard(lv_obj_t* parent);
lv_obj_t* build_ams      (lv_obj_t* parent);
lv_obj_t* build_printers (lv_obj_t* parent);
lv_obj_t* build_history  (lv_obj_t* parent);
lv_obj_t* build_settings (lv_obj_t* parent);

// Each screen exposes an update() that copies fresh data from the
// Bambuddy client snapshot into its widgets. Called from the UI manager.
void update_dashboard(int printer_id);
void update_ams(int printer_id);
void update_printers(int selected_index);
void update_history();
void update_settings();

// Cycle visible AMS unit when the selected printer has more than one. Called
// by the UI manager on long-press OK while the AMS screen is active.
void ams_cycle_unit(int dir);

// Toast / message overlay used by the manager.
lv_obj_t* build_toast(lv_obj_t* parent);
void      show_toast(const char* msg, lv_color_t bg);

// Per-printer actions modal (HMS clear / clear plate). Triggered from
// the Printers screen via long-press OK. The UI manager funnels button
// events to actions_navigate/confirm/close while it's open and skips
// the normal screen routing.
lv_obj_t* build_actions   (lv_obj_t* parent);
void      actions_open    (int printer_id, const char* printer_name);
void      actions_close   ();
void      actions_navigate(int dir);
void      actions_confirm ();
bool      actions_is_open ();

// Shared header (title + connectivity indicator). Used by the carousel.
lv_obj_t* build_header(lv_obj_t* parent);
void      header_set_title(const char* title);
void      header_set_online(bool online, uint32_t latency_ms);
void      header_set_printer_name(const char* name);

}  // namespace ui::screens
