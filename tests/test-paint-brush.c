#include <stdint.h>
#include <stdio.h>

#include "extras/paint/paint_brush.h"

#define WIDTH 32
#define HEIGHT 24
#define WHITE 0x00ffffffU
#define INK 0x00123456U
#define GUARD 0x00abcdefU

static void clear(uint32_t *pixels)
{
    for (int i = 0; i < WIDTH * HEIGHT; i++) pixels[i] = WHITE;
}

static int ink_count(const uint32_t *pixels)
{
    int count = 0;

    for (int i = 0; i < WIDTH * HEIGHT; i++)
        if (pixels[i] == INK) count++;
    return count;
}

int main(void)
{
    uint32_t guarded[WIDTH * HEIGHT + 2];
    uint32_t *pixels = &guarded[1];
    int failures = 0;

    guarded[0] = GUARD;
    guarded[WIDTH * HEIGHT + 1] = GUARD;
    clear(pixels);
    paint_brush_stamp(pixels, WIDTH, HEIGHT, 12, 10, 1, INK);
    failures += ink_count(pixels) != 1;
    clear(pixels);
    paint_brush_stamp(pixels, WIDTH, HEIGHT, 12, 10, 3, INK);
    failures += ink_count(pixels) != 9;
    clear(pixels);
    paint_brush_stamp(pixels, WIDTH, HEIGHT, 12, 10, 5, INK);
    failures += ink_count(pixels) != 21;

    clear(pixels);
    paint_brush_stroke(pixels, WIDTH, HEIGHT, 2, 12, 28, 12, 1, INK);
    for (int x = 2; x <= 28; x++)
        failures += pixels[12 * WIDTH + x] != INK;
    clear(pixels);
    paint_brush_stroke(pixels, WIDTH, HEIGHT, 2, 2, 28, 20, 9, INK);
    for (int x = 2; x <= 28; x++) {
        int found = 0;

        for (int y = 0; y < HEIGHT; y++)
            if (pixels[y * WIDTH + x] == INK) found = 1;
        failures += !found;
    }

    clear(pixels);
    paint_brush_stamp(pixels, WIDTH, HEIGHT, 0, 0, 15, INK);
    failures += ink_count(pixels) == 0;
    failures += guarded[0] != GUARD ||
                guarded[WIDTH * HEIGHT + 1] != GUARD;
    failures += paint_brush_radius(1) != 0 ||
                paint_brush_radius(15) != 7;

    if (failures != 0) {
        fprintf(stderr, "FAIL paint brush tests: %d\n", failures);
        return 1;
    }
    puts("Paint brush tests passed");
    return 0;
}
