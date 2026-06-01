// Bamboard screens — Settings (diagnostics + brightness + factory reset).
//
//   y =  36 ..  82 : 4 read-only rows (Bambuddy / IP / RSSI / Uptime)
//   y =  84 .. 116 : Brightness 1..5 segmented selector (panel + 5 chips)
//   y = bot - 52   : Factory-reset pill (two-tap confirm within 3 s)
//
// Declarations from main.cpp / the sim stubs:
//   g_cfg_bambuddy_url     — current Bambuddy URL string
//   g_cfg_brightness_level — currently-applied brightness level (1..5)
//   save_brightness_level  — persists + applies a new brightness level
//   factory_reset          — wipes NVS + reboots into captive portal

#include "theme.h"

#include <WiFi.h>

extern String  g_cfg_bambuddy_url;
extern uint8_t g_cfg_brightness_level;
extern void    save_brightness_level(uint8_t level);
extern void    factory_reset();

namespace ui::screens {

static lv_obj_t* s_set_root          = nullptr;
static lv_obj_t* s_set_url           = nullptr;
static lv_obj_t* s_set_ip            = nullptr;
static lv_obj_t* s_set_rssi          = nullptr;
static lv_obj_t* s_set_uptime        = nullptr;
static lv_obj_t* s_set_bright_bar    = nullptr;
static lv_obj_t* s_set_bright_seg[5] = {};
static lv_obj_t* s_set_reset_btn     = nullptr;
static lv_obj_t* s_set_reset_lbl     = nullptr;
static uint32_t  s_set_reset_armed_at = 0;

// ---- brightness segmented control ------------------------------------------

static void brightness_seg_clicked(lv_event_t* e) {
    uint8_t level = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    ::save_brightness_level(level);
    char buf[32];
    snprintf(buf, sizeof(buf), i18n::tr(i18n::Str::BRIGHTNESS_FMT), (unsigned)level);
    show_toast(buf, lv_color_hex(::ui::C_ACCENT));
}

static void brightness_apply(uint8_t level) {
    if (level < 1) level = 1;
    if (level > 5) level = 5;
    for (uint8_t i = 0; i < 5; ++i) {
        bool on = (i + 1 == level);
        lv_obj_set_style_bg_color(s_set_bright_seg[i],
                                   lv_color_hex(on ? ::ui::C_ACCENT
                                                   : ::ui::C_PANEL_HI), 0);
        lv_obj_t* lbl = lv_obj_get_child(s_set_bright_seg[i], 0);
        if (lbl) {
            lv_obj_set_style_text_color(lbl,
                lv_color_hex(on ? ::ui::C_TEXT_INV : ::ui::C_TEXT_DIM), 0);
        }
    }
}

// ---- factory reset ---------------------------------------------------------

static void set_reset_clicked(lv_event_t*) {
    uint32_t now = lv_tick_get();
    if (s_set_reset_armed_at != 0 &&
        (now - s_set_reset_armed_at) < 3000) {
        // Second tap within 3 s — go.
        show_toast(i18n::tr(i18n::Str::RESETTING), lv_color_hex(::ui::C_WARN));
        ::factory_reset();
    } else {
        // First tap — arm.
        s_set_reset_armed_at = now;
        lv_label_set_text(s_set_reset_lbl, i18n::tr(i18n::Str::CONFIRM_RESET));
        lv_obj_set_style_bg_color(s_set_reset_btn,
                                   lv_color_hex(::ui::C_ERR), 0);
    }
}

// ---- builder ---------------------------------------------------------------

lv_obj_t* build_settings(lv_obj_t* parent) {
    ensure_styles();
    s_set_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_set_root);
    lv_obj_set_size(s_set_root, LV_HOR_RES, body_h());
    lv_obj_align(s_set_root, LV_ALIGN_TOP_LEFT, 0, ::ui::HEADER_H);
    lv_obj_clear_flag(s_set_root, LV_OBJ_FLAG_SCROLLABLE);

    // A card panel sits behind the read-only diagnostics rows (the rows
    // themselves stay absolutely positioned and draw on top of it). Created
    // first so it lands underneath.
    lv_obj_t* diag_card = lv_obj_create(s_set_root);
    lv_obj_add_style(diag_card, &s_panel, 0);
    lv_obj_set_size(diag_card, LV_HOR_RES - 24, 80);
    lv_obj_align(diag_card, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(diag_card, LV_OBJ_FLAG_SCROLLABLE);

    auto make_row = [&](const char* k, int y) {
        lv_obj_t* kl = lv_label_create(s_set_root);
        lv_label_set_text(kl, k);
        lv_obj_add_style(kl, &s_label_dim, 0);
        lv_obj_align(kl, LV_ALIGN_TOP_LEFT, 18, y);

        lv_obj_t* vl = lv_label_create(s_set_root);
        lv_label_set_text(vl, "-");
        lv_obj_set_width(vl, LV_HOR_RES - 160);
        lv_label_set_long_mode(vl, LV_LABEL_LONG_DOT);
        lv_obj_align(vl, LV_ALIGN_TOP_LEFT, 130, y);
        lv_obj_set_style_text_color(vl, lv_color_hex(::ui::C_TEXT), 0);
        lv_obj_set_style_text_font(vl, &bb_font_14, 0);
        return vl;
    };

    s_set_url    = make_row("Bambuddy",    2);
    s_set_ip     = make_row(i18n::tr(i18n::Str::LOCAL_IP),   22);
    s_set_rssi   = make_row(i18n::tr(i18n::Str::WIFI_RSSI), 42);
    s_set_uptime = make_row(i18n::tr(i18n::Str::UPTIME),     62);

    // --- Brightness 1..5 selector ---------------------------------------
    // Identical visual language to the speed chip on Live so the user
    // recognises segmented controls as "pick one of N" without any other
    // affordance.
    lv_obj_t* bl_lbl = lv_label_create(s_set_root);
    lv_label_set_text(bl_lbl, i18n::tr(i18n::Str::BRIGHTNESS));
    lv_obj_add_style(bl_lbl, &s_label_dim, 0);
    lv_obj_align(bl_lbl, LV_ALIGN_TOP_LEFT, 18, 90);

    s_set_bright_bar = lv_obj_create(s_set_root);
    lv_obj_remove_style_all(s_set_bright_bar);
    lv_obj_align(s_set_bright_bar, LV_ALIGN_TOP_LEFT, 130, 84);
    lv_obj_set_size(s_set_bright_bar, LV_HOR_RES - 148, 32);
    lv_obj_set_style_bg_color(s_set_bright_bar,
                               lv_color_hex(::ui::C_PANEL_HI), 0);
    lv_obj_set_style_bg_opa  (s_set_bright_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius  (s_set_bright_bar, ::ui::R_PILL, 0);
    lv_obj_set_style_border_width(s_set_bright_bar, 1, 0);
    lv_obj_set_style_border_color(s_set_bright_bar,
                                  lv_color_hex(::ui::C_PANEL_LINE), 0);
    lv_obj_set_style_border_opa(s_set_bright_bar, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(s_set_bright_bar, 2, 0);
    lv_obj_clear_flag(s_set_bright_bar, LV_OBJ_FLAG_SCROLLABLE);

    int seg_w = (LV_HOR_RES - 148 - 4) / 5;
    for (uint8_t i = 0; i < 5; ++i) {
        lv_obj_t* seg = lv_btn_create(s_set_bright_bar);
        lv_obj_remove_style_all(seg);
        lv_obj_add_style(seg, &s_chip_off, 0);
        lv_obj_add_style(seg, &s_btn_pressed, LV_STATE_PRESSED);
        lv_obj_set_size(seg, seg_w, 28);
        lv_obj_set_pos (seg, i * seg_w, 0);
        lv_obj_add_event_cb(seg, brightness_seg_clicked, LV_EVENT_CLICKED,
                            (void*)(uintptr_t)(i + 1));
        lv_obj_t* l = lv_label_create(seg);
        char buf[4]; snprintf(buf, sizeof(buf), "%u", (unsigned)(i + 1));
        lv_label_set_text(l, buf);
        lv_obj_set_style_text_font(l, &bb_font_14, 0);
        lv_obj_center(l);
        s_set_bright_seg[i] = seg;
    }
    brightness_apply(::g_cfg_brightness_level);

    // --- Factory reset button (two-tap confirm) -------------------------
    s_set_reset_btn = lv_btn_create(s_set_root);
    lv_obj_remove_style_all(s_set_reset_btn);
    lv_obj_add_style(s_set_reset_btn, &s_btn, 0);
    lv_obj_add_style(s_set_reset_btn, &s_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_radius(s_set_reset_btn, ::ui::R_PILL, 0);
    lv_obj_set_size(s_set_reset_btn, LV_HOR_RES - 36, ::ui::H_BTN);
    lv_obj_align(s_set_reset_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(s_set_reset_btn,
                               lv_color_hex(::ui::C_PANEL_HI), 0);
    lv_obj_add_event_cb(s_set_reset_btn, set_reset_clicked, LV_EVENT_CLICKED, nullptr);

    s_set_reset_lbl = lv_label_create(s_set_reset_btn);
    lv_label_set_text(s_set_reset_lbl,
                      (String(LV_SYMBOL_TRASH "  ") +
                       i18n::tr(i18n::Str::FACTORY_RESET)).c_str());
    lv_obj_set_style_text_font(s_set_reset_lbl, &bb_font_16, 0);
    lv_obj_center(s_set_reset_lbl);

    // --- Firmware version line (just above the reset pill) --------------
    // Lets the user confirm at a glance which build is running — handy after
    // the boot-time OTA pulls a new release from GitHub.
    lv_obj_t* ver = lv_label_create(s_set_root);
    lv_label_set_text(ver,
                      (String(i18n::tr(i18n::Str::FIRMWARE)) +
                       BAMBOARD_VERSION).c_str());
    lv_obj_add_style(ver, &s_label_dim, 0);
    lv_obj_set_style_text_font(ver, &bb_font_14, 0);
    lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, -56);

    return s_set_root;
}

void update_settings() {
    maybe_hide_toast();
    lv_label_set_text(s_set_url, ::g_cfg_bambuddy_url.length()
                                      ? ::g_cfg_bambuddy_url.c_str()
                                      : i18n::tr(i18n::Str::NOT_CONFIGURED));
    // Format IPv4 by hand instead of allocating String via toString() — this
    // refresh runs on every UI tick and toString() would churn the heap.
    IPAddress ip = WiFi.localIP();
    char ipbuf[20];
    snprintf(ipbuf, sizeof(ipbuf), "%u.%u.%u.%u",
             (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
    lv_label_set_text(s_set_ip, ipbuf);
    char r[16];
    snprintf(r, sizeof(r), "%d dBm", WiFi.RSSI());
    lv_label_set_text(s_set_rssi, r);
    uint32_t up = millis() / 1000;
    char u[44];
    // Uptime + free heap on one line — the cheapest on-screen health signal
    // (a steadily shrinking heap points at a leak/fragmentation).
    snprintf(u, sizeof(u), "%uh %02um %02us \xC2\xB7 %uK free",
             (unsigned)(up / 3600), (unsigned)((up % 3600) / 60),
             (unsigned)(up % 60), (unsigned)(ESP.getFreeHeap() / 1024));
    lv_label_set_text(s_set_uptime, u);

    // Keep the segmented brightness control in sync with the live value
    // (the captive portal could rewrite it; we never do that today, but
    // it's free to keep the binding bidirectional).
    brightness_apply(::g_cfg_brightness_level);

    // Reset the "tap again to confirm" arm window after 3 s.
    if (s_set_reset_armed_at != 0 &&
        (lv_tick_get() - s_set_reset_armed_at) > 3000) {
        s_set_reset_armed_at = 0;
        char rbuf[48];
        snprintf(rbuf, sizeof(rbuf), LV_SYMBOL_TRASH "  %s",
                 i18n::tr(i18n::Str::FACTORY_RESET));
        lv_label_set_text(s_set_reset_lbl, rbuf);
        lv_obj_set_style_bg_color(s_set_reset_btn,
                                   lv_color_hex(::ui::C_PANEL_HI), 0);
    }
}

}  // namespace ui::screens
