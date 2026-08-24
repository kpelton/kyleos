#include <asm/asm.h>
#include <locks/spinlock.h>
#include <output/font.h>
#include <output/framebuffer.h>
#include <output/tty.h>
#include <output/vga.h>

#ifndef LEGACY_EN
#define COLS 160
#define ROWS 45
#else
#define COLS 80
#define ROWS 25
#endif
#define CELL_W 8
#define CELL_H 16
#define FG 0x00d0d0d0
#define BG 0x00000000

struct vga_console {
    int cursor_x;
    int cursor_y;
    char cells[ROWS][COLS];
    int ansi_state;
    char ansi_params[16];
    int ansi_param_len;
    bool redraw_pending;
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
    if (!console_visible(console) || !framebuffer_is_ready())
        return;
    framebuffer_fill_rect(x * CELL_W, y * CELL_H, CELL_W, CELL_H, BG);
    for (int row = 0; row < CELL_H; row++) {
        uint8_t bits =
            terminal_font[(uint8_t)console->cells[y][x] * CELL_H + row];

        for (int col = 0; col < CELL_W; col++)
            if (bits & (0x80 >> col))
                framebuffer_fill_rect(x * CELL_W + col,
                                      y * CELL_H + row, 1, 1, FG);
    }
}

static void draw_cursor(struct vga_console *console, bool visible)
{
    if (!console_visible(console) || !framebuffer_is_ready())
        return;
    if (!visible) {
        draw_cell(console, console->cursor_x, console->cursor_y);
        return;
    }
    framebuffer_fill_rect(console->cursor_x * CELL_W,
                          console->cursor_y * CELL_H + CELL_H - 2,
                          CELL_W, 2, FG);
}

static void redraw(struct vga_console *console)
{
    if (!console_visible(console))
        return;
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            draw_cell(console, x, y);
}

static void scroll(struct vga_console *console)
{
    for (int y = 1; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            console->cells[y - 1][x] = console->cells[y][x];
    for (int x = 0; x < COLS; x++)
        console->cells[ROWS - 1][x] = ' ';
    console->cursor_y = ROWS - 1;
    redraw(console);
}

static void clear_cells(struct vga_console *console, bool paint)
{
    console->cursor_x = 0;
    console->cursor_y = 0;
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            console->cells[y][x] = ' ';
    if (paint && console_visible(console) && framebuffer_is_ready())
        framebuffer_clear(BG);
}

static int ansi_number(struct vga_console *console, int index, int fallback)
{
    int value = 0;
    int found = 0;

    for (int i = 0; i < console->ansi_param_len; i++) {
        if (console->ansi_params[i] == ';') {
            if (index-- == 0)
                return found ? value : fallback;
            value = 0;
            found = 0;
        } else if (console->ansi_params[i] >= '0' &&
                   console->ansi_params[i] <= '9') {
            value = value * 10 + console->ansi_params[i] - '0';
            found = 1;
        }
    }
    return index == 0 && found ? value : fallback;
}

static void ansi_execute(struct vga_console *console, char command)
{
    int row;
    int col;
    int amount;

    if (command == 'J' && ansi_number(console, 0, 0) == 2) {
        clear_cells(console, false);
        console->redraw_pending = true;
        return;
    }
    if (command == 'H' || command == 'f') {
        row = ansi_number(console, 0, 1) - 1;
        col = ansi_number(console, 1, 1) - 1;
        console->cursor_y = row < 0 ? 0 : (row >= ROWS ? ROWS - 1 : row);
        console->cursor_x = col < 0 ? 0 : (col >= COLS ? COLS - 1 : col);
        if (console->redraw_pending &&
            (console->cursor_x != 0 || console->cursor_y != 0)) {
            redraw(console);
            console->redraw_pending = false;
        }
        return;
    }
    amount = ansi_number(console, 0, 1);
    if (command == 'A')
        console->cursor_y = console->cursor_y > amount ?
                            console->cursor_y - amount : 0;
    if (command == 'B')
        console->cursor_y = console->cursor_y + amount < ROWS ?
                            console->cursor_y + amount : ROWS - 1;
    if (command == 'C')
        console->cursor_x = console->cursor_x + amount < COLS ?
                            console->cursor_x + amount : COLS - 1;
    if (command == 'D')
        console->cursor_x = console->cursor_x > amount ?
                            console->cursor_x - amount : 0;
}

#ifdef LEGACY_EN
static void legacy_put(struct vga_console *console, char character)
{
    int offset;

    if (!console_visible(console))
        return;
    offset = (console->cursor_x * 2) + ((console->cursor_y * COLS) * 2);
    vram[offset] = character;
    vram[offset + 1] = 0x03;
}
#endif

static void put_char(struct vga_console *console, char character)
{
    if (console->ansi_state == 1) {
        console->ansi_state = character == '[' ? 2 : 0;
        console->ansi_param_len = 0;
        return;
    }
    if (console->ansi_state == 2) {
        if (character >= '@' && character <= '~') {
            ansi_execute(console, character);
            console->ansi_state = 0;
        } else if (console->ansi_param_len <
                   (int)sizeof(console->ansi_params)) {
            console->ansi_params[console->ansi_param_len++] = character;
        }
        return;
    }
    if (character == 27) {
        console->ansi_state = 1;
        return;
    }
    if (character == '\r') {
        console->cursor_x = 0;
        return;
    }
    if (character == '\n') {
        console->cursor_x = 0;
        console->cursor_y++;
    } else if (character == '\b') {
        if (console->cursor_x > 0)
            console->cursor_x--;
        console->cells[console->cursor_y][console->cursor_x] = ' ';
        if (!console->redraw_pending)
            draw_cell(console, console->cursor_x, console->cursor_y);
    } else {
        console->cells[console->cursor_y][console->cursor_x] = character;
#ifdef LEGACY_EN
        legacy_put(console, character);
#endif
        if (!console->redraw_pending)
            draw_cell(console, console->cursor_x, console->cursor_y);
        console->cursor_x++;
    }
    if (console->cursor_x >= COLS) {
        console->cursor_x = 0;
        console->cursor_y++;
    }
    if (console->cursor_y >= ROWS)
        scroll(console);
}

void vga_init(void)
{
    init_spinlock(&vga_spinlock);
    active_console = 0;
    for (int index = 0; index < TTY_CONSOLE_COUNT; index++) {
        consoles[index].ansi_state = 0;
        consoles[index].ansi_param_len = 0;
        consoles[index].redraw_pending = false;
        clear_cells(&consoles[index], index == active_console);
    }
    for (int index = 0; index < COLS * ROWS * 2; index++)
        vram[index] = 0;
}

void vga_clear(void)
{
    struct vga_console *console;

    acquire_spinlock(&vga_spinlock);
    console = &consoles[active_console];
    console->redraw_pending = false;
    clear_cells(console, true);
    for (int index = 0; index < COLS * ROWS * 2; index++)
        vram[index] = 0;
    release_spinlock(&vga_spinlock);
}

void vga_framebuffer_ready(void)
{
    struct vga_console *console;

    acquire_spinlock(&vga_spinlock);
    console = &consoles[active_console];
    redraw(console);
    if (!console->redraw_pending)
        draw_cursor(console, true);
    release_spinlock(&vga_spinlock);
}

void vga_console_write(int console_index, const void *buffer, uint32_t count)
{
    const char *bytes = buffer;
    struct vga_console *console;

    if (!console_valid(console_index) || buffer == NULL)
        return;
    acquire_spinlock(&vga_spinlock);
    console = &consoles[console_index];
    draw_cursor(console, false);
    for (uint32_t index = 0; index < count; index++)
        put_char(console, bytes[index]);
    if (!console->redraw_pending)
        draw_cursor(console, true);
    release_spinlock(&vga_spinlock);
}

void vga_kprintf(char *string)
{
    uint32_t count = 0;

    while (string[count] != '\0')
        count++;
    vga_console_write(active_console, string, count);
}

bool vga_switch_console(int console_index)
{
    struct vga_console *console;

    if (!console_valid(console_index))
        return false;
    acquire_spinlock(&vga_spinlock);
    if (console_index == active_console) {
        release_spinlock(&vga_spinlock);
        return true;
    }
    draw_cursor(&consoles[active_console], false);
    active_console = console_index;
    console = &consoles[active_console];
    if (framebuffer_is_ready())
        framebuffer_clear(BG);
    redraw(console);
    if (!console->redraw_pending)
        draw_cursor(console, true);
    release_spinlock(&vga_spinlock);
    return true;
}
