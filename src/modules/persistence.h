#pragma once

#include <pebble.h>

#define PKEY_COLOR 0
#define PKEY_TEMP_UNIT 1
#define PKEY_UPDATE_INTERVAL 2

#define PVALUE_COLOR_THEME_BLUE 0
#define PVALUE_COLOR_THEME_RED 1
#define PVALUE_COLOR_THEME_GREEN 2
#define PVALUE_COLOR_THEME_BLACK 3

#define PVALUE_UPDATE_INTERVAL_MINUTE 0
#define PVALUE_UPDATE_INTERVAL_SECOND 1

GColor color_theme_to_background_color(int32_t);
GColor color_theme_to_foreground_color(int32_t);

TimeUnits update_interval_mask(int);
