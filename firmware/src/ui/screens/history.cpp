// Bamboard screens — History (stats + recent archives).
//
// Top KPI strip (4 cells): PRINTS / SUCCESS / FILAMENT / TIME.
// Below: scrolling list of the last N archives, each with a coloured
// status dot, filename, duration, filament used.
//
// All widgets are pre-built once and reused: the 4 KPI cells are static, and
// up to MAX_RECENT_ARCHIVES archive rows live in a pool whose unused slots
// are hidden each refresh. snprintf into stack buffers replaces the previous
// String concat churn (the screen redraws at the UI refresh cadence, so heap
// fragmentation on each tick used to be very visible over a long uptime).

#include "theme.h"

namespace ui::screens {

static lv_obj_t* s_hist_root  = nullptr;
static lv_obj_t* s_hist_stats = nullptr;
static lv_obj_t* s_hist_list  = nullptr;

// --- KPI strip (4 cells) ---------------------------------------------------

struct Kpi {
    lv_obj_t* key   = nullptr;
    lv_obj_t* value = nullptr;
};
static Kpi s_kpi[4];

static void build_kpi(uint8_t idx, const char* key, int x, uint32_t value_col) {
    s_kpi[idx].key = lv_label_create(s_hist_stats);
    lv_label_set_text(s_kpi[idx].key, key);
    lv_obj_add_style(s_kpi[idx].key, &s_label_dim, 0);
    lv_obj_align(s_kpi[idx].key, LV_ALIGN_TOP_LEFT, x, 0);

    s_kpi[idx].value = lv_label_create(s_hist_stats);
    lv_label_set_text(s_kpi[idx].value, "");
    lv_obj_align(s_kpi[idx].value, LV_ALIGN_TOP_LEFT, x, 14);
    lv_obj_set_style_text_font(s_kpi[idx].value, &bb_font_16, 0);
    lv_obj_set_style_text_color(s_kpi[idx].value, lv_color_hex(value_col), 0);
}

// --- Archive row pool ------------------------------------------------------

struct ArchiveRow {
    lv_obj_t* row  = nullptr;
    lv_obj_t* dot  = nullptr;
    lv_obj_t* name = nullptr;
    lv_obj_t* meta = nullptr;
};
static ArchiveRow s_arows[::bambuddy::MAX_RECENT_ARCHIVES];
static lv_obj_t*  s_empty_lbl = nullptr;

static void build_archive_row(uint8_t i) {
    ArchiveRow& r = s_arows[i];
    r.row = lv_obj_create(s_hist_list);
    lv_obj_add_style(r.row, &s_panel, 0);
    lv_obj_set_size(r.row, LV_PCT(100), 28);
    lv_obj_set_style_pad_all(r.row, 4, 0);
    lv_obj_clear_flag(r.row, LV_OBJ_FLAG_SCROLLABLE);

    r.dot = lv_obj_create(r.row);
    lv_obj_remove_style_all(r.dot);
    lv_obj_set_size(r.dot, 8, 8);
    lv_obj_set_style_radius(r.dot, 4, 0);
    lv_obj_set_style_bg_opa(r.dot, LV_OPA_COVER, 0);
    lv_obj_align(r.dot, LV_ALIGN_LEFT_MID, 2, 0);

    r.name = lv_label_create(r.row);
    lv_label_set_text(r.name, "");
    lv_obj_set_width(r.name, 220);
    lv_label_set_long_mode(r.name, LV_LABEL_LONG_DOT);
    lv_obj_align(r.name, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_set_style_text_color(r.name, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_set_style_text_font(r.name, &bb_font_14, 0);

    r.meta = lv_label_create(r.row);
    lv_label_set_text(r.meta, "");
    lv_obj_align(r.meta, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_style(r.meta, &s_label_dim, 0);

    lv_obj_add_flag(r.row, LV_OBJ_FLAG_HIDDEN);
}

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

    build_kpi(0, i18n::tr(i18n::Str::PRINTS),   0,   ::ui::C_TEXT);
    build_kpi(1, i18n::tr(i18n::Str::SUCCESS),  110, ::ui::C_OK);
    build_kpi(2, i18n::tr(i18n::Str::FILAMENT), 230, ::ui::C_TEXT);
    build_kpi(3, i18n::tr(i18n::Str::TIME),     350, ::ui::C_TEXT);

    s_hist_list = lv_obj_create(s_hist_root);
    lv_obj_remove_style_all(s_hist_list);
    lv_obj_set_size(s_hist_list, LV_HOR_RES - 24, body_h() - 64);
    lv_obj_align(s_hist_list, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_flex_flow(s_hist_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_hist_list, 4, 0);
    lv_obj_set_scroll_dir(s_hist_list, LV_DIR_VER);

    for (uint8_t i = 0; i < ::bambuddy::MAX_RECENT_ARCHIVES; ++i) {
        build_archive_row(i);
    }
    return s_hist_root;
}

static uint32_t archive_status_color(const String& status) {
    if (status == "success") return ::ui::C_OK;
    if (status == "failed")  return ::ui::C_ERR;
    if (status == "stopped") return ::ui::C_WARN;
    return ::ui::C_TEXT_DIM;
}

void update_history() {
    maybe_hide_toast();
    ::bambuddy::Stats st = ::bambuddy::g_client.snapshot_stats();

    char buf[32];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)st.total_prints);
    lv_label_set_text(s_kpi[0].value, buf);
    snprintf(buf, sizeof(buf), "%.1f%%", (double)st.success_rate);
    lv_label_set_text(s_kpi[1].value, buf);
    snprintf(buf, sizeof(buf), "%d g", (int)st.total_filament_g);
    lv_label_set_text(s_kpi[2].value, buf);
    snprintf(buf, sizeof(buf), "%lu h", (unsigned long)(st.total_time_s / 3600));
    lv_label_set_text(s_kpi[3].value, buf);

    ::bambuddy::Archive ar[::bambuddy::MAX_RECENT_ARCHIVES]; uint8_t n = 0;
    ::bambuddy::g_client.snapshot_recent(ar, n);
    if (n > ::bambuddy::MAX_RECENT_ARCHIVES) n = ::bambuddy::MAX_RECENT_ARCHIVES;

    if (n == 0) {
        for (uint8_t i = 0; i < ::bambuddy::MAX_RECENT_ARCHIVES; ++i) {
            lv_obj_add_flag(s_arows[i].row, LV_OBJ_FLAG_HIDDEN);
        }
        if (!s_empty_lbl) {
            s_empty_lbl = lv_label_create(s_hist_list);
            lv_obj_add_style(s_empty_lbl, &s_label_dim, 0);
            lv_label_set_text(s_empty_lbl, i18n::tr(i18n::Str::NO_PRINTS));
        }
        lv_obj_clear_flag(s_empty_lbl, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (s_empty_lbl) lv_obj_add_flag(s_empty_lbl, LV_OBJ_FLAG_HIDDEN);

    for (uint8_t i = 0; i < ::bambuddy::MAX_RECENT_ARCHIVES; ++i) {
        ArchiveRow& r = s_arows[i];
        if (i >= n) {
            lv_obj_add_flag(r.row, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(r.row, LV_OBJ_FLAG_HIDDEN);

        lv_obj_set_style_bg_color(r.dot,
            lv_color_hex(archive_status_color(ar[i].status)), 0);

        lv_label_set_text(r.name,
            ar[i].name.length() ? ar[i].name.c_str() : "(unnamed)");

        char meta[24];
        snprintf(meta, sizeof(meta), "%lum \xC2\xB7 %.0fg",
                 (unsigned long)(ar[i].duration_s / 60),
                 (double)ar[i].filament_g);
        lv_label_set_text(r.meta, meta);
    }
}

}  // namespace ui::screens
