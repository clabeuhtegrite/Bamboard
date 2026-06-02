// Bamboard screens — Printers (farm grid).
//
// A control-wall grid of printer tiles. Each tile shows a progress ring, the
// printer name and an at-a-glance status line (state, or temps + ETA while
// printing). Tap a tile to focus that printer and jump to the dashboard.
//
// Ring meaning:
//   • printing / paused → accent arc filled to progress %, % in the centre
//   • otherwise         → a full ring in the state colour (idle grey, finish
//                         green, failed red …), centre blank — a glanceable
//                         state dot you can read across the room.
//
// Two tiles per row (222 × 84). Up to bambuddy::MAX_PRINTERS tiles are created
// once at build time and reused on every update — mutate in place and hide the
// unused ones, no teardown/rebuild churn each refresh.

#include "theme.h"

namespace ui::screens {

static lv_obj_t* s_pr_root = nullptr;
static lv_obj_t* s_pr_grid = nullptr;

struct PrinterTile {
    lv_obj_t* tile = nullptr;
    lv_obj_t* arc  = nullptr;
    lv_obj_t* ring = nullptr;   // % label centred in the arc (printing only)
    lv_obj_t* name = nullptr;
    lv_obj_t* sub  = nullptr;
    int       id   = -1;        // wired into the event cb's user_data
};
static PrinterTile s_tiles[::bambuddy::MAX_PRINTERS];

static void pr_tile_clicked(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= (int)::bambuddy::MAX_PRINTERS) return;
    int id = s_tiles[idx].id;
    if (id < 0) return;
    ::ui::g_ui.set_selected_printer(id);
    ::ui::g_ui.go_to(::ui::Screen::Dashboard);
}

static void build_one_tile(uint8_t idx) {
    PrinterTile& t = s_tiles[idx];

    t.tile = lv_btn_create(s_pr_grid);
    lv_obj_remove_style_all(t.tile);
    lv_obj_add_style(t.tile, &s_panel, 0);
    lv_obj_add_style(t.tile, &s_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_radius(t.tile, ::ui::R_PANEL, 0);
    lv_obj_set_size(t.tile, 222, 84);
    lv_obj_set_style_pad_all(t.tile, 8, 0);
    lv_obj_clear_flag(t.tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(t.tile, 0, 0);
    lv_obj_add_event_cb(t.tile, pr_tile_clicked, LV_EVENT_CLICKED,
                        (void*)(intptr_t)idx);

    // Progress ring (left). Non-interactive: drop the knob + clickable flag so a
    // drag can't move the value and taps fall through to the tile button.
    t.arc = lv_arc_create(t.tile);
    lv_obj_set_size(t.arc, 56, 56);
    lv_obj_align(t.arc, LV_ALIGN_LEFT_MID, 0, 0);
    lv_arc_set_rotation(t.arc, 270);
    lv_arc_set_bg_angles(t.arc, 0, 360);
    lv_arc_set_range(t.arc, 0, 100);
    lv_arc_set_value(t.arc, 0);
    lv_obj_remove_style(t.arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(t.arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(t.arc, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(t.arc, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(t.arc, lv_color_hex(::ui::C_PANEL_HI), LV_PART_MAIN);
    lv_obj_set_style_arc_color(t.arc, lv_color_hex(::ui::C_ACCENT), LV_PART_INDICATOR);

    t.ring = lv_label_create(t.tile);
    lv_label_set_text(t.ring, "");
    lv_obj_set_style_text_font(t.ring, &bb_font_16, 0);
    lv_obj_set_style_text_color(t.ring, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_align_to(t.ring, t.arc, LV_ALIGN_CENTER, 0, 0);

    // Name + status (right of the ring).
    t.name = lv_label_create(t.tile);
    lv_label_set_text(t.name, "");
    lv_obj_set_width(t.name, 138);
    lv_label_set_long_mode(t.name, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(t.name, &bb_font_16, 0);
    lv_obj_set_style_text_color(t.name, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_align(t.name, LV_ALIGN_LEFT_MID, 68, -12);

    t.sub = lv_label_create(t.tile);
    lv_label_set_text(t.sub, "");
    lv_obj_set_width(t.sub, 138);
    lv_label_set_long_mode(t.sub, LV_LABEL_LONG_DOT);
    lv_obj_add_style(t.sub, &s_label_dim, 0);
    lv_obj_align(t.sub, LV_ALIGN_LEFT_MID, 68, 12);

    lv_obj_add_flag(t.tile, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* build_printers(lv_obj_t* parent) {
    ensure_styles();
    s_pr_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_pr_root);
    lv_obj_set_size(s_pr_root, LV_HOR_RES, body_h());
    lv_obj_align(s_pr_root, LV_ALIGN_TOP_LEFT, 0, ::ui::HEADER_H);
    lv_obj_clear_flag(s_pr_root, LV_OBJ_FLAG_SCROLLABLE);

    s_pr_grid = lv_obj_create(s_pr_root);
    lv_obj_remove_style_all(s_pr_grid);
    lv_obj_set_size(s_pr_grid, LV_HOR_RES - 12, body_h() - 8);
    lv_obj_align(s_pr_grid, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_flex_flow(s_pr_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(s_pr_grid, 6, 0);
    lv_obj_set_style_pad_column(s_pr_grid, 6, 0);
    lv_obj_set_style_pad_all(s_pr_grid, 2, 0);
    lv_obj_set_scroll_dir(s_pr_grid, LV_DIR_VER);

    for (uint8_t i = 0; i < ::bambuddy::MAX_PRINTERS; ++i) build_one_tile(i);
    return s_pr_root;
}

// Empty-state placeholder. Created lazily so it doesn't take a slot in the tile
// pool above; reused across refreshes.
static lv_obj_t* s_empty_lbl = nullptr;
static void show_empty_state() {
    if (!s_empty_lbl) {
        s_empty_lbl = lv_label_create(s_pr_grid);
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
            if (s_tiles[i].tile) lv_obj_add_flag(s_tiles[i].tile, LV_OBJ_FLAG_HIDDEN);
            s_tiles[i].id = -1;
        }
        show_empty_state();
        return;
    }
    hide_empty_state();

    if (n > ::bambuddy::MAX_PRINTERS) n = ::bambuddy::MAX_PRINTERS;

    using PState = ::bambuddy::PrinterState;   // 'PS' clashes with the Xtensa register macro
    for (uint8_t i = 0; i < ::bambuddy::MAX_PRINTERS; ++i) {
        PrinterTile& t = s_tiles[i];
        if (i >= n) {
            lv_obj_add_flag(t.tile, LV_OBJ_FLAG_HIDDEN);
            t.id = -1;
            continue;
        }
        t.id = (int)ps[i].id;
        lv_obj_clear_flag(t.tile, LV_OBJ_FLAG_HIDDEN);

        bool focused = ((int)ps[i].id == focused_id);
        lv_obj_set_style_border_width(t.tile, focused ? 2 : 0, 0);
        if (focused) {
            lv_obj_set_style_border_color(t.tile, lv_color_hex(::ui::C_ACCENT), 0);
            lv_obj_set_style_border_opa  (t.tile, LV_OPA_COVER, 0);
        }

        bool active = (ps[i].state == PState::Printing || ps[i].state == PState::Paused);
        uint32_t scol = state_color(ps[i].state);

        // Ring: accent progress arc while printing, else a full state-coloured ring.
        if (active) {
            lv_obj_set_style_arc_color(t.arc, lv_color_hex(::ui::C_PANEL_HI), LV_PART_MAIN);
            lv_arc_set_value(t.arc, ps[i].progress);
            char pbuf[8];
            snprintf(pbuf, sizeof(pbuf), "%u%%", (unsigned)ps[i].progress);
            lv_label_set_text(t.ring, pbuf);
        } else {
            lv_obj_set_style_arc_color(t.arc, lv_color_hex(scol), LV_PART_MAIN);
            lv_arc_set_value(t.arc, 0);
            lv_label_set_text(t.ring, "");
        }
        lv_obj_align_to(t.ring, t.arc, LV_ALIGN_CENTER, 0, 0);

        lv_label_set_text(t.name, ps[i].name.c_str());

        char sub[80];
        if (active) {
            char eta[16] = "";
            if (ps[i].remaining_s > 0) {
                char e[12]; short_eta(ps[i].remaining_s, e, sizeof(e));
                snprintf(eta, sizeof(eta), "  \xC2\xB7  %s", e);
            }
            snprintf(sub, sizeof(sub), "%.0f\xC2\xB0/%.0f\xC2\xB0%s",
                     ps[i].temps.nozzle, ps[i].temps.bed, eta);
        } else {
            snprintf(sub, sizeof(sub), "%s  \xC2\xB7  %s",
                     ps[i].model.c_str(), state_name(ps[i].state));
        }
        lv_label_set_text(t.sub, sub);
    }
}

}  // namespace ui::screens
