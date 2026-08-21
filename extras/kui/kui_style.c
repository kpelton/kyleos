#include "extras/kui/kui_style.h"

#include <string.h>

static struct kui_style current;
static int initialized;

const struct kui_style *kui_style_current(void)
{
    if (!initialized) {
        kui_style_win95(&current);
        initialized = 1;
    }
    return &current;
}

void kui_style_apply(const struct kui_style *style)
{
    if (style == NULL)
        return;
    current = *style;
    initialized = 1;
}
