// Bamboard screens — shared theme + helpers.
//
// Every per-screen TU (header.cpp, dashboard.cpp, ams.cpp, …) includes
// this header for the shared styles + the small handful of helpers that
// appear on more than one screen.
//
// Styles are defined once in theme.cpp and referenced by extern from
// every screen. ensure_styles() initialises them on first call and is
// idempotent — every screen builder calls it before using any style so
// the order of build_*() invocations doesn't matter.
//
// Internal-only — do NOT include this from outside firmware/src/ui/screens/.
// External callers go through firmware/src/ui/screens.h.

#pragma once

#include <lvgl.h>
#include <Arduino.h>

#include "../../config.h"
#include "../../net/bambuddy_client.h"
#include "../screens.h"
#include "../ui.h"

namespace ui::screens {

// ---------- Shared styles (initialised by ensure_styles) -------------------

extern lv_style_t s_panel;        // raised card surface (panel_hi bg, rounded)
extern lv_style_t s_panel_flat;   // same shape with the base C_PANEL bg
extern lv_style_t s_label_dim;    // captions / secondary text
extern lv_style_t s_label_value;  // primary readouts
extern lv_style_t s_label_big;    // hero numbers
extern lv_style_t s_btn;          // base touch button (neutral)
extern lv_style_t s_btn_accent;   // CTA button (teal bg, dark text)
extern lv_style_t s_btn_pressed;  // press state shared by every button
extern lv_style_t s_chip_off;     // inactive segment in segmented controls
extern lv_style_t s_chip_on;      // active segment in segmented controls

void ensure_styles();

// ---------- Layout helpers --------------------------------------------------

// Height of the body region between the header and the tab bar. Used by
// every screen builder to size its root container. We pull from
// ::display::HEIGHT (a real compile-time constant) rather than from
// LV_VER_RES — the latter expands to lv_disp_get_ver_res(NULL), which
// isn't valid in a constexpr context.
constexpr int body_h() {
    return ::display::HEIGHT - (int)::ui::HEADER_H - (int)::ui::TAB_BAR_H;
}

// ---------- Tiny widget tree helpers ---------------------------------------

void clear_children(lv_obj_t* o);

// ---------- Printer-state cosmetics -----------------------------------------
//
// Both the dashboard and the printers-list screens describe a printer
// the same way; centralising the strings + colours keeps them in sync.

const char* state_name (::bambuddy::PrinterState s);
uint32_t    state_color(::bambuddy::PrinterState s);
const char* speed_mode_name(uint8_t mode);   // 1..4

// ---------- Toast auto-hide --------------------------------------------------
//
// The toast widget itself is owned by overlays.cpp; every per-screen
// update_*() calls this on entry to expire stale toasts. Cheap when no
// toast is visible — just a flag check.

void maybe_hide_toast();

}  // namespace ui::screens
