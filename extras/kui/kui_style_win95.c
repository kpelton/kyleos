#include "extras/kui/kui_style.h"

#include <string.h>

void kui_style_win95(struct kui_style *style)
{
    memset(style, 0, sizeof(*style));
    style->id = 1;
    style->face = 0x00ece9d8U;
    style->light = 0x00f1f5f9U;
    style->shadow = 0x00716686U;
    style->text = 0x00000000U;
    style->title_active = 0x000057c9U;
    style->title_inactive = 0x00657791U;
    style->selection = 0x00316ac5U;
    style->desktop = 0x002d79cfU;
    style->desktop_dark = 0x001e5aa8U;
    style->panel = 0x000057c9U;
    style->window_border = 4;
    style->title_height = 24;
    style->window_bottom = 4;
    style->panel_bottom = 32;
}
