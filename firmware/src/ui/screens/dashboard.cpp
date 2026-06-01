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

namespace ui::screens {

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

// Action row. While printing: segmented speed chip spans the whole row.
// When finished: single accent pill. While HMS error: red pill.
static lv_obj_t* s_dash_speed_bar     = nullptr;
static lv_obj_t* s_dash_speed_seg[4]  = {};
static lv_obj_t* s_dash_speed_lbl[4]  = {};
static lv_obj_t* s_dash_speed_caption = nullptr;
static lv_obj_t* s_dash_btn_plate     = nullptr;
static lv_obj_t* s_dash_btn_hms       = nullptr;

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

// Segmented speed chip — each segment passes its mode (1..4) as user_data.
static void speed_seg_clicked(lv_event_t* e) {
    int id = ::ui::g_ui.selected_printer_id();
    if (id < 0) return;
    uint8_t mode = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (mode < 1 || mode > 4) return;
    bool ok = ::bambuddy::g_client.set_print_speed(id, mode);
    String msg = String(LV_SYMBOL_OK " ") + speed_mode_name(mode);
    show_toast(ok ? msg.c_str() : i18n::tr(i18n::Str::SPEED_CHANGE_FAILED),
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

static void build_speed_segmented(lv_obj_t* parent, int x, int y, int w) {
    s_dash_speed_caption = lv_label_create(parent);
    lv_label_set_text(s_dash_speed_caption, i18n::tr(i18n::Str::SPEED));
    lv_obj_add_style(s_dash_speed_caption, &s_label_dim, 0);
    lv_obj_set_pos(s_dash_speed_caption, x + 4, y - 14);

    s_dash_speed_bar = lv_obj_create(parent);
    lv_obj_remove_style_all(s_dash_speed_bar);
    lv_obj_set_pos (s_dash_speed_bar, x, y);
    lv_obj_set_size(s_dash_speed_bar, w, ::ui::H_BTN);
    lv_obj_set_style_bg_color(s_dash_speed_bar,
                               lv_color_hex(::ui::C_PANEL_HI), 0);
    lv_obj_set_style_bg_opa  (s_dash_speed_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius  (s_dash_speed_bar, ::ui::R_PILL, 0);
    lv_obj_set_style_border_width(s_dash_speed_bar, 1, 0);
    lv_obj_set_style_border_color(s_dash_speed_bar,
                                  lv_color_hex(::ui::C_PANEL_LINE), 0);
    lv_obj_set_style_border_opa(s_dash_speed_bar, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(s_dash_speed_bar, 3, 0);
    lv_obj_clear_flag(s_dash_speed_bar, LV_OBJ_FLAG_SCROLLABLE);

    static const char* k_short[4] = { "Silent", "Standard", "Sport", "Ludicrous" };
    int seg_w = (w - 6) / 4;
    int seg_h = ::ui::H_BTN - 6;
    for (uint8_t i = 0; i < 4; ++i) {
        lv_obj_t* seg = lv_btn_create(s_dash_speed_bar);
        lv_obj_remove_style_all(seg);
        lv_obj_add_style(seg, &s_chip_off, 0);
        lv_obj_add_style(seg, &s_btn_pressed, LV_STATE_PRESSED);
        lv_obj_set_size(seg, seg_w, seg_h);
        lv_obj_set_pos(seg, i * seg_w, 0);
        lv_obj_add_event_cb(seg, speed_seg_clicked, LV_EVENT_CLICKED,
                            (void*)(uintptr_t)(i + 1));
        lv_obj_t* lbl = lv_label_create(seg);
        lv_label_set_text(lbl, k_short[i]);
        lv_obj_set_style_text_font(lbl, &bb_font_14, 0);
        lv_obj_center(lbl);
        s_dash_speed_seg[i] = seg;
        s_dash_speed_lbl[i] = lbl;
    }
}

static void speed_segmented_set_active(uint8_t mode /* 1..4 */) {
    if (mode < 1 || mode > 4) mode = 2;
    for (uint8_t i = 0; i < 4; ++i) {
        bool on = (i + 1 == mode);
        lv_obj_set_style_bg_color(s_dash_speed_seg[i],
                                   lv_color_hex(on ? ::ui::C_ACCENT
                                                   : ::ui::C_PANEL_HI), 0);
        lv_obj_set_style_text_color(s_dash_speed_lbl[i],
                                    lv_color_hex(on ? ::ui::C_TEXT_INV
                                                    : ::ui::C_TEXT_DIM), 0);
    }
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
    lv_obj_clear_flag(s_dash_progress_arc, LV_OBJ_FLAG_CLICKABLE);

    s_dash_progress_lbl = lv_label_create(s_dash_progress_arc);
    lv_label_set_text(s_dash_progress_lbl, "--%");
    lv_obj_set_style_text_font(s_dash_progress_lbl, &bb_font_16, 0);
    lv_obj_set_style_text_color(s_dash_progress_lbl, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_center(s_dash_progress_lbl);

    s_dash_state_lbl = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_state_lbl, "—");
    lv_obj_set_pos(s_dash_state_lbl, 88, 0);
    lv_obj_set_style_text_font(s_dash_state_lbl, &bb_font_20, 0);
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
    lv_obj_set_pos(s_dash_file_lbl, 88, 26);
    lv_obj_add_style(s_dash_file_lbl, &s_label_dim, 0);

    s_dash_layer_lbl = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_layer_lbl,
                      (String(i18n::tr(i18n::Str::LAYER)) + " --/--").c_str());
    lv_obj_set_pos(s_dash_layer_lbl, 320, 26);
    lv_obj_add_style(s_dash_layer_lbl, &s_label_dim, 0);

    // --- Temp row ---
    s_dash_t_noz  = make_temp_cell(s_dash_root, i18n::tr(i18n::Str::NOZZLE),   12, 142);
    s_dash_t_bed  = make_temp_cell(s_dash_root, i18n::tr(i18n::Str::BED),     162, 142);
    s_dash_t_cham = make_temp_cell(s_dash_root, i18n::tr(i18n::Str::CHAMBER), 312, 156);

    // --- HMS line (hidden when "ok") ---
    s_dash_hms = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_hms, "");
    lv_obj_set_width(s_dash_hms, LV_HOR_RES - 24);
    lv_label_set_long_mode(s_dash_hms, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_dash_hms, 12, 108);
    lv_obj_set_style_text_color(s_dash_hms, lv_color_hex(::ui::C_ERR), 0);
    lv_obj_set_style_text_font(s_dash_hms, &bb_font_14, 0);

    // --- Inline action area ---
    build_speed_segmented(s_dash_root, 12, 128, LV_HOR_RES - 24);
    s_dash_btn_plate = make_action_btn(s_dash_root, 12, 128, LV_HOR_RES - 24,
                                       (String(LV_SYMBOL_OK "  ") +
                                        i18n::tr(i18n::Str::CLEAR_PLATE)).c_str(),
                                       btn_plate_clicked);
    s_dash_btn_hms   = make_action_btn(s_dash_root, 12, 128, LV_HOR_RES - 24,
                                       (String(LV_SYMBOL_WARNING "  ") +
                                        i18n::tr(i18n::Str::CLEAR_HMS)).c_str(),
                                       btn_hms_clicked, ::ui::C_ERR);

    lv_obj_add_flag(s_dash_speed_bar,     LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_dash_speed_caption, LV_OBJ_FLAG_HIDDEN);
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
        lv_label_set_text(s_dash_file_lbl, i18n::tr(i18n::Str::ADD_IN_BAMBUDDY));
        header_set_printer_name("");
        lv_obj_add_flag(s_dash_speed_bar,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_dash_speed_caption, LV_OBJ_FLAG_HIDDEN);
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

    lv_label_set_text(s_dash_file_lbl,
                       sel->filename.length() ? sel->filename.c_str()
                                              : i18n::tr(i18n::Str::IDLE_PAREN));

    lv_arc_set_value(s_dash_progress_arc, sel->progress);
    char pbuf[8];
    snprintf(pbuf, sizeof(pbuf), "%u%%", (unsigned)sel->progress);
    lv_label_set_text(s_dash_progress_lbl, pbuf);

    char eta_val[16];
    char eta_buf[32];
    snprintf(eta_buf, sizeof(eta_buf), "%s%s",
             i18n::tr(i18n::Str::ETA),
             fmt_eta(sel->remaining_s, eta_val, sizeof(eta_val)));
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

    show(s_dash_btn_hms,         can_hms);
    show(s_dash_btn_plate,       !can_hms && can_plate);
    show(s_dash_speed_bar,       !can_hms && !can_plate && can_speed);
    show(s_dash_speed_caption,   !can_hms && !can_plate && can_speed);

    if (can_speed) {
        uint8_t cur = (sel->speed_level >= 1 && sel->speed_level <= 4)
                       ? sel->speed_level : 2;
        speed_segmented_set_active(cur);
    }
}

}  // namespace ui::screens
