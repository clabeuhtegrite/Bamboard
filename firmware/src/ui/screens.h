// Per-screen builders. Each screen returns its root container; the UI
// manager wires them into a single carousel.

#pragma once

#include <lvgl.h>

#include "../net/bambuddy_client.h"

namespace ui::screens {

lv_obj_t* build_dashboard(lv_obj_t* parent);
lv_obj_t* build_printers (lv_obj_t* parent);
lv_obj_t* build_history  (lv_obj_t* parent);
lv_obj_t* build_settings (lv_obj_t* parent);

// Each screen exposes an update() that copies fresh data from the
// Bambuddy client snapshot into its widgets. Called from the UI manager.
void update_dashboard(int printer_id);
void update_printers(int selected_index);
void update_history();
void update_settings();

// Toast / message overlay used by the manager.
lv_obj_t* build_toast(lv_obj_t* parent);
void      show_toast(const char* msg, lv_color_t bg);

// Shared header (title + connectivity indicator). Used by the carousel.
lv_obj_t* build_header(lv_obj_t* parent);
void      header_set_title(const char* title);
void      header_set_online(bool online, uint32_t latency_ms);
void      header_set_printer_name(const char* name);

}  // namespace ui::screens
