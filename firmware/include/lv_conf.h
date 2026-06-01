/**
 * lv_conf.h — LVGL configuration for Bamboard
 *
 * Trimmed-down version of the LVGL v8.3 default conf. Only what Bamboard
 * actually needs is enabled, to keep flash use sensible on the 4 MB
 * partitions and leave headroom in internal SRAM (the panel draw buffers
 * live in PSRAM — see hw/display.cpp — but LVGL's object tree, the
 * LV_MEM_SIZE pool below, stays in DRAM).
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* -------- Colour depth -------- */
#define LV_COLOR_DEPTH 16        /* RGB565 — LovyanGFX drives the JC4827W543 panel in 16-bit */
#define LV_COLOR_16_SWAP 0       /* RGB parallel bus, no byte swap needed */

/* -------- Memory -------- */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (64U * 1024U)   /* PSRAM-friendly; S3 has plenty */

/* -------- HAL settings -------- */
#define LV_DISP_DEF_REFR_PERIOD 16   /* ~60 fps cap */
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* -------- Feature usage -------- */
#define LV_USE_LOG 0
/* Asserts panic() the device on null/OOM. Useful while iterating on the UI,
 * but a wall-mounted device with no serial console shouldn't reboot-loop on
 * a transient malloc failure — gate them behind the dev sentinel so only
 * unflashed-from-CI builds keep them on. The CI tag-build path defines
 * BAMBOARD_FW_VERSION, which flips BAMBOARD_VERSION_IS_DEV to 0 (see
 * src/config.h).  */
#if defined(BAMBOARD_FW_VERSION)
#  define LV_USE_ASSERT_NULL        0
#  define LV_USE_ASSERT_MALLOC      0
#else
#  define LV_USE_ASSERT_NULL        1
#  define LV_USE_ASSERT_MALLOC      1
#endif

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

/* -------- Widgets -------- */
#define LV_USE_ARC      1
#define LV_USE_BAR      1
#define LV_USE_BTN      1
#define LV_USE_BTNMATRIX 1
#define LV_USE_CANVAS   0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_IMG      1
#define LV_USE_LABEL    1
#define LV_LABEL_TEXT_SELECTION 0
#define LV_LABEL_LONG_TXT_HINT  1
#define LV_USE_LINE     1
#define LV_USE_ROLLER   0
#define LV_USE_SLIDER   1
#define LV_USE_SWITCH   1
#define LV_USE_TEXTAREA 0
#define LV_USE_TABLE    1

/* -------- Extra widgets -------- */
/* Anything we don't explicitly use is forced off here. LVGL otherwise
 * picks "1" as the default for every widget in this section, and a
 * couple of them (lv_keyboard) require sibling widgets that we also
 * disabled (lv_textarea) — the build then fatal-errors out of the
 * header. Be explicit to avoid surprises on future LVGL bumps. */
#define LV_USE_ANIMIMG     0
#define LV_USE_CALENDAR    0
#define LV_USE_CHART       0
#define LV_USE_COLORWHEEL  0
#define LV_USE_IMGBTN      0
#define LV_USE_KEYBOARD    0   /* would require LV_USE_TEXTAREA = 1 */
#define LV_USE_LED         0
#define LV_USE_LIST        1
#define LV_USE_MENU        1
#define LV_USE_METER       0
#define LV_USE_MSGBOX      0
#define LV_USE_SPAN        0
#define LV_USE_SPINBOX     0
#define LV_USE_SPINNER     1
#define LV_USE_TABVIEW     0
#define LV_USE_TILEVIEW    0
#define LV_USE_WIN         0

/* -------- Themes -------- */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

/* -------- Fonts -------- */
// Built-in Montserrat fonts are disabled: Bamboard ships its own bb_font_*
// (Montserrat + Latin-1 accents + the FontAwesome symbols the UI uses) so the
// i18n strings render with accents. See firmware/src/ui/fonts.h.
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_48 0
#define LV_FONT_CUSTOM_DECLARE \
    LV_FONT_DECLARE(bb_font_12) LV_FONT_DECLARE(bb_font_14) \
    LV_FONT_DECLARE(bb_font_16) LV_FONT_DECLARE(bb_font_20) \
    LV_FONT_DECLARE(bb_font_28) LV_FONT_DECLARE(bb_font_36)
#define LV_FONT_DEFAULT &bb_font_16

/* -------- Text -------- */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

#endif /* LV_CONF_H */
