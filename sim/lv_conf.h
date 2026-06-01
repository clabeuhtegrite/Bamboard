/* lv_conf.h — host simulator build of LVGL 8.3 for Bamboard.
 *
 * Mirrors the firmware's settings (RGB565, the widgets the UI uses, flex) but
 * sized for a host: bigger LV_MEM, a built-in default font, asserts off. The
 * UI sets bb_font_* explicitly per widget, so the default font is only a
 * fallback. Tick comes from the Arduino shim's millis(). */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (256U * 1024U)

#define LV_DISP_DEF_REFR_PERIOD 16
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL   0
#define LV_USE_ASSERT_MALLOC 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

/* Widgets the Bamboard UI actually instantiates. */
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
#define LV_USE_SLIDER   0
#define LV_USE_SWITCH   0
#define LV_USE_TABLE    0
/* TEXTAREA on: LVGL's bundled extra widgets (keyboard/spinbox) hard-require it
 * to compile even though Bamboard never instantiates them. No render impact. */
#define LV_USE_TEXTAREA 1

#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/* Fonts: provide a built-in default; the UI overrides with bb_font_* anyway. */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

#endif /* LV_CONF_H */
