#include <output/vga.h>
#include <output/framebuffer.h>
#include <locks/spinlock.h>
#include <asm/asm.h>
#include <output/font.h>

#define COLS 80
#define ROWS 25
#define CELL_W 8
#define CELL_H 16
#define FG 0x00d0d0d0
#define BG 0x00000000

static int cursor_x;
static int cursor_y;
static unsigned char *vram = (unsigned char *)0xffffffff800B8000;
static char cells[ROWS][COLS];
static struct spinlock vga_spinlock;
static int ansi_state;
static char ansi_params[16];
static int ansi_param_len;
static bool redraw_pending;

static void draw_cell(int x, int y);

static void draw_cell(int x, int y)
{
    if (!framebuffer_is_ready()) return;
    framebuffer_fill_rect(x * CELL_W, y * CELL_H, CELL_W, CELL_H, BG);
    for (int row = 0; row < CELL_H; row++) {
        uint8_t bits = terminal_font[(uint8_t)cells[y][x] * CELL_H + row];
        for (int col = 0; col < CELL_W; col++)
            if (bits & (0x80 >> col))
                framebuffer_fill_rect(x * CELL_W + col, y * CELL_H + row, 1, 1, FG);
    }
}

static void draw_cursor(bool visible)
{
    if (!framebuffer_is_ready()) return;
    if (!visible) {
        /* Restore the whole cell; blanking only the underline clipped glyph
         * pixels when the cursor moved after Enter or an ANSI redraw. */
        draw_cell(cursor_x, cursor_y);
        return;
    }
    framebuffer_fill_rect(cursor_x * CELL_W, cursor_y * CELL_H + CELL_H - 2,
                          CELL_W, 2, FG);
}

static void redraw(void)
{
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++) draw_cell(x, y);
}

static void scroll(void)
{
    for (int y = 1; y < ROWS; y++)
        for (int x = 0; x < COLS; x++) cells[y - 1][x] = cells[y][x];
    for (int x = 0; x < COLS; x++) cells[ROWS - 1][x] = ' ';
    cursor_y = ROWS - 1;
    redraw();
}

static void clear_cells(bool paint)
{
    cursor_x = cursor_y = 0;
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++) cells[y][x] = ' ';
    if (paint && framebuffer_is_ready()) framebuffer_clear(BG);
}

static int ansi_number(int index, int fallback)
{
    int value = 0;
    int found = 0;
    for (int i = 0; i < ansi_param_len; i++) {
        if (ansi_params[i] == ';') {
            if (index-- == 0) return found ? value : fallback;
            value = found = 0;
        } else if (ansi_params[i] >= '0' && ansi_params[i] <= '9') {
            value = value * 10 + ansi_params[i] - '0';
            found = 1;
        }
    }
    return index == 0 && found ? value : fallback;
}

static void ansi_execute(char command)
{
    int row, col, amount;
    if (command == 'J' && ansi_number(0, 0) == 2) {
        /* Full-screen applications (kedit) redraw immediately after this
         * sequence.  Keep the old frame visible until their final cursor
         * position arrives, then present the new frame in one update. */
        clear_cells(false);
        redraw_pending = true;
        return;
    }
    if (command == 'H' || command == 'f') {
        row = ansi_number(0, 1) - 1;
        col = ansi_number(1, 1) - 1;
        cursor_y = row < 0 ? 0 : (row >= ROWS ? ROWS - 1 : row);
        cursor_x = col < 0 ? 0 : (col >= COLS ? COLS - 1 : col);
        /* kedit emits ESC[2J ESC[H first, then writes its whole screen and
         * finally moves to its editing cursor.  The initial home is not a
         * frame boundary; present only at that final cursor movement. */
        if (redraw_pending && (cursor_x != 0 || cursor_y != 0)) {
            redraw();
            redraw_pending = false;
        }
        return;
    }
    amount = ansi_number(0, 1);
    if (command == 'A') cursor_y = cursor_y > amount ? cursor_y - amount : 0;
    if (command == 'B') cursor_y = cursor_y + amount < ROWS ? cursor_y + amount : ROWS - 1;
    if (command == 'C') cursor_x = cursor_x + amount < COLS ? cursor_x + amount : COLS - 1;
    if (command == 'D') cursor_x = cursor_x > amount ? cursor_x - amount : 0;
}

static void legacy_put(char ch)
{
    int i = (cursor_x * 2) + ((cursor_y * COLS) * 2);
    vram[i] = ch;
    vram[i + 1] = 0x03;
}

static void put_char(char ch)
{
    if (ansi_state == 1) {
        ansi_state = ch == '[' ? 2 : 0;
        ansi_param_len = 0;
        return;
    }
    if (ansi_state == 2) {
        if (ch >= '@' && ch <= '~') {
            ansi_execute(ch);
            ansi_state = 0;
        } else if (ansi_param_len < (int)sizeof(ansi_params)) {
            ansi_params[ansi_param_len++] = ch;
        }
        return;
    }
    if (ch == 27) {
        ansi_state = 1;
        return;
    }
    if (ch == '\r') { cursor_x = 0; return; }
    if (ch == '\n') { cursor_x = 0; cursor_y++; }
    else if (ch == '\b') {
        if (cursor_x > 0) cursor_x--;
        cells[cursor_y][cursor_x] = ' ';
        if (framebuffer_is_ready() && !redraw_pending) draw_cell(cursor_x, cursor_y);
    }
    else {
        cells[cursor_y][cursor_x] = ch;
        legacy_put(ch);
        if (framebuffer_is_ready() && !redraw_pending) draw_cell(cursor_x, cursor_y);
        cursor_x++;
    }
    if (cursor_x >= COLS) { cursor_x = 0; cursor_y++; }
    if (cursor_y >= ROWS) scroll();
}

void vga_init(void)
{
    init_spinlock(&vga_spinlock);
    vga_clear();
}

void vga_clear(void)
{
    acquire_spinlock(&vga_spinlock);
    redraw_pending = false;
    clear_cells(true);
    for (int i = 0; i < COLS * ROWS * 2; i++) vram[i] = 0;
    release_spinlock(&vga_spinlock);
}

void vga_framebuffer_ready(void)
{
    acquire_spinlock(&vga_spinlock);
    redraw();
    if (!redraw_pending) draw_cursor(true);
    release_spinlock(&vga_spinlock);
}

void vga_kprintf(char *str)
{
    acquire_spinlock(&vga_spinlock);
    draw_cursor(false);
    for (; *str; str++) put_char(*str);
    if (!redraw_pending) draw_cursor(true);
    release_spinlock(&vga_spinlock);
}
