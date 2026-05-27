/**
 * lv_conf.h — LVGL configuration for Bamboard
 *
 * Trimmed-down version of the LVGL v8.3 default conf. Only what Bamboard
 * actually needs is enabled, to keep flash and RAM use sensible on the C3-
 * sized SRAM budget (although we run on S3, this keeps the firmware lean).
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* -------- Colour depth -------- */
#define LV_COLOR_DEPTH 16        /* ILI9488 driven in 16-bit via TFT_eSPI */
#define LV_COLOR_16_SWAP 1

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
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1

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
#define LV_USE_LIST     1
#define LV_USE_MENU     1
#define LV_USE_MSGBOX   1
#define LV_USE_SPINNER  1
#define LV_USE_TABVIEW  0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN      0

/* -------- Themes -------- */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

/* -------- Fonts -------- */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* -------- Text -------- */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

#endif /* LV_CONF_H */
