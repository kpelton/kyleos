#ifndef KYLEOS_KUI_H
#define KYLEOS_KUI_H

#include <stdint.h>

#define KUI_SCREEN_WIDTH 1280
#define KUI_SCREEN_HEIGHT 720

#define KUI_BLACK 0x00000000U
#define KUI_WHITE 0x00ffffffU
#define KUI_LIGHT 0x00dfdfdfU
#define KUI_FACE 0x00c0c0c0U
#define KUI_SHADOW 0x00808080U
#define KUI_TITLE_BLUE 0x00000080U
#define KUI_DESKTOP_TEAL 0x00008080U
#define KUI_DESKTOP_DARK 0x00006060U

struct kui_surface {
    uint32_t *pixels;
    int width;
    int height;
    int stride;
};

void kui_surface_init(struct kui_surface *surface, uint32_t *pixels,
                      int width, int height);

int kui_present(const struct kui_surface *surface);
int kui_screen_size(int *width, int *height);
int kui_key_poll(int *pressed, uint8_t *scancode, uint8_t *extended);
void kui_tty_raw(int enabled);
int kui_mouse_poll(int *dx, int *dy, uint8_t *buttons);
uint32_t kui_ticks_ms(void);
void kui_sleep_ms(uint32_t milliseconds);

int kui_pointer_delta(int delta);
char kui_scan_to_char(uint8_t scan, int shift);
int kui_in_rect(int px, int py, int x, int y, int w, int h);

void kui_pixel(struct kui_surface *surface, int x, int y, uint32_t color);
void kui_fill_rect(struct kui_surface *surface, int x, int y, int w, int h,
                   uint32_t color);
void kui_outline_rect(struct kui_surface *surface, int x, int y, int w, int h,
                      uint32_t color);
void kui_line(struct kui_surface *surface, int x0, int y0, int x1, int y1,
              uint32_t color);
void kui_draw_char(struct kui_surface *surface, int x, int y,
                   unsigned char ch, uint32_t foreground,
                   uint32_t background, int opaque);
void kui_draw_text(struct kui_surface *surface, int x, int y,
                   const char *text, uint32_t foreground,
                   uint32_t background, int opaque);
int kui_text_width(const char *text);

void kui_draw_desktop(struct kui_surface *surface, int menu_height);
void kui_draw_bevel(struct kui_surface *surface, int x, int y, int w, int h,
                    uint32_t face, int pressed);
void kui_draw_button(struct kui_surface *surface, int x, int y, int w, int h,
                     const char *label, int pressed);
void kui_draw_window(struct kui_surface *surface, int x, int y, int w, int h,
                     const char *title, int active);

#endif
