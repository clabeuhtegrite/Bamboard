// Bamboard screens — Print queue.
//
// Read-only list of the jobs Bambuddy still has queued (status "pending"), in
// queue order. One reused row per slot (MAX_QUEUE_ITEMS), same pool trick as
// the Printers screen — hide unused rows and mutate text in place rather than
// rebuild every refresh. Populated from g_client.snapshot_queue(); the net task
// refreshes it via the periodic GET /api/v1/queue.
//
// Layout per row (44 px, panel radius): "#N" position (accent) on the left,
// the job name on the upper line, the target printer (or "-") on the lower line.

#include "theme.h"

#include "../control.h"   // ui::ctrl::enqueue — cancel a queued job off the UI task

namespace ui::screens {

static lv_obj_t* s_q_root  = nullptr;
static lv_obj_t* s_q_list  = nullptr;
static lv_obj_t* s_q_empty = nullptr;

struct QueueRow {
    lv_obj_t* row     = nullptr;
    lv_obj_t* pos     = nullptr;
    lv_obj_t* name    = nullptr;
    lv_obj_t* sub     = nullptr;
    lv_obj_t* del     = nullptr;   // trash button (two-tap to remove the job)
    lv_obj_t* del_lbl = nullptr;
};
static QueueRow s_qrows[::bambuddy::MAX_QUEUE_ITEMS];
static int      s_qrow_id[::bambuddy::MAX_QUEUE_ITEMS] = {};  // item id shown per row
static int      s_q_armed_id = -1;   // job armed for removal (by item id)
static uint32_t s_q_armed_at = 0;

// Two-tap remove: the first tap arms the job (its trash turns red), a second tap
// on the SAME job within 3 s sends the cancel. Keyed by item id so a queue
// reshuffle between taps can't remove the wrong job; update_queue disarms after 3 s.
static void q_del_clicked(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= (int)::bambuddy::MAX_QUEUE_ITEMS) return;
    int id = s_qrow_id[idx];
    if (id < 0) return;
    uint32_t now = lv_tick_get();
    if (s_q_armed_id == id && (now - s_q_armed_at) < 3000) {
        s_q_armed_id = -1;
        ::ui::ctrl::enqueue(::ui::ctrl::QueueCancel, id);   // id carried in printer_id slot
    } else {
        s_q_armed_id = id;
        s_q_armed_at = now;
        show_toast(i18n::tr(i18n::Str::CONFIRM_REMOVE), lv_color_hex(::ui::C_WARN));
    }
}

static void build_one_qrow(uint8_t idx) {
    QueueRow& r = s_qrows[idx];

    r.row = lv_obj_create(s_q_list);
    lv_obj_remove_style_all(r.row);
    lv_obj_add_style(r.row, &s_panel, 0);
    lv_obj_set_style_radius(r.row, ::ui::R_PANEL, 0);
    lv_obj_set_size(r.row, LV_PCT(100), 44);
    lv_obj_clear_flag(r.row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(r.row, 0, 0);

    r.pos = lv_label_create(r.row);
    lv_label_set_text(r.pos, "");
    lv_obj_align(r.pos, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_font(r.pos, &bb_font_16, 0);
    lv_obj_set_style_text_color(r.pos, lv_color_hex(::ui::C_ACCENT), 0);

    r.name = lv_label_create(r.row);
    lv_label_set_text(r.name, "");
    lv_obj_set_width(r.name, LV_HOR_RES - 24 - 64 - 46);   // room for the trash button
    lv_label_set_long_mode(r.name, LV_LABEL_LONG_DOT);
    lv_obj_align(r.name, LV_ALIGN_LEFT_MID, 52, -8);
    lv_obj_set_style_text_font(r.name, &bb_font_16, 0);
    lv_obj_set_style_text_color(r.name, lv_color_hex(::ui::C_TEXT), 0);

    r.sub = lv_label_create(r.row);
    lv_label_set_text(r.sub, "");
    lv_obj_set_width(r.sub, LV_HOR_RES - 24 - 64 - 46);
    lv_label_set_long_mode(r.sub, LV_LABEL_LONG_DOT);
    lv_obj_align(r.sub, LV_ALIGN_LEFT_MID, 52, 10);
    lv_obj_add_style(r.sub, &s_label_dim, 0);

    // Trash button (right): two-tap to remove the job from the queue.
    r.del = lv_btn_create(r.row);
    lv_obj_remove_style_all(r.del);
    lv_obj_add_style(r.del, &s_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(r.del, 36, 36);
    lv_obj_align(r.del, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_radius(r.del, ::ui::R_BUTTON, 0);
    lv_obj_set_style_bg_color(r.del, lv_color_hex(::ui::C_PANEL_HI), 0);
    lv_obj_set_style_bg_opa(r.del, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(r.del, q_del_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)idx);
    r.del_lbl = lv_label_create(r.del);
    lv_label_set_text(r.del_lbl, LV_SYMBOL_TRASH);
    lv_obj_set_style_text_font(r.del_lbl, &bb_font_16, 0);
    lv_obj_set_style_text_color(r.del_lbl, lv_color_hex(::ui::C_TEXT_DIM), 0);
    lv_obj_center(r.del_lbl);

    lv_obj_add_flag(r.row, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* build_queue(lv_obj_t* parent) {
    ensure_styles();
    s_q_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_q_root);
    lv_obj_set_size(s_q_root, LV_HOR_RES, body_h());
    lv_obj_align(s_q_root, LV_ALIGN_TOP_LEFT, 0, ::ui::HEADER_H);
    lv_obj_clear_flag(s_q_root, LV_OBJ_FLAG_SCROLLABLE);

    s_q_list = lv_obj_create(s_q_root);
    lv_obj_remove_style_all(s_q_list);
    lv_obj_set_size(s_q_list, LV_HOR_RES - 24, body_h() - 8);
    lv_obj_align(s_q_list, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_flex_flow(s_q_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_q_list, 6, 0);
    lv_obj_set_scroll_dir(s_q_list, LV_DIR_VER);

    for (uint8_t i = 0; i < ::bambuddy::MAX_QUEUE_ITEMS; ++i) build_one_qrow(i);

    // Empty-state placeholder (reused; shares the flex list so it centres nicely).
    s_q_empty = lv_label_create(s_q_list);
    lv_obj_add_style(s_q_empty, &s_label_dim, 0);
    lv_label_set_text(s_q_empty, i18n::tr(i18n::Str::QUEUE_EMPTY));
    lv_obj_add_flag(s_q_empty, LV_OBJ_FLAG_HIDDEN);
    return s_q_root;
}

void update_queue() {
    maybe_hide_toast();
    if (s_q_armed_id >= 0 && (lv_tick_get() - s_q_armed_at) > 3000) s_q_armed_id = -1;
    ::bambuddy::QueueItem q[::bambuddy::MAX_QUEUE_ITEMS]; uint8_t n = 0;
    ::bambuddy::g_client.snapshot_queue(q, n);
    // Resolve printer_id → name for the sub-line.
    ::bambuddy::Printer ps[::bambuddy::MAX_PRINTERS]; uint8_t pn = 0;
    ::bambuddy::g_client.snapshot_printers(ps, pn);

    if (n == 0) {
        for (uint8_t i = 0; i < ::bambuddy::MAX_QUEUE_ITEMS; ++i) {
            if (s_qrows[i].row) lv_obj_add_flag(s_qrows[i].row, LV_OBJ_FLAG_HIDDEN);
            s_qrow_id[i] = -1;
        }
        if (s_q_empty) lv_obj_clear_flag(s_q_empty, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (s_q_empty) lv_obj_add_flag(s_q_empty, LV_OBJ_FLAG_HIDDEN);
    if (n > ::bambuddy::MAX_QUEUE_ITEMS) n = ::bambuddy::MAX_QUEUE_ITEMS;

    for (uint8_t i = 0; i < ::bambuddy::MAX_QUEUE_ITEMS; ++i) {
        QueueRow& r = s_qrows[i];
        if (i >= n) { lv_obj_add_flag(r.row, LV_OBJ_FLAG_HIDDEN); s_qrow_id[i] = -1; continue; }
        lv_obj_clear_flag(r.row, LV_OBJ_FLAG_HIDDEN);
        s_qrow_id[i] = q[i].id;

        char pbuf[8];
        snprintf(pbuf, sizeof(pbuf), i18n::tr(i18n::Str::QUEUE_POS), (int)(i + 1));
        lv_label_set_text(r.pos, pbuf);
        lv_label_set_text(r.name, q[i].name.c_str());

        const char* pname = nullptr;
        if (q[i].printer_id >= 0)
            for (uint8_t k = 0; k < pn; ++k)
                if (ps[k].id == q[i].printer_id) pname = ps[k].name.c_str();
        lv_label_set_text(r.sub, pname ? pname : "-");

        // Trash turns red while this job is armed for removal (first tap).
        bool armed = (q[i].id >= 0 && q[i].id == s_q_armed_id);
        lv_obj_set_style_text_color(r.del_lbl,
            lv_color_hex(armed ? ::ui::C_ERR : ::ui::C_TEXT_DIM), 0);
    }
}

}  // namespace ui::screens
