#include "persistence.h"

GColor color_theme_to_background_color(int32_t theme)
{
    switch (theme) {
        case PVALUE_COLOR_THEME_BLACK:
            return GColorBlack;
        case PVALUE_COLOR_THEME_GREEN:
            return GColorDarkGreen;
        case PVALUE_COLOR_THEME_RED:
            return GColorDarkCandyAppleRed;
        case PVALUE_COLOR_THEME_BLUE:
        default:
            return GColorOxfordBlue;
            break;
    }
}

GColor color_theme_to_foreground_color(int32_t theme)
{
    switch (theme) {
        case PVALUE_COLOR_THEME_BLACK:
            return GColorWhite;
        case PVALUE_COLOR_THEME_GREEN:
            return GColorMintGreen;
        case PVALUE_COLOR_THEME_RED:
            return GColorMelon;
        case PVALUE_COLOR_THEME_BLUE:
        default:
            return GColorPictonBlue;
    }
}

TimeUnits update_interval_mask(int ui) {
    switch (ui) {
        case PVALUE_UPDATE_INTERVAL_SECOND:
            return SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT | MONTH_UNIT | YEAR_UNIT;
        case PVALUE_UPDATE_INTERVAL_MINUTE:
        default:
            return MINUTE_UNIT | HOUR_UNIT | DAY_UNIT | MONTH_UNIT | YEAR_UNIT;
    }
}
