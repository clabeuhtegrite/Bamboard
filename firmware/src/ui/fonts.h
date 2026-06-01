// Bamboard UI fonts.
//
// These replace LVGL's built-in lv_font_montserrat_* (disabled in lv_conf.h).
// They're Montserrat-Medium regenerated with the Latin-1 Supplement range
// (0xA0-0xFF) so accented characters render for the French / Spanish / German
// / Portuguese translations, plus the 12 FontAwesome symbol glyphs the UI uses
// (LV_SYMBOL_OK / WARNING / TINT / WIFI / …). Generated with lv_font_conv from
// the same TTFs LVGL ships, so glyph shapes match the old built-ins.
//
// The bb_font_<size>.c sources live in fonts/ and are compiled by PlatformIO.

#pragma once

#include <lvgl.h>

LV_FONT_DECLARE(bb_font_12);
LV_FONT_DECLARE(bb_font_14);
LV_FONT_DECLARE(bb_font_16);
LV_FONT_DECLARE(bb_font_20);
LV_FONT_DECLARE(bb_font_28);
LV_FONT_DECLARE(bb_font_36);
