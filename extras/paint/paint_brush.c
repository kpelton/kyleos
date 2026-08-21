#include "extras/paint/paint_brush.h"

int paint_brush_radius(int size)
{
    if (size < 1) size = 1;
    if (size > 31) size = 31;
    return size / 2;
}

void paint_brush_stamp(uint32_t *pixels, int width, int height,
                       int x, int y, int size, uint32_t color)
{
    int radius;
    int limit;

    if (pixels == 0 || width <= 0 || height <= 0) return;
    if (size < 1) size = 1;
    if (size > 31) size = 31;
    radius = paint_brush_radius(size);
    limit = size * size;
    for (int row = y - radius; row <= y + radius; row++)
        for (int column = x - radius; column <= x + radius; column++) {
            int dx = column - x;
            int dy = row - y;

            if ((unsigned)column < (unsigned)width &&
                (unsigned)row < (unsigned)height &&
                4 * (dx * dx + dy * dy) <= limit)
                pixels[row * width + column] = color;
        }
}

void paint_brush_stroke(uint32_t *pixels, int width, int height,
                        int x0, int y0, int x1, int y1,
                        int size, uint32_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        int twice;

        paint_brush_stamp(pixels, width, height, x0, y0, size, color);
        if (x0 == x1 && y0 == y1) break;
        twice = error * 2;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}
