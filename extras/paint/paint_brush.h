#ifndef KYLEOS_PAINT_BRUSH_H
#define KYLEOS_PAINT_BRUSH_H

#include <stdint.h>

int paint_brush_radius(int size);
void paint_brush_stamp(uint32_t *pixels, int width, int height,
                       int x, int y, int size, uint32_t color);
void paint_brush_stroke(uint32_t *pixels, int width, int height,
                        int x0, int y0, int x1, int y1,
                        int size, uint32_t color);

#endif
