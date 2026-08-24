#ifndef KYLEOS_DOOM_AQUA_UI_H
#define KYLEOS_DOOM_AQUA_UI_H

#include <stdint.h>

#include "extras/kui/kui.h"

unsigned char doom_aqua_convert_key(int code, int extended);
void doom_aqua_draw_frame(struct kui_surface *surface,
                          const uint32_t *pixels,
                          int source_width, int source_height);

#endif
