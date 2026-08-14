#ifndef KYLEOS_KUI_H
#define KYLEOS_KUI_H

#include <stdint.h>

#include "extras/kui/kui_protocol.h"

#define KUI_SCREEN_WIDTH 1280
#define KUI_SCREEN_HEIGHT 720

#define KUI_BLACK 0x00000000U
#define KUI_WHITE 0x00ffffffU
#define KUI_LIGHT 0x00f1f5f9U
#define KUI_FACE 0x00ece9d8U
#define KUI_SHADOW 0x00716686U
#define KUI_TITLE_BLUE 0x000057c9U
#define KUI_DESKTOP_TEAL 0x002d79cfU
#define KUI_DESKTOP_DARK 0x001e5aa8U

#define KUI_KIMG_FLAG_OPAQUE 1U

struct kui_surface {
    uint32_t *pixels;
    int width;
    int height;
    int stride;
    struct kui_command_buffer *commands;
};

struct kui_command_buffer {
    unsigned char *data;
    uint32_t length;
    uint32_t capacity;
    int failed;
};

struct kui_image {
    uint32_t *pixels;
    int width;
    int height;
    int opaque;
};

void kui_surface_init(struct kui_surface *surface, uint32_t *pixels,
                      int width, int height);
void kui_command_buffer_init(struct kui_command_buffer *buffer);
void kui_command_buffer_reset(struct kui_command_buffer *buffer);
void kui_command_buffer_free(struct kui_command_buffer *buffer);
void kui_surface_init_commands(struct kui_surface *surface,
                               struct kui_command_buffer *buffer,
                               int width, int height);
int kui_display_replay(struct kui_surface *surface,
                       const void *commands, uint32_t bytes);
int kui_read_all(int fd, void *buffer, uint32_t bytes);
int kui_write_all(int fd, const void *buffer, uint32_t bytes);

int kui_present(const struct kui_surface *surface);
int kui_present_rect(const struct kui_surface *surface,
                     int x, int y, int width, int height);
int kui_screen_size(int *width, int *height);
int kui_key_poll(int *pressed, uint8_t *scancode, uint8_t *extended);
void kui_tty_raw(int enabled);
int kui_mouse_poll(int *dx, int *dy, uint8_t *buttons);
int kui_mouse_poll_wheel(int *dx, int *dy, uint8_t *buttons, int *wheel);
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

int kui_image_load(const char *path, struct kui_image *image);
void kui_image_free(struct kui_image *image);
void kui_blit_image(struct kui_surface *surface,
                    const struct kui_image *image, int x, int y);
void kui_blit_image_scaled(struct kui_surface *surface,
                           const struct kui_image *image,
                           int x, int y, int width, int height);
void kui_draw_bitmap_scaled(struct kui_surface *surface,
                            const uint32_t *pixels,
                            int source_width, int source_height,
                            int x, int y, int width, int height);
void kui_draw_bitmap_region_scaled(struct kui_surface *surface,
                                   const uint32_t *pixels,
                                   int source_width, int source_height,
                                   int source_x, int source_y,
                                   int region_width, int region_height,
                                   int x, int y, int width, int height);

void kui_draw_desktop(struct kui_surface *surface, int menu_height);
void kui_draw_bevel(struct kui_surface *surface, int x, int y, int w, int h,
                    uint32_t face, int pressed);
void kui_draw_button(struct kui_surface *surface, int x, int y, int w, int h,
                     const char *label, int pressed);
void kui_draw_window(struct kui_surface *surface, int x, int y, int w, int h,
                     const char *title, int active);

#endif
