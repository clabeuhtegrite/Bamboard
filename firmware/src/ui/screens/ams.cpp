// Bamboard screens — AMS overview.
//
// Top strip (left → right at y=36):
//   [ ◀ ]  AMS 1 / 2  [ ▶ ]   〈humidity〉  〈temp〉  …  [ Dry / Stop ]
//
// Slot row (y=76..224): up to 4 cards (1 for AMS-HT, 4 for vanilla AMS).
// Each card carries a colour swatch + filament type + remaining % + bar.
// Every swatch gets a 1 px outline so a black spool reads against the dark
// panel; clear/translucent filament (low tray_color alpha) is drawn as a
// checkerboard instead of a flat near-black square.
//
// The chevrons sit flush against the unit label and only become visible
// when the focused printer has more than one chained AMS / AMS-HT unit.
// The Dry / Stop button shows on any heater-equipped unit — AMS-HT *and*
// AMS 2 Pro (the latter reports is_ams_ht=false but still dries); it's hidden
// only on the first-gen AMS, which has no heater.

#include "theme.h"

#include <cstring>   // strncmp for the filament-type drying fallback table

#include "../control.h"     // marshal drying POSTs to the net task (never block UI)
#include "../dry_default.h"  // per-filament drying fallback table (host-tested)
#include "../units.h"        // °C/°F display preference (ambient AMS temp)

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
static uint8_t s_ams_count_cache = 0;   // last-seen unit count, for cycle clamping

// Drying-countdown baseline: Bambuddy reports whole minutes remaining, so we
// latch that value + the tick it arrived on and tick the seconds down locally
// between polls. s_dry_unit_seen is the unit index the baseline belongs to.
static int      s_dry_unit_seen   = -1;
static uint32_t s_dry_min_seen    = 0;
static uint32_t s_dry_baseline_ms = 0;

uint8_t ams_visible_unit_index() {
    return (uint8_t)(s_ams_visible_index < 0 ? 0 : s_ams_visible_index);
}

// A tiny 2-shade checkerboard, tiled behind translucent / "clear" filament so a
// see-through spool reads as transparent instead of as a flat near-black square
// (Bambu reports clear PETG as RGB 000000 — identical to black without alpha).
// INDEXED_1BIT: a 2-colour palette (4 bytes BGRA each) followed by a 16x16
// 1-bpp bitmap (MSB-first, 2 bytes/row), 8 px squares. Both shades are light so
// the pattern pops on the dark panel; render_ams_slot can wash it toward the
// filament's own colour for tinted-translucent spools.
static const uint8_t kCheckerMap[] = {
    0xB0, 0xB0, 0xB0, 0xFF,   // idx 0 — light grey
    0xE6, 0xE6, 0xE6, 0xFF,   // idx 1 — near-white
    0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,   // rows  0..3
    0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,   // rows  4..7
    0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,   // rows  8..11
    0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,   // rows 12..15
};
static const lv_img_dsc_t kChecker = {
    { LV_IMG_CF_INDEXED_1BIT, 0, 0, 16, 16 },   // header: cf, always_zero, reserved, w, h
    sizeof(kCheckerMap),
    kCheckerMap,
};

// AMS-HT and AMS 2 Pro both carry a heater + ambient-temp sensor and can run a
// drying cycle; the first-gen AMS has neither. "Is HT, or reports a temperature"
// is the dryer-capability proxy — it lights the Dry control on Pro units, which
// report is_ams_ht=false.
static bool ams_unit_can_dry(const ::bambuddy::AmsUnit& u) {
    return u.is_ht || u.temp > 0.5f;
}

// The per-filament drying fallback table + dry_default_for() now live in
// ui/dry_default.{h,cpp} (pure, host-unit-tested); unit_dry_params() below
// layers the RFID-then-fallback-then-generic selection on top.
using ui::DryDefault;
using ui::dry_default_for;

// Drying setpoint for a whole unit. One heater warms the shared chamber, so we
// protect the most heat-sensitive loaded spool: temperature is the LOWEST
// recommendation across present slots; time is the LONGEST. RFID values win;
// spools without a tag fall back to the type table, then a generic 55 °C / 6 h.
static void unit_dry_params(const ::bambuddy::AmsUnit& u,
                            uint16_t& minutes_out, uint8_t& temp_out) {
    uint8_t temp = 0;    // running MIN
    uint8_t hours = 0;   // running MAX
    for (uint8_t i = 0; i < u.slot_count; ++i) {
        const ::bambuddy::AmsSlot& s = u.slots[i];
        if (!s.present) continue;
        uint8_t t = s.dry_temp_c, h = s.dry_time_h;
        if (!t || !h) {
            DryDefault d = dry_default_for(s.type);
            if (!t) t = d.temp_c;
            if (!h) h = d.hours;
        }
        if (t) temp  = temp ? (t < temp ? t : temp) : t;
        if (h) hours = h > hours ? h : hours;
    }
    if (!temp)  temp  = 55;
    if (!hours) hours = 6;
    temp_out    = temp;
    minutes_out = (uint16_t)hours * 60;
}

// ---- slot card --------------------------------------------------------------

static lv_obj_t* make_ams_slot_card(lv_obj_t* parent,
                                     lv_obj_t** swatch,
                                     lv_obj_t** type_lbl,
                                     lv_obj_t** pct_lbl,
                                     lv_obj_t** bar) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_add_style(card, &s_panel, 0);
    lv_obj_set_size(card, 108, 156);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card, 0, 0);

    *swatch = lv_obj_create(card);
    lv_obj_remove_style_all(*swatch);
    lv_obj_set_size(*swatch, 108, 50);
    lv_obj_align(*swatch, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_radius(*swatch, 8, 0);
    lv_obj_set_style_bg_opa(*swatch, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(*swatch, lv_color_hex(::ui::C_PANEL_HI), 0);
    // Outline (so a black/dark/clear swatch reads against the dark card) +
    // tiling for the checkerboard bg-image. Width is toggled per-slot in
    // render_ams_slot (0 on empty slots); the colour/opacity are static.
    lv_obj_set_style_border_color(*swatch, lv_color_hex(::ui::C_TEXT_DIM), 0);
    lv_obj_set_style_border_opa(*swatch, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(*swatch, 0, 0);
    lv_obj_set_style_bg_img_tiled(*swatch, true, 0);

    *type_lbl = lv_label_create(card);
    lv_label_set_text(*type_lbl, "-");
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

// Drying toggle. When the unit reports a non-zero countdown, tapping fires
// Stop; otherwise it starts a cycle whose temperature + duration are derived
// from the loaded filament (unit_dry_params), and the toast reports them.
static void ams_dry_clicked(lv_event_t*) {
    int id = ::ui::g_ui.selected_printer_id();
    if (id < 0) return;
    uint8_t unit = ams_visible_unit_index();
    ::bambuddy::Printer ps[::bambuddy::MAX_PRINTERS]; uint8_t n = 0;
    ::bambuddy::g_client.snapshot_printers(ps, n);
    const ::bambuddy::AmsUnit* u = nullptr;
    for (uint8_t i = 0; i < n; ++i)
        if (ps[i].id == id && ps[i].ams_count > unit) { u = &ps[i].ams[unit]; break; }
    if (!u) return;

    if (u->dry_time_min > 0) {            // a cycle is running → Stop
        ::ui::ctrl::enqueue(::ui::ctrl::DryStop, id, unit);
        return;
    }

    // Derive the setpoint from the loaded filament (RFID profile, else the
    // per-type fallback) here — a cheap snapshot read on the UI task — then hand
    // the blocking POST to the net task. (a = unit, b = temp °C, c = minutes.)
    uint16_t minutes; uint8_t temp_c;
    unit_dry_params(*u, minutes, temp_c);
    ::ui::ctrl::enqueue(::ui::ctrl::DryStart, id, unit, (uint8_t)temp_c, minutes);
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

    // --- Slot row --- (unified 12 px gutter; 4×108 cards + 3×8 gaps = 456)
    s_ams_row = lv_obj_create(s_ams_root);
    lv_obj_remove_style_all(s_ams_row);
    lv_obj_set_size(s_ams_row, LV_HOR_RES - 24, 148);
    lv_obj_align(s_ams_row, LV_ALIGN_TOP_LEFT, 12, 40);
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
    lv_obj_t* sw = s_ams_card_swatch[idx];
    lv_obj_clear_flag(s_ams_card[idx], LV_OBJ_FLAG_HIDDEN);
    if (!s || !s->present) {
        lv_obj_set_style_bg_img_src(sw, NULL, 0);
        lv_obj_set_style_bg_color(sw, lv_color_hex(::ui::C_PANEL_HI), 0);
        lv_obj_set_style_border_width(sw, 0, 0);
        lv_label_set_text(s_ams_card_type[idx], i18n::tr(i18n::Str::EMPTY));
        lv_obj_set_style_text_color(s_ams_card_type[idx],
                                     lv_color_hex(::ui::C_TEXT_DIM), 0);
        lv_label_set_text(s_ams_card_pct[idx], "-");
        lv_obj_set_style_text_color(s_ams_card_pct[idx],
                                     lv_color_hex(::ui::C_TEXT_DIM), 0);
        lv_bar_set_value(s_ams_card_bar[idx], 0, LV_ANIM_OFF);
        return;
    }
    // Translucent / clear filament → checkerboard (so a see-through spool reads
    // as transparent); opaque → its real colour, even pure black (the static
    // 1 px outline keeps a black swatch off the dark card). Reset every property
    // each pass since the 4 swatch objects are reused across refreshes.
    if (s->translucent) {
        lv_obj_set_style_bg_color(sw, lv_color_hex(0xE6E6E6), 0);
        lv_obj_set_style_bg_img_src(sw, &kChecker, 0);
        if (s->color_rgb) {   // tinted-translucent → wash the checker toward it
            lv_obj_set_style_bg_img_recolor(sw, lv_color_hex(s->color_rgb), 0);
            lv_obj_set_style_bg_img_recolor_opa(sw, LV_OPA_40, 0);
        } else {
            lv_obj_set_style_bg_img_recolor_opa(sw, LV_OPA_TRANSP, 0);
        }
    } else {
        lv_obj_set_style_bg_img_src(sw, NULL, 0);
        lv_obj_set_style_bg_color(sw, lv_color_hex(s->color_rgb), 0);
    }
    lv_obj_set_style_border_width(sw, 1, 0);
    lv_label_set_text(s_ams_card_type[idx], s->type[0] ? s->type : "-");
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
    // Clamp to the last-seen unit count so the getter can't return an index past
    // the chained-unit count between this tap and the next refresh.
    if (s_ams_count_cache > 0 && s_ams_visible_index >= s_ams_count_cache)
        s_ams_visible_index = s_ams_count_cache - 1;
}

void update_ams(int printer_id) {
    maybe_hide_toast();
    ::bambuddy::Printer ps[::bambuddy::MAX_PRINTERS]; uint8_t n = 0;
    ::bambuddy::g_client.snapshot_printers(ps, n);

    const ::bambuddy::Printer* sel = nullptr;
    for (uint8_t i = 0; i < n; ++i) if (ps[i].id == printer_id) sel = &ps[i];
    if (!sel && n > 0) sel = &ps[0];

    bool has_ams = (sel != nullptr) && sel->ams_exists && sel->ams_count > 0;
    s_ams_count_cache = has_ams ? sel->ams_count : 0;
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

    // Drying button label + colour reflect the current state of *this* unit
    // (not the whole printer). Shown on any heater-equipped unit — AMS-HT and
    // AMS 2 Pro (see ams_unit_can_dry); hidden on the heater-less first-gen AMS.
    if (ams_unit_can_dry(u)) {
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
        snprintf(tbuf, sizeof(tbuf), "%.0f %s", ui::temp_value(u.temp), ui::temp_unit());
        lv_label_set_text(s_ams_temp_lbl, tbuf);
    } else {
        lv_label_set_text(s_ams_temp_lbl, "");
    }

    if (u.dry_time_min > 0) {
        // Re-baseline whenever Bambuddy reports a new minutes value (or we switch
        // units), then tick the seconds down locally so the readout feels live.
        // The daily reboot keeps lv_tick_get() from wrapping during a cycle.
        if (s_ams_visible_index != s_dry_unit_seen || u.dry_time_min != s_dry_min_seen) {
            s_dry_unit_seen   = s_ams_visible_index;
            s_dry_min_seen    = u.dry_time_min;
            s_dry_baseline_ms = lv_tick_get();
        }
        uint32_t elapsed_s = (lv_tick_get() - s_dry_baseline_ms) / 1000;
        uint32_t total_s   = s_dry_min_seen * 60;
        uint32_t left_s    = (elapsed_s < total_s) ? (total_s - elapsed_s) : 0;
        char dbuf[24];
        if (left_s >= 3600)
            snprintf(dbuf, sizeof(dbuf), "%s %luh%02lu",
                     i18n::tr(i18n::Str::DRY),
                     (unsigned long)(left_s / 3600), (unsigned long)((left_s % 3600) / 60));
        else
            snprintf(dbuf, sizeof(dbuf), "%s %02lu:%02lu",
                     i18n::tr(i18n::Str::DRY),
                     (unsigned long)(left_s / 60), (unsigned long)(left_s % 60));
        lv_label_set_text(s_ams_dry_lbl, dbuf);
    } else {
        lv_label_set_text(s_ams_dry_lbl, "");
        s_dry_unit_seen = -1;   // reset so the next cycle re-bases cleanly
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
