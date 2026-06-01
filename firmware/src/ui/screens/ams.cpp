// Bamboard screens — AMS overview.
//
// Top strip (left → right at y=36):
//   [ ◀ ]  AMS 1 / 2  [ ▶ ]   〈humidity〉  〈temp〉  …  [ Dry / Stop ]
//
// Slot row (y=76..224): up to 4 cards (1 for AMS-HT, 4 for vanilla AMS).
// Each card carries a colour swatch + filament type + remaining % + bar.
//
// The chevrons sit flush against the unit label and only become visible
// when the focused printer has more than one chained AMS / AMS-HT unit.
// The Dry / Stop button is hidden entirely on non-HT units (no heater).

#include "theme.h"

namespace ui::screens {

static lv_obj_t* s_ams_root          = nullptr;
static lv_obj_t* s_ams_empty         = nullptr;
static lv_obj_t* s_ams_unit_lbl      = nullptr;
static lv_obj_t* s_ams_humid_lbl     = nullptr;
static lv_obj_t* s_ams_temp_lbl      = nullptr;
static lv_obj_t* s_ams_dry_lbl       = nullptr;
static lv_obj_t* s_ams_prev_btn      = nullptr;
static lv_obj_t* s_ams_next_btn      = nullptr;
static lv_obj_t* s_ams_dry_btn       = nullptr;
static lv_obj_t* s_ams_dry_btn_lbl   = nullptr;
static lv_obj_t* s_ams_row           = nullptr;
static lv_obj_t* s_ams_card        [4] = {};
static lv_obj_t* s_ams_card_swatch [4] = {};
static lv_obj_t* s_ams_card_type   [4] = {};
static lv_obj_t* s_ams_card_pct    [4] = {};
static lv_obj_t* s_ams_card_bar    [4] = {};

static int s_ams_visible_index = 0;

uint8_t ams_visible_unit_index() {
    return (uint8_t)(s_ams_visible_index < 0 ? 0 : s_ams_visible_index);
}

// ---- slot card --------------------------------------------------------------

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
    lv_obj_set_style_text_font(*type_lbl, &bb_font_16, 0);
    lv_obj_set_style_text_color(*type_lbl, lv_color_hex(::ui::C_TEXT), 0);

    *pct_lbl = lv_label_create(card);
    lv_label_set_text(*pct_lbl, "--%");
    lv_obj_align(*pct_lbl, LV_ALIGN_TOP_LEFT, 8, 86);
    lv_obj_set_style_text_font(*pct_lbl, &bb_font_28, 0);
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

// ---- click callbacks --------------------------------------------------------

static void ams_prev_clicked(lv_event_t*) { ams_cycle_unit(-1); }
static void ams_next_clicked(lv_event_t*) { ams_cycle_unit(+1); }

// Drying toggle. The button label is updated each refresh based on the
// `dry_time_min` field — when the unit reports a non-zero countdown,
// tapping fires Stop; otherwise it starts a default-shaped cycle
// (60 min @ 55 °C — a reasonable PLA / PETG dry).
static void ams_dry_clicked(lv_event_t*) {
    int id = ::ui::g_ui.selected_printer_id();
    if (id < 0) return;
    uint8_t unit = ams_visible_unit_index();
    ::bambuddy::Printer ps[8]; uint8_t n = 0;
    ::bambuddy::g_client.snapshot_printers(ps, n);
    bool currently_drying = false;
    for (uint8_t i = 0; i < n; ++i) {
        if (ps[i].id == id && ps[i].ams_count > unit &&
            ps[i].ams[unit].dry_time_min > 0) {
            currently_drying = true; break;
        }
    }
    bool ok;
    if (currently_drying) {
        ok = ::bambuddy::g_client.stop_ams_drying(id, unit);
        show_toast(ok ? i18n::tr(i18n::Str::DRYING_STOPPED)
                      : i18n::tr(i18n::Str::STOP_DRYING_FAILED),
                   lv_color_hex(ok ? ::ui::C_OK : ::ui::C_ERR));
    } else {
        ok = ::bambuddy::g_client.start_ams_drying(id, unit, 60, 55);
        show_toast(ok ? i18n::tr(i18n::Str::DRYING_STARTED)
                      : i18n::tr(i18n::Str::START_DRYING_FAILED),
                   lv_color_hex(ok ? ::ui::C_WARN : ::ui::C_ERR));
    }
}

// ---- builder ----------------------------------------------------------------

lv_obj_t* build_ams(lv_obj_t* parent) {
    ensure_styles();
    s_ams_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_ams_root);
    lv_obj_set_size(s_ams_root, LV_HOR_RES, body_h());
    lv_obj_align(s_ams_root, LV_ALIGN_TOP_LEFT, 0, ::ui::HEADER_H);
    lv_obj_clear_flag(s_ams_root, LV_OBJ_FLAG_SCROLLABLE);

    // Left chevron
    s_ams_prev_btn = lv_btn_create(s_ams_root);
    lv_obj_remove_style_all(s_ams_prev_btn);
    lv_obj_add_style(s_ams_prev_btn, &s_btn, 0);
    lv_obj_add_style(s_ams_prev_btn, &s_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_radius(s_ams_prev_btn, ::ui::R_BUTTON, 0);
    lv_obj_set_size(s_ams_prev_btn, 36, 32);
    lv_obj_align(s_ams_prev_btn, LV_ALIGN_TOP_LEFT, 12, 0);
    lv_obj_set_style_pad_all(s_ams_prev_btn, 0, 0);
    lv_obj_add_event_cb(s_ams_prev_btn, ams_prev_clicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* p_lbl = lv_label_create(s_ams_prev_btn);
    lv_label_set_text(p_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(p_lbl, &bb_font_16, 0);
    lv_obj_center(p_lbl);
    lv_obj_add_flag(s_ams_prev_btn, LV_OBJ_FLAG_HIDDEN);

    s_ams_unit_lbl = lv_label_create(s_ams_root);
    lv_label_set_text(s_ams_unit_lbl, "AMS");
    lv_obj_align(s_ams_unit_lbl, LV_ALIGN_TOP_LEFT, 56, 6);
    lv_obj_set_style_text_font(s_ams_unit_lbl, &bb_font_20, 0);
    lv_obj_set_style_text_color(s_ams_unit_lbl, lv_color_hex(::ui::C_ACCENT), 0);

    s_ams_next_btn = lv_btn_create(s_ams_root);
    lv_obj_remove_style_all(s_ams_next_btn);
    lv_obj_add_style(s_ams_next_btn, &s_btn, 0);
    lv_obj_add_style(s_ams_next_btn, &s_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_radius(s_ams_next_btn, ::ui::R_BUTTON, 0);
    lv_obj_set_size(s_ams_next_btn, 36, 32);
    lv_obj_align(s_ams_next_btn, LV_ALIGN_TOP_LEFT, 138, 0);
    lv_obj_set_style_pad_all(s_ams_next_btn, 0, 0);
    lv_obj_add_event_cb(s_ams_next_btn, ams_next_clicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* n_lbl = lv_label_create(s_ams_next_btn);
    lv_label_set_text(n_lbl, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(n_lbl, &bb_font_16, 0);
    lv_obj_center(n_lbl);
    lv_obj_add_flag(s_ams_next_btn, LV_OBJ_FLAG_HIDDEN);

    s_ams_humid_lbl = lv_label_create(s_ams_root);
    lv_label_set_text(s_ams_humid_lbl, "");
    lv_obj_align(s_ams_humid_lbl, LV_ALIGN_TOP_LEFT, 186, 8);
    lv_obj_set_style_text_font(s_ams_humid_lbl, &bb_font_16, 0);

    s_ams_temp_lbl = lv_label_create(s_ams_root);
    lv_label_set_text(s_ams_temp_lbl, "");
    lv_obj_align(s_ams_temp_lbl, LV_ALIGN_TOP_LEFT, 260, 8);
    lv_obj_set_style_text_font(s_ams_temp_lbl, &bb_font_16, 0);
    lv_obj_set_style_text_color(s_ams_temp_lbl, lv_color_hex(::ui::C_TEXT), 0);

    s_ams_dry_lbl = lv_label_create(s_ams_root);
    lv_label_set_text(s_ams_dry_lbl, "");
    lv_obj_align(s_ams_dry_lbl, LV_ALIGN_TOP_LEFT, 316, 8);
    lv_obj_set_style_text_font(s_ams_dry_lbl, &bb_font_16, 0);
    lv_obj_set_style_text_color(s_ams_dry_lbl, lv_color_hex(::ui::C_WARN), 0);

    // Right-side drying control.
    s_ams_dry_btn = lv_btn_create(s_ams_root);
    lv_obj_remove_style_all(s_ams_dry_btn);
    lv_obj_add_style(s_ams_dry_btn, &s_btn, 0);
    lv_obj_add_style(s_ams_dry_btn, &s_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_radius(s_ams_dry_btn, ::ui::R_PILL, 0);
    lv_obj_set_size(s_ams_dry_btn, 80, 32);
    lv_obj_align(s_ams_dry_btn, LV_ALIGN_TOP_RIGHT, -12, 0);
    lv_obj_add_event_cb(s_ams_dry_btn, ams_dry_clicked, LV_EVENT_CLICKED, nullptr);
    s_ams_dry_btn_lbl = lv_label_create(s_ams_dry_btn);
    lv_label_set_text(s_ams_dry_btn_lbl, i18n::tr(i18n::Str::DRY));
    lv_obj_set_style_text_font(s_ams_dry_btn_lbl, &bb_font_14, 0);
    lv_obj_center(s_ams_dry_btn_lbl);
    lv_obj_add_flag(s_ams_dry_btn, LV_OBJ_FLAG_HIDDEN);

    // --- Slot row ---
    s_ams_row = lv_obj_create(s_ams_root);
    lv_obj_remove_style_all(s_ams_row);
    lv_obj_set_size(s_ams_row, LV_HOR_RES - 32, 148);
    lv_obj_align(s_ams_row, LV_ALIGN_TOP_LEFT, 16, 40);
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
    lv_label_set_text(s_ams_empty, i18n::tr(i18n::Str::NO_AMS));
    lv_obj_set_style_text_align(s_ams_empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(s_ams_empty, &s_label_dim, 0);
    lv_obj_align(s_ams_empty, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_ams_empty, LV_OBJ_FLAG_HIDDEN);

    return s_ams_root;
}

// ---- updater ----------------------------------------------------------------

static void render_ams_slot(uint8_t idx, const ::bambuddy::AmsSlot* s) {
    lv_obj_clear_flag(s_ams_card[idx], LV_OBJ_FLAG_HIDDEN);
    if (!s || !s->present) {
        lv_obj_set_style_bg_color(s_ams_card_swatch[idx],
                                   lv_color_hex(::ui::C_PANEL_HI), 0);
        lv_label_set_text(s_ams_card_type[idx], i18n::tr(i18n::Str::EMPTY));
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
        lv_obj_add_flag(s_ams_row,      LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ams_prev_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ams_next_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ams_dry_btn,  LV_OBJ_FLAG_HIDDEN);
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
        snprintf(hdr, sizeof(hdr), "%d / %u%s",
                 s_ams_visible_index + 1, (unsigned)sel->ams_count,
                 u.is_ht ? "  HT" : "");
    else
        snprintf(hdr, sizeof(hdr), "AMS%s", u.is_ht ? "-HT" : "");
    lv_label_set_text(s_ams_unit_lbl, hdr);

    bool multi = (sel->ams_count > 1);
    if (multi) {
        lv_obj_clear_flag(s_ams_prev_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_ams_next_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_ams_prev_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ams_next_btn, LV_OBJ_FLAG_HIDDEN);
    }

    // Drying button label + colour reflect the current state of *this*
    // unit (not the whole printer). HT units have a heater; vanilla AMS
    // doesn't, so we hide the button entirely on those.
    if (u.is_ht) {
        lv_obj_clear_flag(s_ams_dry_btn, LV_OBJ_FLAG_HIDDEN);
        if (u.dry_time_min > 0) {
            lv_label_set_text(s_ams_dry_btn_lbl, i18n::tr(i18n::Str::STOP));
            lv_obj_set_style_bg_color(s_ams_dry_btn,
                                       lv_color_hex(::ui::C_ERR), 0);
            lv_obj_set_style_text_color(s_ams_dry_btn_lbl,
                                        lv_color_hex(::ui::C_TEXT_INV), 0);
        } else {
            char dlbl[32];
            snprintf(dlbl, sizeof(dlbl), LV_SYMBOL_TINT " %s",
                     i18n::tr(i18n::Str::DRY));
            lv_label_set_text(s_ams_dry_btn_lbl, dlbl);
            lv_obj_set_style_bg_color(s_ams_dry_btn,
                                       lv_color_hex(::ui::C_PANEL_HI), 0);
            lv_obj_set_style_text_color(s_ams_dry_btn_lbl,
                                        lv_color_hex(::ui::C_TEXT), 0);
        }
    } else {
        lv_obj_add_flag(s_ams_dry_btn, LV_OBJ_FLAG_HIDDEN);
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
        char dbuf[24];
        if (u.dry_time_min >= 60)
            snprintf(dbuf, sizeof(dbuf), "%s %luh%02lu",
                     i18n::tr(i18n::Str::DRY),
                     (unsigned long)(u.dry_time_min / 60),
                     (unsigned long)(u.dry_time_min % 60));
        else
            snprintf(dbuf, sizeof(dbuf), "%s %lum",
                     i18n::tr(i18n::Str::DRY),
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

}  // namespace ui::screens
