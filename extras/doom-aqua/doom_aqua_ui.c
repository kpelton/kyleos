#include "extras/doom-aqua/doom_aqua_ui.h"

#include "extras/doom/doomgeneric/doomkeys.h"

#include <stddef.h>

unsigned char doom_aqua_convert_key(int code, int extended)
{
    if (extended) {
        if (code == 72) return KEY_UPARROW;
        if (code == 80) return KEY_DOWNARROW;
        if (code == 75) return KEY_LEFTARROW;
        if (code == 77) return KEY_RIGHTARROW;
        if (code == 29) return KEY_FIRE;
        if (code == 56) return KEY_LALT;
        return 0;
    }

    switch (code) {
    case 1: return KEY_ESCAPE;
    case 2: return '1'; case 3: return '2'; case 4: return '3';
    case 5: return '4'; case 6: return '5'; case 7: return '6';
    case 8: return '7'; case 9: return '8'; case 10: return '9';
    case 11: return '0'; case 12: return '-'; case 13: return '=';
    case 14: return KEY_BACKSPACE;
    case 15: return KEY_TAB;
    case 16: return 'q'; case 17: return 'w'; case 18: return 'e';
    case 19: return 'r'; case 20: return 't'; case 21: return 'y';
    case 22: return 'u'; case 23: return 'i'; case 24: return 'o';
    case 25: return 'p'; case 26: return '['; case 27: return ']';
    case 28: return KEY_ENTER;
    case 29: return KEY_FIRE;
    case 30: return 'a'; case 31: return 's'; case 32: return 'd';
    case 33: return 'f'; case 34: return 'g'; case 35: return 'h';
    case 36: return 'j'; case 37: return 'k'; case 38: return 'l';
    case 39: return ';'; case 40: return '\''; case 41: return '`';
    case 42: case 54: return KEY_RSHIFT;
    case 43: return '\\';
    case 44: return 'z'; case 45: return 'x'; case 46: return 'c';
    case 47: return 'v'; case 48: return 'b'; case 49: return 'n';
    case 50: return 'm'; case 51: return ','; case 52: return '.';
    case 53: return '/';
    case 56: return KEY_LALT;
    case 57: return KEY_USE;
    case 59: return KEY_F1; case 60: return KEY_F2;
    case 61: return KEY_F3; case 62: return KEY_F4;
    case 63: return KEY_F5; case 64: return KEY_F6;
    case 65: return KEY_F7; case 66: return KEY_F8;
    case 67: return KEY_F9; case 68: return KEY_F10;
    case 87: return KEY_F11; case 88: return KEY_F12;
    default: return 0;
    }
}

void doom_aqua_draw_frame(struct kui_surface *surface,
                          const uint32_t *pixels,
                          int source_width, int source_height)
{
    int draw_width;
    int draw_height;
    int x;
    int y;

    if (surface == NULL || pixels == NULL || surface->width <= 0 ||
        surface->height <= 0 || source_width <= 0 || source_height <= 0)
        return;
    draw_width = surface->width;
    draw_height = draw_width * source_height / source_width;
    if (draw_height > surface->height) {
        draw_height = surface->height;
        draw_width = draw_height * source_width / source_height;
    }
    x = (surface->width - draw_width) / 2;
    y = (surface->height - draw_height) / 2;
    kui_fill_rect(surface, 0, 0, surface->width, surface->height, KUI_BLACK);
    kui_draw_bitmap_scaled(surface, pixels, source_width, source_height,
                           x, y, draw_width, draw_height);
}
