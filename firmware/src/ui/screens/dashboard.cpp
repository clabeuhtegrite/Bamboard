// Bamboard screens — Live dashboard.
//
//   y =  36 .. 100  : progress arc (left) + state / file / ETA / layer (right)
//   y = 100 .. 144  : 3-cell temperature row (NOZZLE / BED / CHAMBER)
//   y = 144 .. 160  : HMS line (hidden when "ok")
//   y = 164 .. 208  : contextual action row — exactly one of:
//                       - segmented Silent/Standard/Sport/Ludicrous chip
//                       - "Clear plate" pill (when finished)
//                       - red "Clear HMS" pill (priority — when HMS active)
//
// The dashboard is the default landing screen — what the user sees as
// soon as the device boots into the carousel.

#include "theme.h"

#include <time.h>   // mktime / localtime_r for the wall-clock ETA

namespace ui::screens {

static lv_obj_t* s_dash_root         = nullptr;
static lv_obj_t* s_dash_state_pill   = nullptr;
static lv_obj_t* s_dash_state_dot    = nullptr;
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

// Action row (contextual, mutually exclusive). While printing: a "speed"
// caption + a single button that opens a modal 4-item picker. When finished:
// a "Clear plate" pill. While an HMS error is active: a red "Clear HMS" pill
// (highest priority).
static lv_obj_t* s_dash_speed_lbl_cap = nullptr;
static lv_obj_t* s_dash_speed_btn     = nullptr;
static lv_obj_t* s_dash_speed_btn_lbl = nullptr;
static lv_obj_t* s_dash_btn_plate     = nullptr;
static lv_obj_t* s_dash_btn_hms       = nullptr;
// Modal speed picker (built once, hidden until the speed button is tapped).
static lv_obj_t* s_speed_scrim        = nullptr;
static lv_obj_t* s_speed_menu         = nullptr;
static lv_obj_t* s_speed_opt[4]       = {};
static lv_obj_t* s_speed_opt_lbl[4]   = {};
static uint8_t   s_dash_cur_speed     = 2;

// ---- temp cell --------------------------------------------------------------

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
    lv_obj_set_style_text_font(v, &bb_font_16, 0);
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
    return v;
}

// ---- action callbacks -------------------------------------------------------

// Speed picker — close + option handlers. The button that opens it and the
// menu builder live further down (build_speed_menu / build_speed_control).
static void speed_menu_close() {
    if (s_speed_menu)  lv_obj_add_flag(s_speed_menu,  LV_OBJ_FLAG_HIDDEN);
    if (s_speed_scrim) lv_obj_add_flag(s_speed_scrim, LV_OBJ_FLAG_HIDDEN);
}
static void speed_scrim_clicked(lv_event_t*) { speed_menu_close(); }
static void speed_opt_clicked(lv_event_t* e) {
    uint8_t mode = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    speed_menu_close();
    int id = ::ui::g_ui.selected_printer_id();
    if (id < 0 || mode < 1 || mode > 4) return;
    bool ok = ::bambuddy::g_client.set_print_speed(id, mode);
    char msg[40];
    snprintf(msg, sizeof(msg), LV_SYMBOL_OK " %s", speed_mode_name(mode));
    show_toast(ok ? msg : i18n::tr(i18n::Str::SPEED_CHANGE_FAILED),
               lv_color_hex(ok ? ::ui::C_OK : ::ui::C_ERR));
}

static void btn_plate_clicked(lv_event_t*) {
    int id = ::ui::g_ui.selected_printer_id();
    if (id < 0) return;
    bool ok = ::bambuddy::g_client.clear_plate(id);
    show_toast(ok ? i18n::tr(i18n::Str::PLATE_CLEARED)
                  : i18n::tr(i18n::Str::CLEAR_PLATE_FAILED),
               lv_color_hex(ok ? ::ui::C_OK : ::ui::C_ERR));
}

static void btn_hms_clicked(lv_event_t*) {
    int id = ::ui::g_ui.selected_printer_id();
    if (id < 0) return;
    bool ok = ::bambuddy::g_client.clear_hms(id);
    show_toast(ok ? i18n::tr(i18n::Str::HMS_CLEARED)
                  : i18n::tr(i18n::Str::CLEAR_HMS_FAILED),
               lv_color_hex(ok ? ::ui::C_OK : ::ui::C_ERR));
}

// Tapping the progress ring opens the full-screen camera snapshot overlay.
static void dash_arc_clicked(lv_event_t*) { camera_overlay_open(); }

// Single primary-action pill (used for Clear plate / Clear HMS).
static lv_obj_t* make_action_btn(lv_obj_t* parent, int x, int y, int w,
                                  const char* text,
                                  lv_event_cb_t cb,
                                  uint32_t accent = ::ui::C_ACCENT) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, &s_btn, 0);
    lv_obj_add_style(btn, &s_btn_accent, 0);
    lv_obj_add_style(btn, &s_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(accent), 0);
    lv_obj_set_style_radius  (btn, ::ui::R_PILL, 0);
    lv_obj_set_size(btn, w, ::ui::H_BTN);
    lv_obj_set_pos(btn, x, y);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &bb_font_16, 0);
    lv_obj_center(lbl);
    return btn;
}

static void speed_menu_set_active(uint8_t mode /* 1..4 */) {
    if (mode < 1 || mode > 4) mode = 2;
    for (uint8_t i = 0; i < 4; ++i) {
        bool on = (i + 1 == mode);
        lv_obj_set_style_bg_color(s_speed_opt[i],
            lv_color_hex(on ? ::ui::C_ACCENT : ::ui::C_PANEL_HI), 0);
        lv_obj_set_style_bg_grad_color(s_speed_opt[i],
            lv_color_hex(on ? ::ui::C_ACCENT_DARK : ::ui::C_PANEL_HI), 0);
        lv_obj_set_style_bg_grad_dir(s_speed_opt[i],
            on ? LV_GRAD_DIR_VER : LV_GRAD_DIR_NONE, 0);
        lv_obj_set_style_text_color(s_speed_opt_lbl[i],
            lv_color_hex(on ? ::ui::C_TEXT_INV : ::ui::C_TEXT), 0);
    }
}

static void speed_btn_clicked(lv_event_t*) {
    if (!s_speed_menu || !s_speed_scrim) return;
    speed_menu_set_active(s_dash_cur_speed);
    lv_obj_clear_flag(s_speed_scrim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_speed_menu,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_speed_scrim);
    lv_obj_move_foreground(s_speed_menu);
}

// Modal picker: a dimming scrim (tap to dismiss) + a centred card listing the
// four Bambu speed modes. Built once; shown only while the user is choosing.
static void build_speed_menu(lv_obj_t* parent) {
    s_speed_scrim = lv_obj_create(parent);
    lv_obj_remove_style_all(s_speed_scrim);
    lv_obj_set_size(s_speed_scrim, LV_HOR_RES, body_h());
    lv_obj_set_pos(s_speed_scrim, 0, 0);
    lv_obj_set_style_bg_color(s_speed_scrim, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_speed_scrim, LV_OPA_50, 0);
    lv_obj_add_flag(s_speed_scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_speed_scrim, speed_scrim_clicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(s_speed_scrim, LV_OBJ_FLAG_HIDDEN);

    s_speed_menu = lv_obj_create(parent);
    lv_obj_remove_style_all(s_speed_menu);
    lv_obj_add_style(s_speed_menu, &s_panel, 0);
    lv_obj_set_size(s_speed_menu, 220, 176);
    lv_obj_align(s_speed_menu, LV_ALIGN_CENTER, 0, -8);
    lv_obj_set_style_pad_all(s_speed_menu, 10, 0);
    lv_obj_clear_flag(s_speed_menu, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(s_speed_menu);
    lv_label_set_text(title, i18n::tr(i18n::Str::SPEED));
    lv_obj_add_style(title, &s_label_dim, 0);
    lv_obj_set_pos(title, 2, 0);

    for (uint8_t i = 0; i < 4; ++i) {
        lv_obj_t* opt = lv_btn_create(s_speed_menu);
        lv_obj_remove_style_all(opt);
        lv_obj_add_style(opt, &s_btn_pressed, LV_STATE_PRESSED);
        lv_obj_set_style_radius(opt, ::ui::R_CHIP, 0);
        lv_obj_set_style_bg_opa(opt, LV_OPA_COVER, 0);
        lv_obj_set_size(opt, 196, 30);
        lv_obj_set_pos(opt, 2, 18 + i * 34);
        lv_obj_add_event_cb(opt, speed_opt_clicked, LV_EVENT_CLICKED,
                            (void*)(uintptr_t)(i + 1));
        lv_obj_t* l = lv_label_create(opt);
        lv_label_set_text(l, speed_mode_name(i + 1));
        lv_obj_set_style_text_font(l, &bb_font_14, 0);
        lv_obj_center(l);
        s_speed_opt[i]     = opt;
        s_speed_opt_lbl[i] = l;
    }
    lv_obj_add_flag(s_speed_menu, LV_OBJ_FLAG_HIDDEN);
}

// Print-speed row: a dim caption + the single button that opens the menu.
static void build_speed_control(lv_obj_t* parent, int y) {
    s_dash_speed_lbl_cap = lv_label_create(parent);
    lv_label_set_text(s_dash_speed_lbl_cap, i18n::tr(i18n::Str::SPEED));
    lv_obj_add_style(s_dash_speed_lbl_cap, &s_label_dim, 0);
    lv_obj_set_pos(s_dash_speed_lbl_cap, 16, y + 14);

    s_dash_speed_btn = lv_btn_create(parent);
    lv_obj_remove_style_all(s_dash_speed_btn);
    lv_obj_add_style(s_dash_speed_btn, &s_btn, 0);
    lv_obj_add_style(s_dash_speed_btn, &s_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_radius(s_dash_speed_btn, ::ui::R_PILL, 0);
    lv_obj_set_size(s_dash_speed_btn, 182, ::ui::H_BTN);
    lv_obj_set_pos(s_dash_speed_btn, 286, y);
    lv_obj_add_event_cb(s_dash_speed_btn, speed_btn_clicked, LV_EVENT_CLICKED, nullptr);

    s_dash_speed_btn_lbl = lv_label_create(s_dash_speed_btn);
    lv_label_set_text(s_dash_speed_btn_lbl, speed_mode_name(2));
    lv_obj_set_style_text_font(s_dash_speed_btn_lbl, &bb_font_16, 0);
    lv_obj_center(s_dash_speed_btn_lbl);
}

// ---- builders ---------------------------------------------------------------

lv_obj_t* build_dashboard(lv_obj_t* parent) {
    ensure_styles();
    s_dash_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_dash_root);
    lv_obj_set_size(s_dash_root, LV_HOR_RES, body_h());
    lv_obj_align(s_dash_root, LV_ALIGN_TOP_LEFT, 0, ::ui::HEADER_H);
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
    // Tap the ring → full-screen camera snapshot.
    lv_obj_add_flag(s_dash_progress_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_dash_progress_arc, dash_arc_clicked, LV_EVENT_CLICKED, nullptr);

    s_dash_progress_lbl = lv_label_create(s_dash_progress_arc);
    lv_label_set_text(s_dash_progress_lbl, "--%");
    lv_obj_set_style_text_font(s_dash_progress_lbl, &bb_font_16, 0);
    lv_obj_set_style_text_color(s_dash_progress_lbl, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_center(s_dash_progress_lbl);

    // State pill: a rounded chip carrying a state-coloured dot + label.
    // Auto-sizes to its contents so long localized state names (e.g. German
    // "VORBEREITUNG") never get clipped.
    s_dash_state_pill = lv_obj_create(s_dash_root);
    lv_obj_remove_style_all(s_dash_state_pill);
    lv_obj_set_style_bg_color(s_dash_state_pill, lv_color_hex(::ui::C_PANEL_HI), 0);
    lv_obj_set_style_bg_opa(s_dash_state_pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_dash_state_pill, ::ui::R_PILL, 0);
    lv_obj_set_style_pad_hor(s_dash_state_pill, 12, 0);
    lv_obj_set_style_pad_ver(s_dash_state_pill, 4, 0);
    lv_obj_set_style_pad_column(s_dash_state_pill, 8, 0);
    lv_obj_set_flex_flow(s_dash_state_pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_dash_state_pill, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(s_dash_state_pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(s_dash_state_pill, 88, 2);
    lv_obj_clear_flag(s_dash_state_pill, LV_OBJ_FLAG_SCROLLABLE);

    s_dash_state_dot = lv_obj_create(s_dash_state_pill);
    lv_obj_remove_style_all(s_dash_state_dot);
    lv_obj_set_size(s_dash_state_dot, 8, 8);
    lv_obj_set_style_radius(s_dash_state_dot, 4, 0);
    lv_obj_set_style_bg_opa(s_dash_state_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_dash_state_dot, lv_color_hex(::ui::C_ACCENT), 0);

    s_dash_state_lbl = lv_label_create(s_dash_state_pill);
    lv_label_set_text(s_dash_state_lbl, "—");
    lv_obj_set_style_text_font(s_dash_state_lbl, &bb_font_16, 0);
    lv_obj_set_style_text_color(s_dash_state_lbl, lv_color_hex(::ui::C_ACCENT), 0);

    s_dash_eta_lbl = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_eta_lbl, i18n::tr(i18n::Str::ETA_NONE));
    lv_obj_set_pos(s_dash_eta_lbl, 320, 0);
    lv_obj_set_style_text_font(s_dash_eta_lbl, &bb_font_16, 0);
    lv_obj_set_style_text_color(s_dash_eta_lbl, lv_color_hex(::ui::C_TEXT), 0);

    s_dash_file_lbl = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_file_lbl, i18n::tr(i18n::Str::NO_FILE));
    lv_obj_set_width(s_dash_file_lbl, 220);
    lv_label_set_long_mode(s_dash_file_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_dash_file_lbl, 88, 32);
    lv_obj_add_style(s_dash_file_lbl, &s_label_dim, 0);

    s_dash_layer_lbl = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_layer_lbl,
                      (String(i18n::tr(i18n::Str::LAYER)) + " --/--").c_str());
    lv_obj_set_pos(s_dash_layer_lbl, 320, 32);
    lv_obj_add_style(s_dash_layer_lbl, &s_label_dim, 0);

    // --- Temp row ---
    // Three equal-width cells (146 px) at a uniform 12 px gutter / 9 px gap.
    s_dash_t_noz  = make_temp_cell(s_dash_root, i18n::tr(i18n::Str::NOZZLE),   12, 146);
    s_dash_t_bed  = make_temp_cell(s_dash_root, i18n::tr(i18n::Str::BED),     167, 146);
    s_dash_t_cham = make_temp_cell(s_dash_root, i18n::tr(i18n::Str::CHAMBER), 322, 146);

    // --- HMS line (hidden when "ok") ---
    s_dash_hms = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_hms, "");
    lv_obj_set_width(s_dash_hms, LV_HOR_RES - 24);
    lv_label_set_long_mode(s_dash_hms, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_dash_hms, 12, 108);
    lv_obj_set_style_text_color(s_dash_hms, lv_color_hex(::ui::C_ERR), 0);
    lv_obj_set_style_text_font(s_dash_hms, &bb_font_14, 0);

    // --- Inline action area (contextual) ---
    build_speed_control(s_dash_root, 128);
    s_dash_btn_plate = make_action_btn(s_dash_root, 12, 128, LV_HOR_RES - 24,
                                       (String(LV_SYMBOL_OK "  ") +
                                        i18n::tr(i18n::Str::CLEAR_PLATE)).c_str(),
                                       btn_plate_clicked);
    s_dash_btn_hms   = make_action_btn(s_dash_root, 12, 128, LV_HOR_RES - 24,
                                       (String(LV_SYMBOL_WARNING "  ") +
                                        i18n::tr(i18n::Str::CLEAR_HMS)).c_str(),
                                       btn_hms_clicked, ::ui::C_ERR);
    build_speed_menu(s_dash_root);   // modal picker, hidden until the button is tapped

    lv_obj_add_flag(s_dash_speed_lbl_cap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_dash_speed_btn,     LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_dash_btn_plate,     LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_dash_btn_hms,       LV_OBJ_FLAG_HIDDEN);
    return s_dash_root;
}

// ---- updater ----------------------------------------------------------------

// Writes "Xh YYm" / "YYm" / "—" into out (size out_n) and returns out.
// Stack-buffer-only to keep this off the heap; called every refresh.
static const char* fmt_eta(uint32_t secs, char* out, size_t out_n) {
    if (secs == 0) {
        snprintf(out, out_n, "\xE2\x80\x94");   // em-dash
        return out;
    }
    uint32_t h = secs / 3600;
    uint32_t m = (secs % 3600) / 60;
    if (h > 0) snprintf(out, out_n, "%uh%02u", (unsigned)h, (unsigned)m);
    else       snprintf(out, out_n, "%um",     (unsigned)m);
    return out;
}

void update_dashboard(int printer_id) {
    maybe_hide_toast();
    ::bambuddy::Printer ps[8]; uint8_t n = 0;
    ::bambuddy::g_client.snapshot_printers(ps, n);
    if (n == 0) {
        lv_label_set_text(s_dash_state_lbl, i18n::tr(i18n::Str::NO_PRINTER));
        lv_obj_set_style_text_color(s_dash_state_lbl, lv_color_hex(::ui::C_TEXT_DIM), 0);
        lv_obj_set_style_bg_color(s_dash_state_dot, lv_color_hex(::ui::C_TEXT_DIM), 0);
        lv_label_set_text(s_dash_file_lbl, i18n::tr(i18n::Str::ADD_IN_BAMBUDDY));
        header_set_printer_name("");
        speed_menu_close();
        lv_obj_add_flag(s_dash_speed_lbl_cap, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_dash_speed_btn,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_dash_btn_plate,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_dash_btn_hms,       LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const ::bambuddy::Printer* sel = nullptr;
    for (uint8_t i = 0; i < n; ++i) if (ps[i].id == printer_id) sel = &ps[i];
    if (!sel) sel = &ps[0];

    header_set_printer_name(sel->name.c_str());

    lv_label_set_text(s_dash_state_lbl, state_name(sel->state));
    lv_obj_set_style_text_color(s_dash_state_lbl,
                                 lv_color_hex(state_color(sel->state)), 0);
    lv_obj_set_style_bg_color(s_dash_state_dot,
                              lv_color_hex(state_color(sel->state)), 0);

    lv_label_set_text(s_dash_file_lbl,
                       sel->filename.length() ? sel->filename.c_str()
                                              : i18n::tr(i18n::Str::IDLE_PAREN));

    lv_arc_set_value(s_dash_progress_arc, sel->progress);
    char pbuf[8];
    snprintf(pbuf, sizeof(pbuf), "%u%%", (unsigned)sel->progress);
    lv_label_set_text(s_dash_progress_lbl, pbuf);

    char eta_val[16];
    char eta_buf[48];
    fmt_eta(sel->remaining_s, eta_val, sizeof(eta_val));
    // Append the wall-clock finish time ("ETA 1h12 · 14:32") when we have both a
    // remaining estimate and a synced clock (SNTP). localtime is already set up
    // for the daily reboot, so this is free.
    struct tm lt;
    if (sel->remaining_s > 0 && getLocalTime(&lt, 0)) {
        time_t done = mktime(&lt) + (time_t)sel->remaining_s;
        struct tm dt;
        localtime_r(&done, &dt);
        snprintf(eta_buf, sizeof(eta_buf), "%s%s \xE2\x80\xA2 %02d:%02d",
                 i18n::tr(i18n::Str::ETA), eta_val, dt.tm_hour, dt.tm_min);
    } else {
        snprintf(eta_buf, sizeof(eta_buf), "%s%s",
                 i18n::tr(i18n::Str::ETA), eta_val);
    }
    lv_label_set_text(s_dash_eta_lbl, eta_buf);

    char lay[24];
    if (sel->total_layers > 0)
        snprintf(lay, sizeof(lay), "%s %u/%u", i18n::tr(i18n::Str::LAYER),
                 (unsigned)sel->current_layer, (unsigned)sel->total_layers);
    else
        snprintf(lay, sizeof(lay), "%s —", i18n::tr(i18n::Str::LAYER));
    lv_label_set_text(s_dash_layer_lbl, lay);

    bool hms_active = sel->hms.length() && sel->hms != "ok";
    if (hms_active) {
        char hbuf[96];
        snprintf(hbuf, sizeof(hbuf), LV_SYMBOL_WARNING " %s%s",
                 i18n::tr(i18n::Str::HMS_PREFIX), sel->hms.c_str());
        lv_label_set_text(s_dash_hms, hbuf);
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

    // --- Contextual action row ---
    // Priority order: an active HMS error wins (Clear HMS pill), then
    // "finished but plate dirty" wins, then while-printing speed chip.
    using PState = ::bambuddy::PrinterState;
    bool can_speed = (sel->state == PState::Printing ||
                      sel->state == PState::Paused   ||
                      sel->state == PState::Prepare);
    bool can_plate = (sel->state == PState::Finish || sel->state == PState::Failed);
    bool can_hms   = hms_active;

    auto show = [](lv_obj_t* o, bool v) {
        if (!o) return;
        if (v) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
        else   lv_obj_add_flag  (o, LV_OBJ_FLAG_HIDDEN);
    };

    bool show_speed = !can_hms && !can_plate && can_speed;
    show(s_dash_btn_hms,        can_hms);
    show(s_dash_btn_plate,      !can_hms && can_plate);
    show(s_dash_speed_btn,      show_speed);
    show(s_dash_speed_lbl_cap,  show_speed);

    if (can_speed) {
        s_dash_cur_speed = (sel->speed_level >= 1 && sel->speed_level <= 4)
                            ? sel->speed_level : 2;
        lv_label_set_text(s_dash_speed_btn_lbl, speed_mode_name(s_dash_cur_speed));
    }
    // If the context left "printing" while the picker was open, dismiss it.
    if (!show_speed && s_speed_menu &&
        !lv_obj_has_flag(s_speed_menu, LV_OBJ_FLAG_HIDDEN)) {
        speed_menu_close();
    }
}

}  // namespace ui::screens
