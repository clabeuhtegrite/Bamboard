// All Bamboard screen builders and updaters live here. Kept in one file
// because the screens share a lot of styling and helpers, and because the
// UI surface is small enough that splitting per-file would be more friction
// than it's worth.

#include "screens.h"

#include <lvgl.h>
#include <WiFi.h>

#include "../config.h"
#include "../net/bambuddy_client.h"

namespace ui::screens {

// ---------- shared style ---------------------------------------------------

static lv_style_t s_panel;
static lv_style_t s_label_dim;
static lv_style_t s_label_value;
static lv_style_t s_label_big;
static bool styles_ready = false;

static void ensure_styles() {
    if (styles_ready) return;
    styles_ready = true;

    lv_style_init(&s_panel);
    lv_style_set_bg_color(&s_panel, lv_color_hex(::ui::C_PANEL));
    lv_style_set_bg_opa(&s_panel, LV_OPA_COVER);
    lv_style_set_border_width(&s_panel, 0);
    lv_style_set_radius(&s_panel, 10);
    lv_style_set_pad_all(&s_panel, 10);

    lv_style_init(&s_label_dim);
    lv_style_set_text_color(&s_label_dim, lv_color_hex(::ui::C_TEXT_DIM));
    lv_style_set_text_font(&s_label_dim, &lv_font_montserrat_12);

    lv_style_init(&s_label_value);
    lv_style_set_text_color(&s_label_value, lv_color_hex(::ui::C_TEXT));
    lv_style_set_text_font(&s_label_value, &lv_font_montserrat_20);

    lv_style_init(&s_label_big);
    lv_style_set_text_color(&s_label_big, lv_color_hex(::ui::C_TEXT));
    lv_style_set_text_font(&s_label_big, &lv_font_montserrat_36);
}

// ---------- header ---------------------------------------------------------

static lv_obj_t* s_hdr_title       = nullptr;
static lv_obj_t* s_hdr_printer     = nullptr;
static lv_obj_t* s_hdr_conn        = nullptr;

lv_obj_t* build_header(lv_obj_t* parent) {
    ensure_styles();
    lv_obj_t* hdr = lv_obj_create(parent);
    lv_obj_remove_style_all(hdr);
    lv_obj_set_size(hdr, LV_HOR_RES, 36);
    lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(::ui::C_PANEL), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    s_hdr_title = lv_label_create(hdr);
    lv_label_set_text(s_hdr_title, "Bamboard");
    lv_obj_align(s_hdr_title, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_set_style_text_color(s_hdr_title, lv_color_hex(::ui::C_ACCENT), 0);
    lv_obj_set_style_text_font(s_hdr_title, &lv_font_montserrat_20, 0);

    s_hdr_printer = lv_label_create(hdr);
    lv_label_set_text(s_hdr_printer, "");
    lv_obj_align(s_hdr_printer, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(s_hdr_printer, lv_color_hex(::ui::C_TEXT), 0);
    lv_obj_set_style_text_font(s_hdr_printer, &lv_font_montserrat_16, 0);

    s_hdr_conn = lv_label_create(hdr);
    lv_label_set_text(s_hdr_conn, LV_SYMBOL_WIFI " --");
    lv_obj_align(s_hdr_conn, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_text_color(s_hdr_conn, lv_color_hex(::ui::C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_hdr_conn, &lv_font_montserrat_14, 0);
    return hdr;
}

void header_set_title(const char* title) {
    if (s_hdr_title) lv_label_set_text(s_hdr_title, title);
}

void header_set_online(bool online, uint32_t latency_ms) {
    if (!s_hdr_conn) return;
    if (online) {
        char buf[24];
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %ums", (unsigned)latency_ms);
        lv_label_set_text(s_hdr_conn, buf);
        lv_obj_set_style_text_color(s_hdr_conn, lv_color_hex(::ui::C_OK), 0);
    } else {
        lv_label_set_text(s_hdr_conn, LV_SYMBOL_WARNING " offline");
        lv_obj_set_style_text_color(s_hdr_conn, lv_color_hex(::ui::C_ERR), 0);
    }
}

void header_set_printer_name(const char* name) {
    if (s_hdr_printer) lv_label_set_text(s_hdr_printer, name ? name : "");
}

// ---------- toast ----------------------------------------------------------

static lv_obj_t* s_toast       = nullptr;
static lv_obj_t* s_toast_label = nullptr;
static uint32_t  s_toast_hide_at = 0;

lv_obj_t* build_toast(lv_obj_t* parent) {
    s_toast = lv_obj_create(parent);
    lv_obj_remove_style_all(s_toast);
    lv_obj_set_size(s_toast, 280, 44);
    lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_set_style_bg_color(s_toast, lv_color_hex(::ui::C_ACCENT), 0);
    lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_toast, 8, 0);
    lv_obj_clear_flag(s_toast, LV_OBJ_FLAG_SCROLLABLE);

    s_toast_label = lv_label_create(s_toast);
    lv_label_set_text(s_toast_label, "");
    lv_obj_center(s_toast_label);
    lv_obj_set_style_text_color(s_toast_label, lv_color_hex(0x101418), 0);
    lv_obj_set_style_text_font(s_toast_label, &lv_font_montserrat_16, 0);

    lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    return s_toast;
}

void show_toast(const char* msg, lv_color_t bg) {
    if (!s_toast) return;
    lv_label_set_text(s_toast_label, msg);
    lv_obj_set_style_bg_color(s_toast, bg, 0);
    lv_obj_clear_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    s_toast_hide_at = lv_tick_get() + 1800;
}

// Toast auto-hide is checked on each update_*().
static void maybe_hide_toast() {
    if (!s_toast || lv_obj_has_flag(s_toast, LV_OBJ_FLAG_HIDDEN)) return;
    if (s_toast_hide_at && lv_tick_get() > s_toast_hide_at) {
        lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    }
}

// ---------- HMS full-screen flash ------------------------------------------
//
// Big red overlay that the UI manager pops up periodically while the
// selected printer's HMS string is anything other than "ok". The bg
// colour pulses between two shades of red via lv_anim so the panel
// *visibly* throbs — the whole point of this surface is to be obvious
// from across the room when a print needs attention.

static lv_obj_t* s_hms_overlay = nullptr;
static lv_obj_t* s_hms_msg     = nullptr;
static bool      s_hms_visible = false;

// Animate the overlay's background colour between a deep maroon (0x801F22)
// and the dashboard's error red (::ui::C_ERR). 0..100 maps to that range.
static void hms_pulse_cb(void* var, int32_t v) {
    const uint8_t r0 = 0x80, g0 = 0x1F, b0 = 0x22;
    const uint8_t r1 = (::ui::C_ERR >> 16) & 0xFF;
    const uint8_t g1 = (::ui::C_ERR >>  8) & 0xFF;
    const uint8_t b1 = (::ui::C_ERR      ) & 0xFF;
    uint8_t r = r0 + (uint8_t)(((int32_t)(r1 - r0) * v) / 100);
    uint8_t g = g0 + (uint8_t)(((int32_t)(g1 - g0) * v) / 100);
    uint8_t b = b0 + (uint8_t)(((int32_t)(b1 - b0) * v) / 100);
    lv_obj_set_style_bg_color((lv_obj_t*)var,
        lv_color_make(r, g, b), 0);
}

lv_obj_t* build_hms_flash(lv_obj_t* parent) {
    ensure_styles();

    s_hms_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_hms_overlay);
    lv_obj_set_size(s_hms_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(s_hms_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_hms_overlay, lv_color_hex(::ui::C_ERR), 0);
    lv_obj_set_style_bg_opa(s_hms_overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_hms_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* icon = lv_label_create(s_hms_overlay);
    lv_label_set_text(icon, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -86);

    lv_obj_t* title = lv_label_create(s_hms_overlay);
    lv_label_set_text(title, "HMS ERROR");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -34);

    s_hms_msg = lv_label_create(s_hms_overlay);
    lv_label_set_text(s_hms_msg, "");
    lv_obj_set_width(s_hms_msg, LV_HOR_RES - 80);
    lv_label_set_long_mode(s_hms_msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_hms_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_hms_msg, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_hms_msg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_hms_msg, LV_ALIGN_CENTER, 0, 24);

    lv_obj_t* hint = lv_label_create(s_hms_overlay);
    lv_label_set_text(hint, "Press any button to dismiss");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xFFE0E0), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -16);

    lv_obj_add_flag(s_hms_overlay, LV_OBJ_FLAG_HIDDEN);
    return s_hms_overlay;
}

void hms_flash_show(const char* msg) {
    if (!s_hms_overlay) return;
    lv_label_set_text(s_hms_msg, (msg && *msg) ? msg : "Check the printer.");
    lv_obj_clear_flag(s_hms_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_hms_overlay);
    s_hms_visible = true;

    // Kick off the colour pulse — slow enough to read as a heartbeat rather
    // than a strobe, fast enough to feel urgent. The anim is auto-deleted
    // when we delete the matching exec_cb on hide.
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_hms_overlay);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 700);
    lv_anim_set_playback_time(&a, 700);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, hms_pulse_cb);
    lv_anim_start(&a);
}

void hms_flash_hide() {
    if (!s_hms_overlay) return;
    lv_anim_del(s_hms_overlay, hms_pulse_cb);
    lv_obj_add_flag(s_hms_overlay, LV_OBJ_FLAG_HIDDEN);
    s_hms_visible = false;
}

void hms_flash_update_msg(const char* msg) {
    if (s_hms_msg && msg) lv_label_set_text(s_hms_msg, msg);
}

bool hms_flash_is_visible() { return s_hms_visible; }

// ---------- actions modal --------------------------------------------------
//
// A context menu for the currently-selected printer. The caller chooses
// which items appear: the Printers screen opens the full triplet (Clear
// HMS / Clear plate / Cancel) on long-press OK, while the Live screen
// double-click opens a state-dependent subset built by
// actions_open_for_live(). The modal is a single LVGL object graph we
// hide/show; the UI manager funnels button events into us while
// `actions_is_open()` is true.

static constexpr uint8_t ACT_MAX_ITEMS = 5;

static lv_obj_t*   s_act_backdrop = nullptr;
static lv_obj_t*   s_act_title    = nullptr;
static lv_obj_t*   s_act_items[ACT_MAX_ITEMS]  = {};
static lv_obj_t*   s_act_labels[ACT_MAX_ITEMS] = {};
static bool        s_act_open      = false;
static uint8_t     s_act_index     = 0;
static int         s_act_printer   = -1;
// Snapshot of the speed at open time — we offer "Cycle speed" as a single
// menu entry (advance to the next mode) and reuse this to label the row
// with the *current* mode name.
static uint8_t     s_act_speed_lvl = 2;
static ActionItem  s_act_visible[ACT_MAX_ITEMS] = {};
static uint8_t     s_act_visible_count = 0;

static const char* speed_mode_name(uint8_t mode) {
    switch (mode) {
        case 1: return "Silent";
        case 2: return "Standard";
        case 3: return "Sport";
        case 4: return "Ludicrous";
        default: return "?";
    }
}

static String action_label(ActionItem a) {
    switch (a) {
        case ActionItem::ClearHms:   return String("Clear HMS");
        case ActionItem::ClearPlate: return String("Clear plate");
        case ActionItem::CycleSpeed: {
            uint8_t next = (uint8_t)((s_act_speed_lvl % 4) + 1);
            return String("Speed: ") + speed_mode_name(s_act_speed_lvl)
                 + " " LV_SYMBOL_RIGHT " " + speed_mode_name(next);
        }
        case ActionItem::Cancel:     return String("Cancel");
        default: return String("?");
    }
}

static void redraw_action_highlight() {
    for (uint8_t i = 0; i < ACT_MAX_ITEMS; ++i) {
        bool visible = (i < s_act_visible_count);
        if (!visible) {
            lv_obj_add_flag(s_act_items[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(s_act_items[i], LV_OBJ_FLAG_HIDDEN);
        bool sel = (i == s_act_index);
        lv_obj_set_style_bg_color(s_act_items[i],
            lv_color_hex(sel ? ::ui::C_PANEL_HI : ::ui::C_PANEL), 0);
        lv_obj_set_style_border_width(s_act_items[i], sel ? 2 : 0, 0);
        lv_obj_set_style_border_color(s_act_items[i],
            lv_color_hex(::ui::C_ACCENT), 0);
        lv_obj_set_style_text_color(s_act_labels[i],
            lv_color_hex(sel ? ::ui::C_ACCENT : ::ui::C_TEXT), 0);
    }
}

lv_obj_t* build_actions(lv_obj_t* parent) {
    ensure_styles();

    s_act_backdrop = lv_obj_create(parent);
    lv_obj_remove_style_all(s_act_backdrop);
    lv_obj_set_size(s_act_backdrop, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(s_act_backdrop, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_act_backdrop, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_act_backdrop, LV_OPA_70, 0);
    lv_obj_clear_flag(s_act_backdrop, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* panel = lv_obj_create(s_act_backdrop);
    lv_obj_add_style(panel, &s_panel, 0);
    lv_obj_set_size(panel, 380, 300);
    lv_obj_center(panel);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(panel, 14, 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(::ui::C_ACCENT), 0);

    s_act_title = lv_label_create(panel);
    lv_label_set_text(s_act_title, "Actions");
    lv_obj_align(s_act_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_font(s_act_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_act_title, lv_color_hex(::ui::C_ACCENT), 0);

    for (uint8_t i = 0; i < ACT_MAX_ITEMS; ++i) {
        lv_obj_t* row = lv_obj_create(panel);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, 348, 36);
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 36 + i * 40);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_all(row, 8, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, "");
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);

        s_act_items[i]  = row;
        s_act_labels[i] = lbl;
    }

    lv_obj_t* hint = lv_label_create(panel);
    lv_label_set_text(hint,
        LV_SYMBOL_LEFT " / " LV_SYMBOL_RIGHT "  select"
        "       OK  confirm       hold OK  close");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_style(hint, &s_label_dim, 0);

    lv_obj_add_flag(s_act_backdrop, LV_OBJ_FLAG_HIDDEN);
    return s_act_backdrop;
}

void actions_open(int printer_id,
                  const char* printer_name,
                  const ActionItem* items,
                  uint8_t count) {
    if (!s_act_backdrop || !items || count == 0) return;
    if (count > ACT_MAX_ITEMS) count = ACT_MAX_ITEMS;

    s_act_open     = true;
    s_act_index    = 0;
    s_act_printer  = printer_id;
    s_act_visible_count = count;
    for (uint8_t i = 0; i < count; ++i) {
        s_act_visible[i] = items[i];
        lv_label_set_text(s_act_labels[i], action_label(items[i]).c_str());
    }

    String t = String("Actions — ") + (printer_name ? printer_name : "Printer");
    lv_label_set_text(s_act_title, t.c_str());
    redraw_action_highlight();
    lv_obj_clear_flag(s_act_backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_act_backdrop);
}

void actions_open_for_live(int printer_id,
                           const char* printer_name,
                           ::bambuddy::PrinterState state,
                           uint8_t speed_level) {
    using PS = ::bambuddy::PrinterState;
    s_act_speed_lvl = (speed_level >= 1 && speed_level <= 4) ? speed_level : 2;

    ActionItem items[ACT_MAX_ITEMS];
    uint8_t n = 0;

    // Speed cycling only makes sense while the printer is actively running.
    if (state == PS::Printing || state == PS::Paused || state == PS::Prepare) {
        items[n++] = ActionItem::CycleSpeed;
    }
    // Clear plate is what unblocks the queue after a finished/failed print.
    if (state == PS::Finish || state == PS::Failed) {
        items[n++] = ActionItem::ClearPlate;
    }
    items[n++] = ActionItem::ClearHms;
    items[n++] = ActionItem::Cancel;

    actions_open(printer_id, printer_name, items, n);
}

void actions_close() {
    if (!s_act_backdrop) return;
    s_act_open = false;
    lv_obj_add_flag(s_act_backdrop, LV_OBJ_FLAG_HIDDEN);
}

bool actions_is_open() { return s_act_open; }

void actions_navigate(int dir) {
    if (!s_act_open || s_act_visible_count == 0) return;
    int n = (int)s_act_visible_count;
    s_act_index = (uint8_t)(((int)s_act_index + dir % n + n) % n);
    redraw_action_highlight();
}

void actions_confirm() {
    if (!s_act_open || s_act_index >= s_act_visible_count) return;
    ActionItem a = s_act_visible[s_act_index];
    int id = s_act_printer;
    actions_close();   // close first so the toast lands on the normal UI

    if (id < 0 && a != ActionItem::Cancel) {
        show_toast("No printer selected", lv_color_hex(::ui::C_WARN));
        return;
    }
    switch (a) {
        case ActionItem::ClearHms: {
            bool ok = ::bambuddy::g_client.clear_hms(id);
            show_toast(ok ? "HMS cleared" : "Clear HMS failed",
                       lv_color_hex(ok ? ::ui::C_OK : ::ui::C_ERR));
            break;
        }
        case ActionItem::ClearPlate: {
            bool ok = ::bambuddy::g_client.clear_plate(id);
            show_toast(ok ? "Plate cleared" : "Clear plate failed",
                       lv_color_hex(ok ? ::ui::C_OK : ::ui::C_ERR));
            break;
        }
        case ActionItem::CycleSpeed: {
            uint8_t next = (uint8_t)((s_act_speed_lvl % 4) + 1);
            bool ok = ::bambuddy::g_client.set_print_speed(id, next);
            if (ok) {
                s_act_speed_lvl = next;
                String msg = String("Speed: ") + speed_mode_name(next);
                show_toast(msg.c_str(), lv_color_hex(::ui::C_OK));
            } else {
                show_toast("Set speed failed", lv_color_hex(::ui::C_ERR));
            }
            break;
        }
        case ActionItem::Cancel:
        default:
            break;
    }
}

// ---------- dashboard ------------------------------------------------------

static lv_obj_t* s_dash_root      = nullptr;
static lv_obj_t* s_dash_state_lbl = nullptr;
static lv_obj_t* s_dash_file_lbl  = nullptr;
static lv_obj_t* s_dash_progress_arc = nullptr;
static lv_obj_t* s_dash_progress_lbl = nullptr;
static lv_obj_t* s_dash_eta_lbl   = nullptr;
static lv_obj_t* s_dash_layer_lbl = nullptr;
static lv_obj_t* s_dash_t_noz     = nullptr;
static lv_obj_t* s_dash_t_bed     = nullptr;
static lv_obj_t* s_dash_t_cham    = nullptr;
static lv_obj_t* s_dash_hms       = nullptr;

static const char* state_name(::bambuddy::PrinterState s) {
    using PS = ::bambuddy::PrinterState;
    switch (s) {
        case PS::Idle:     return "IDLE";
        case PS::Prepare:  return "PREPARE";
        case PS::Printing: return "PRINTING";
        case PS::Paused:   return "PAUSED";
        case PS::Finish:   return "FINISH";
        case PS::Failed:   return "FAILED";
        case PS::Offline:  return "OFFLINE";
        case PS::Error:    return "ERROR";
        default:           return "?";
    }
}

static uint32_t state_color(::bambuddy::PrinterState s) {
    using PS = ::bambuddy::PrinterState;
    switch (s) {
        case PS::Printing: return ::ui::C_ACCENT;
        case PS::Paused:   return ::ui::C_WARN;
        case PS::Finish:   return ::ui::C_OK;
        case PS::Failed:
        case PS::Error:    return ::ui::C_ERR;
        case PS::Offline:  return ::ui::C_TEXT_DIM;
        default:           return ::ui::C_TEXT_DIM;
    }
}

static lv_obj_t* make_temp_card(lv_obj_t* parent, const char* title) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_add_style(card, &s_panel, 0);
    lv_obj_set_size(card, 130, 78);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* t = lv_label_create(card);
    lv_label_set_text(t, title);
    lv_obj_add_style(t, &s_label_dim, 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* v = lv_label_create(card);
    lv_label_set_text(v, "-- °C");
    lv_obj_add_style(v, &s_label_value, 0);
    lv_obj_align(v, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    return v;
}

lv_obj_t* build_dashboard(lv_obj_t* parent) {
    ensure_styles();
    s_dash_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_dash_root);
    lv_obj_set_size(s_dash_root, LV_HOR_RES, LV_VER_RES - 36 - 18);
    lv_obj_align(s_dash_root, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_obj_clear_flag(s_dash_root, LV_OBJ_FLAG_SCROLLABLE);

    // Big progress arc on the left.
    s_dash_progress_arc = lv_arc_create(s_dash_root);
    lv_obj_set_size(s_dash_progress_arc, 170, 170);
    lv_obj_align(s_dash_progress_arc, LV_ALIGN_LEFT_MID, 14, 10);
    lv_arc_set_rotation(s_dash_progress_arc, 270);
    lv_arc_set_bg_angles(s_dash_progress_arc, 0, 360);
    lv_arc_set_range(s_dash_progress_arc, 0, 100);
    lv_arc_set_value(s_dash_progress_arc, 0);
    lv_obj_remove_style(s_dash_progress_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(s_dash_progress_arc, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_dash_progress_arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_dash_progress_arc,
                                lv_color_hex(::ui::C_PANEL_HI), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_dash_progress_arc,
                                lv_color_hex(::ui::C_ACCENT), LV_PART_INDICATOR);
    lv_obj_clear_flag(s_dash_progress_arc, LV_OBJ_FLAG_CLICKABLE);

    s_dash_progress_lbl = lv_label_create(s_dash_progress_arc);
    lv_label_set_text(s_dash_progress_lbl, "--%");
    lv_obj_add_style(s_dash_progress_lbl, &s_label_big, 0);
    lv_obj_center(s_dash_progress_lbl);

    // State + filename + ETA on the right top.
    s_dash_state_lbl = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_state_lbl, "—");
    lv_obj_align(s_dash_state_lbl, LV_ALIGN_TOP_LEFT, 200, 0);
    lv_obj_set_style_text_font(s_dash_state_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_dash_state_lbl, lv_color_hex(::ui::C_ACCENT), 0);

    s_dash_file_lbl = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_file_lbl, "no file");
    lv_obj_set_width(s_dash_file_lbl, 260);
    lv_label_set_long_mode(s_dash_file_lbl, LV_LABEL_LONG_DOT);
    lv_obj_align(s_dash_file_lbl, LV_ALIGN_TOP_LEFT, 200, 38);
    lv_obj_add_style(s_dash_file_lbl, &s_label_dim, 0);

    s_dash_eta_lbl = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_eta_lbl, "ETA --:--");
    lv_obj_align(s_dash_eta_lbl, LV_ALIGN_TOP_LEFT, 200, 62);
    lv_obj_set_style_text_font(s_dash_eta_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_dash_eta_lbl, lv_color_hex(::ui::C_TEXT), 0);

    s_dash_layer_lbl = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_layer_lbl, "layer --/--");
    lv_obj_align(s_dash_layer_lbl, LV_ALIGN_TOP_LEFT, 200, 92);
    lv_obj_add_style(s_dash_layer_lbl, &s_label_dim, 0);

    s_dash_hms = lv_label_create(s_dash_root);
    lv_label_set_text(s_dash_hms, "");
    lv_obj_align(s_dash_hms, LV_ALIGN_TOP_LEFT, 200, 116);
    lv_obj_set_style_text_color(s_dash_hms, lv_color_hex(::ui::C_ERR), 0);

    // Three temperature cards along the bottom.
    s_dash_t_noz  = make_temp_card(s_dash_root, "NOZZLE");
    lv_obj_align(lv_obj_get_parent(s_dash_t_noz), LV_ALIGN_BOTTOM_LEFT,  16, -6);
    s_dash_t_bed  = make_temp_card(s_dash_root, "BED");
    lv_obj_align(lv_obj_get_parent(s_dash_t_bed), LV_ALIGN_BOTTOM_MID,    0, -6);
    s_dash_t_cham = make_temp_card(s_dash_root, "CHAMBER");
    lv_obj_align(lv_obj_get_parent(s_dash_t_cham), LV_ALIGN_BOTTOM_RIGHT, -16, -6);

    return s_dash_root;
}

static String fmt_eta(uint32_t secs) {
    if (secs == 0) return "—";
    uint32_t h = secs / 3600;
    uint32_t m = (secs % 3600) / 60;
    char buf[16];
    if (h > 0) snprintf(buf, sizeof(buf), "%uh%02u", (unsigned)h, (unsigned)m);
    else       snprintf(buf, sizeof(buf), "%um",     (unsigned)m);
    return String(buf);
}

void update_dashboard(int printer_id) {
    maybe_hide_toast();
    ::bambuddy::Printer ps[8]; uint8_t n = 0;
    ::bambuddy::g_client.snapshot_printers(ps, n);
    if (n == 0) {
        lv_label_set_text(s_dash_state_lbl, "NO PRINTER");
        lv_label_set_text(s_dash_file_lbl, "Add one in Bambuddy");
        header_set_printer_name("");
        return;
    }
    const ::bambuddy::Printer* sel = nullptr;
    for (uint8_t i = 0; i < n; ++i) if (ps[i].id == printer_id) sel = &ps[i];
    if (!sel) sel = &ps[0];

    header_set_printer_name(sel->name.c_str());

    lv_label_set_text(s_dash_state_lbl, state_name(sel->state));
    lv_obj_set_style_text_color(s_dash_state_lbl,
                                 lv_color_hex(state_color(sel->state)), 0);

    if (sel->filename.length()) {
        lv_label_set_text(s_dash_file_lbl, sel->filename.c_str());
    } else {
        lv_label_set_text(s_dash_file_lbl, "(idle)");
    }

    lv_arc_set_value(s_dash_progress_arc, sel->progress);
    char pbuf[8];
    snprintf(pbuf, sizeof(pbuf), "%u%%", (unsigned)sel->progress);
    lv_label_set_text(s_dash_progress_lbl, pbuf);

    String eta = String("ETA ") + fmt_eta(sel->remaining_s);
    lv_label_set_text(s_dash_eta_lbl, eta.c_str());

    char lay[24];
    if (sel->total_layers > 0)
        snprintf(lay, sizeof(lay), "layer %u/%u",
                 (unsigned)sel->current_layer, (unsigned)sel->total_layers);
    else
        snprintf(lay, sizeof(lay), "layer —");
    lv_label_set_text(s_dash_layer_lbl, lay);

    if (sel->hms.length() && sel->hms != "ok") {
        String h = String(LV_SYMBOL_WARNING " HMS: ") + sel->hms;
        lv_label_set_text(s_dash_hms, h.c_str());
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
}

// ---------- AMS ------------------------------------------------------------
//
// Layout (480×266 content area):
//   y  44..  76 : status strip (unit label, humidity, temp, drying badge)
//   y  86.. 276 : up to 4 slot cards in a single row (104w, 8px gaps)
// When the printer has more than one AMS the UI manager cycles the visible
// unit via long-press OK; we cap visible at 4 slots which is the regular AMS
// capacity (AMS-HT reports a single slot and we centre it).

static lv_obj_t* s_ams_root        = nullptr;
static lv_obj_t* s_ams_empty       = nullptr;
static lv_obj_t* s_ams_unit_lbl    = nullptr;
static lv_obj_t* s_ams_humid_lbl   = nullptr;
static lv_obj_t* s_ams_temp_lbl    = nullptr;
static lv_obj_t* s_ams_dry_lbl     = nullptr;
static lv_obj_t* s_ams_row         = nullptr;
static lv_obj_t* s_ams_card[4]     = {};
static lv_obj_t* s_ams_card_swatch[4] = {};
static lv_obj_t* s_ams_card_type[4]   = {};
static lv_obj_t* s_ams_card_pct[4]    = {};
static lv_obj_t* s_ams_card_bar[4]    = {};

static int s_ams_visible_index = 0;   // index into Printer.ams[]

static lv_obj_t* make_slot_card(lv_obj_t* parent,
                                lv_obj_t** swatch,
                                lv_obj_t** type_lbl,
                                lv_obj_t** pct_lbl,
                                lv_obj_t** bar) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_add_style(card, &s_panel, 0);
    lv_obj_set_size(card, 104, 190);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card, 0, 0);

    *swatch = lv_obj_create(card);
    lv_obj_remove_style_all(*swatch);
    lv_obj_set_size(*swatch, 104, 60);
    lv_obj_align(*swatch, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_radius(*swatch, 10, 0);
    lv_obj_set_style_bg_opa(*swatch, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(*swatch, lv_color_hex(::ui::C_PANEL_HI), 0);

    *type_lbl = lv_label_create(card);
    lv_label_set_text(*type_lbl, "—");
    lv_obj_align(*type_lbl, LV_ALIGN_TOP_LEFT, 10, 72);
    lv_obj_set_style_text_font(*type_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(*type_lbl, lv_color_hex(::ui::C_TEXT), 0);

    *pct_lbl = lv_label_create(card);
    lv_label_set_text(*pct_lbl, "--%");
    lv_obj_align(*pct_lbl, LV_ALIGN_TOP_LEFT, 10, 102);
    lv_obj_set_style_text_font(*pct_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(*pct_lbl, lv_color_hex(::ui::C_TEXT), 0);

    *bar = lv_bar_create(card);
    lv_obj_set_size(*bar, 84, 8);
    lv_obj_align(*bar, LV_ALIGN_BOTTOM_LEFT, 10, -12);
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

lv_obj_t* build_ams(lv_obj_t* parent) {
    ensure_styles();
    s_ams_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_ams_root);
    lv_obj_set_size(s_ams_root, LV_HOR_RES, LV_VER_RES - 36 - 18);
    lv_obj_align(s_ams_root, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_obj_clear_flag(s_ams_root, LV_OBJ_FLAG_SCROLLABLE);

    // Status strip
    s_ams_unit_lbl = lv_label_create(s_ams_root);
    lv_label_set_text(s_ams_unit_lbl, "AMS");
    lv_obj_align(s_ams_unit_lbl, LV_ALIGN_TOP_LEFT, 20, 12);
    lv_obj_set_style_text_font(s_ams_unit_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_ams_unit_lbl, lv_color_hex(::ui::C_ACCENT), 0);

    s_ams_humid_lbl = lv_label_create(s_ams_root);
    lv_label_set_text(s_ams_humid_lbl, "");
    lv_obj_align(s_ams_humid_lbl, LV_ALIGN_TOP_LEFT, 140, 16);
    lv_obj_set_style_text_font(s_ams_humid_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_ams_humid_lbl, lv_color_hex(::ui::C_TEXT), 0);

    s_ams_temp_lbl = lv_label_create(s_ams_root);
    lv_label_set_text(s_ams_temp_lbl, "");
    lv_obj_align(s_ams_temp_lbl, LV_ALIGN_TOP_LEFT, 260, 16);
    lv_obj_set_style_text_font(s_ams_temp_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_ams_temp_lbl, lv_color_hex(::ui::C_TEXT), 0);

    s_ams_dry_lbl = lv_label_create(s_ams_root);
    lv_label_set_text(s_ams_dry_lbl, "");
    lv_obj_align(s_ams_dry_lbl, LV_ALIGN_TOP_RIGHT, -20, 16);
    lv_obj_set_style_text_font(s_ams_dry_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_ams_dry_lbl, lv_color_hex(::ui::C_WARN), 0);

    // Slot row
    s_ams_row = lv_obj_create(s_ams_root);
    lv_obj_remove_style_all(s_ams_row);
    lv_obj_set_size(s_ams_row, LV_HOR_RES - 40, 196);
    lv_obj_align(s_ams_row, LV_ALIGN_TOP_LEFT, 20, 50);
    lv_obj_set_flex_flow(s_ams_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s_ams_row, 8, 0);
    lv_obj_clear_flag(s_ams_row, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0; i < 4; ++i) {
        s_ams_card[i] = make_slot_card(s_ams_row,
                                       &s_ams_card_swatch[i],
                                       &s_ams_card_type[i],
                                       &s_ams_card_pct[i],
                                       &s_ams_card_bar[i]);
    }

    // Empty state overlay (centered, hidden by default)
    s_ams_empty = lv_label_create(s_ams_root);
    lv_label_set_text(s_ams_empty,
        "No AMS connected.\n"
        "Connect an AMS or AMS-HT to this printer\n"
        "in Bambu Studio to see its slots here.");
    lv_obj_set_style_text_align(s_ams_empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(s_ams_empty, &s_label_dim, 0);
    lv_obj_align(s_ams_empty, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_ams_empty, LV_OBJ_FLAG_HIDDEN);

    return s_ams_root;
}

static void render_slot(uint8_t slot_idx, const ::bambuddy::AmsSlot* s) {
    lv_obj_clear_flag(s_ams_card[slot_idx], LV_OBJ_FLAG_HIDDEN);

    if (!s || !s->present) {
        lv_obj_set_style_bg_color(s_ams_card_swatch[slot_idx],
                                   lv_color_hex(::ui::C_PANEL_HI), 0);
        lv_label_set_text(s_ams_card_type[slot_idx], "EMPTY");
        lv_obj_set_style_text_color(s_ams_card_type[slot_idx],
                                     lv_color_hex(::ui::C_TEXT_DIM), 0);
        lv_label_set_text(s_ams_card_pct[slot_idx], "—");
        lv_obj_set_style_text_color(s_ams_card_pct[slot_idx],
                                     lv_color_hex(::ui::C_TEXT_DIM), 0);
        lv_bar_set_value(s_ams_card_bar[slot_idx], 0, LV_ANIM_OFF);
        return;
    }

    uint32_t rgb = s->color_rgb ? s->color_rgb : ::ui::C_PANEL_HI;
    lv_obj_set_style_bg_color(s_ams_card_swatch[slot_idx], lv_color_hex(rgb), 0);

    lv_label_set_text(s_ams_card_type[slot_idx],
                       s->type[0] ? s->type : "—");
    lv_obj_set_style_text_color(s_ams_card_type[slot_idx],
                                 lv_color_hex(::ui::C_TEXT), 0);

    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)s->remain);
    lv_label_set_text(s_ams_card_pct[slot_idx], buf);

    uint32_t pct_col = ::ui::C_TEXT;
    if (s->remain < 15)      pct_col = ::ui::C_ERR;
    else if (s->remain < 30) pct_col = ::ui::C_WARN;
    lv_obj_set_style_text_color(s_ams_card_pct[slot_idx], lv_color_hex(pct_col), 0);

    lv_bar_set_value(s_ams_card_bar[slot_idx], s->remain, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_ams_card_bar[slot_idx],
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
        lv_obj_add_flag(s_ams_row, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_ams_unit_lbl, "AMS");
        lv_label_set_text(s_ams_humid_lbl, "");
        lv_label_set_text(s_ams_temp_lbl,  "");
        lv_label_set_text(s_ams_dry_lbl,   "");
        return;
    }

    lv_obj_add_flag(s_ams_empty, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_ams_row, LV_OBJ_FLAG_HIDDEN);

    // Clamp visible index — ams_count can drop when a unit is unplugged live.
    if (s_ams_visible_index >= sel->ams_count) s_ams_visible_index = 0;
    const ::bambuddy::AmsUnit& u = sel->ams[s_ams_visible_index];

    // Header
    char hdr[24];
    if (sel->ams_count > 1)
        snprintf(hdr, sizeof(hdr), "AMS %d/%u%s",
                 s_ams_visible_index + 1, (unsigned)sel->ams_count,
                 u.is_ht ? " HT" : "");
    else
        snprintf(hdr, sizeof(hdr), "AMS%s", u.is_ht ? "-HT" : "");
    lv_label_set_text(s_ams_unit_lbl, hdr);

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
        char dbuf[20];
        if (u.dry_time_min >= 60)
            snprintf(dbuf, sizeof(dbuf), "Dry %luh%02lu",
                     (unsigned long)(u.dry_time_min / 60),
                     (unsigned long)(u.dry_time_min % 60));
        else
            snprintf(dbuf, sizeof(dbuf), "Dry %lum",
                     (unsigned long)u.dry_time_min);
        lv_label_set_text(s_ams_dry_lbl, dbuf);
    } else {
        lv_label_set_text(s_ams_dry_lbl, "");
    }

    // Slots — AMS regular has 4, AMS-HT has 1 (we hide the rest).
    uint8_t visible = u.slot_count > 0 ? u.slot_count : (u.is_ht ? 1 : 4);
    if (visible > 4) visible = 4;
    for (uint8_t i = 0; i < 4; ++i) {
        if (i < visible) {
            render_slot(i, i < u.slot_count ? &u.slots[i] : nullptr);
        } else {
            lv_obj_add_flag(s_ams_card[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// ---------- printer list ---------------------------------------------------

static lv_obj_t* s_pr_root = nullptr;
static lv_obj_t* s_pr_list = nullptr;

lv_obj_t* build_printers(lv_obj_t* parent) {
    ensure_styles();
    s_pr_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_pr_root);
    lv_obj_set_size(s_pr_root, LV_HOR_RES, LV_VER_RES - 36 - 18);
    lv_obj_align(s_pr_root, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_obj_clear_flag(s_pr_root, LV_OBJ_FLAG_SCROLLABLE);

    s_pr_list = lv_obj_create(s_pr_root);
    lv_obj_remove_style_all(s_pr_list);
    lv_obj_set_size(s_pr_list, LV_HOR_RES - 24, LV_VER_RES - 36 - 18 - 16);
    lv_obj_align(s_pr_list, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_flex_flow(s_pr_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_pr_list, 8, 0);
    return s_pr_root;
}

static void clear_children(lv_obj_t* o) {
    while (lv_obj_get_child_cnt(o) > 0) {
        lv_obj_t* c = lv_obj_get_child(o, 0);
        lv_obj_del(c);
    }
}

void update_printers(int selected_index) {
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
        lv_obj_t* row = lv_obj_create(s_pr_list);
        lv_obj_add_style(row, &s_panel, 0);
        if ((int)i == selected_index) {
            lv_obj_set_style_border_width(row, 2, 0);
            lv_obj_set_style_border_color(row, lv_color_hex(::ui::C_ACCENT), 0);
        }
        lv_obj_set_size(row, LV_PCT(100), 56);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* dot = lv_obj_create(row);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 12, 12);
        lv_obj_set_style_radius(dot, 6, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(state_color(ps[i].state)), 0);
        lv_obj_align(dot, LV_ALIGN_LEFT_MID, 4, 0);

        lv_obj_t* name = lv_label_create(row);
        lv_label_set_text(name, ps[i].name.c_str());
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 26, -10);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(name, lv_color_hex(::ui::C_TEXT), 0);

        lv_obj_t* sub = lv_label_create(row);
        String s = ps[i].model + "  " + state_name(ps[i].state);
        lv_label_set_text(sub, s.c_str());
        lv_obj_align(sub, LV_ALIGN_LEFT_MID, 26, 12);
        lv_obj_add_style(sub, &s_label_dim, 0);

        if (ps[i].state == ::bambuddy::PrinterState::Printing) {
            char pbuf[8];
            snprintf(pbuf, sizeof(pbuf), "%u%%", (unsigned)ps[i].progress);
            lv_obj_t* prog = lv_label_create(row);
            lv_label_set_text(prog, pbuf);
            lv_obj_align(prog, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_set_style_text_font(prog, &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_color(prog, lv_color_hex(::ui::C_ACCENT), 0);
        }
    }
}

// ---------- history --------------------------------------------------------

static lv_obj_t* s_hist_root      = nullptr;
static lv_obj_t* s_hist_stats     = nullptr;
static lv_obj_t* s_hist_list      = nullptr;

lv_obj_t* build_history(lv_obj_t* parent) {
    ensure_styles();
    s_hist_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_hist_root);
    lv_obj_set_size(s_hist_root, LV_HOR_RES, LV_VER_RES - 36 - 18);
    lv_obj_align(s_hist_root, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_obj_clear_flag(s_hist_root, LV_OBJ_FLAG_SCROLLABLE);

    s_hist_stats = lv_obj_create(s_hist_root);
    lv_obj_add_style(s_hist_stats, &s_panel, 0);
    lv_obj_set_size(s_hist_stats, LV_HOR_RES - 24, 64);
    lv_obj_align(s_hist_stats, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_clear_flag(s_hist_stats, LV_OBJ_FLAG_SCROLLABLE);

    s_hist_list = lv_obj_create(s_hist_root);
    lv_obj_remove_style_all(s_hist_list);
    lv_obj_set_size(s_hist_list, LV_HOR_RES - 24, LV_VER_RES - 36 - 18 - 88);
    lv_obj_align(s_hist_list, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_flex_flow(s_hist_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_hist_list, 6, 0);
    return s_hist_root;
}

void update_history() {
    maybe_hide_toast();
    // Stats bar
    clear_children(s_hist_stats);
    ::bambuddy::Stats st = ::bambuddy::g_client.snapshot_stats();

    auto add_kpi = [&](const char* k, const String& v, lv_color_t c, int x){
        lv_obj_t* kl = lv_label_create(s_hist_stats);
        lv_label_set_text(kl, k);
        lv_obj_add_style(kl, &s_label_dim, 0);
        lv_obj_align(kl, LV_ALIGN_TOP_LEFT, x, 0);

        lv_obj_t* vl = lv_label_create(s_hist_stats);
        lv_label_set_text(vl, v.c_str());
        lv_obj_align(vl, LV_ALIGN_TOP_LEFT, x, 18);
        lv_obj_set_style_text_font(vl, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(vl, c, 0);
    };
    add_kpi("PRINTS",  String(st.total_prints),
            lv_color_hex(::ui::C_TEXT), 0);
    add_kpi("SUCCESS", String(st.success_rate, 1) + "%",
            lv_color_hex(::ui::C_OK), 110);
    add_kpi("FILAMENT", String((int)(st.total_filament_g)) + " g",
            lv_color_hex(::ui::C_TEXT), 230);
    add_kpi("TIME",  String(st.total_time_s / 3600) + " h",
            lv_color_hex(::ui::C_TEXT), 360);

    // Recent list
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
        lv_obj_set_size(row, LV_PCT(100), 38);
        lv_obj_set_style_pad_all(row, 6, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* dot = lv_obj_create(row);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 10, 10);
        lv_obj_set_style_radius(dot, 5, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        uint32_t c = ::ui::C_TEXT_DIM;
        if (ar[i].status == "success")  c = ::ui::C_OK;
        else if (ar[i].status == "failed") c = ::ui::C_ERR;
        else if (ar[i].status == "stopped") c = ::ui::C_WARN;
        lv_obj_set_style_bg_color(dot, lv_color_hex(c), 0);
        lv_obj_align(dot, LV_ALIGN_LEFT_MID, 4, 0);

        lv_obj_t* nm = lv_label_create(row);
        lv_label_set_text(nm, ar[i].name.length() ? ar[i].name.c_str() : "(unnamed)");
        lv_obj_set_width(nm, 240);
        lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
        lv_obj_align(nm, LV_ALIGN_LEFT_MID, 22, 0);
        lv_obj_set_style_text_color(nm, lv_color_hex(::ui::C_TEXT), 0);

        char buf[24];
        snprintf(buf, sizeof(buf), "%lum • %.0fg",
                 (unsigned long)(ar[i].duration_s / 60), ar[i].filament_g);
        lv_obj_t* meta = lv_label_create(row);
        lv_label_set_text(meta, buf);
        lv_obj_align(meta, LV_ALIGN_RIGHT_MID, -8, 0);
        lv_obj_add_style(meta, &s_label_dim, 0);
    }
}

// ---------- settings -------------------------------------------------------

static lv_obj_t* s_set_root = nullptr;
static lv_obj_t* s_set_url  = nullptr;
static lv_obj_t* s_set_ip   = nullptr;
static lv_obj_t* s_set_rssi = nullptr;
static lv_obj_t* s_set_uptime = nullptr;
static lv_obj_t* s_set_hint = nullptr;

lv_obj_t* build_settings(lv_obj_t* parent) {
    ensure_styles();
    s_set_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_set_root);
    lv_obj_set_size(s_set_root, LV_HOR_RES, LV_VER_RES - 36 - 18);
    lv_obj_align(s_set_root, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_obj_clear_flag(s_set_root, LV_OBJ_FLAG_SCROLLABLE);

    auto make_row = [&](const char* k, int y) {
        lv_obj_t* kl = lv_label_create(s_set_root);
        lv_label_set_text(kl, k);
        lv_obj_add_style(kl, &s_label_dim, 0);
        lv_obj_align(kl, LV_ALIGN_TOP_LEFT, 18, y);

        lv_obj_t* vl = lv_label_create(s_set_root);
        lv_label_set_text(vl, "-");
        lv_obj_set_width(vl, LV_HOR_RES - 200);
        lv_label_set_long_mode(vl, LV_LABEL_LONG_DOT);
        lv_obj_align(vl, LV_ALIGN_TOP_LEFT, 160, y);
        lv_obj_set_style_text_color(vl, lv_color_hex(::ui::C_TEXT), 0);
        return vl;
    };

    s_set_url    = make_row("Bambuddy",   8);
    s_set_ip     = make_row("Local IP",  38);
    s_set_rssi   = make_row("Wi-Fi RSSI",68);
    s_set_uptime = make_row("Uptime",    98);

    s_set_hint = lv_label_create(s_set_root);
    lv_label_set_text(s_set_hint,
        LV_SYMBOL_SETTINGS "  Double-click OK on Live: quick actions\n"
        "          (speed / clear plate).\n"
        LV_SYMBOL_SETTINGS "  Long-press OK on Live: cycle print speed.\n"
        LV_SYMBOL_SETTINGS "  Long-press OK on Printers: full actions.\n"
        LV_SYMBOL_REFRESH "  Hold a button at boot to clear settings.");
    lv_obj_align(s_set_hint, LV_ALIGN_BOTTOM_LEFT, 18, -10);
    lv_obj_add_style(s_set_hint, &s_label_dim, 0);
    return s_set_root;
}

void update_settings() {
    maybe_hide_toast();
    extern String g_cfg_bambuddy_url;   // from main.cpp
    lv_label_set_text(s_set_url, g_cfg_bambuddy_url.length()
                                      ? g_cfg_bambuddy_url.c_str()
                                      : "(not configured)");
    lv_label_set_text(s_set_ip, WiFi.localIP().toString().c_str());
    char r[16];
    snprintf(r, sizeof(r), "%d dBm", WiFi.RSSI());
    lv_label_set_text(s_set_rssi, r);
    uint32_t up = millis() / 1000;
    char u[24];
    snprintf(u, sizeof(u), "%uh %02um %02us",
             (unsigned)(up / 3600), (unsigned)((up % 3600) / 60),
             (unsigned)(up % 60));
    lv_label_set_text(s_set_uptime, u);
}

}  // namespace ui::screens
