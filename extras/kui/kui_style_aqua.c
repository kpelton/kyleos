#include "extras/kui/kui_style.h"

#include <string.h>

void kui_style_aqua(struct kui_style *style)
{
    memset(style, 0, sizeof(*style));
    style->id = 2;
    style->flags = KUI_STYLE_AQUA;
    style->face = 0x00e8e8e8U;
    style->light = 0x00f7f7f7U;
    style->shadow = 0x00797979U;
    style->text = 0x00151515U;
    style->title_active = 0x00a9c8eeU;
    style->title_inactive = 0x00bcbcbcU;
    style->selection = 0x003e80d6U;
    style->desktop = 0x00367fc1U;
    style->desktop_dark = 0x001b4f86U;
    style->panel = 0x00f2f2f2U;
    style->window_border = 3;
    style->title_height = 25;
    style->window_bottom = 3;
    style->panel_top = 24;
    style->panel_bottom = 64;
}
