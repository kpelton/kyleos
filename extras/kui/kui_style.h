#ifndef KYLEOS_KUI_STYLE_H
#define KYLEOS_KUI_STYLE_H

#include <stdint.h>

#define KUI_STYLE_AQUA 1U

struct kui_style {
    uint32_t id;
    uint32_t flags;
    uint32_t face;
    uint32_t light;
    uint32_t shadow;
    uint32_t text;
    uint32_t title_active;
    uint32_t title_inactive;
    uint32_t selection;
    uint32_t desktop;
    uint32_t desktop_dark;
    uint32_t panel;
    int window_border;
    int title_height;
    int window_bottom;
    int panel_top;
    int panel_bottom;
};

const struct kui_style *kui_style_current(void);
void kui_style_apply(const struct kui_style *style);
void kui_style_win95(struct kui_style *style);
void kui_style_aqua(struct kui_style *style);

#endif
