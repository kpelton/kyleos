#include <asm/asm.h>
#include <locks/spinlock.h>
#include <output/font.h>
#include <output/framebuffer.h>
#include <output/tty.h>
#include <output/vga.h>
#include <output/vt100.h>

#define CELL_W 8
#define CELL_H 16

struct vga_console {
    struct kvt_terminal terminal;
};

static unsigned char *vram = (unsigned char *)0xffffffff800B8000;
static struct vga_console consoles[TTY_CONSOLE_COUNT];
static struct spinlock vga_spinlock;
static int active_console;

static bool console_valid(int console)
{
    return console >= 0 && console < TTY_CONSOLE_COUNT;
}

static bool console_visible(const struct vga_console *console)
{
    return console == &consoles[active_console];
}

static void draw_cell(struct vga_console *console, int x, int y)
{
    struct kvt_cell *cell;
    uint32_t foreground;
    uint32_t background;
    uint8_t character;

    if (!console_visible(console) || !framebuffer_is_ready()) return;
    cell = &console->terminal.cells[y][x];
    foreground = kvt_palette_color(cell->foreground);
    background = kvt_palette_color(cell->background);
    character = (cell->attributes & KVT_ATTR_CONCEAL) ? ' ' :
                cell->character;
    framebuffer_fill_rect(x * CELL_W, y * CELL_H, CELL_W, CELL_H,
                          background);
    for (int row = 0; row < CELL_H; row++) {
        uint8_t bits = terminal_font[character * CELL_H + row];

        for (int column = 0; column < CELL_W; column++)
            if (bits & (0x80 >> column))
                framebuffer_fill_rect(x * CELL_W + column,
                                      y * CELL_H + row, 1, 1, foreground);
    }
    if (cell->attributes & KVT_ATTR_UNDERLINE)
        framebuffer_fill_rect(x * CELL_W, y * CELL_H + CELL_H - 2,
                              CELL_W, 1, foreground);
#ifdef LEGACY_EN
    {
        int offset = (x * 2) + ((y * KVT_COLUMNS) * 2);
        vram[offset] = character;
        vram[offset + 1] = 0x03;
    }
#endif
}

static void draw_cursor(struct vga_console *console, bool visible)
{
    int x = console->terminal.cursor_column;
    int y = console->terminal.cursor_row;

    if (!console_visible(console) || !framebuffer_is_ready()) return;
    if (!visible || !console->terminal.cursor_visible) {
        draw_cell(console, x, y);
        return;
    }
    framebuffer_fill_rect(x * CELL_W, y * CELL_H + CELL_H - 2,
                          CELL_W, 2,
                          kvt_palette_color(
                              console->terminal.cells[y][x].foreground));
}

static void redraw(struct vga_console *console, bool all)
{
    if (!console_visible(console) || !framebuffer_is_ready()) return;
    for (int row = 0; row < KVT_ROWS; row++) {
        if (!all && !console->terminal.dirty_rows[row]) continue;
        for (int column = 0; column < KVT_COLUMNS; column++)
            draw_cell(console, column, row);
        console->terminal.dirty_rows[row] = false;
    }
}

void vga_init(void)
{
    init_spinlock(&vga_spinlock);
    active_console = 0;
    for (int index = 0; index < TTY_CONSOLE_COUNT; index++)
        kvt_init(&consoles[index].terminal);
    for (int index = 0; index < KVT_COLUMNS * KVT_ROWS * 2; index++)
        vram[index] = 0;
}

void vga_clear(void)
{
    struct vga_console *console;

    acquire_spinlock(&vga_spinlock);
    console = &consoles[active_console];
    kvt_clear(&console->terminal);
    if (framebuffer_is_ready()) framebuffer_clear(0);
    redraw(console, true);
    draw_cursor(console, true);
    release_spinlock(&vga_spinlock);
}

void vga_framebuffer_ready(void)
{
    struct vga_console *console;

    acquire_spinlock(&vga_spinlock);
    console = &consoles[active_console];
    redraw(console, true);
    draw_cursor(console, true);
    release_spinlock(&vga_spinlock);
}

void vga_console_write(int console_index, const void *buffer, uint32_t count)
{
    struct vga_console *console;

    if (!console_valid(console_index) || buffer == NULL) return;
    acquire_spinlock(&vga_spinlock);
    console = &consoles[console_index];
    draw_cursor(console, false);
    kvt_write(&console->terminal, buffer, count);
    redraw(console, false);
    draw_cursor(console, true);
    release_spinlock(&vga_spinlock);
}

void vga_kprintf(char *string)
{
    uint32_t count = 0;
    while (string[count] != '\0') count++;
    vga_console_write(active_console, string, count);
}

bool vga_switch_console(int console_index)
{
    struct vga_console *console;

    if (!console_valid(console_index)) return false;
    acquire_spinlock(&vga_spinlock);
    if (console_index == active_console) {
        release_spinlock(&vga_spinlock);
        return true;
    }
    draw_cursor(&consoles[active_console], false);
    active_console = console_index;
    console = &consoles[active_console];
    if (framebuffer_is_ready()) framebuffer_clear(0);
    redraw(console, true);
    draw_cursor(console, true);
    release_spinlock(&vga_spinlock);
    return true;
}
