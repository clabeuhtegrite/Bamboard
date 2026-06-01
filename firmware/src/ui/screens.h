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

// Currently visible AMS unit index inside the focused printer. Used by
// the AMS drying-button event handler to address the right unit when it
// fires the POST.
uint8_t ams_visible_unit_index();

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

// Full-screen camera snapshot overlay. Opened by tapping the dashboard
// progress ring; while it's open the net task fetches + decodes JPEG frames
// (camera_decode_frame, called ON the net task), camera_apply() (UI task)
// publishes the latest frame to the on-screen image, and a tap dismisses it.
lv_obj_t* build_camera_overlay(lv_obj_t* parent);
void      camera_overlay_open();
void      camera_overlay_close();
bool      camera_overlay_is_open();
void      camera_decode_frame(const uint8_t* jpeg, size_t len);  // net task
void      camera_apply();                                        // UI task
// Optional inline thumbnail of the same frame (the Live dashboard registers
// one). camera_has_frame() is true once a frame has actually decoded.
void      camera_attach_thumbnail(lv_obj_t* img, uint16_t w, uint16_t h);
bool      camera_has_frame();

// Shared header (title + connectivity indicator). Used by the carousel.
// header_set_online() is safe to call from any task — it parks the new
// state and header_apply() (pumped by the UI manager) syncs it into the
// widget on the UI task. header_set_title() / header_set_printer_name()
// must only be called from the UI task.
lv_obj_t* build_header(lv_obj_t* parent);
void      header_set_title       (const char* title);
void      header_set_online      (bool online, uint32_t latency_ms);
void      header_set_printer_name(const char* name);
void      header_apply           ();

// Bottom tab bar. Permanent — sits above every screen and routes taps
// to ui::g_ui.go_to(Screen). tab_bar_set_active() highlights the tab
// matching the currently visible screen.
lv_obj_t* build_tab_bar(lv_obj_t* parent);
void      tab_bar_set_active(uint8_t idx);

}  // namespace ui::screens
