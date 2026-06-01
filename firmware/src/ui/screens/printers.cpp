// Bamboard screens — Printers list.
//
// One row per printer Bambuddy knows about. Each row is a touch button —
// tap a row to focus that printer (sets ui::g_ui.set_selected_printer()
// and jumps to the dashboard).
//
// Layout per row (50 px high, panel radius 12):
//   • state dot at left (12 px)
//   • name on the upper line
//   • model + state text on the lower line
//   • when printing: big progress % in accent at the right edge
//
// Up to bambuddy::MAX_PRINTERS rows are created once at build time and
// reused on every update — we just hide the unused ones and mutate
// label text + colour in place. That avoids a teardown/rebuild storm
// each refresh (~4 Hz × 8 rows = a lot of lv_obj_create / del churn).

#include "theme.h"

namespace ui::screens {

static lv_obj_t* s_pr_root = nullptr;
static lv_obj_t* s_pr_list = nullptr;

struct PrinterRow {
    lv_obj_t* row   = nullptr;
    lv_obj_t* dot   = nullptr;
    lv_obj_t* name  = nullptr;
    lv_obj_t* sub   = nullptr;
    lv_obj_t* prog  = nullptr;
    int       id    = -1;       // wired into the event cb's user_data
};
static PrinterRow s_rows[::bambuddy::MAX_PRINTERS];

static void pr_row_clicked(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= (int)::bambuddy::MAX_PRINTERS) return;
    int id = s_rows[idx].id;
    if (id < 0) return;
    ::ui::g_ui.set_selected_printer(id);
    ::ui::g_ui.go_to(::ui::Screen::Dashboard);
}

static void build_one_row(uint8_t idx) {
    PrinterRow& r = s_rows[idx];

    r.row = lv_btn_create(s_pr_list);
    lv_obj_remove_style_all(r.row);
    lv_obj_add_style(r.row, &s_panel, 0);
    lv_obj_add_style(r.row, &s_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_radius(r.row, ::ui::R_PANEL, 0);
    lv_obj_set_size(r.row, LV_PCT(100), 50);
    lv_obj_clear_flag(r.row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(r.row, 0, 0);
    lv_obj_add_event_cb(r.row, pr_row_clicked, LV_EVENT_CLICKED,
                        (void*)(intptr_t)idx);

    r.dot = lv_obj_create(r.row);
    lv_obj_remove_style_all(r.dot);
    lv_obj_set_size(r.dot, 12, 12);
    lv_obj_set_style_radius(r.dot, 6, 0);
    lv_obj_set_style_bg_opa(r.dot, LV_OPA_COVER, 0);
    lv_obj_align(r.dot, LV_ALIGN_LEFT_MID, 4, 0);

    r.name = lv_label_create(r.row);
    lv_label_set_text(r.name, "");
    lv_obj_align(r.name, LV_ALIGN_LEFT_MID, 26, -8);
    lv_obj_set_style_text_font(r.name, &bb_font_16, 0);
    lv_obj_set_style_text_color(r.name, lv_color_hex(::ui::C_TEXT), 0);

    r.sub = lv_label_create(r.row);
    lv_label_set_text(r.sub, "");
    lv_obj_set_width(r.sub, 340);   // leave room for the right-edge progress %
    lv_label_set_long_mode(r.sub, LV_LABEL_LONG_DOT);
    lv_obj_align(r.sub, LV_ALIGN_LEFT_MID, 26, 10);
    lv_obj_add_style(r.sub, &s_label_dim, 0);

    r.prog = lv_label_create(r.row);
    lv_label_set_text(r.prog, "");
    lv_obj_align(r.prog, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_text_font(r.prog, &bb_font_20, 0);
    lv_obj_set_style_text_color(r.prog, lv_color_hex(::ui::C_ACCENT), 0);

    lv_obj_add_flag(r.row, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* build_printers(lv_obj_t* parent) {
    ensure_styles();
    s_pr_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_pr_root);
    lv_obj_set_size(s_pr_root, LV_HOR_RES, body_h());
    lv_obj_align(s_pr_root, LV_ALIGN_TOP_LEFT, 0, ::ui::HEADER_H);
    lv_obj_clear_flag(s_pr_root, LV_OBJ_FLAG_SCROLLABLE);

    s_pr_list = lv_obj_create(s_pr_root);
    lv_obj_remove_style_all(s_pr_list);
    lv_obj_set_size(s_pr_list, LV_HOR_RES - 24, body_h() - 8);
    lv_obj_align(s_pr_list, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_flex_flow(s_pr_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_pr_list, 6, 0);
    lv_obj_set_scroll_dir(s_pr_list, LV_DIR_VER);

    for (uint8_t i = 0; i < ::bambuddy::MAX_PRINTERS; ++i) build_one_row(i);
    return s_pr_root;
}

// Empty-state placeholder. Created lazily so it doesn't take a slot in the
// row pool above; reused across refreshes.
static lv_obj_t* s_empty_lbl = nullptr;
static void show_empty_state() {
    if (!s_empty_lbl) {
        s_empty_lbl = lv_label_create(s_pr_list);
        lv_obj_add_style(s_empty_lbl, &s_label_dim, 0);
    }
    lv_label_set_text(s_empty_lbl, i18n::tr(i18n::Str::NO_PRINTERS_FOUND));
    lv_obj_clear_flag(s_empty_lbl, LV_OBJ_FLAG_HIDDEN);
}
static void hide_empty_state() {
    if (s_empty_lbl) lv_obj_add_flag(s_empty_lbl, LV_OBJ_FLAG_HIDDEN);
}

// Compact remaining-time string ("1h12" / "12m") for the at-a-glance sub-line.
static void short_eta(uint32_t secs, char* out, size_t n) {
    uint32_t h = secs / 3600, m = (secs % 3600) / 60;
    if (h) snprintf(out, n, "%uh%02u", (unsigned)h, (unsigned)m);
    else   snprintf(out, n, "%um", (unsigned)m);
}

void update_printers(int focused_id) {
    maybe_hide_toast();
    ::bambuddy::Printer ps[::bambuddy::MAX_PRINTERS]; uint8_t n = 0;
    ::bambuddy::g_client.snapshot_printers(ps, n);

    if (n == 0) {
        for (uint8_t i = 0; i < ::bambuddy::MAX_PRINTERS; ++i) {
            if (s_rows[i].row) lv_obj_add_flag(s_rows[i].row, LV_OBJ_FLAG_HIDDEN);
            s_rows[i].id = -1;
        }
        show_empty_state();
        return;
    }
    hide_empty_state();

    if (n > ::bambuddy::MAX_PRINTERS) n = ::bambuddy::MAX_PRINTERS;

    for (uint8_t i = 0; i < ::bambuddy::MAX_PRINTERS; ++i) {
        PrinterRow& r = s_rows[i];
        if (i >= n) {
            lv_obj_add_flag(r.row, LV_OBJ_FLAG_HIDDEN);
            r.id = -1;
            continue;
        }
        r.id = (int)ps[i].id;
        lv_obj_clear_flag(r.row, LV_OBJ_FLAG_HIDDEN);

        bool focused = ((int)ps[i].id == focused_id);
        lv_obj_set_style_border_width(r.row, focused ? 2 : 0, 0);
        if (focused) {
            lv_obj_set_style_border_color(r.row, lv_color_hex(::ui::C_ACCENT), 0);
            lv_obj_set_style_border_opa  (r.row, LV_OPA_COVER, 0);
        }

        lv_obj_set_style_bg_color(r.dot, lv_color_hex(state_color(ps[i].state)), 0);
        lv_label_set_text(r.name, ps[i].name.c_str());

        using PS = ::bambuddy::PrinterState;
        bool active = (ps[i].state == PS::Printing || ps[i].state == PS::Paused);
        char sub[80];
        if (active) {
            // Live at-a-glance status: nozzle/bed temps + remaining ETA.
            char eta[16] = "";
            if (ps[i].remaining_s > 0) {
                char e[12]; short_eta(ps[i].remaining_s, e, sizeof(e));
                snprintf(eta, sizeof(eta), "  \xC2\xB7  %s", e);
            }
            snprintf(sub, sizeof(sub), "%s  \xC2\xB7  %.0f\xC2\xB0/%.0f\xC2\xB0%s",
                     ps[i].model.c_str(), ps[i].temps.nozzle, ps[i].temps.bed, eta);
        } else {
            snprintf(sub, sizeof(sub), "%s  \xC2\xB7  %s",
                     ps[i].model.c_str(), state_name(ps[i].state));
        }
        lv_label_set_text(r.sub, sub);

        if (active) {
            char pbuf[8];
            snprintf(pbuf, sizeof(pbuf), "%u%%", (unsigned)ps[i].progress);
            lv_label_set_text(r.prog, pbuf);
            lv_obj_clear_flag(r.prog, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(r.prog, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

}  // namespace ui::screens
