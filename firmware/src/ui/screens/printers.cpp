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

#include "theme.h"

namespace ui::screens {

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
    lv_obj_align(s_pr_root, LV_ALIGN_TOP_LEFT, 0, ::ui::HEADER_H);
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
        lv_obj_add_style(row, &s_btn_pressed, LV_STATE_PRESSED);
        lv_obj_set_style_radius(row, ::ui::R_PANEL, 0);
        lv_obj_set_size(row, LV_PCT(100), 50);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        if ((int)ps[i].id == focused_id) {
            lv_obj_set_style_border_width(row, 2, 0);
            lv_obj_set_style_border_color(row, lv_color_hex(::ui::C_ACCENT), 0);
            lv_obj_set_style_border_opa(row, LV_OPA_COVER, 0);
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

}  // namespace ui::screens
