// Shared theme definitions for the ui::screens namespace.
// See theme.h for the rationale.

#include "theme.h"

namespace ui::screens {

// ---------- Shared styles ---------------------------------------------------

lv_style_t s_panel;
lv_style_t s_panel_flat;
lv_style_t s_label_dim;
lv_style_t s_label_value;
lv_style_t s_label_big;
lv_style_t s_btn;
lv_style_t s_btn_accent;
lv_style_t s_btn_pressed;
lv_style_t s_chip_off;
lv_style_t s_chip_on;

static bool s_styles_ready = false;

void ensure_styles() {
    if (s_styles_ready) return;
    s_styles_ready = true;

    // v0.10: cards carry a subtle vertical gradient (lighter top → darker
    // bottom) + a crisp hairline for depth. No blur shadow — it would tax the
    // ESP32 frame rate once a dozen cards are on screen at once.
    lv_style_init(&s_panel);
    lv_style_set_bg_color(&s_panel, lv_color_hex(::ui::C_PANEL_GRAD_TOP));
    lv_style_set_bg_grad_color(&s_panel, lv_color_hex(::ui::C_PANEL_GRAD_BOT));
    lv_style_set_bg_grad_dir(&s_panel, LV_GRAD_DIR_VER);
    lv_style_set_bg_opa(&s_panel, LV_OPA_COVER);
    lv_style_set_border_width(&s_panel, 1);
    lv_style_set_border_color(&s_panel, lv_color_hex(::ui::C_PANEL_LINE));
    lv_style_set_border_opa(&s_panel, LV_OPA_60);
    lv_style_set_radius(&s_panel, ::ui::R_PANEL);
    lv_style_set_pad_all(&s_panel, 8);

    lv_style_init(&s_panel_flat);
    lv_style_set_bg_color(&s_panel_flat, lv_color_hex(::ui::C_PANEL));
    lv_style_set_bg_opa(&s_panel_flat, LV_OPA_COVER);
    lv_style_set_border_width(&s_panel_flat, 0);
    lv_style_set_radius(&s_panel_flat, ::ui::R_PANEL);
    lv_style_set_pad_all(&s_panel_flat, 8);

    lv_style_init(&s_label_dim);
    lv_style_set_text_color(&s_label_dim, lv_color_hex(::ui::C_TEXT_DIM));
    lv_style_set_text_font(&s_label_dim, &bb_font_12);

    lv_style_init(&s_label_value);
    lv_style_set_text_color(&s_label_value, lv_color_hex(::ui::C_TEXT));
    lv_style_set_text_font(&s_label_value, &bb_font_20);

    lv_style_init(&s_label_big);
    lv_style_set_text_color(&s_label_big, lv_color_hex(::ui::C_TEXT));
    lv_style_set_text_font(&s_label_big, &bb_font_28);

    // Base touch button — neutral pill, used for secondary actions like
    // AMS prev/next chevrons or the Factory-reset row.
    lv_style_init(&s_btn);
    lv_style_set_bg_color(&s_btn, lv_color_hex(::ui::C_PANEL_HI));
    lv_style_set_bg_opa(&s_btn, LV_OPA_COVER);
    lv_style_set_radius(&s_btn, ::ui::R_BUTTON);
    lv_style_set_border_width(&s_btn, 1);
    lv_style_set_border_color(&s_btn, lv_color_hex(::ui::C_PANEL_LINE));
    lv_style_set_border_opa(&s_btn, LV_OPA_80);
    lv_style_set_pad_all(&s_btn, 8);
    lv_style_set_text_color(&s_btn, lv_color_hex(::ui::C_TEXT));
    lv_style_set_text_font(&s_btn, &bb_font_16);

    // Accent variant — the "do the thing" primary action. Vertical accent
    // gradient gives it a little dimensionality vs the flat chips.
    lv_style_init(&s_btn_accent);
    lv_style_set_bg_color(&s_btn_accent, lv_color_hex(::ui::C_ACCENT));
    lv_style_set_bg_grad_color(&s_btn_accent, lv_color_hex(::ui::C_ACCENT_DARK));
    lv_style_set_bg_grad_dir(&s_btn_accent, LV_GRAD_DIR_VER);
    lv_style_set_border_color(&s_btn_accent,
                              lv_color_hex(::ui::C_ACCENT_DARK));
    lv_style_set_border_opa(&s_btn_accent, LV_OPA_COVER);
    lv_style_set_text_color(&s_btn_accent, lv_color_hex(::ui::C_TEXT_INV));

    // Shared press state — every button gets this so the tap feedback
    // looks the same everywhere.
    lv_style_init(&s_btn_pressed);
    lv_style_set_bg_color(&s_btn_pressed, lv_color_hex(::ui::C_ACCENT_DARK));
    lv_style_set_transform_height(&s_btn_pressed, -1);
    lv_style_set_transform_width (&s_btn_pressed, -1);

    // Segmented-control chips. We use these for the speed selector on
    // Live and the brightness selector on Settings — same look in both
    // places by design.
    lv_style_init(&s_chip_off);
    lv_style_set_bg_color(&s_chip_off, lv_color_hex(::ui::C_PANEL_HI));
    lv_style_set_bg_opa(&s_chip_off, LV_OPA_COVER);
    lv_style_set_border_width(&s_chip_off, 0);
    lv_style_set_radius(&s_chip_off, ::ui::R_CHIP);
    lv_style_set_pad_all(&s_chip_off, 0);
    lv_style_set_text_color(&s_chip_off, lv_color_hex(::ui::C_TEXT_DIM));
    lv_style_set_text_font(&s_chip_off, &bb_font_14);

    lv_style_init(&s_chip_on);
    lv_style_set_bg_color(&s_chip_on, lv_color_hex(::ui::C_ACCENT));
    lv_style_set_bg_grad_color(&s_chip_on, lv_color_hex(::ui::C_ACCENT_DARK));
    lv_style_set_bg_grad_dir(&s_chip_on, LV_GRAD_DIR_VER);
    lv_style_set_text_color(&s_chip_on, lv_color_hex(::ui::C_TEXT_INV));
    lv_style_set_text_font(&s_chip_on, &bb_font_14);
}

// ---------- Tiny widget tree helpers ---------------------------------------

void clear_children(lv_obj_t* o) {
    while (lv_obj_get_child_cnt(o) > 0) {
        lv_obj_t* c = lv_obj_get_child(o, 0);
        lv_obj_del(c);
    }
}

// ---------- Printer-state cosmetics ----------------------------------------

const char* state_name(::bambuddy::PrinterState s) {
    using PState = ::bambuddy::PrinterState;
    switch (s) {
        case PState::Idle:     return i18n::tr(i18n::Str::STATE_IDLE);
        case PState::Prepare:  return i18n::tr(i18n::Str::STATE_PREPARE);
        case PState::Printing: return i18n::tr(i18n::Str::STATE_PRINTING);
        case PState::Paused:   return i18n::tr(i18n::Str::STATE_PAUSED);
        case PState::Finish:   return i18n::tr(i18n::Str::STATE_FINISH);
        case PState::Failed:   return i18n::tr(i18n::Str::STATE_FAILED);
        case PState::Offline:  return i18n::tr(i18n::Str::STATE_OFFLINE);
        case PState::Error:    return i18n::tr(i18n::Str::STATE_ERROR);
        default:           return i18n::tr(i18n::Str::STATE_UNKNOWN);
    }
}

uint32_t state_color(::bambuddy::PrinterState s) {
    using PState = ::bambuddy::PrinterState;
    switch (s) {
        case PState::Printing: return ::ui::C_ACCENT;
        case PState::Paused:   return ::ui::C_WARN;
        case PState::Finish:   return ::ui::C_OK;
        case PState::Failed:
        case PState::Error:    return ::ui::C_ERR;
        case PState::Offline:  return ::ui::C_TEXT_DIM;
        default:           return ::ui::C_TEXT_DIM;
    }
}

const char* speed_mode_name(uint8_t mode) {
    switch (mode) {
        case 1: return "Silent";
        case 2: return "Standard";
        case 3: return "Sport";
        case 4: return "Ludicrous";
        default: return "?";
    }
}

}  // namespace ui::screens
