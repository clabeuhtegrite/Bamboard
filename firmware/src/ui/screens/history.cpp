// Bamboard screens — History (stats + recent archives).
//
// Top KPI strip (4 cells): PRINTS / SUCCESS / FILAMENT / TIME.
// Below: scrolling list of the last N archives, each with a coloured
// status dot, filename, duration, filament used.

#include "theme.h"

namespace ui::screens {

static lv_obj_t* s_hist_root  = nullptr;
static lv_obj_t* s_hist_stats = nullptr;
static lv_obj_t* s_hist_list  = nullptr;

lv_obj_t* build_history(lv_obj_t* parent) {
    ensure_styles();
    s_hist_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_hist_root);
    lv_obj_set_size(s_hist_root, LV_HOR_RES, body_h());
    lv_obj_align(s_hist_root, LV_ALIGN_TOP_LEFT, 0, ::ui::HEADER_H);
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
        lv_obj_set_style_text_font(vl, &bb_font_16, 0);
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
        lv_obj_set_style_text_font(nm, &bb_font_14, 0);

        char buf[24];
        snprintf(buf, sizeof(buf), "%lum • %.0fg",
                 (unsigned long)(ar[i].duration_s / 60), ar[i].filament_g);
        lv_obj_t* meta = lv_label_create(row);
        lv_label_set_text(meta, buf);
        lv_obj_align(meta, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_add_style(meta, &s_label_dim, 0);
    }
}

}  // namespace ui::screens
