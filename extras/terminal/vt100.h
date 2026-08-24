#ifndef KYLEOS_VT100_H
#define KYLEOS_VT100_H

#include <stddef.h>
#include <stdint.h>

#define VT100_MAX_COLUMNS 128
#define VT100_MAX_LINES 256
#define VT100_MAX_SCREEN_ROWS 64

#define VT100_ATTR_BOLD 0x01
#define VT100_ATTR_UNDERLINE 0x02
#define VT100_ATTR_CONCEAL 0x04

struct vt100_cell {
    uint8_t character;
    uint8_t foreground;
    uint8_t background;
    uint8_t attributes;
};

struct vt100_terminal {
    struct vt100_cell *cells;
    int columns;
    int screen_rows;
    int line_count;
    int screen_top;
    int cursor_line;
    int cursor_column;
    int saved_row;
    int saved_column;
    uint8_t foreground;
    uint8_t background;
    uint8_t attributes;
    uint8_t inverse;
    uint8_t cursor_visible;
    uint8_t autowrap;
    uint8_t wrap_pending;
    uint8_t parser_state;
    uint8_t private_sequence;
    int parameters[16];
    int parameter_count;
};

int vt100_init(struct vt100_terminal *terminal, int columns, int rows);
void vt100_free(struct vt100_terminal *terminal);
void vt100_resize(struct vt100_terminal *terminal, int columns, int rows);
void vt100_reset(struct vt100_terminal *terminal);
void vt100_clear_all(struct vt100_terminal *terminal);
void vt100_write(struct vt100_terminal *terminal, const char *bytes,
                 int length);

const struct vt100_cell *vt100_cell_at(const struct vt100_terminal *terminal,
                                       int line, int column);
int vt100_line_count(const struct vt100_terminal *terminal);
int vt100_screen_top(const struct vt100_terminal *terminal);
int vt100_cursor_line(const struct vt100_terminal *terminal);
int vt100_cursor_column(const struct vt100_terminal *terminal);
int vt100_cursor_visible(const struct vt100_terminal *terminal);
uint8_t vt100_current_foreground(const struct vt100_terminal *terminal);
uint8_t vt100_current_background(const struct vt100_terminal *terminal);
uint8_t vt100_current_attributes(const struct vt100_terminal *terminal);
uint32_t vt100_palette_color(uint8_t color);

#endif
