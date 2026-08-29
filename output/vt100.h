#ifndef KYLEOS_KERNEL_VT100_H
#define KYLEOS_KERNEL_VT100_H

#include <include/types.h>

#define KVT_COLUMNS 160
#define KVT_ROWS 45

#define KVT_ATTR_BOLD      0x01
#define KVT_ATTR_UNDERLINE 0x02
#define KVT_ATTR_CONCEAL   0x04

struct kvt_cell {
    uint8_t character;
    uint8_t foreground;
    uint8_t background;
    uint8_t attributes;
};

struct kvt_terminal {
    struct kvt_cell cells[KVT_ROWS][KVT_COLUMNS];
    struct kvt_cell saved_cells[KVT_ROWS][KVT_COLUMNS];
    int cursor_row;
    int cursor_column;
    int saved_row;
    int saved_column;
    uint8_t foreground;
    uint8_t background;
    uint8_t attributes;
    bool inverse;
    bool cursor_visible;
    bool autowrap;
    bool wrap_pending;
    uint8_t parser_state;
    bool private_sequence;
    int parameters[16];
    int parameter_count;
    bool alternate_active;
    int primary_cursor_row;
    int primary_cursor_column;
    int primary_saved_row;
    int primary_saved_column;
    uint8_t primary_foreground;
    uint8_t primary_background;
    uint8_t primary_attributes;
    bool primary_inverse;
    bool primary_cursor_visible;
    bool primary_autowrap;
    bool dirty_rows[KVT_ROWS];
};

void kvt_init(struct kvt_terminal *terminal);
void kvt_clear(struct kvt_terminal *terminal);
void kvt_write(struct kvt_terminal *terminal, const char *bytes,
               uint32_t length);
uint32_t kvt_palette_color(uint8_t color);

#endif
