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

    lv_style_init(&s_panel);
    lv_style_set_bg_color(&s_panel, lv_color_hex(::ui::C_PANEL_HI));
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
    lv_style_set_text_font(&s_label_dim, &lv_font_montserrat_12);

    lv_style_init(&s_label_value);
    lv_style_set_text_color(&s_label_value, lv_color_hex(::ui::C_TEXT));
    lv_style_set_text_font(&s_label_value, &lv_font_montserrat_20);

    lv_style_init(&s_label_big);
    lv_style_set_text_color(&s_label_big, lv_color_hex(::ui::C_TEXT));
    lv_style_set_text_font(&s_label_big, &lv_font_montserrat_28);

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
    lv_style_set_text_font(&s_btn, &lv_font_montserrat_16);

    // Accent variant — the "do the thing" primary action.
    lv_style_init(&s_btn_accent);
    lv_style_set_bg_color(&s_btn_accent, lv_color_hex(::ui::C_ACCENT));
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
    lv_style_set_text_font(&s_chip_off, &lv_font_montserrat_14);

    lv_style_init(&s_chip_on);
    lv_style_set_bg_color(&s_chip_on, lv_color_hex(::ui::C_ACCENT));
    lv_style_set_text_color(&s_chip_on, lv_color_hex(::ui::C_TEXT_INV));
    lv_style_set_text_font(&s_chip_on, &lv_font_montserrat_14);
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

uint32_t state_color(::bambuddy::PrinterState s) {
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
