// =============================================================================
// Bamboard screens — v1.0 touch UI for the Guition JC4827W543 (480 × 272)
// =============================================================================
//
// Five screens stacked under a fixed bottom tab bar:
//
//   y =   0 .. 36   shared header (Bamboard logo / printer name / connectivity)
//   y =  36 .. 228  screen body (192 px tall, varies per screen)
//   y = 228 .. 272  tab bar (44 px tall, always on top)
//
// On top of that, three full-screen overlays:
//
//   - toast       : ephemeral confirmation at the bottom of the body
//   - hms_flash   : full-screen red pulse when the focused printer has an
//                   HMS error; tap anywhere to dismiss
//   - ota_overlay : opaque progress UI while ArduinoOTA is receiving a
//                   firmware image
//
// Touch input is routed by LVGL itself via event callbacks attached at
// build time — tap a tab to switch screens, tap a row in the Printers
// list to focus that printer, tap an inline action button on Live to
// run Clear HMS / Clear plate / Cycle speed.

#include "screens.h"

#include <lvgl.h>
#include <WiFi.h>

#include "../config.h"
#include "../net/bambuddy_client.h"
#include "ui.h"

extern void factory_reset();   // defined in main.cpp — used by Settings

// Defined in main.cpp (or sim/stubs/ArduinoStubs.cpp on the desktop sim).
// MUST be declared at file scope, NOT inside the `ui::screens` namespace
// — a `block-scope` extern declaration introduces the name in the
// nearest enclosing namespace, which would make the linker look for
// `ui::screens::g_cfg_bambuddy_url` and not find it (MSVC catches this
// strictly; gcc accepts it as a quirk). Keeping it up here forces the
// reference to resolve to the global ::g_cfg_bambuddy_url symbol.
extern String g_cfg_bambuddy_url;

namespace ui::screens {

// ---------- Shared styles ---------------------------------------------------

static lv_style_t s_panel;
static lv_style_t s_label_dim;
static lv_style_t s_label_value;
static lv_style_t s_label_big;
static lv_style_t s_btn;
static lv_style_t s_btn_accent;
static bool       styles_ready = false;

static void ensure_styles() {
    if (styles_ready) return;
    styles_ready = true;

    lv_style_init(&s_panel);
    lv_style_set_bg_color(&s_panel, lv_color_hex(::ui::C_PANEL));
    lv_style_set_bg_opa(&s_panel, LV_OPA_COVER);
    lv_style_set_border_width(&s_panel, 0);
    lv_style_set_radius(&s_panel, 8);
    lv_style_set_pad_all(&s_panel, 8);

    lv_style_init(&s_label_dim);
    lv_style_set_text_color(&s_label_dim, lv_color_hex(::ui::C_TEXT_DIM));
    lv_style_set_text_font(&s_label_dim, &lv_font_montserrat_12);

    lv_style_init(&s_label_value);
    lv_style_set_text_color(&s_label_value, lv_color_hex(::ui::C_TEXT));
    lv_style_set_text_font(&s_label_value, &lv_font_montserrat_20);

    lv_style_init(&s_label_big);
    lv_style_set_text_color(&s_label_big, lv_color_hex(::ui::C_TEXT));
    lv_style_set_text_font(&s_label_big, &lv_font_montserrat_28);

    // Touch button base style — rounded, panel-ish, big enough for fat
    // fingers, with a darker pressed state.
    lv_style_init(&s_btn);
    lv_style_set_bg_color(&s_btn, lv_color_hex(::ui::C_PANEL_HI));
    lv_style_set_bg_opa(&s_btn, LV_OPA_COVER);
    lv_style_set_radius(&s_btn, 10);
    lv_style_set_border_width(&s_btn, 0);
    lv_style_set_pad_all(&s_btn, 8);
    lv_style_set_text_color(&s_btn, lv_color_hex(::ui::C_TEXT));
    lv_style_set_text_font(&s_btn, &lv_font_montserrat_16);

    // Accent variant — used for the "do the thing" buttons.
    lv_style_init(&s_btn_accent);
    lv_style_set_bg_color(&s_btn_accent, lv_color_hex(::ui::C_ACCENT));
    lv_style_set_text_color(&s_btn_accent, lv_color_hex(0x101418));
}

// =============================================================================
// HEADER  (y = 0 .. 36)
// =============================================================================

static lv_obj_t* s_hdr_title       = nullptr;
static lv_obj_t* s_hdr_printer     = nullptr;
static lv_obj_t* s_hdr_conn        = nullptr;

lv_obj_t* build_header(lv_obj_t* parent) {
    ensure_styles();
    lv_obj_t* hdr = lv_obj_create(parent);
    lv_obj_remove_style_all(hdr);
    lv_obj_set_size(hdr, LV_HOR_RES, 36);
    lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(::ui::C_PANEL), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    s_hdr_title = lv_label_create(hdr);
    lv_label_set_text(s_hdr_title, "Bamboard");
    lv_obj_align(s_hdr_title, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_set_style_text_color(s_hdr_title, lv_color_hex(::ui::C_ACCENT), 0);
    lv_obj_set_style_text_font(s_hdr_title, &lv_font_montserrat_20, 0);

    s_hdr_printer = lv_label_create(hdr);
    lv_label_set_text(s_hdr_printer, "");
    lv_obj_align(s_hdr_printer, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(s_hdr_printer, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_set_style_text_font(s_hdr_printer, &lv_font_montserrat_16, 0);

    s_hdr_conn = lv_label_create(hdr);
    lv_label_set_text(s_hdr_conn, LV_SYMBOL_WIFI " --");
    lv_obj_align(s_hdr_conn, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_text_color(s_hdr_conn, lv_color_hex(::ui::C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_hdr_conn, &lv_font_montserrat_14, 0);
    return hdr;
}

void header_set_title(const char* title) {
    if (s_hdr_title) lv_label_set_text(s_hdr_title, title);
}

void header_set_online(bool online, uint32_t latency_ms) {
    if (!s_hdr_conn) return;
    if (online) {
        char buf[24];
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %ums", (unsigned)latency_ms);
        lv_label_set_text(s_hdr_conn, buf);
        lv_obj_set_style_text_color(s_hdr_conn, lv_color_hex(::ui::C_OK), 0);
    } else {
        lv_label_set_text(s_hdr_conn, LV_SYMBOL_WARNING " offline");
        lv_obj_set_style_text_color(s_hdr_conn, lv_color_hex(::ui::C_ERR), 0);
    }
}

void header_set_printer_name(const char* name) {
    if (s_hdr_printer) lv_label_set_text(s_hdr_printer, name ? name : "");
}

// =============================================================================
// TOAST
// =============================================================================

static lv_obj_t* s_toast       = nullptr;
static lv_obj_t* s_toast_label = nullptr;
static uint32_t  s_toast_hide_at = 0;

lv_obj_t* build_toast(lv_obj_t* parent) {
    s_toast = lv_obj_create(parent);
    lv_obj_remove_style_all(s_toast);
    lv_obj_set_size(s_toast, 280, 36);
    // Sit just above the tab bar — y offset = tab_bar_h + 8 px gap.
    lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0,
                 -(int)::ui::TAB_BAR_H - 8);
    lv_obj_set_style_bg_color(s_toast, lv_color_hex(::ui::C_ACCENT), 0);
    lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_toast, 8, 0);
    lv_obj_clear_flag(s_toast, LV_OBJ_FLAG_SCROLLABLE);

    s_toast_label = lv_label_create(s_toast);
    lv_label_set_text(s_toast_label, "");
    lv_obj_center(s_toast_label);
    lv_obj_set_style_text_color(s_toast_label, lv_color_hex(0x101418), 0);
    lv_obj_set_style_text_font(s_toast_label, &lv_font_montserrat_16, 0);

    lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    return s_toast;
}

void show_toast(const char* msg, lv_color_t bg) {
    if (!s_toast) return;
    lv_label_set_text(s_toast_label, msg);
    lv_obj_set_style_bg_color(s_toast, bg, 0);
    lv_obj_clear_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_toast);
    s_toast_hide_at = lv_tick_get() + 1800;
}

// Toast auto-hide is checked on each update_*().
static void maybe_hide_toast() {
    if (!s_toast || lv_obj_has_flag(s_toast, LV_OBJ_FLAG_HIDDEN)) return;
    if (s_toast_hide_at && lv_tick_get() > s_toast_hide_at) {
        lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    }
}

// =============================================================================
// TAB BAR  (y = 228 .. 272)
// =============================================================================

static lv_obj_t* s_tab_bar = nullptr;
static lv_obj_t* s_tab_btns  [(uint8_t)Screen::_Count] = {};
static lv_obj_t* s_tab_icons [(uint8_t)Screen::_Count] = {};
static lv_obj_t* s_tab_labels[(uint8_t)Screen::_Count] = {};
static uint8_t   s_tab_active = 0;

static const char* k_tab_labels[(uint8_t)Screen::_Count] = {
    "Live", "AMS", "Printers", "History", "Settings",
};
static const char* k_tab_icons[(uint8_t)Screen::_Count] = {
    LV_SYMBOL_HOME, LV_SYMBOL_TINT, LV_SYMBOL_LIST, LV_SYMBOL_LOOP, LV_SYMBOL_SETTINGS,
};

static void tab_clicked_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    uint8_t idx = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    (void)btn;
    ::ui::g_ui.go_to((::ui::Screen)idx);
}

lv_obj_t* build_tab_bar(lv_obj_t* parent) {
    ensure_styles();
    s_tab_bar = lv_obj_create(parent);
    lv_obj_remove_style_all(s_tab_bar);
    lv_obj_set_size(s_tab_bar, LV_HOR_RES, ::ui::TAB_BAR_H);
    lv_obj_align(s_tab_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_tab_bar, lv_color_hex(::ui::C_PANEL), 0);
    lv_obj_set_style_bg_opa(s_tab_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_tab_bar, 0, 0);
    lv_obj_clear_flag(s_tab_bar, LV_OBJ_FLAG_SCROLLABLE);

    // 5 buttons, equal width, sliced edge-to-edge.
    int tab_w = LV_HOR_RES / (int)Screen::_Count;
    for (uint8_t i = 0; i < (uint8_t)Screen::_Count; ++i) {
        lv_obj_t* btn = lv_btn_create(s_tab_bar);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, tab_w, ::ui::TAB_BAR_H);
        lv_obj_set_pos(btn, i * tab_w, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_add_event_cb(btn, tab_clicked_cb, LV_EVENT_CLICKED,
                            (void*)(intptr_t)i);

        lv_obj_t* icon = lv_label_create(btn);
        lv_label_set_text(icon, k_tab_icons[i]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(::ui::C_TEXT_DIM), 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 4);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, k_tab_labels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(::ui::C_TEXT_DIM), 0);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -3);

        s_tab_btns  [i] = btn;
        s_tab_icons [i] = icon;
        s_tab_labels[i] = lbl;
    }

    tab_bar_set_active(0);
    return s_tab_bar;
}

void tab_bar_set_active(uint8_t idx) {
    if (idx >= (uint8_t)Screen::_Count) return;
    s_tab_active = idx;
    for (uint8_t i = 0; i < (uint8_t)Screen::_Count; ++i) {
        uint32_t col = (i == idx) ? ::ui::C_ACCENT : ::ui::C_TEXT_DIM;
        lv_obj_set_style_text_color(s_tab_icons [i], lv_color_hex(col), 0);
        lv_obj_set_style_text_color(s_tab_labels[i], lv_color_hex(col), 0);
    }
}

// Helper used by the screen builders to size their root container to the
// body area between the header and the tab bar. We pull from
// ::display::HEIGHT (a real compile-time constant) rather than from
// LV_VER_RES — the latter expands to lv_disp_get_ver_res(NULL), which
// isn't valid in a constexpr context.
static constexpr int body_h() {
    return ::display::HEIGHT - 36 - (int)::ui::TAB_BAR_H;
}

// =============================================================================
// DASHBOARD  (Live)
// =============================================================================

static lv_obj_t* s_dash_root         = nullptr;
static lv_obj_t* s_dash_state_lbl    = nullptr;
static lv_obj_t* s_dash_file_lbl     = nullptr;
static lv_obj_t* s_dash_progress_arc = nullptr;
static lv_obj_t* s_dash_progress_lbl = nullptr;
static lv_obj_t* s_dash_eta_lbl      = nullptr;
static lv_obj_t* s_dash_layer_lbl    = nullptr;
static lv_obj_t* s_dash_t_noz        = nullptr;
static lv_obj_t* s_dash_t_bed        = nullptr;
static lv_obj_t* s_dash_t_cham       = nullptr;
static lv_obj_t* s_dash_hms          = nullptr;

static lv_obj_t* s_dash_btn_speed       = nullptr;
static lv_obj_t* s_dash_btn_speed_lbl   = nullptr;
static lv_obj_t* s_dash_btn_plate       = nullptr;
static lv_obj_t* s_dash_btn_hms         = nullptr;

static const char* state_name(::bambuddy::PrinterState s) {
    using PS = ::bambuddy::PrinterState;
    switch (s) {
        case PS::Idle:     return "IDLE";
        case PS::Prepare:  return "PREPARE";
        case PS::Printing: return "PRINTING";
        case PS::Paused:   return "PAUSED";
        case PS::Finish:   return "FINISH";
        case PS::Failed:   return "FAILED";
        case PS::Offline:  return "OFFLINE";
        case PS::Error:    return "ERROR";
        default:           return "?";
    }
}

static uint32_t state_color(::bambuddy::PrinterState s) {
    using PS = ::bambuddy::PrinterState;
    switch (s) {
        case PS::Printing: return ::ui::C_ACCENT;
        case PS::Paused:   return ::ui::C_WARN;
        case PS::Finish:   return ::ui::C_OK;
        case PS::Failed:
        case PS::Error:    return ::ui::C_ERR;
        case PS::Offline:  return ::ui::C_TEXT_DIM;
        default:           return ::ui::C_TEXT_DIM;
    }
}

static const char* speed_mode_name(uint8_t mode) {
    switch (mode) {
        case 1: return "Silent";
        case 2: return "Standard";
        case 3: return "Sport";
        case 4: return "Ludicrous";
        default: return "?";
    }
}

// Build a tiny "label / value" temp cell.
static lv_obj_t* make_temp_cell(lv_obj_t* parent, const char* title,
                                 int x, int w) {
    lv_obj_t* cell = lv_obj_create(parent);
    lv_obj_add_style(cell, &s_panel, 0);
    lv_obj_set_size(cell, w, 36);
    lv_obj_set_pos(cell, x, 64);
    lv_obj_set_style_pad_all(cell, 6, 0);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* t = lv_label_create(cell);
    lv_label_set_text(t, title);
    lv_obj_add_style(t, &s_label_dim, 0);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* v = lv_label_create(cell);
    lv_label_set_text(v, "-- °C");
    lv_obj_set_style_text_color(v, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_set_style_text_font(v, &lv_font_montserrat_16, 0);
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
    return v;
}

// ----- inline action callbacks -----

static void btn_speed_clicked(lv_event_t*) {
    int id = ::ui::g_ui.selected_printer_id();
    if (id < 0) return;
    ::bambuddy::Printer ps[8]; uint8_t n = 0;
    ::bambuddy::g_client.snapshot_printers(ps, n);
    uint8_t cur = 2;
    for (uint8_t i = 0; i < n; ++i) if (ps[i].id == id) cur = ps[i].speed_level;
    uint8_t next = (uint8_t)((cur % 4) + 1);
    bool ok = ::bambuddy::g_client.set_print_speed(id, next);
    String msg = String("Speed: ") + speed_mode_name(next);
    show_toast(msg.c_str(),
               lv_color_hex(ok ? ::ui::C_OK : ::ui::C_ERR));
}

static void btn_plate_clicked(lv_event_t*) {
    int id = ::ui::g_ui.selected_printer_id();
    if (id < 0) return;
    bool ok = ::bambuddy::g_client.clear_plate(id);
    show_toast(ok ? "Plate cleared" : "Clear plate failed",
               lv_color_hex(ok ? ::ui::C_OK : ::ui::C_ERR));
}

static void btn_hms_clicked(lv_event_t*) {
    int id = ::ui::g_ui.selected_printer_id();
    if (id < 0) return;
    bool ok = ::bambuddy::g_client.clear_hms(id);
    show_toast(ok ? "HMS cleared" : "Clear HMS failed",
               lv_color_hex(ok ? ::ui::C_OK : ::ui::C_ERR));
}

static lv_obj_t* make_action_btn(lv_obj_t* parent, int x, int w,
                                  const char* text,
                                  lv_event_cb_t cb,
                                  lv_obj_t** out_label = nullptr) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, &s_btn, 0);
    lv_obj_add_style(btn, &s_btn_accent, LV_STATE_DEFAULT);
    lv_obj_set_size(btn, w, 44);
    lv_obj_set_pos(btn, x, 128);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    if (out_label) *out_label = lbl;
    return btn;
}

lv_obj_t* build_dashboard(lv_obj_t* parent) {
    ensure_styles();
    s_dash_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_dash_root);
    lv_obj_set_size(s_dash_root, LV_HOR_RES, body_h());
    lv_obj_align(s_dash_root, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_obj_clear_flag(s_dash_root, LV_OBJ_FLAG_SCROLLABLE);

    // --- Top section : arc on the left, info on the right ---
    s_dash_progress_arc = lv_arc_create(s_dash_root);
    lv_obj_set_size(s_dash_progress_arc, 60, 60);
    lv_obj_set_pos(s_dash_progress_arc, 12, 0);
    lv_arc_set_rotation(s_dash_progress_arc, 270);
    lv_arc_set_bg_angles(s_dash_progress_arc, 0, 360);
    lv_arc_set_range(s_dash_progress_arc, 0, 100);
    lv_arc_set_value(s_dash_progress_arc, 0);
    lv_obj_remove_style(s_dash_progress_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(s_dash_progress_arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_dash_progress_arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_dash_progress_arc,
                                lv_color_hex(::ui::C_PANEL_HI), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_dash_progress_arc,
                                lv_color_hex(::ui::C_ACCENT), LV_PART_INDICATOR);
    lv_obj_clear_flag(s_dash_progress_arc, LV_OBJ_FLAG_CLICKABLE);

    s_dash_progress_lbl = lv_label_create(s_dash_progress_arc);
    lv_label_set_text(s_dash_progress_lbl, "--%");
    lv_obj_set_style_text_font(s_dash_progress_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_dash_progress_lbl, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_center(s_dash_progress_lbl);

    s_dash_state_lbl = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_state_lbl, "—");
    lv_obj_set_pos(s_dash_state_lbl, 88, 0);
    lv_obj_set_style_text_font(s_dash_state_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_dash_state_lbl, lv_color_hex(::ui::C_ACCENT), 0);

    s_dash_eta_lbl = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_eta_lbl, "ETA --:--");
    lv_obj_set_pos(s_dash_eta_lbl, 320, 0);
    lv_obj_set_style_text_font(s_dash_eta_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_dash_eta_lbl, lv_color_hex(::ui::C_TEXT), 0);

    s_dash_file_lbl = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_file_lbl, "no file");
    lv_obj_set_width(s_dash_file_lbl, 220);
    lv_label_set_long_mode(s_dash_file_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_dash_file_lbl, 88, 26);
    lv_obj_add_style(s_dash_file_lbl, &s_label_dim, 0);

    s_dash_layer_lbl = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_layer_lbl, "layer --/--");
    lv_obj_set_pos(s_dash_layer_lbl, 320, 26);
    lv_obj_add_style(s_dash_layer_lbl, &s_label_dim, 0);

    // --- Temp row ---
    s_dash_t_noz  = make_temp_cell(s_dash_root, "NOZZLE",   12, 142);
    s_dash_t_bed  = make_temp_cell(s_dash_root, "BED",     162, 142);
    s_dash_t_cham = make_temp_cell(s_dash_root, "CHAMBER", 312, 156);

    // --- HMS line (hidden when "ok") ---
    s_dash_hms = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_hms, "");
    lv_obj_set_width(s_dash_hms, LV_HOR_RES - 24);
    lv_label_set_long_mode(s_dash_hms, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_dash_hms, 12, 108);
    lv_obj_set_style_text_color(s_dash_hms, lv_color_hex(::ui::C_ERR), 0);
    lv_obj_set_style_text_font(s_dash_hms, &lv_font_montserrat_14, 0);

    // --- Inline action buttons (visibility flipped each refresh) ---
    s_dash_btn_speed = make_action_btn(s_dash_root,  12, 156,
                                       "Speed", btn_speed_clicked,
                                       &s_dash_btn_speed_lbl);
    s_dash_btn_plate = make_action_btn(s_dash_root, 174, 130,
                                       "Clear plate", btn_plate_clicked);
    s_dash_btn_hms   = make_action_btn(s_dash_root, 314, 154,
                                       "Clear HMS", btn_hms_clicked);

    lv_obj_add_flag(s_dash_btn_speed, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_dash_btn_plate, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_dash_btn_hms,   LV_OBJ_FLAG_HIDDEN);
    return s_dash_root;
}

static String fmt_eta(uint32_t secs) {
    if (secs == 0) return "—";
    uint32_t h = secs / 3600;
    uint32_t m = (secs % 3600) / 60;
    char buf[16];
    if (h > 0) snprintf(buf, sizeof(buf), "%uh%02u", (unsigned)h, (unsigned)m);
    else       snprintf(buf, sizeof(buf), "%um",     (unsigned)m);
    return String(buf);
}

void update_dashboard(int printer_id) {
    maybe_hide_toast();
    ::bambuddy::Printer ps[8]; uint8_t n = 0;
    ::bambuddy::g_client.snapshot_printers(ps, n);
    if (n == 0) {
        lv_label_set_text(s_dash_state_lbl, "NO PRINTER");
        lv_label_set_text(s_dash_file_lbl, "Add one in Bambuddy");
        header_set_printer_name("");
        lv_obj_add_flag(s_dash_btn_speed, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_dash_btn_plate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_dash_btn_hms,   LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const ::bambuddy::Printer* sel = nullptr;
    for (uint8_t i = 0; i < n; ++i) if (ps[i].id == printer_id) sel = &ps[i];
    if (!sel) sel = &ps[0];

    header_set_printer_name(sel->name.c_str());

    lv_label_set_text(s_dash_state_lbl, state_name(sel->state));
    lv_obj_set_style_text_color(s_dash_state_lbl,
                                 lv_color_hex(state_color(sel->state)), 0);

    lv_label_set_text(s_dash_file_lbl,
                       sel->filename.length() ? sel->filename.c_str() : "(idle)");

    lv_arc_set_value(s_dash_progress_arc, sel->progress);
    char pbuf[8];
    snprintf(pbuf, sizeof(pbuf), "%u%%", (unsigned)sel->progress);
    lv_label_set_text(s_dash_progress_lbl, pbuf);

    String eta = String("ETA ") + fmt_eta(sel->remaining_s);
    lv_label_set_text(s_dash_eta_lbl, eta.c_str());

    char lay[24];
    if (sel->total_layers > 0)
        snprintf(lay, sizeof(lay), "layer %u/%u",
                 (unsigned)sel->current_layer, (unsigned)sel->total_layers);
    else
        snprintf(lay, sizeof(lay), "layer —");
    lv_label_set_text(s_dash_layer_lbl, lay);

    bool hms_active = sel->hms.length() && sel->hms != "ok";
    if (hms_active) {
        String h = String(LV_SYMBOL_WARNING " HMS: ") + sel->hms;
        lv_label_set_text(s_dash_hms, h.c_str());
    } else {
        lv_label_set_text(s_dash_hms, "");
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f °C", sel->temps.nozzle);
    lv_label_set_text(s_dash_t_noz, buf);
    snprintf(buf, sizeof(buf), "%.0f °C", sel->temps.bed);
    lv_label_set_text(s_dash_t_bed, buf);
    snprintf(buf, sizeof(buf), "%.0f °C", sel->temps.chamber);
    lv_label_set_text(s_dash_t_cham, buf);

    // --- Contextual action buttons ---
    using PS = ::bambuddy::PrinterState;
    bool can_speed = (sel->state == PS::Printing ||
                      sel->state == PS::Paused   ||
                      sel->state == PS::Prepare);
    bool can_plate = (sel->state == PS::Finish || sel->state == PS::Failed);
    bool can_hms   = hms_active;

    if (can_speed) {
        uint8_t cur  = (sel->speed_level >= 1 && sel->speed_level <= 4)
                       ? sel->speed_level : 2;
        uint8_t next = (uint8_t)((cur % 4) + 1);
        String label = String("Speed: ") + speed_mode_name(cur)
                     + " " LV_SYMBOL_RIGHT " " + speed_mode_name(next);
        lv_label_set_text(s_dash_btn_speed_lbl, label.c_str());
        lv_obj_clear_flag(s_dash_btn_speed, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_dash_btn_speed, LV_OBJ_FLAG_HIDDEN);
    }
    if (can_plate) lv_obj_clear_flag(s_dash_btn_plate, LV_OBJ_FLAG_HIDDEN);
    else           lv_obj_add_flag  (s_dash_btn_plate, LV_OBJ_FLAG_HIDDEN);
    if (can_hms)   lv_obj_clear_flag(s_dash_btn_hms,   LV_OBJ_FLAG_HIDDEN);
    else           lv_obj_add_flag  (s_dash_btn_hms,   LV_OBJ_FLAG_HIDDEN);
}

// =============================================================================
// AMS
// =============================================================================

static lv_obj_t* s_ams_root        = nullptr;
static lv_obj_t* s_ams_empty       = nullptr;
static lv_obj_t* s_ams_unit_lbl    = nullptr;
static lv_obj_t* s_ams_humid_lbl   = nullptr;
static lv_obj_t* s_ams_temp_lbl    = nullptr;
static lv_obj_t* s_ams_dry_lbl     = nullptr;
static lv_obj_t* s_ams_prev_btn    = nullptr;
static lv_obj_t* s_ams_next_btn    = nullptr;
static lv_obj_t* s_ams_row         = nullptr;
static lv_obj_t* s_ams_card[4]     = {};
static lv_obj_t* s_ams_card_swatch[4] = {};
static lv_obj_t* s_ams_card_type[4]   = {};
static lv_obj_t* s_ams_card_pct[4]    = {};
static lv_obj_t* s_ams_card_bar[4]    = {};

static int s_ams_visible_index = 0;

static lv_obj_t* make_ams_slot_card(lv_obj_t* parent,
                                     lv_obj_t** swatch,
                                     lv_obj_t** type_lbl,
                                     lv_obj_t** pct_lbl,
                                     lv_obj_t** bar) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_add_style(card, &s_panel, 0);
    lv_obj_set_size(card, 106, 156);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card, 0, 0);

    *swatch = lv_obj_create(card);
    lv_obj_remove_style_all(*swatch);
    lv_obj_set_size(*swatch, 106, 50);
    lv_obj_align(*swatch, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_radius(*swatch, 8, 0);
    lv_obj_set_style_bg_opa(*swatch, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(*swatch, lv_color_hex(::ui::C_PANEL_HI), 0);

    *type_lbl = lv_label_create(card);
    lv_label_set_text(*type_lbl, "—");
    lv_obj_align(*type_lbl, LV_ALIGN_TOP_LEFT, 8, 60);
    lv_obj_set_style_text_font(*type_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(*type_lbl, lv_color_hex(::ui::C_TEXT), 0);

    *pct_lbl = lv_label_create(card);
    lv_label_set_text(*pct_lbl, "--%");
    lv_obj_align(*pct_lbl, LV_ALIGN_TOP_LEFT, 8, 86);
    lv_obj_set_style_text_font(*pct_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(*pct_lbl, lv_color_hex(::ui::C_TEXT), 0);

    *bar = lv_bar_create(card);
    lv_obj_set_size(*bar, 90, 8);
    lv_obj_align(*bar, LV_ALIGN_BOTTOM_LEFT, 8, -12);
    lv_bar_set_range(*bar, 0, 100);
    lv_bar_set_value(*bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(*bar, lv_color_hex(::ui::C_PANEL_HI), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(*bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(*bar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(*bar, lv_color_hex(::ui::C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(*bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(*bar, 4, LV_PART_INDICATOR);
    return card;
}

static void ams_prev_clicked(lv_event_t*) { ams_cycle_unit(-1); }
static void ams_next_clicked(lv_event_t*) { ams_cycle_unit(+1); }

lv_obj_t* build_ams(lv_obj_t* parent) {
    ensure_styles();
    s_ams_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_ams_root);
    lv_obj_set_size(s_ams_root, LV_HOR_RES, body_h());
    lv_obj_align(s_ams_root, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_obj_clear_flag(s_ams_root, LV_OBJ_FLAG_SCROLLABLE);

    // --- Status strip ---
    s_ams_unit_lbl = lv_label_create(s_ams_root);
    lv_label_set_text(s_ams_unit_lbl, "AMS");
    lv_obj_align(s_ams_unit_lbl, LV_ALIGN_TOP_LEFT, 16, 4);
    lv_obj_set_style_text_font(s_ams_unit_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_ams_unit_lbl, lv_color_hex(::ui::C_ACCENT), 0);

    s_ams_humid_lbl = lv_label_create(s_ams_root);
    lv_label_set_text(s_ams_humid_lbl, "");
    lv_obj_align(s_ams_humid_lbl, LV_ALIGN_TOP_LEFT, 130, 6);
    lv_obj_set_style_text_font(s_ams_humid_lbl, &lv_font_montserrat_16, 0);

    s_ams_temp_lbl = lv_label_create(s_ams_root);
    lv_label_set_text(s_ams_temp_lbl, "");
    lv_obj_align(s_ams_temp_lbl, LV_ALIGN_TOP_LEFT, 220, 6);
    lv_obj_set_style_text_font(s_ams_temp_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_ams_temp_lbl, lv_color_hex(::ui::C_TEXT), 0);

    s_ams_dry_lbl = lv_label_create(s_ams_root);
    lv_label_set_text(s_ams_dry_lbl, "");
    lv_obj_align(s_ams_dry_lbl, LV_ALIGN_TOP_RIGHT, -16, 6);
    lv_obj_set_style_text_font(s_ams_dry_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_ams_dry_lbl, lv_color_hex(::ui::C_WARN), 0);

    // --- AMS-unit cycle buttons (shown only when >1 unit) ---
    s_ams_prev_btn = lv_btn_create(s_ams_root);
    lv_obj_remove_style_all(s_ams_prev_btn);
    lv_obj_add_style(s_ams_prev_btn, &s_btn, 0);
    lv_obj_set_size(s_ams_prev_btn, 32, 28);
    lv_obj_align(s_ams_prev_btn, LV_ALIGN_TOP_LEFT, 76, 2);
    lv_obj_set_style_pad_all(s_ams_prev_btn, 0, 0);
    lv_obj_add_event_cb(s_ams_prev_btn, ams_prev_clicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* p_lbl = lv_label_create(s_ams_prev_btn);
    lv_label_set_text(p_lbl, LV_SYMBOL_LEFT);
    lv_obj_center(p_lbl);
    lv_obj_add_flag(s_ams_prev_btn, LV_OBJ_FLAG_HIDDEN);

    s_ams_next_btn = lv_btn_create(s_ams_root);
    lv_obj_remove_style_all(s_ams_next_btn);
    lv_obj_add_style(s_ams_next_btn, &s_btn, 0);
    lv_obj_set_size(s_ams_next_btn, 32, 28);
    lv_obj_align(s_ams_next_btn, LV_ALIGN_TOP_LEFT, 90, 2);
    lv_obj_set_style_pad_all(s_ams_next_btn, 0, 0);
    lv_obj_add_event_cb(s_ams_next_btn, ams_next_clicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* n_lbl = lv_label_create(s_ams_next_btn);
    lv_label_set_text(n_lbl, LV_SYMBOL_RIGHT);
    lv_obj_center(n_lbl);
    lv_obj_add_flag(s_ams_next_btn, LV_OBJ_FLAG_HIDDEN);

    // --- Slot row ---
    s_ams_row = lv_obj_create(s_ams_root);
    lv_obj_remove_style_all(s_ams_row);
    lv_obj_set_size(s_ams_row, LV_HOR_RES - 32, 156);
    lv_obj_align(s_ams_row, LV_ALIGN_TOP_LEFT, 16, 32);
    lv_obj_set_flex_flow(s_ams_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s_ams_row, 8, 0);
    lv_obj_clear_flag(s_ams_row, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0; i < 4; ++i) {
        s_ams_card[i] = make_ams_slot_card(s_ams_row,
                                            &s_ams_card_swatch[i],
                                            &s_ams_card_type[i],
                                            &s_ams_card_pct[i],
                                            &s_ams_card_bar[i]);
    }

    // --- Empty state overlay ---
    s_ams_empty = lv_label_create(s_ams_root);
    lv_label_set_text(s_ams_empty,
        "No AMS connected.\n"
        "Connect an AMS or AMS-HT to this printer\n"
        "in Bambu Studio to see its slots here.");
    lv_obj_set_style_text_align(s_ams_empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(s_ams_empty, &s_label_dim, 0);
    lv_obj_align(s_ams_empty, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_ams_empty, LV_OBJ_FLAG_HIDDEN);

    return s_ams_root;
}

static void render_ams_slot(uint8_t idx, const ::bambuddy::AmsSlot* s) {
    lv_obj_clear_flag(s_ams_card[idx], LV_OBJ_FLAG_HIDDEN);
    if (!s || !s->present) {
        lv_obj_set_style_bg_color(s_ams_card_swatch[idx],
                                   lv_color_hex(::ui::C_PANEL_HI), 0);
        lv_label_set_text(s_ams_card_type[idx], "EMPTY");
        lv_obj_set_style_text_color(s_ams_card_type[idx],
                                     lv_color_hex(::ui::C_TEXT_DIM), 0);
        lv_label_set_text(s_ams_card_pct[idx], "—");
        lv_obj_set_style_text_color(s_ams_card_pct[idx],
                                     lv_color_hex(::ui::C_TEXT_DIM), 0);
        lv_bar_set_value(s_ams_card_bar[idx], 0, LV_ANIM_OFF);
        return;
    }
    uint32_t rgb = s->color_rgb ? s->color_rgb : ::ui::C_PANEL_HI;
    lv_obj_set_style_bg_color(s_ams_card_swatch[idx], lv_color_hex(rgb), 0);
    lv_label_set_text(s_ams_card_type[idx], s->type[0] ? s->type : "—");
    lv_obj_set_style_text_color(s_ams_card_type[idx],
                                 lv_color_hex(::ui::C_TEXT), 0);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)s->remain);
    lv_label_set_text(s_ams_card_pct[idx], buf);
    uint32_t pct_col = ::ui::C_TEXT;
    if (s->remain < 15)      pct_col = ::ui::C_ERR;
    else if (s->remain < 30) pct_col = ::ui::C_WARN;
    lv_obj_set_style_text_color(s_ams_card_pct[idx], lv_color_hex(pct_col), 0);
    lv_bar_set_value(s_ams_card_bar[idx], s->remain, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_ams_card_bar[idx],
                               lv_color_hex(pct_col), LV_PART_INDICATOR);
}

void ams_cycle_unit(int dir) {
    s_ams_visible_index += dir;
    if (s_ams_visible_index < 0) s_ams_visible_index = 0;
}

void update_ams(int printer_id) {
    maybe_hide_toast();
    ::bambuddy::Printer ps[8]; uint8_t n = 0;
    ::bambuddy::g_client.snapshot_printers(ps, n);

    const ::bambuddy::Printer* sel = nullptr;
    for (uint8_t i = 0; i < n; ++i) if (ps[i].id == printer_id) sel = &ps[i];
    if (!sel && n > 0) sel = &ps[0];

    bool has_ams = (sel != nullptr) && sel->ams_exists && sel->ams_count > 0;
    if (!has_ams) {
        lv_obj_clear_flag(s_ams_empty, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ams_row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ams_prev_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ams_next_btn, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_ams_unit_lbl, "AMS");
        lv_label_set_text(s_ams_humid_lbl, "");
        lv_label_set_text(s_ams_temp_lbl,  "");
        lv_label_set_text(s_ams_dry_lbl,   "");
        return;
    }
    lv_obj_add_flag(s_ams_empty, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_ams_row, LV_OBJ_FLAG_HIDDEN);

    if (s_ams_visible_index >= sel->ams_count) s_ams_visible_index = 0;
    const ::bambuddy::AmsUnit& u = sel->ams[s_ams_visible_index];

    char hdr[24];
    if (sel->ams_count > 1)
        snprintf(hdr, sizeof(hdr), "AMS %d/%u%s",
                 s_ams_visible_index + 1, (unsigned)sel->ams_count,
                 u.is_ht ? " HT" : "");
    else
        snprintf(hdr, sizeof(hdr), "AMS%s", u.is_ht ? "-HT" : "");
    lv_label_set_text(s_ams_unit_lbl, hdr);

    if (sel->ams_count > 1) {
        lv_obj_clear_flag(s_ams_prev_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_ams_next_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_ams_prev_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ams_next_btn, LV_OBJ_FLAG_HIDDEN);
    }

    if (u.humidity >= 0) {
        char hbuf[16];
        snprintf(hbuf, sizeof(hbuf), LV_SYMBOL_TINT " %d%%", (int)u.humidity);
        lv_label_set_text(s_ams_humid_lbl, hbuf);
        uint32_t hc = ::ui::C_OK;
        if (u.humidity > 60)      hc = ::ui::C_ERR;
        else if (u.humidity > 40) hc = ::ui::C_WARN;
        lv_obj_set_style_text_color(s_ams_humid_lbl, lv_color_hex(hc), 0);
    } else {
        lv_label_set_text(s_ams_humid_lbl, LV_SYMBOL_TINT " --");
        lv_obj_set_style_text_color(s_ams_humid_lbl,
                                     lv_color_hex(::ui::C_TEXT_DIM), 0);
    }

    if (u.temp > 0.5f) {
        char tbuf[16];
        snprintf(tbuf, sizeof(tbuf), "%.0f \xC2\xB0""C", u.temp);
        lv_label_set_text(s_ams_temp_lbl, tbuf);
    } else {
        lv_label_set_text(s_ams_temp_lbl, "");
    }

    if (u.dry_time_min > 0) {
        char dbuf[20];
        if (u.dry_time_min >= 60)
            snprintf(dbuf, sizeof(dbuf), "Dry %luh%02lu",
                     (unsigned long)(u.dry_time_min / 60),
                     (unsigned long)(u.dry_time_min % 60));
        else
            snprintf(dbuf, sizeof(dbuf), "Dry %lum",
                     (unsigned long)u.dry_time_min);
        lv_label_set_text(s_ams_dry_lbl, dbuf);
    } else {
        lv_label_set_text(s_ams_dry_lbl, "");
    }

    uint8_t visible = u.slot_count > 0 ? u.slot_count : (u.is_ht ? 1 : 4);
    if (visible > 4) visible = 4;
    for (uint8_t i = 0; i < 4; ++i) {
        if (i < visible) {
            render_ams_slot(i, i < u.slot_count ? &u.slots[i] : nullptr);
        } else {
            lv_obj_add_flag(s_ams_card[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// =============================================================================
// PRINTERS LIST
// =============================================================================

static lv_obj_t* s_pr_root = nullptr;
static lv_obj_t* s_pr_list = nullptr;

static void pr_row_clicked(lv_event_t* e) {
    int id = (int)(intptr_t)lv_event_get_user_data(e);
    ::ui::g_ui.set_selected_printer(id);
    ::ui::g_ui.go_to(::ui::Screen::Dashboard);
}

lv_obj_t* build_printers(lv_obj_t* parent) {
    ensure_styles();
    s_pr_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_pr_root);
    lv_obj_set_size(s_pr_root, LV_HOR_RES, body_h());
    lv_obj_align(s_pr_root, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_obj_clear_flag(s_pr_root, LV_OBJ_FLAG_SCROLLABLE);

    s_pr_list = lv_obj_create(s_pr_root);
    lv_obj_remove_style_all(s_pr_list);
    lv_obj_set_size(s_pr_list, LV_HOR_RES - 24, body_h() - 8);
    lv_obj_align(s_pr_list, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_flex_flow(s_pr_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_pr_list, 6, 0);
    lv_obj_set_scroll_dir(s_pr_list, LV_DIR_VER);
    return s_pr_root;
}

static void clear_children(lv_obj_t* o) {
    while (lv_obj_get_child_cnt(o) > 0) {
        lv_obj_t* c = lv_obj_get_child(o, 0);
        lv_obj_del(c);
    }
}

void update_printers(int focused_id) {
    maybe_hide_toast();
    ::bambuddy::Printer ps[8]; uint8_t n = 0;
    ::bambuddy::g_client.snapshot_printers(ps, n);
    clear_children(s_pr_list);

    if (n == 0) {
        lv_obj_t* lbl = lv_label_create(s_pr_list);
        lv_label_set_text(lbl,
            "No printers found in Bambuddy.\n"
            "Add one in the Bambuddy web UI first.");
        lv_obj_add_style(lbl, &s_label_dim, 0);
        return;
    }

    for (uint8_t i = 0; i < n; ++i) {
        lv_obj_t* row = lv_btn_create(s_pr_list);
        lv_obj_remove_style_all(row);
        lv_obj_add_style(row, &s_panel, 0);
        lv_obj_set_size(row, LV_PCT(100), 48);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        if ((int)ps[i].id == focused_id) {
            lv_obj_set_style_border_width(row, 2, 0);
            lv_obj_set_style_border_color(row, lv_color_hex(::ui::C_ACCENT), 0);
        }
        lv_obj_add_event_cb(row, pr_row_clicked, LV_EVENT_CLICKED,
                            (void*)(intptr_t)ps[i].id);

        lv_obj_t* dot = lv_obj_create(row);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 12, 12);
        lv_obj_set_style_radius(dot, 6, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(state_color(ps[i].state)), 0);
        lv_obj_align(dot, LV_ALIGN_LEFT_MID, 4, 0);

        lv_obj_t* name = lv_label_create(row);
        lv_label_set_text(name, ps[i].name.c_str());
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 26, -8);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(name, lv_color_hex(::ui::C_TEXT), 0);

        lv_obj_t* sub = lv_label_create(row);
        String s = ps[i].model + "  " + state_name(ps[i].state);
        lv_label_set_text(sub, s.c_str());
        lv_obj_align(sub, LV_ALIGN_LEFT_MID, 26, 10);
        lv_obj_add_style(sub, &s_label_dim, 0);

        if (ps[i].state == ::bambuddy::PrinterState::Printing) {
            char pbuf[8];
            snprintf(pbuf, sizeof(pbuf), "%u%%", (unsigned)ps[i].progress);
            lv_obj_t* prog = lv_label_create(row);
            lv_label_set_text(prog, pbuf);
            lv_obj_align(prog, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_set_style_text_font(prog, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(prog, lv_color_hex(::ui::C_ACCENT), 0);
        }
    }
}

// =============================================================================
// HISTORY
// =============================================================================

static lv_obj_t* s_hist_root      = nullptr;
static lv_obj_t* s_hist_stats     = nullptr;
static lv_obj_t* s_hist_list      = nullptr;

lv_obj_t* build_history(lv_obj_t* parent) {
    ensure_styles();
    s_hist_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_hist_root);
    lv_obj_set_size(s_hist_root, LV_HOR_RES, body_h());
    lv_obj_align(s_hist_root, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_obj_clear_flag(s_hist_root, LV_OBJ_FLAG_SCROLLABLE);

    s_hist_stats = lv_obj_create(s_hist_root);
    lv_obj_add_style(s_hist_stats, &s_panel, 0);
    lv_obj_set_size(s_hist_stats, LV_HOR_RES - 24, 48);
    lv_obj_align(s_hist_stats, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_clear_flag(s_hist_stats, LV_OBJ_FLAG_SCROLLABLE);

    s_hist_list = lv_obj_create(s_hist_root);
    lv_obj_remove_style_all(s_hist_list);
    lv_obj_set_size(s_hist_list, LV_HOR_RES - 24, body_h() - 64);
    lv_obj_align(s_hist_list, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_flex_flow(s_hist_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_hist_list, 4, 0);
    lv_obj_set_scroll_dir(s_hist_list, LV_DIR_VER);
    return s_hist_root;
}

void update_history() {
    maybe_hide_toast();
    clear_children(s_hist_stats);
    ::bambuddy::Stats st = ::bambuddy::g_client.snapshot_stats();

    auto add_kpi = [&](const char* k, const String& v, lv_color_t c, int x) {
        lv_obj_t* kl = lv_label_create(s_hist_stats);
        lv_label_set_text(kl, k);
        lv_obj_add_style(kl, &s_label_dim, 0);
        lv_obj_align(kl, LV_ALIGN_TOP_LEFT, x, 0);

        lv_obj_t* vl = lv_label_create(s_hist_stats);
        lv_label_set_text(vl, v.c_str());
        lv_obj_align(vl, LV_ALIGN_TOP_LEFT, x, 14);
        lv_obj_set_style_text_font(vl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(vl, c, 0);
    };
    add_kpi("PRINTS",   String(st.total_prints),
            lv_color_hex(::ui::C_TEXT), 0);
    add_kpi("SUCCESS",  String(st.success_rate, 1) + "%",
            lv_color_hex(::ui::C_OK), 110);
    add_kpi("FILAMENT", String((int)(st.total_filament_g)) + " g",
            lv_color_hex(::ui::C_TEXT), 230);
    add_kpi("TIME",     String(st.total_time_s / 3600) + " h",
            lv_color_hex(::ui::C_TEXT), 350);

    clear_children(s_hist_list);
    ::bambuddy::Archive ar[10]; uint8_t n = 0;
    ::bambuddy::g_client.snapshot_recent(ar, n);
    if (n == 0) {
        lv_obj_t* lbl = lv_label_create(s_hist_list);
        lv_label_set_text(lbl, "No prints yet.");
        lv_obj_add_style(lbl, &s_label_dim, 0);
        return;
    }
    for (uint8_t i = 0; i < n; ++i) {
        lv_obj_t* row = lv_obj_create(s_hist_list);
        lv_obj_add_style(row, &s_panel, 0);
        lv_obj_set_size(row, LV_PCT(100), 28);
        lv_obj_set_style_pad_all(row, 4, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* dot = lv_obj_create(row);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, 4, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        uint32_t c = ::ui::C_TEXT_DIM;
        if (ar[i].status == "success")      c = ::ui::C_OK;
        else if (ar[i].status == "failed")  c = ::ui::C_ERR;
        else if (ar[i].status == "stopped") c = ::ui::C_WARN;
        lv_obj_set_style_bg_color(dot, lv_color_hex(c), 0);
        lv_obj_align(dot, LV_ALIGN_LEFT_MID, 2, 0);

        lv_obj_t* nm = lv_label_create(row);
        lv_label_set_text(nm,
                           ar[i].name.length() ? ar[i].name.c_str() : "(unnamed)");
        lv_obj_set_width(nm, 220);
        lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
        lv_obj_align(nm, LV_ALIGN_LEFT_MID, 16, 0);
        lv_obj_set_style_text_color(nm, lv_color_hex(::ui::C_TEXT), 0);
        lv_obj_set_style_text_font(nm, &lv_font_montserrat_14, 0);

        char buf[24];
        snprintf(buf, sizeof(buf), "%lum • %.0fg",
                 (unsigned long)(ar[i].duration_s / 60), ar[i].filament_g);
        lv_obj_t* meta = lv_label_create(row);
        lv_label_set_text(meta, buf);
        lv_obj_align(meta, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_add_style(meta, &s_label_dim, 0);
    }
}

// =============================================================================
// SETTINGS
// =============================================================================

static lv_obj_t* s_set_root    = nullptr;
static lv_obj_t* s_set_url     = nullptr;
static lv_obj_t* s_set_ip      = nullptr;
static lv_obj_t* s_set_rssi    = nullptr;
static lv_obj_t* s_set_uptime  = nullptr;
static lv_obj_t* s_set_reset_btn = nullptr;
static lv_obj_t* s_set_reset_lbl = nullptr;
static uint32_t  s_set_reset_armed_at = 0;

static void set_reset_clicked(lv_event_t*) {
    uint32_t now = lv_tick_get();
    if (s_set_reset_armed_at != 0 &&
        (now - s_set_reset_armed_at) < 3000) {
        // Second tap within 3 s — go.
        show_toast("Resetting…", lv_color_hex(::ui::C_WARN));
        factory_reset();
    } else {
        // First tap — arm.
        s_set_reset_armed_at = now;
        lv_label_set_text(s_set_reset_lbl, "Tap again to confirm (3 s)");
        lv_obj_set_style_bg_color(s_set_reset_btn,
                                   lv_color_hex(::ui::C_ERR), 0);
    }
}

lv_obj_t* build_settings(lv_obj_t* parent) {
    ensure_styles();
    s_set_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_set_root);
    lv_obj_set_size(s_set_root, LV_HOR_RES, body_h());
    lv_obj_align(s_set_root, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_obj_clear_flag(s_set_root, LV_OBJ_FLAG_SCROLLABLE);

    auto make_row = [&](const char* k, int y) {
        lv_obj_t* kl = lv_label_create(s_set_root);
        lv_label_set_text(kl, k);
        lv_obj_add_style(kl, &s_label_dim, 0);
        lv_obj_align(kl, LV_ALIGN_TOP_LEFT, 18, y);

        lv_obj_t* vl = lv_label_create(s_set_root);
        lv_label_set_text(vl, "-");
        lv_obj_set_width(vl, LV_HOR_RES - 160);
        lv_label_set_long_mode(vl, LV_LABEL_LONG_DOT);
        lv_obj_align(vl, LV_ALIGN_TOP_LEFT, 130, y);
        lv_obj_set_style_text_color(vl, lv_color_hex(::ui::C_TEXT), 0);
        lv_obj_set_style_text_font(vl, &lv_font_montserrat_14, 0);
        return vl;
    };

    s_set_url    = make_row("Bambuddy",    4);
    s_set_ip     = make_row("Local IP",   26);
    s_set_rssi   = make_row("Wi-Fi RSSI", 48);
    s_set_uptime = make_row("Uptime",     70);

    // --- Factory reset button (two-tap confirm) ---
    s_set_reset_btn = lv_btn_create(s_set_root);
    lv_obj_remove_style_all(s_set_reset_btn);
    lv_obj_add_style(s_set_reset_btn, &s_btn, 0);
    lv_obj_set_size(s_set_reset_btn, LV_HOR_RES - 36, 44);
    lv_obj_align(s_set_reset_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(s_set_reset_btn,
                               lv_color_hex(::ui::C_PANEL_HI), 0);
    lv_obj_add_event_cb(s_set_reset_btn, set_reset_clicked, LV_EVENT_CLICKED, nullptr);

    s_set_reset_lbl = lv_label_create(s_set_reset_btn);
    lv_label_set_text(s_set_reset_lbl, "Factory reset");
    lv_obj_set_style_text_font(s_set_reset_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(s_set_reset_lbl);

    return s_set_root;
}

void update_settings() {
    maybe_hide_toast();
    lv_label_set_text(s_set_url, ::g_cfg_bambuddy_url.length()
                                      ? ::g_cfg_bambuddy_url.c_str()
                                      : "(not configured)");
    lv_label_set_text(s_set_ip,   WiFi.localIP().toString().c_str());
    char r[16];
    snprintf(r, sizeof(r), "%d dBm", WiFi.RSSI());
    lv_label_set_text(s_set_rssi, r);
    uint32_t up = millis() / 1000;
    char u[24];
    snprintf(u, sizeof(u), "%uh %02um %02us",
             (unsigned)(up / 3600), (unsigned)((up % 3600) / 60),
             (unsigned)(up % 60));
    lv_label_set_text(s_set_uptime, u);

    // Reset the "tap again to confirm" arm window after 3 s.
    if (s_set_reset_armed_at != 0 &&
        (lv_tick_get() - s_set_reset_armed_at) > 3000) {
        s_set_reset_armed_at = 0;
        lv_label_set_text(s_set_reset_lbl, "Factory reset");
        lv_obj_set_style_bg_color(s_set_reset_btn,
                                   lv_color_hex(::ui::C_PANEL_HI), 0);
    }
}

// =============================================================================
// HMS FULL-SCREEN FLASH
// =============================================================================

static lv_obj_t* s_hms_overlay = nullptr;
static lv_obj_t* s_hms_msg     = nullptr;
static bool      s_hms_visible = false;

static void hms_pulse_cb(void* var, int32_t v) {
    const uint8_t r0 = 0x80, g0 = 0x1F, b0 = 0x22;
    const uint8_t r1 = (::ui::C_ERR >> 16) & 0xFF;
    const uint8_t g1 = (::ui::C_ERR >>  8) & 0xFF;
    const uint8_t b1 = (::ui::C_ERR      ) & 0xFF;
    uint8_t r = r0 + (uint8_t)(((int32_t)(r1 - r0) * v) / 100);
    uint8_t g = g0 + (uint8_t)(((int32_t)(g1 - g0) * v) / 100);
    uint8_t b = b0 + (uint8_t)(((int32_t)(b1 - b0) * v) / 100);
    lv_obj_set_style_bg_color((lv_obj_t*)var, lv_color_make(r, g, b), 0);
}

static void hms_overlay_clicked(lv_event_t*) {
    hms_flash_hide();
}

lv_obj_t* build_hms_flash(lv_obj_t* parent) {
    ensure_styles();
    s_hms_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_hms_overlay);
    lv_obj_set_size(s_hms_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(s_hms_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_hms_overlay, lv_color_hex(::ui::C_ERR), 0);
    lv_obj_set_style_bg_opa(s_hms_overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_hms_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_hms_overlay, hms_overlay_clicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* icon = lv_label_create(s_hms_overlay);
    lv_label_set_text(icon, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -66);

    lv_obj_t* title = lv_label_create(s_hms_overlay);
    lv_label_set_text(title, "HMS ERROR");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -22);

    s_hms_msg = lv_label_create(s_hms_overlay);
    lv_label_set_text(s_hms_msg, "");
    lv_obj_set_width(s_hms_msg, LV_HOR_RES - 60);
    lv_label_set_long_mode(s_hms_msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_hms_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_hms_msg, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_hms_msg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_hms_msg, LV_ALIGN_CENTER, 0, 28);

    lv_obj_t* hint = lv_label_create(s_hms_overlay);
    lv_label_set_text(hint, "Tap anywhere to dismiss");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xFFE0E0), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -14);

    lv_obj_add_flag(s_hms_overlay, LV_OBJ_FLAG_HIDDEN);
    return s_hms_overlay;
}

void hms_flash_show(const char* msg) {
    if (!s_hms_overlay) return;
    lv_label_set_text(s_hms_msg, (msg && *msg) ? msg : "Check the printer.");
    lv_obj_clear_flag(s_hms_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_hms_overlay);
    s_hms_visible = true;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_hms_overlay);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 700);
    lv_anim_set_playback_time(&a, 700);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, hms_pulse_cb);
    lv_anim_start(&a);
}

void hms_flash_hide() {
    if (!s_hms_overlay) return;
    lv_anim_del(s_hms_overlay, hms_pulse_cb);
    lv_obj_add_flag(s_hms_overlay, LV_OBJ_FLAG_HIDDEN);
    s_hms_visible = false;
}

void hms_flash_update_msg(const char* msg) {
    if (s_hms_msg && msg) lv_label_set_text(s_hms_msg, msg);
}

bool hms_flash_is_visible() { return s_hms_visible; }

// =============================================================================
// OTA PROGRESS OVERLAY
// =============================================================================
//
// OTA callbacks fire on net_task (core 0) and the UI task runs on core 1;
// the setters poke a tiny volatile state and ota_apply() (called from
// ui::Manager::refresh()) syncs it into the widget on the right core.

static lv_obj_t* s_ota_overlay = nullptr;
static lv_obj_t* s_ota_bar     = nullptr;
static lv_obj_t* s_ota_pct_lbl = nullptr;
static lv_obj_t* s_ota_status  = nullptr;

static volatile bool    s_ota_active        = false;
static volatile bool    s_ota_dirty         = true;
static volatile uint8_t s_ota_pct           = 0;
static volatile bool    s_ota_error_flag    = false;
static char             s_ota_err_msg[80]   = {};
static bool             s_ota_visible_cache = false;
static uint8_t          s_ota_pct_cache     = 0;
static bool             s_ota_error_cache   = false;

lv_obj_t* build_ota_overlay(lv_obj_t* parent) {
    ensure_styles();
    s_ota_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_ota_overlay);
    lv_obj_set_size(s_ota_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(s_ota_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_ota_overlay, lv_color_hex(::ui::C_BG), 0);
    lv_obj_set_style_bg_opa(s_ota_overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_ota_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* icon = lv_label_create(s_ota_overlay);
    lv_label_set_text(icon, LV_SYMBOL_DOWNLOAD);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(::ui::C_ACCENT), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -66);

    lv_obj_t* title = lv_label_create(s_ota_overlay);
    lv_label_set_text(title, "Updating firmware");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -22);

    s_ota_bar = lv_bar_create(s_ota_overlay);
    lv_obj_set_size(s_ota_bar, 320, 16);
    lv_obj_align(s_ota_bar, LV_ALIGN_CENTER, 0, 20);
    lv_bar_set_range(s_ota_bar, 0, 100);
    lv_bar_set_value(s_ota_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_ota_bar, lv_color_hex(::ui::C_PANEL_HI), LV_PART_MAIN);
    lv_obj_set_style_bg_opa  (s_ota_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius  (s_ota_bar, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ota_bar, lv_color_hex(::ui::C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa  (s_ota_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius  (s_ota_bar, 8, LV_PART_INDICATOR);

    s_ota_pct_lbl = lv_label_create(s_ota_overlay);
    lv_label_set_text(s_ota_pct_lbl, "0%");
    lv_obj_set_style_text_font(s_ota_pct_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_ota_pct_lbl, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_align(s_ota_pct_lbl, LV_ALIGN_CENTER, 0, 50);

    s_ota_status = lv_label_create(s_ota_overlay);
    lv_label_set_text(s_ota_status, "Do not power off the device.");
    lv_obj_set_style_text_font(s_ota_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_ota_status, lv_color_hex(::ui::C_TEXT_DIM), 0);
    lv_obj_align(s_ota_status, LV_ALIGN_BOTTOM_MID, 0, -14);

    lv_obj_add_flag(s_ota_overlay, LV_OBJ_FLAG_HIDDEN);
    return s_ota_overlay;
}

void ota_set_active(bool active) {
    if (active != s_ota_active) { s_ota_active = active; s_ota_dirty = true; }
}
void ota_set_progress(uint8_t pct) {
    if (pct > 100) pct = 100;
    if (pct != s_ota_pct) { s_ota_pct = pct; s_ota_dirty = true; }
}
void ota_set_error(const char* msg) {
    if (!msg) msg = "Update failed.";
    strncpy(s_ota_err_msg, msg, sizeof(s_ota_err_msg) - 1);
    s_ota_err_msg[sizeof(s_ota_err_msg) - 1] = '\0';
    s_ota_error_flag = true;
    s_ota_dirty      = true;
}
bool ota_is_active() { return s_ota_active; }

void ota_apply() {
    if (!s_ota_dirty) return;
    s_ota_dirty = false;
    bool    active = s_ota_active;
    uint8_t pct    = s_ota_pct;
    bool    err    = s_ota_error_flag;

    if (active != s_ota_visible_cache) {
        if (active) {
            lv_obj_clear_flag(s_ota_overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(s_ota_overlay);
        } else {
            lv_obj_add_flag(s_ota_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        s_ota_visible_cache = active;
    }
    if (pct != s_ota_pct_cache) {
        lv_bar_set_value(s_ota_bar, pct, LV_ANIM_OFF);
        char buf[8];
        snprintf(buf, sizeof(buf), "%u%%", (unsigned)pct);
        lv_label_set_text(s_ota_pct_lbl, buf);
        s_ota_pct_cache = pct;
    }
    if (err && !s_ota_error_cache) {
        lv_label_set_text(s_ota_status, s_ota_err_msg);
        lv_obj_set_style_text_color(s_ota_status, lv_color_hex(::ui::C_ERR), 0);
        s_ota_error_cache = true;
    }
}

}  // namespace ui::screens
