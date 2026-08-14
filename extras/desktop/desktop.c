/* A compact, early-Mac-inspired desktop for the KyleOS framebuffer. */
#include <stdint.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "extras/calculator/calculator.h"
#include "extras/calculator/calculator_ipc.h"
#include "extras/kui/kui.h"
#include "extras/minesweeper/minesweeper.h"
#include "extras/solitaire/solitaire.h"
#include "extras/solitaire/solitaire_ipc.h"

#define WIDTH screen_width
#define HEIGHT screen_height
#define MENU_H 21
#define MAX_WINDOWS 10
#define ICON_COUNT 9
#define MAX_FINDER_ITEMS 48
#define FINDER_NAME_MAX 32
#define KYLEOS_I_DIR 40000
#define FINDER_CELL_W 88
#define FINDER_CELL_H 82
#define MIN_WINDOW_W 260
#define MIN_WINDOW_H 150
#define EDIT_MAX 8192
#define TERM_MAX 4096
#define COMMAND_MAX 128

#define BLACK KUI_BLACK
#define WHITE KUI_WHITE
#define LIGHT KUI_LIGHT
#define MID KUI_FACE
#define DARK KUI_SHADOW
#define TITLE_BLUE KUI_TITLE_BLUE
#define DESKTOP_TEAL KUI_DESKTOP_TEAL
#define DESKTOP_DARK KUI_DESKTOP_DARK
#define TERMINAL_GREEN 0x0000ff80U

#define pointer_delta kui_pointer_delta
#define scan_to_char kui_scan_to_char
#define framebuffer_present(pixels, bytes) kui_present(&surface)
#define key_poll kui_key_poll
#define tty_raw kui_tty_raw
#define mouse_poll kui_mouse_poll
#define sleep_ms kui_sleep_ms
#define in_rect kui_in_rect
#define pixel(x, y, color) kui_pixel(&surface, x, y, color)
#define fill_rect(x, y, w, h, color) \
    kui_fill_rect(&surface, x, y, w, h, color)
#define outline_rect(x, y, w, h, color) \
    kui_outline_rect(&surface, x, y, w, h, color)
#define line(x0, y0, x1, y1, color) \
    kui_line(&surface, x0, y0, x1, y1, color)
#define draw_char(x, y, ch, fg, bg, opaque) \
    kui_draw_char(&surface, x, y, ch, fg, bg, opaque)
#define draw_text(x, y, text, fg, bg, opaque) \
    kui_draw_text(&surface, x, y, text, fg, bg, opaque)
#define text_width kui_text_width
#define desktop_pattern() kui_draw_desktop(&surface, MENU_H)

enum window_kind {
    WINDOW_FINDER, WINDOW_ABOUT, WINDOW_README, WINDOW_DUKE, WINDOW_ITEM,
    WINDOW_EDITOR, WINDOW_TERMINAL, WINDOW_MINESWEEPER, WINDOW_SOLITAIRE,
    WINDOW_CALCULATOR
};
enum menu_kind { MENU_NONE, MENU_APPLE, MENU_FILE, MENU_VIEW, MENU_SPECIAL };

struct window {
    int visible;
    int x;
    int y;
    int w;
    int h;
    enum window_kind kind;
    char title[32];
    char path[128];
    int item_count;
    int scroll_row;
    char item_names[MAX_FINDER_ITEMS][FINDER_NAME_MAX];
    int item_types[MAX_FINDER_ITEMS];
    char text[EDIT_MAX];
    int text_length;
    int text_cursor;
    int dirty;
    int save_failed;
    char terminal[TERM_MAX];
    int terminal_length;
    char command[COMMAND_MAX];
    int command_length;
    struct minesweeper_game minesweeper;
    struct solitaire_game solitaire;
    struct calculator_state calculator;
    int app_pid;
    int app_input;
    int app_output;
};

struct kyleos_dirent {
    uint64_t inode;
    char name[257];
    int type;
    uint64_t size;
};

struct desktop_icon {
    int x;
    int y;
    const char *label;
    enum window_kind kind;
};

static uint32_t *frame;
static uint32_t frame_bytes;
static int screen_width;
static int screen_height;
static struct kui_surface surface;
static struct window windows[MAX_WINDOWS];
static struct desktop_icon icons[ICON_COUNT] = {
    { 562, 45,  "System Disk", WINDOW_FINDER },
    { 562, 125, "Read Me",     WINDOW_README },
    { 562, 205, "Duke 3D",     WINDOW_DUKE },
    { 562, 315, "Trash",       WINDOW_FINDER },
    { 478, 45,  "Text Editor", WINDOW_EDITOR },
    { 478, 125, "Terminal",    WINDOW_TERMINAL },
    { 478, 205, "Mines",       WINDOW_MINESWEEPER },
    { 478, 285, "Solitaire",   WINDOW_SOLITAIRE },
    { 394, 285, "Calculator",  WINDOW_CALCULATOR }
};

static int cursor_x;
static int cursor_y;
static uint8_t mouse_buttons;
static int selected_icon = -1;
static enum menu_kind open_menu;
static int drag_window = -1;
static int drag_dx;
static int drag_dy;
static int resize_window = -1;
static int resize_right_gap;
static int resize_bottom_gap;
static int scroll_window = -1;
static int scroll_drag_offset;
static int running = 1;

static void render(void);

static void layout_desktop_icons(void)
{
    int right = WIDTH - 78;
    int left = WIDTH - 162;

    icons[0].x = icons[1].x = icons[2].x = icons[3].x = right;
    icons[4].x = icons[5].x = icons[6].x = icons[7].x = left;
    if (HEIGHT >= 900) {
        icons[0].y = icons[4].y = 65;
        icons[1].y = icons[5].y = 225;
        icons[2].y = icons[6].y = 385;
        icons[7].y = 545;
        icons[8].x = left;
        icons[8].y = 705;
        icons[3].y = HEIGHT - 72;
    } else if (HEIGHT >= 600) {
        icons[0].y = icons[4].y = 55;
        icons[1].y = icons[5].y = 155;
        icons[2].y = icons[6].y = 255;
        icons[7].y = 355;
        icons[8].x = left;
        icons[8].y = 455;
        icons[3].y = HEIGHT - 72;
    }
}

static int move_text_vertical(const char *text, int length, int position,
                              int direction)
{
    int start = position;
    int column;
    int target_start;
    int target_end;

    while (start > 0 && text[start - 1] != '\n') start--;
    column = position - start;
    if (direction < 0) {
        if (start == 0) return position;
        target_end = start - 1;
        target_start = target_end;
        while (target_start > 0 && text[target_start - 1] != '\n')
            target_start--;
    } else {
        target_start = position;
        while (target_start < length && text[target_start] != '\n')
            target_start++;
        if (target_start == length) return position;
        target_start++;
        target_end = target_start;
        while (target_end < length && text[target_end] != '\n') target_end++;
    }
    return target_start +
        (column < target_end - target_start ? column : target_end - target_start);
}

static int get_directory_entries(int fd, struct kyleos_dirent *entries,
                                 int count)
{
    register long rax asm("rax") = 16;
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"((long)fd), "S"(entries), "d"((long)count)
                 : "memory");
    return (int)rax;
}

static void centered_text(int x, int y, int w, const char *text)
{
    draw_text(x + (w - text_width(text)) / 2, y, text, BLACK, WHITE, 0);
}

static void draw_apple(int x, int y, uint32_t color, uint32_t bite)
{
    fill_rect(x + 5, y + 5, 10, 9, color);
    fill_rect(x + 3, y + 7, 14, 5, color);
    fill_rect(x + 6, y + 14, 3, 2, color);
    fill_rect(x + 12, y + 14, 3, 2, color);
    fill_rect(x + 12, y + 2, 5, 2, color);
    pixel(x + 11, y + 4, color);
    fill_rect(x + 15, y + 8, 3, 3, bite);
}

static void draw_menu_bar(void)
{
    fill_rect(0, 0, WIDTH, MENU_H, MID);
    fill_rect(0, MENU_H - 2, WIDTH, 1, WHITE);
    fill_rect(0, MENU_H - 1, WIDTH, 1, DARK);
    draw_apple(7, 1, BLACK, MID);
    draw_text(35, 2, "File", BLACK, MID, 0);
    draw_text(84, 2, "View", BLACK, MID, 0);
    draw_text(137, 2, "Special", BLACK, MID, 0);
    draw_text(534, 2, "KyleOS", TITLE_BLUE, MID, 0);

    if (open_menu == MENU_APPLE) fill_rect(2, 0, 28, MENU_H - 2, TITLE_BLUE);
    if (open_menu == MENU_FILE) fill_rect(31, 0, 49, MENU_H - 2, TITLE_BLUE);
    if (open_menu == MENU_VIEW) fill_rect(80, 0, 52, MENU_H - 2, TITLE_BLUE);
    if (open_menu == MENU_SPECIAL) fill_rect(132, 0, 74, MENU_H - 2, TITLE_BLUE);

    if (open_menu == MENU_APPLE) draw_apple(7, 1, WHITE, TITLE_BLUE);
    if (open_menu == MENU_FILE) draw_text(35, 2, "File", WHITE, TITLE_BLUE, 0);
    if (open_menu == MENU_VIEW) draw_text(84, 2, "View", WHITE, TITLE_BLUE, 0);
    if (open_menu == MENU_SPECIAL) draw_text(137, 2, "Special", WHITE, TITLE_BLUE, 0);
}

static void draw_disk_icon(int x, int y, int selected)
{
    if (selected) fill_rect(x - 4, y - 4, 43, 40, BLACK);
    fill_rect(x, y, 35, 30, selected ? WHITE : LIGHT);
    outline_rect(x, y, 35, 30, BLACK);
    fill_rect(x + 4, y + 4, 27, 3, BLACK);
    fill_rect(x + 5, y + 20, 25, 5, WHITE);
    outline_rect(x + 5, y + 20, 25, 5, BLACK);
}

static void draw_document_icon(int x, int y, int selected)
{
    if (selected) fill_rect(x - 4, y - 4, 39, 42, BLACK);
    fill_rect(x, y, 30, 34, WHITE);
    outline_rect(x, y, 30, 34, BLACK);
    line(x + 21, y, x + 29, y + 8, BLACK);
    line(x + 21, y, x + 21, y + 8, BLACK);
    line(x + 21, y + 8, x + 29, y + 8, BLACK);
    for (int row = 13; row < 28; row += 5)
        fill_rect(x + 5, y + row, 19, 2, BLACK);
}

static void draw_trash_icon(int x, int y, int selected)
{
    if (selected) fill_rect(x - 4, y - 4, 39, 42, BLACK);
    fill_rect(x + 4, y + 8, 24, 26, WHITE);
    outline_rect(x + 4, y + 8, 24, 26, BLACK);
    fill_rect(x, y + 5, 32, 4, BLACK);
    fill_rect(x + 10, y + 1, 12, 4, BLACK);
    for (int col = 9; col < 25; col += 7)
        fill_rect(x + col, y + 13, 2, 15, BLACK);
}

static void draw_mines_icon(int x, int y, int selected)
{
    if (selected) fill_rect(x - 4, y - 4, 39, 42, TITLE_BLUE);
    fill_rect(x + 7, y + 7, 18, 18, BLACK);
    fill_rect(x + 12, y + 2, 8, 28, BLACK);
    fill_rect(x + 2, y + 12, 28, 8, BLACK);
    fill_rect(x + 10, y + 10, 5, 5, WHITE);
}

static void draw_solitaire_icon(int x, int y, int selected)
{
    if (selected) fill_rect(x - 4, y - 4, 39, 42, TITLE_BLUE);
    fill_rect(x + 3, y + 1, 22, 29, WHITE);
    outline_rect(x + 3, y + 1, 22, 29, BLACK);
    draw_text(x + 7, y + 3, "A", 0x00c00000U, WHITE, 0);
    fill_rect(x + 12, y + 16, 5, 5, 0x00c00000U);
    pixel(x + 11, y + 18, 0x00c00000U);
    pixel(x + 17, y + 18, 0x00c00000U);
}

static void draw_calculator_icon(int x, int y, int selected)
{
    if (selected) fill_rect(x - 4, y - 4, 39, 42, TITLE_BLUE);
    fill_rect(x + 3, y + 1, 25, 32, LIGHT);
    outline_rect(x + 3, y + 1, 25, 32, BLACK);
    fill_rect(x + 7, y + 5, 17, 6, WHITE);
    outline_rect(x + 7, y + 5, 17, 6, DARK);
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 3; col++)
            fill_rect(x + 7 + col * 6, y + 15 + row * 5,
                      4, 3, col == 2 ? TITLE_BLUE : DARK);
}

static void draw_icon(int index)
{
    const struct desktop_icon *icon = &icons[index];
    int selected = selected_icon == index;

    if (icons[index].kind == WINDOW_MINESWEEPER)
        draw_mines_icon(icon->x + 12, icon->y, selected);
    else if (icons[index].kind == WINDOW_SOLITAIRE)
        draw_solitaire_icon(icon->x + 12, icon->y, selected);
    else if (icons[index].kind == WINDOW_CALCULATOR)
        draw_calculator_icon(icon->x + 12, icon->y, selected);
    else if (index == 0) draw_disk_icon(icon->x + 10, icon->y, selected);
    else if (index == 3) draw_trash_icon(icon->x + 11, icon->y, selected);
    else draw_document_icon(icon->x + 12, icon->y, selected);

    int width = text_width(icon->label);
    int tx = icon->x + 27 - width / 2;
    if (selected)
        fill_rect(tx - 2, icon->y + 39, width + 4, 16, TITLE_BLUE);
    else
        draw_text(tx + 1, icon->y + 40, icon->label,
                  BLACK, DESKTOP_TEAL, 0);
    draw_text(tx, icon->y + 39, icon->label, WHITE,
              selected ? TITLE_BLUE : DESKTOP_TEAL, 0);
}

static int top_window(void)
{
    for (int i = MAX_WINDOWS - 1; i >= 0; i--)
        if (windows[i].visible) return i;
    return -1;
}

static int raise_window(int index)
{
    struct window selected = windows[index];
    for (int i = index; i < MAX_WINDOWS - 1; i++)
        windows[i] = windows[i + 1];
    windows[MAX_WINDOWS - 1] = selected;
    return MAX_WINDOWS - 1;
}

static int write_all(int fd, const void *buffer, int length)
{
    int total = 0;
    while (total < length) {
        int count = write(fd, (const char *)buffer + total, length - total);
        if (count <= 0) return -1;
        total += count;
    }
    return 0;
}

static int read_all(int fd, void *buffer, int length)
{
    int total = 0;
    while (total < length) {
        int count = read(fd, (char *)buffer + total, length - total);
        if (count <= 0) return -1;
        total += count;
    }
    return 0;
}

static int solitaire_process_request(struct window *window, int command,
                                     int pointer_x, int pointer_y)
{
    struct solitaire_request request;
    struct solitaire_response response;

    if (window->app_pid <= 0 || window->app_input < 0 ||
        window->app_output < 0)
        return -1;
    memset(&request, 0, sizeof(request));
    request.command = command;
    request.seed = kui_ticks_ms() ^ ((uint32_t)pointer_x << 16) ^
                   (uint32_t)pointer_y;
    request.width = window->w - 21;
    request.height = window->h - 39;
    request.pointer_x = pointer_x - (window->x + 4);
    request.pointer_y = pointer_y - (window->y + 22);
    if (write_all(window->app_input, &request, sizeof(request)) < 0 ||
        read_all(window->app_output, &response, sizeof(response)) < 0)
        return -1;
    window->solitaire = response.game;
    return response.changed;
}

static int calculator_process_request(struct window *window, int command,
                                      int pointer_x, int pointer_y, char key)
{
    struct calculator_request request;
    struct calculator_response response;

    if (window->app_pid <= 0 || window->app_input < 0 ||
        window->app_output < 0)
        return -1;
    memset(&request, 0, sizeof(request));
    request.command = command;
    request.width = window->w - 21;
    request.height = window->h - 39;
    request.pointer_x = pointer_x - (window->x + 4);
    request.pointer_y = pointer_y - (window->y + 22);
    request.key = key;
    if (write_all(window->app_input, &request, sizeof(request)) < 0 ||
        read_all(window->app_output, &response, sizeof(response)) < 0)
        return -1;
    window->calculator = response.state;
    return response.changed;
}

static void stop_window_process(struct window *window)
{
    if (window->kind == WINDOW_SOLITAIRE && window->app_pid > 0) {
        struct solitaire_request request;
        int status;

        memset(&request, 0, sizeof(request));
        request.command = SOLITAIRE_COMMAND_QUIT;
        write_all(window->app_input, &request, sizeof(request));
        close(window->app_input);
        close(window->app_output);
        waitpid(window->app_pid, &status, 0);
        window->app_pid = -1;
        window->app_input = -1;
        window->app_output = -1;
    } else if (window->kind == WINDOW_CALCULATOR && window->app_pid > 0) {
        struct calculator_request request;
        int status;

        memset(&request, 0, sizeof(request));
        request.command = CALCULATOR_COMMAND_QUIT;
        write_all(window->app_input, &request, sizeof(request));
        close(window->app_input);
        close(window->app_output);
        waitpid(window->app_pid, &status, 0);
        window->app_pid = -1;
        window->app_input = -1;
        window->app_output = -1;
    }
}

static int start_solitaire_process(struct window *window)
{
    int to_child[2];
    int from_child[2];
    int child;

    if (pipe(to_child) < 0) return -1;
    if (pipe(from_child) < 0) {
        close(to_child[0]);
        close(to_child[1]);
        return -1;
    }
    child = fork();
    if (child == 0) {
        char *argv[] = { "solitaire", "--embedded", NULL };
        close(to_child[1]);
        close(from_child[0]);
        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        close(to_child[0]);
        close(from_child[1]);
        execve("/bin/solitaire", argv, NULL);
        _exit(127);
    }
    close(to_child[0]);
    close(from_child[1]);
    if (child < 0) {
        close(to_child[1]);
        close(from_child[0]);
        return -1;
    }
    window->app_pid = child;
    window->app_input = to_child[1];
    window->app_output = from_child[0];
    if (solitaire_process_request(window, SOLITAIRE_COMMAND_RESET, 0, 0) < 0) {
        stop_window_process(window);
        return -1;
    }
    return 0;
}

static int start_calculator_process(struct window *window)
{
    int to_child[2];
    int from_child[2];
    int child;

    if (pipe(to_child) < 0) return -1;
    if (pipe(from_child) < 0) {
        close(to_child[0]);
        close(to_child[1]);
        return -1;
    }
    child = fork();
    if (child == 0) {
        char *argv[] = { "calculator", "--embedded", NULL };
        close(to_child[1]);
        close(from_child[0]);
        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        close(to_child[0]);
        close(from_child[1]);
        execve("/bin/calculator", argv, NULL);
        _exit(127);
    }
    close(to_child[0]);
    close(from_child[1]);
    if (child < 0) {
        close(to_child[1]);
        close(from_child[0]);
        return -1;
    }
    window->app_pid = child;
    window->app_input = to_child[1];
    window->app_output = from_child[0];
    if (calculator_process_request(window, CALCULATOR_COMMAND_RESET,
                                   0, 0, 0) < 0) {
        stop_window_process(window);
        return -1;
    }
    return 0;
}

static void close_window(struct window *window)
{
    stop_window_process(window);
    window->visible = 0;
}

static int open_window(enum window_kind kind)
{
    int slot = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i].visible) { slot = i; break; }
    }
    if (slot < 0) return -1;

    memset(&windows[slot], 0, sizeof(windows[slot]));
    windows[slot].app_pid = -1;
    windows[slot].app_input = -1;
    windows[slot].app_output = -1;
    windows[slot].visible = 1;
    windows[slot].kind = kind;
    windows[slot].x = 58 + slot * 22;
    windows[slot].y = 46 + slot * 14;
    /* Keep both desktop application columns visible beside new windows. */
    windows[slot].w = kind == WINDOW_ABOUT ? 340 :
                      WIDTH >= 1600 ? 1480 :
                      WIDTH >= 900 ? 720 : 400;
    windows[slot].h = kind == WINDOW_ABOUT ? 184 :
                      HEIGHT >= 1000 ? 820 :
                      HEIGHT >= 700 ? 520 : 265;
    if (kind == WINDOW_FINDER) strcpy(windows[slot].title, "System Disk");
    if (kind == WINDOW_ABOUT) strcpy(windows[slot].title, "About KyleOS");
    if (kind == WINDOW_README) strcpy(windows[slot].title, "Read Me");
    if (kind == WINDOW_DUKE) strcpy(windows[slot].title, "Duke Nukem 3D");
    if (kind == WINDOW_ITEM) strcpy(windows[slot].title, "Item Information");
    if (kind == WINDOW_EDITOR) strcpy(windows[slot].title, "Text Editor");
    if (kind == WINDOW_TERMINAL) strcpy(windows[slot].title, "Terminal");
    if (kind == WINDOW_MINESWEEPER) {
        strcpy(windows[slot].title, "Minesweeper");
        if (WIDTH >= 1600) windows[slot].w = 540;
        if (HEIGHT >= 1000) windows[slot].h = 620;
        minesweeper_reset(&windows[slot].minesweeper,
                          kui_ticks_ms() ^ (uint32_t)slot);
    }
    if (kind == WINDOW_SOLITAIRE) {
        strcpy(windows[slot].title, "Solitaire");
        if (WIDTH >= 1600) windows[slot].w = 1000;
        if (HEIGHT >= 1000) windows[slot].h = 850;
        if (start_solitaire_process(&windows[slot]) < 0) {
            windows[slot].visible = 0;
            return -1;
        }
    }
    if (kind == WINDOW_CALCULATOR) {
        strcpy(windows[slot].title, "Calculator");
        windows[slot].w = WIDTH >= 1600 ? 520 :
                          WIDTH >= 900 ? 420 : 320;
        windows[slot].h = HEIGHT >= 1000 ? 700 :
                          HEIGHT >= 700 ? 560 : 360;
        if (start_calculator_process(&windows[slot]) < 0) {
            windows[slot].visible = 0;
            return -1;
        }
    }
    if (windows[slot].y + windows[slot].h > HEIGHT)
        windows[slot].y = HEIGHT - windows[slot].h;
    return raise_window(slot);
}

static const char *path_name(const char *path)
{
    const char *name = strrchr(path, '/');

    if (path[0] == '/' && path[1] == '\0') return "System Disk";
    return name != NULL ? name + 1 : path;
}

static void load_finder(struct window *window, const char *path)
{
    struct kyleos_dirent entries[64];
    int fd;
    int count;

    window->item_count = 0;
    window->scroll_row = 0;
    strncpy(window->path, path, sizeof(window->path) - 1);
    window->path[sizeof(window->path) - 1] = '\0';
    strncpy(window->title, path_name(window->path), sizeof(window->title) - 1);
    window->title[sizeof(window->title) - 1] = '\0';

    fd = open(window->path, O_RDONLY);
    if (fd < 0) return;
    count = get_directory_entries(fd, entries,
                                  (int)(sizeof(entries) / sizeof(entries[0])));
    close(fd);
    if (count < 0) return;

    for (int i = 0; i < count && window->item_count < MAX_FINDER_ITEMS; i++) {
        int slot;
        if (strcmp(entries[i].name, ".") == 0 ||
            strcmp(entries[i].name, "..") == 0)
            continue;
        slot = window->item_count++;
        strncpy(window->item_names[slot], entries[i].name,
                FINDER_NAME_MAX - 1);
        window->item_names[slot][FINDER_NAME_MAX - 1] = '\0';
        window->item_types[slot] = entries[i].type;
    }
}

static int open_finder(const char *path)
{
    int index = open_window(WINDOW_FINDER);

    if (index >= 0) load_finder(&windows[index], path);
    return index;
}

static int open_editor(const char *path)
{
    int index = open_window(WINDOW_EDITOR);

    if (index >= 0) {
        struct window *window = &windows[index];
        int fd;
        int count;

        strncpy(window->path, path, sizeof(window->path) - 1);
        window->path[sizeof(window->path) - 1] = '\0';
        strncpy(window->title, path_name(path), sizeof(window->title) - 1);
        window->title[sizeof(window->title) - 1] = '\0';
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            count = read(fd, window->text, EDIT_MAX - 1);
            close(fd);
            if (count > 0) window->text_length = count;
        } else window->dirty = 1;
    }
    return index;
}

static int is_text_document(const char *path)
{
    unsigned char sample[512];
    int fd = open(path, O_RDONLY);
    int count;

    if (fd < 0) return 0;
    count = read(fd, sample, sizeof(sample));
    close(fd);
    if (count < 0) return 0;
    for (int i = 0; i < count; i++) {
        unsigned char ch = sample[i];
        if (ch == 0) return 0;
        if (ch < 32 && ch != '\n' && ch != '\r' && ch != '\t' &&
            ch != '\f')
            return 0;
    }
    return 1;
}

static int is_application(const char *path)
{
    unsigned char magic[4];
    int fd = open(path, O_RDONLY);
    int count;

    if (fd < 0) return 0;
    count = read(fd, magic, sizeof(magic));
    close(fd);
    return count == 4 && magic[0] == 0x7f && magic[1] == 'E' &&
           magic[2] == 'L' && magic[3] == 'F';
}

static int open_item(const char *title)
{
    int index = open_window(WINDOW_ITEM);

    if (index >= 0) {
        strncpy(windows[index].title, title,
                sizeof(windows[index].title) - 1);
        windows[index].title[sizeof(windows[index].title) - 1] = '\0';
    }
    return index;
}

static int open_terminal(void)
{
    static const char greeting[] =
        "KyleOS Terminal\nType a command and press Enter.\n";
    int index = open_window(WINDOW_TERMINAL);

    if (index >= 0) {
        struct window *window = &windows[index];
        memcpy(window->terminal, greeting, sizeof(greeting) - 1);
        window->terminal_length = sizeof(greeting) - 1;
        strcpy(window->path, "/");
    }
    return index;
}

static int launch_application(const char *path)
{
    char *argv[2];
    int child;
    int status;
    int pressed;
    int dx;
    int dy;
    uint8_t code;
    uint8_t extended;
    uint8_t buttons;

    argv[0] = (char *)path;
    argv[1] = NULL;

    tty_raw(0);
    memset(frame, 0, frame_bytes);
    framebuffer_present(frame, frame_bytes);
    printf("\033[2J\033[H");
    fflush(stdout);

    child = fork();
    if (child == 0) {
        execve(path, argv, NULL);
        fprintf(stderr, "desktop: cannot launch %s\n", path);
        _exit(127);
    }
    if (child > 0) waitpid(child, &status, 0);

    tty_raw(1);
    while (key_poll(&pressed, &code, &extended) > 0) { }
    while (mouse_poll(&dx, &dy, &buttons) > 0) { }
    mouse_buttons = 0;
    render();
    return child < 0 ? -1 : 0;
}

static void activate_desktop_icon(int index)
{
    static int document_number = 1;
    char path[64];

    if (index == 0) open_finder("/");
    else if (index == 3) open_finder("/tmp");
    else if (icons[index].kind == WINDOW_DUKE) {
        launch_application("/usr/bin/duke3d");
    }
    else if (icons[index].kind == WINDOW_EDITOR) {
        snprintf(path, sizeof(path), "/NOTE%d.TXT", document_number++);
        open_editor(path);
    } else if (icons[index].kind == WINDOW_TERMINAL) {
        open_terminal();
    } else if (icons[index].kind == WINDOW_MINESWEEPER) {
        open_window(WINDOW_MINESWEEPER);
    } else if (icons[index].kind == WINDOW_SOLITAIRE) {
        open_window(WINDOW_SOLITAIRE);
    } else if (icons[index].kind == WINDOW_CALCULATOR) {
        open_window(WINDOW_CALCULATOR);
    } else {
        open_window(icons[index].kind);
    }
}

static int save_editor(struct window *window)
{
    int fd = open(window->path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int total = 0;

    if (fd < 0) {
        window->save_failed = 1;
        return -1;
    }
    while (total < window->text_length) {
        int written = write(fd, window->text + total,
                            window->text_length - total);
        if (written <= 0) break;
        total += written;
    }
    close(fd);
    window->save_failed = total != window->text_length;
    if (!window->save_failed) {
        char parent[128];
        char *slash;

        window->dirty = 0;
        strncpy(parent, window->path, sizeof(parent) - 1);
        parent[sizeof(parent) - 1] = '\0';
        slash = strrchr(parent, '/');
        if (slash == parent) parent[1] = '\0';
        else if (slash != NULL) *slash = '\0';
        for (int i = 0; i < MAX_WINDOWS; i++)
            if (windows[i].visible && windows[i].kind == WINDOW_FINDER &&
                strcmp(windows[i].path, parent) == 0)
                load_finder(&windows[i], parent);
    }
    return window->save_failed ? -1 : 0;
}

static void terminal_append(struct window *window, const char *text,
                            int length)
{
    if (length < 0) length = (int)strlen(text);
    if (length >= TERM_MAX) {
        text += length - (TERM_MAX - 1);
        length = TERM_MAX - 1;
    }
    if (window->terminal_length + length >= TERM_MAX) {
        int discard = window->terminal_length + length - (TERM_MAX - 1);
        memmove(window->terminal, window->terminal + discard,
                window->terminal_length - discard);
        window->terminal_length -= discard;
    }
    memcpy(window->terminal + window->terminal_length, text, length);
    window->terminal_length += length;
    window->terminal[window->terminal_length] = '\0';
}

static void terminal_set_path(struct window *window, const char *path)
{
    if (path[0] == '/') {
        strncpy(window->path, path, sizeof(window->path) - 1);
    } else if (strcmp(path, "..") == 0) {
        char *slash;
        if (strcmp(window->path, "/") == 0) return;
        slash = strrchr(window->path, '/');
        if (slash == window->path) window->path[1] = '\0';
        else if (slash != NULL) *slash = '\0';
        return;
    } else if (strcmp(path, ".") == 0) {
        return;
    } else if (strcmp(window->path, "/") == 0) {
        snprintf(window->path, sizeof(window->path), "/%s", path);
    } else {
        int length = strlen(window->path);
        snprintf(window->path + length, sizeof(window->path) - length,
                 "/%s", path);
    }
    window->path[sizeof(window->path) - 1] = '\0';
}

static void terminal_execute(struct window *window)
{
    char input[COMMAND_MAX];
    char display[COMMAND_MAX + 4];
    char program[160];
    char *argv[16];
    char *cursor;
    int argc = 0;
    int output;
    int child;
    int status;

    memcpy(input, window->command, window->command_length);
    input[window->command_length] = '\0';
    snprintf(display, sizeof(display), "$ %s\n", input);
    terminal_append(window, display, -1);
    window->command_length = 0;
    window->command[0] = '\0';

    cursor = input;
    while (*cursor != '\0' && argc < 15) {
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        if (*cursor == '\0') break;
        argv[argc++] = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t')
            cursor++;
        if (*cursor != '\0') *cursor++ = '\0';
    }
    argv[argc] = NULL;
    if (argc == 0) return;
    chdir(window->path);

    if (strcmp(argv[0], "clear") == 0) {
        window->terminal_length = 0;
        window->terminal[0] = '\0';
        return;
    }
    if (strcmp(argv[0], "pwd") == 0) {
        terminal_append(window, window->path, -1);
        terminal_append(window, "\n", 1);
        return;
    }
    if (strcmp(argv[0], "cd") == 0) {
        const char *target = argc > 1 ? argv[1] : "/";
        if (chdir(target) == 0) terminal_set_path(window, target);
        else terminal_append(window, "cd: directory not found\n", -1);
        return;
    }
    if (strcmp(argv[0], "solitaire") == 0 ||
        strcmp(argv[0], "/bin/solitaire") == 0) {
        open_window(WINDOW_SOLITAIRE);
        return;
    }
    if (strcmp(argv[0], "calculator") == 0 ||
        strcmp(argv[0], "/bin/calculator") == 0) {
        open_window(WINDOW_CALCULATOR);
        return;
    }

    if (strchr(argv[0], '/') != NULL) {
        strncpy(program, argv[0], sizeof(program) - 1);
        program[sizeof(program) - 1] = '\0';
    } else {
        snprintf(program, sizeof(program), "/bin/%s", argv[0]);
    }

    output = open("/tmp/.desktop-terminal.out",
                  O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output < 0) {
        terminal_append(window, "terminal: cannot create output file\n", -1);
        return;
    }
    child = fork();
    if (child == 0) {
        int nullfd = open("/dev/null", O_RDONLY);
        if (nullfd >= 0) { dup2(nullfd, 0); close(nullfd); }
        dup2(output, 1);
        dup2(output, 2);
        close(output);
        execve(program, argv, NULL);
        if (strchr(argv[0], '/') == NULL) {
            snprintf(program, sizeof(program), "/usr/bin/%s", argv[0]);
            execve(program, argv, NULL);
        }
        fprintf(stderr, "%s: command not found\n", argv[0]);
        _exit(127);
    }
    if (child < 0) {
        close(output);
        terminal_append(window, "terminal: fork failed\n", -1);
        return;
    }
    close(output);
    waitpid(child, &status, 0);
    output = open("/tmp/.desktop-terminal.out", O_RDONLY);
    if (output >= 0) {
        char buffer[512];
        int count;
        while ((count = read(output, buffer, sizeof(buffer))) > 0)
            terminal_append(window, buffer, count);
        close(output);
    }
    if (window->terminal_length > 0 &&
        window->terminal[window->terminal_length - 1] != '\n')
        terminal_append(window, "\n", 1);
}

static void close_top_window(void)
{
    int top = top_window();
    if (top >= 0) close_window(&windows[top]);
}

static void save_top_editor(void)
{
    int top = top_window();
    if (top >= 0 && windows[top].kind == WINDOW_EDITOR)
        save_editor(&windows[top]);
}

static int finder_columns(const struct window *window)
{
    int columns = (window->w - 40) / FINDER_CELL_W;
    return columns < 1 ? 1 : columns;
}

static int finder_visible_rows(const struct window *window)
{
    int rows = (window->h - 80) / FINDER_CELL_H;
    return rows < 1 ? 1 : rows;
}

static int finder_max_scroll_row(const struct window *window)
{
    int columns = finder_columns(window);
    int total_rows = (window->item_count + columns - 1) / columns;
    int maximum = total_rows - finder_visible_rows(window);
    return maximum > 0 ? maximum : 0;
}

static void clamp_finder_scroll(struct window *window)
{
    int maximum = finder_max_scroll_row(window);
    if (window->scroll_row < 0) window->scroll_row = 0;
    if (window->scroll_row > maximum) window->scroll_row = maximum;
}

static void scroll_finder(struct window *window, int rows)
{
    window->scroll_row += rows;
    clamp_finder_scroll(window);
}

static void scroll_thumb(const struct window *window, int *thumb_y,
                         int *thumb_h)
{
    int bar_y = window->y + 20;
    int bar_h = window->h - 35;
    int track_y = bar_y + 17;
    int track_h = bar_h - 34;
    int maximum = window->kind == WINDOW_FINDER
        ? finder_max_scroll_row(window) : 0;

    *thumb_h = track_h;
    *thumb_y = track_y;
    if (maximum > 0) {
        int total_rows = maximum + finder_visible_rows(window);
        *thumb_h = track_h * finder_visible_rows(window) / total_rows;
        if (*thumb_h < 18) *thumb_h = 18;
        if (*thumb_h > track_h) *thumb_h = track_h;
        *thumb_y += window->scroll_row * (track_h - *thumb_h) / maximum;
    }
}

static void draw_scroll_bar(const struct window *window)
{
    int x = window->x + window->w - 16;
    int y = window->y + 20;
    int h = window->h - 35;
    int thumb_y;
    int thumb_h;

    scroll_thumb(window, &thumb_y, &thumb_h);
    outline_rect(x, y, 15, h, BLACK);
    fill_rect(x + 2, y + 2, 11, 13, LIGHT);
    line(x + 4, y + 10, x + 7, y + 6, BLACK);
    line(x + 7, y + 6, x + 10, y + 10, BLACK);
    fill_rect(x + 2, y + h - 15, 11, 13, LIGHT);
    line(x + 4, y + h - 10, x + 7, y + h - 6, BLACK);
    line(x + 7, y + h - 6, x + 10, y + h - 10, BLACK);
    fill_rect(x + 2, thumb_y, 11, thumb_h, WHITE);
    outline_rect(x + 2, thumb_y, 11, thumb_h, BLACK);
}

static void draw_finder_contents(const struct window *window)
{
    int x = window->x + 20;
    int y = window->y + 45;
    int columns = finder_columns(window);
    int visible = columns * finder_visible_rows(window);
    int first = window->scroll_row * columns;

    for (int slot = 0; slot < visible && first + slot < window->item_count;
         slot++) {
        int i = first + slot;
        int ix = x + (slot % columns) * FINDER_CELL_W;
        int iy = y + (slot / columns) * FINDER_CELL_H;
        char label[11];

        if (window->item_types[i] == KYLEOS_I_DIR) {
            fill_rect(ix + 2, iy + 8, 34, 24, WHITE);
            outline_rect(ix + 2, iy + 8, 34, 24, BLACK);
            fill_rect(ix + 5, iy + 4, 16, 5, WHITE);
            outline_rect(ix + 5, iy + 4, 16, 6, BLACK);
        } else {
            draw_document_icon(ix + 5, iy + 1, 0);
        }
        strncpy(label, window->item_names[i], sizeof(label) - 1);
        label[sizeof(label) - 1] = '\0';
        draw_text(ix, iy + 39, label, BLACK, WHITE, 0);
    }
    char status[64];
    snprintf(status, sizeof(status), "%d item%s          64 MB memory",
             window->item_count, window->item_count == 1 ? "" : "s");
    draw_text(x, window->y + window->h - 28, status, BLACK, WHITE, 0);
}

static int finder_item_at(const struct window *window, int px, int py)
{
    int columns = finder_columns(window);
    int visible = columns * finder_visible_rows(window);
    int first = window->scroll_row * columns;

    for (int slot = 0; slot < visible && first + slot < window->item_count;
         slot++) {
        int x = window->x + 20 + (slot % columns) * FINDER_CELL_W;
        int y = window->y + 45 + (slot / columns) * FINDER_CELL_H;
        if (in_rect(px, py, x, y, 80, 64)) return first + slot;
    }
    return -1;
}

static int text_rows_for_position(const char *text, int position, int columns,
                                  int *column)
{
    int row = 0;
    int col = 0;

    for (int i = 0; i < position; i++) {
        if (text[i] == '\n') {
            row++;
            col = 0;
        } else {
            col++;
            if (col >= columns) { row++; col = 0; }
        }
    }
    if (column != NULL) *column = col;
    return row;
}

static void editor_ensure_cursor(struct window *window)
{
    int columns = (window->w - 38) / 8;
    int rows = (window->h - 65) / 16;
    int cursor_row;

    if (columns < 1) columns = 1;
    if (rows < 1) rows = 1;
    cursor_row = text_rows_for_position(window->text, window->text_cursor,
                                        columns, NULL);
    if (cursor_row < window->scroll_row) window->scroll_row = cursor_row;
    if (cursor_row >= window->scroll_row + rows)
        window->scroll_row = cursor_row - rows + 1;
}

static int editor_position_at(const struct window *window, int px, int py)
{
    int columns = (window->w - 38) / 8;
    int target_col = (px - (window->x + 10)) / 8;
    int target_row = window->scroll_row +
        (py - (window->y + 27)) / 16;
    int row = 0;
    int col = 0;

    if (columns < 1) columns = 1;
    if (target_col < 0) target_col = 0;
    if (target_col >= columns) target_col = columns - 1;
    if (target_row < 0) target_row = 0;

    for (int position = 0; position < window->text_length; position++) {
        unsigned char ch = window->text[position];

        if (row == target_row && col >= target_col) return position;
        if (ch == '\n') {
            if (row == target_row) return position;
            row++;
            col = 0;
        } else {
            col++;
            if (col >= columns) {
                row++;
                col = 0;
            }
        }
    }
    return window->text_length;
}

static void draw_editor_contents(const struct window *window)
{
    int x = window->x + 10;
    int y = window->y + 27;
    int columns = (window->w - 38) / 8;
    int rows = (window->h - 65) / 16;
    int row = 0;
    int col = 0;
    int cursor_row;
    int cursor_col;
    char status[96];

    if (columns < 1) columns = 1;
    if (rows < 1) rows = 1;
    for (int i = 0; i < window->text_length; i++) {
        unsigned char ch = window->text[i];
        if (ch == '\n') { row++; col = 0; continue; }
        if (row >= window->scroll_row && row < window->scroll_row + rows &&
            ch >= 32 && ch < 127)
            draw_char(x + col * 8, y + (row - window->scroll_row) * 16,
                      ch, BLACK, WHITE, 0);
        col++;
        if (col >= columns) { row++; col = 0; }
    }
    cursor_row = text_rows_for_position(window->text, window->text_cursor,
                                        columns, &cursor_col);
    if (cursor_row >= window->scroll_row &&
        cursor_row < window->scroll_row + rows)
        fill_rect(x + cursor_col * 8,
                  y + (cursor_row - window->scroll_row) * 16, 2, 16, BLACK);

    if (window->save_failed)
        snprintf(status, sizeof(status), "Save failed: %s", window->path);
    else
        snprintf(status, sizeof(status), "%s   Ctrl-S Save   %s",
                 window->path, window->dirty ? "Modified" : "Saved");
    fill_rect(window->x + 5, window->y + window->h - 25,
              window->w - 23, 18, LIGHT);
    draw_text(window->x + 8, window->y + window->h - 24,
              status, BLACK, LIGHT, 0);
}

static void draw_terminal_contents(const struct window *window)
{
    char visible[TERM_MAX + COMMAND_MAX + 8];
    int length;
    int columns = (window->w - 38) / 8;
    int rows = (window->h - 48) / 16;
    int total_rows;
    int first_row;
    int row = 0;
    int col = 0;
    int x = window->x + 10;
    int y = window->y + 27;

    if (columns < 1) columns = 1;
    if (rows < 1) rows = 1;
    length = window->terminal_length;
    memcpy(visible, window->terminal, length);
    visible[length++] = '$';
    visible[length++] = ' ';
    memcpy(visible + length, window->command, window->command_length);
    length += window->command_length;
    total_rows = text_rows_for_position(visible, length, columns, NULL) + 1;
    first_row = total_rows > rows ? total_rows - rows : 0;

    for (int i = 0; i < length; i++) {
        unsigned char ch = visible[i];
        if (ch == '\r') continue;
        if (ch == '\n') { row++; col = 0; continue; }
        if (ch == '\t') ch = ' ';
        if (row >= first_row && row < first_row + rows &&
            ch >= 32 && ch < 127)
            draw_char(x + col * 8, y + (row - first_row) * 16,
                      ch, TERMINAL_GREEN, BLACK, 0);
        col++;
        if (col >= columns) { row++; col = 0; }
    }
    if (row >= first_row && row < first_row + rows)
        fill_rect(x + col * 8, y + (row - first_row) * 16,
                  2, 16, TERMINAL_GREEN);
}

static void activate_finder_item(struct window *window, int item)
{
    char path[128];

    if (item < 0 || item >= window->item_count) return;
    if (strcmp(window->path, "/") == 0)
        snprintf(path, sizeof(path), "/%s", window->item_names[item]);
    else
        snprintf(path, sizeof(path), "%s/%s", window->path,
                 window->item_names[item]);

    if (window->item_types[item] == KYLEOS_I_DIR) {
        load_finder(window, path);
    } else if (strcmp(path, "/bin/solitaire") == 0) {
        open_window(WINDOW_SOLITAIRE);
    } else if (strcmp(path, "/bin/calculator") == 0) {
        open_window(WINDOW_CALCULATOR);
    } else if (is_application(path)) {
        if (launch_application(path) < 0)
            open_item(window->item_names[item]);
    } else if (is_text_document(path)) {
        open_editor(path);
    } else {
        open_item(window->item_names[item]);
    }
}

static void draw_window_contents(const struct window *window)
{
    int x = window->x;
    int y = window->y;

    if (window->kind == WINDOW_FINDER) {
        draw_finder_contents(window);
        return;
    }
    if (window->kind == WINDOW_EDITOR) {
        draw_editor_contents(window);
        return;
    }
    if (window->kind == WINDOW_TERMINAL) {
        draw_terminal_contents(window);
        return;
    }
    if (window->kind == WINDOW_MINESWEEPER) {
        minesweeper_draw(&surface, &window->minesweeper,
                         x + 4, y + 22, window->w - 21, window->h - 39);
        return;
    }
    if (window->kind == WINDOW_SOLITAIRE) {
        solitaire_draw(&surface, &window->solitaire,
                       x + 4, y + 22, window->w - 21, window->h - 39);
        return;
    }
    if (window->kind == WINDOW_CALCULATOR) {
        calculator_draw(&surface, &window->calculator,
                        x + 4, y + 22, window->w - 21, window->h - 39);
        return;
    }
    if (window->kind == WINDOW_ABOUT) {
        draw_apple(x + 30, y + 56, BLACK, WHITE);
        draw_text(x + 75, y + 48, "KyleOS Desktop", BLACK, WHITE, 0);
        draw_text(x + 75, y + 69, "Version 0.1", BLACK, WHITE, 0);
        draw_text(x + 75, y + 98, "A small computer with big ideas.", BLACK, WHITE, 0);
        centered_text(x, y + 135, window->w, "Copyright 2026 KyleOS");
        return;
    }
    if (window->kind == WINDOW_README) {
        draw_text(x + 24, y + 43, "Welcome to the KyleOS Desktop.", BLACK, WHITE, 0);
        draw_text(x + 24, y + 70, "- Drag a window by its striped title bar.", BLACK, WHITE, 0);
        draw_text(x + 24, y + 92, "- Drag the lower-right grip to resize.", BLACK, WHITE, 0);
        draw_text(x + 24, y + 114, "- Finder scrolls with its bar or Page Up/Down.", BLACK, WHITE, 0);
        draw_text(x + 24, y + 136, "- Click desktop or Finder icons to open them.", BLACK, WHITE, 0);
        draw_text(x + 24, y + 158, "- Use File > Quit to return to the shell.", BLACK, WHITE, 0);
        draw_text(x + 24, y + 180, "Editor: Ctrl-S saves. Terminal: Enter runs.", BLACK, WHITE, 0);
        draw_text(x + 24, y + 202, "Alt-Q also quits the desktop.", BLACK, WHITE, 0);
        return;
    }
    if (window->kind == WINDOW_DUKE) {
        draw_text(x + 24, y + 45, "Duke Nukem 3D", BLACK, WHITE, 0);
        draw_text(x + 24, y + 74, "Installed at /usr/bin/duke3d", BLACK, WHITE, 0);
        draw_text(x + 24, y + 104, "Close the desktop and type duke3d", BLACK, WHITE, 0);
        draw_text(x + 24, y + 126, "at the shell to launch the game.", BLACK, WHITE, 0);
        return;
    }
    draw_text(x + 24, y + 48, window->title, BLACK, WHITE, 0);
    draw_text(x + 24, y + 78, "This KyleOS item is available.", BLACK, WHITE, 0);
    draw_text(x + 24, y + 108,
              "Application launching will be added next.", BLACK, WHITE, 0);
}

static void draw_window(const struct window *window, int active)
{
    int x = window->x;
    int y = window->y;
    int w = window->w;
    int h = window->h;

    kui_draw_window(&surface, x, y, w, h, window->title, active);
    fill_rect(x + 6, y + 5, 12, 12, MID);
    outline_rect(x + 6, y + 5, 12, 12, BLACK);
    fill_rect(x + 7, y + 6, 10, 1, WHITE);
    fill_rect(x + 7, y + 6, 1, 10, WHITE);
    fill_rect(x + 8, y + 14, 8, 1, DARK);
    fill_rect(x + 16, y + 7, 1, 9, DARK);
    fill_rect(x + 9, y + 11, 6, 2, BLACK);
    fill_rect(x + 4, y + 22, w - 21, h - 39,
              window->kind == WINDOW_TERMINAL ? BLACK : WHITE);
    outline_rect(x + 3, y + 21, w - 19, h - 37, DARK);
    if (window->kind == WINDOW_FINDER) draw_scroll_bar(window);
    fill_rect(x + w - 15, y + h - 15, 13, 13, MID);
    for (int offset = 2; offset < 12; offset += 3)
        line(x + w - 15 + offset, y + h - 2,
             x + w - 2, y + h - 15 + offset, BLACK);
    draw_window_contents(window);
}

static void menu_shadow(int x, int y, int w, int h)
{
    fill_rect(x + 4, y + 4, w, h, BLACK);
    kui_draw_bevel(&surface, x, y, w, h, MID, 0);
}

static void draw_open_menu(void)
{
    if (open_menu == MENU_NONE) return;
    if (open_menu == MENU_APPLE) {
        menu_shadow(2, MENU_H - 2, 176, 32);
        draw_text(12, MENU_H + 5, "About KyleOS...", BLACK, WHITE, 0);
    } else if (open_menu == MENU_FILE) {
        menu_shadow(31, MENU_H - 2, 192, 154);
        draw_text(42, MENU_H + 5, "New Finder Window", BLACK, WHITE, 0);
        draw_text(42, MENU_H + 29, "New Text Document", BLACK, WHITE, 0);
        draw_text(42, MENU_H + 53, "New Terminal", BLACK, WHITE, 0);
        draw_text(42, MENU_H + 77, "Save", BLACK, WHITE, 0);
        draw_text(42, MENU_H + 101, "Close Window", BLACK, WHITE, 0);
        fill_rect(32, MENU_H + 124, 190, 1, BLACK);
        draw_text(42, MENU_H + 130, "Quit to Shell", BLACK, WHITE, 0);
    } else if (open_menu == MENU_VIEW) {
        menu_shadow(80, MENU_H - 2, 144, 56);
        draw_text(91, MENU_H + 5, "by Icon", BLACK, WHITE, 0);
        draw_text(91, MENU_H + 29, "by Name", BLACK, WHITE, 0);
    } else {
        menu_shadow(132, MENU_H - 2, 186, 34);
        draw_text(143, MENU_H + 5, "Clean Up Desktop", BLACK, WHITE, 0);
    }
}

static void draw_cursor(void)
{
    static const uint16_t shape[16] = {
        0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00,
        0xff80, 0xfc00, 0xdc00, 0x8e00, 0x0600, 0x0700, 0x0300, 0x0000
    };
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 10; x++) {
            if (shape[y] & (0x8000 >> x)) {
                int edge = x == 0 || y == 0 ||
                    !(shape[y] & (0x8000 >> (x - 1))) ||
                    (y > 0 && !(shape[y - 1] & (0x8000 >> x)));
                pixel(cursor_x + x, cursor_y + y, edge ? WHITE : BLACK);
            }
        }
    }
}

static void render(void)
{
    desktop_pattern();
    draw_menu_bar();
    for (int i = 0; i < ICON_COUNT; i++) draw_icon(i);
    int active = top_window();
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].visible) draw_window(&windows[i], i == active);
    draw_open_menu();
    draw_cursor();
    framebuffer_present(frame, frame_bytes);
}

static int icon_at(int x, int y)
{
    for (int i = 0; i < ICON_COUNT; i++)
        if (in_rect(x, y, icons[i].x - 5, icons[i].y - 5, 70, 65)) return i;
    return -1;
}

static int window_at(int x, int y)
{
    for (int i = MAX_WINDOWS - 1; i >= 0; i--)
        if (windows[i].visible &&
            in_rect(x, y, windows[i].x, windows[i].y,
                    windows[i].w, windows[i].h)) return i;
    return -1;
}

static void clean_up_icons(void)
{
    selected_icon = -1;
}

static void choose_menu(int x)
{
    if (x < 31) open_menu = MENU_APPLE;
    else if (x < 80) open_menu = MENU_FILE;
    else if (x < 132) open_menu = MENU_VIEW;
    else if (x < 218) open_menu = MENU_SPECIAL;
    else open_menu = MENU_NONE;
}

static void activate_menu_item(int x, int y)
{
    (void)x;
    if (open_menu == MENU_APPLE && y >= MENU_H && y < MENU_H + 32)
        open_window(WINDOW_ABOUT);
    else if (open_menu == MENU_FILE) {
        if (y < MENU_H + 25) open_finder("/");
        else if (y < MENU_H + 49) activate_desktop_icon(4);
        else if (y < MENU_H + 73) open_terminal();
        else if (y < MENU_H + 97) save_top_editor();
        else if (y < MENU_H + 125) close_top_window();
        else if (y < MENU_H + 154) running = 0;
    } else if (open_menu == MENU_SPECIAL && y < MENU_H + 34) {
        clean_up_icons();
    }
    open_menu = MENU_NONE;
}

static void mouse_down(void)
{
    if (cursor_y < MENU_H) {
        choose_menu(cursor_x);
        return;
    }
    if (open_menu != MENU_NONE) {
        activate_menu_item(cursor_x, cursor_y);
        return;
    }

    int index = window_at(cursor_x, cursor_y);
    if (index >= 0) {
        index = raise_window(index);
        struct window *window = &windows[index];
        if (in_rect(cursor_x, cursor_y, window->x + 4, window->y + 3, 17, 16)) {
            close_window(window);
        } else if (in_rect(cursor_x, cursor_y,
                           window->x + window->w - 17,
                           window->y + window->h - 17, 17, 17)) {
            resize_window = index;
            resize_right_gap = window->x + window->w - cursor_x;
            resize_bottom_gap = window->y + window->h - cursor_y;
        } else if (in_rect(cursor_x, cursor_y, window->x, window->y,
                           window->w, 21)) {
            drag_window = index;
            drag_dx = cursor_x - window->x;
            drag_dy = cursor_y - window->y;
        } else if (window->kind == WINDOW_FINDER) {
            int bar_x = window->x + window->w - 16;
            int bar_y = window->y + 20;
            int bar_h = window->h - 35;

            if (in_rect(cursor_x, cursor_y, bar_x, bar_y, 15, bar_h)) {
                int thumb_y;
                int thumb_h;
                scroll_thumb(window, &thumb_y, &thumb_h);
                if (cursor_y < bar_y + 17) scroll_finder(window, -1);
                else if (cursor_y >= bar_y + bar_h - 17)
                    scroll_finder(window, 1);
                else if (cursor_y >= thumb_y &&
                         cursor_y < thumb_y + thumb_h) {
                    scroll_window = index;
                    scroll_drag_offset = cursor_y - thumb_y;
                }
                else if (cursor_y < thumb_y)
                    scroll_finder(window, -finder_visible_rows(window));
                else if (cursor_y >= thumb_y + thumb_h)
                    scroll_finder(window, finder_visible_rows(window));
            } else {
                int item = finder_item_at(window, cursor_x, cursor_y);
                if (item >= 0) activate_finder_item(window, item);
            }
        } else if (window->kind == WINDOW_EDITOR &&
                   in_rect(cursor_x, cursor_y, window->x + 5,
                           window->y + 22, window->w - 22,
                           window->h - 49)) {
            window->text_cursor = editor_position_at(window,
                                                     cursor_x, cursor_y);
            editor_ensure_cursor(window);
        } else if (window->kind == WINDOW_MINESWEEPER) {
            minesweeper_click(&window->minesweeper,
                              window->x + 4, window->y + 22,
                              window->w - 21, window->h - 39,
                              cursor_x, cursor_y, 0,
                              kui_ticks_ms() ^ (uint32_t)cursor_x);
        } else if (window->kind == WINDOW_SOLITAIRE) {
            if (solitaire_process_request(window, SOLITAIRE_COMMAND_CLICK,
                                          cursor_x, cursor_y) < 0)
                close_window(window);
        } else if (window->kind == WINDOW_CALCULATOR) {
            if (calculator_process_request(window, CALCULATOR_COMMAND_CLICK,
                                           cursor_x, cursor_y, 0) < 0)
                close_window(window);
        }
        return;
    }

    index = icon_at(cursor_x, cursor_y);
    if (index >= 0) {
        selected_icon = index;
        activate_desktop_icon(index);
    } else {
        selected_icon = -1;
    }
}

static void mouse_right_down(void)
{
    int index = window_at(cursor_x, cursor_y);

    if (index < 0) return;
    index = raise_window(index);
    struct window *window = &windows[index];
    if (window->kind == WINDOW_MINESWEEPER)
        minesweeper_click(&window->minesweeper,
                          window->x + 4, window->y + 22,
                          window->w - 21, window->h - 39,
                          cursor_x, cursor_y, 1,
                          kui_ticks_ms() ^ (uint32_t)cursor_y);
}

static int handle_mouse(void)
{
    int changed = 0;
    int dx;
    int dy;
    uint8_t buttons;

    while (mouse_poll(&dx, &dy, &buttons) > 0) {
        changed = 1;
        cursor_x += pointer_delta(dx);
        cursor_y -= pointer_delta(dy);
        if (cursor_x < 0) cursor_x = 0;
        if (cursor_y < 0) cursor_y = 0;
        if (cursor_x > WIDTH - 11) cursor_x = WIDTH - 11;
        if (cursor_y > HEIGHT - 17) cursor_y = HEIGHT - 17;

        if ((buttons & 1) && !(mouse_buttons & 1)) mouse_down();
        if ((buttons & 2) && !(mouse_buttons & 2)) mouse_right_down();
        if (!(buttons & 1) && (mouse_buttons & 1)) {
            drag_window = -1;
            resize_window = -1;
            scroll_window = -1;
        }
        mouse_buttons = buttons;

        if (drag_window >= 0 && windows[drag_window].visible) {
            struct window *window = &windows[drag_window];
            window->x = cursor_x - drag_dx;
            window->y = cursor_y - drag_dy;
            if (window->x < 0) window->x = 0;
            if (window->x + window->w > WIDTH) window->x = WIDTH - window->w;
            if (window->y < MENU_H) window->y = MENU_H;
            if (window->y + window->h > HEIGHT) window->y = HEIGHT - window->h;
        }
        if (resize_window >= 0 && windows[resize_window].visible) {
            struct window *window = &windows[resize_window];
            int min_w = window->kind == WINDOW_ABOUT ? 340 :
                        window->kind == WINDOW_SOLITAIRE ? 340 :
                        window->kind == WINDOW_CALCULATOR ? 300 :
                        MIN_WINDOW_W;
            int min_h = window->kind == WINDOW_ABOUT ? 184 :
                        window->kind == WINDOW_SOLITAIRE ? 265 :
                        window->kind == WINDOW_CALCULATOR ? 340 :
                        window->kind == WINDOW_MINESWEEPER ? 220 :
                        MIN_WINDOW_H;
            int width = cursor_x - window->x + resize_right_gap;
            int height = cursor_y - window->y + resize_bottom_gap;

            if (width < min_w) width = min_w;
            if (height < min_h) height = min_h;
            if (window->x + width > WIDTH) width = WIDTH - window->x;
            if (window->y + height > HEIGHT) height = HEIGHT - window->y;
            window->w = width;
            window->h = height;
            if (window->kind == WINDOW_FINDER) clamp_finder_scroll(window);
            if (window->kind == WINDOW_EDITOR) editor_ensure_cursor(window);
        }
        if (scroll_window >= 0 && windows[scroll_window].visible) {
            struct window *window = &windows[scroll_window];
            int thumb_y;
            int thumb_h;
            int track_y = window->y + 37;
            int track_h = window->h - 69;
            int maximum = finder_max_scroll_row(window);
            int travel;
            int position;

            scroll_thumb(window, &thumb_y, &thumb_h);
            travel = track_h - thumb_h;
            position = cursor_y - scroll_drag_offset - track_y;
            if (position < 0) position = 0;
            if (position > travel) position = travel;
            if (maximum > 0 && travel > 0)
                window->scroll_row =
                    (position * maximum + travel / 2) / travel;
        }
    }
    return changed;
}

static void editor_insert(struct window *window, char ch)
{
    if (window->text_length >= EDIT_MAX - 1) return;
    memmove(window->text + window->text_cursor + 1,
            window->text + window->text_cursor,
            window->text_length - window->text_cursor);
    window->text[window->text_cursor++] = ch;
    window->text_length++;
    window->dirty = 1;
    window->save_failed = 0;
    editor_ensure_cursor(window);
}

static void handle_editor_key(struct window *window, uint8_t code,
                              uint8_t extended, int ctrl, int shift)
{
    char ch;

    if (ctrl && !extended && code == 31) {
        save_editor(window);
        return;
    }
    if (extended && code == 75 && window->text_cursor > 0)
        window->text_cursor--;
    else if (extended && code == 77 &&
             window->text_cursor < window->text_length)
        window->text_cursor++;
    else if (extended && code == 72)
        window->text_cursor = move_text_vertical(window->text,
            window->text_length, window->text_cursor, -1);
    else if (extended && code == 80)
        window->text_cursor = move_text_vertical(window->text,
            window->text_length, window->text_cursor, 1);
    else if (!extended && code == 14 && window->text_cursor > 0) {
        memmove(window->text + window->text_cursor - 1,
                window->text + window->text_cursor,
                window->text_length - window->text_cursor);
        window->text_cursor--;
        window->text_length--;
        window->dirty = 1;
        window->save_failed = 0;
    } else if (extended && code == 83 &&
               window->text_cursor < window->text_length) {
        memmove(window->text + window->text_cursor,
                window->text + window->text_cursor + 1,
                window->text_length - window->text_cursor - 1);
        window->text_length--;
        window->dirty = 1;
        window->save_failed = 0;
    } else if (!extended && code == 28) {
        editor_insert(window, '\n');
    } else if (!extended && code == 15) {
        for (int i = 0; i < 4; i++) editor_insert(window, ' ');
    } else if (!ctrl && !extended && (ch = scan_to_char(code, shift)) != 0) {
        editor_insert(window, ch);
    }
    editor_ensure_cursor(window);
}

static void handle_terminal_key(struct window *window, uint8_t code,
                                uint8_t extended, int ctrl, int shift)
{
    char ch;

    if (ctrl && !extended && code == 38) {
        window->terminal_length = 0;
        window->terminal[0] = '\0';
        return;
    }
    if (!extended && code == 14 && window->command_length > 0) {
        window->command[--window->command_length] = '\0';
    } else if (!extended && code == 28) {
        terminal_execute(window);
    } else if (!ctrl && !extended &&
               (ch = scan_to_char(code, shift)) != 0 &&
               window->command_length < COMMAND_MAX - 1) {
        window->command[window->command_length++] = ch;
        window->command[window->command_length] = '\0';
    }
}

static int handle_keyboard(void)
{
    static int alt_down;
    static int ctrl_down;
    static int shift_down;
    int changed = 0;
    int pressed;
    uint8_t code;
    uint8_t extended;

    while (key_poll(&pressed, &code, &extended) > 0) {
        if (!extended && code == 56) { alt_down = pressed; continue; }
        if (!extended && code == 29) { ctrl_down = pressed; continue; }
        if (!extended && (code == 42 || code == 54)) {
            shift_down = pressed;
            continue;
        }
        if (!pressed) continue;
        changed = 1;

        if (!extended && code == 16 && alt_down) {
            running = 0;
            continue;
        }
        int top = top_window();
        if (top >= 0 && windows[top].kind == WINDOW_EDITOR && code != 1) {
            handle_editor_key(&windows[top], code, extended,
                              ctrl_down, shift_down);
            continue;
        }
        if (top >= 0 && windows[top].kind == WINDOW_TERMINAL && code != 1) {
            handle_terminal_key(&windows[top], code, extended,
                                ctrl_down, shift_down);
            continue;
        }
        if (top >= 0 && windows[top].kind == WINDOW_MINESWEEPER &&
            !extended && code == 19) {
            minesweeper_reset(&windows[top].minesweeper, kui_ticks_ms());
            continue;
        }
        if (top >= 0 && windows[top].kind == WINDOW_SOLITAIRE &&
            !extended && code == 19) {
            if (solitaire_process_request(&windows[top],
                                          SOLITAIRE_COMMAND_RESET,
                                          0, 0) < 0)
                close_window(&windows[top]);
            continue;
        }
        if (top >= 0 && windows[top].kind == WINDOW_CALCULATOR &&
            !extended && code != 1) {
            char key = code == 14 ? '\b' : code == 28 ? '=' :
                       scan_to_char(code, shift_down);
            if (key != 0 &&
                calculator_process_request(&windows[top],
                                           CALCULATOR_COMMAND_KEY,
                                           0, 0, key) < 0)
                close_window(&windows[top]);
            continue;
        }

        if (extended && code == 75) cursor_x -= 8;
        else if (extended && code == 77) cursor_x += 8;
        else if (extended && code == 72) cursor_y -= 8;
        else if (extended && code == 80) cursor_y += 8;
        else if (!extended && code == 15)
            selected_icon = (selected_icon + 1) % ICON_COUNT;
        else if (!extended && code == 28 && selected_icon >= 0) {
            activate_desktop_icon(selected_icon);
        }
        else if (!extended && code == 1) {
            if (open_menu != MENU_NONE) open_menu = MENU_NONE;
            else close_top_window();
        } else if (!extended && code == 59) {
            open_window(WINDOW_ABOUT);
        } else if (extended && (code == 73 || code == 81)) {
            top = top_window();
            if (top >= 0 && windows[top].kind == WINDOW_FINDER) {
                int amount = finder_visible_rows(&windows[top]);
                scroll_finder(&windows[top], code == 73 ? -amount : amount);
            }
        }

        if (cursor_x < 0) cursor_x = 0;
        if (cursor_y < 0) cursor_y = 0;
        if (cursor_x > WIDTH - 11) cursor_x = WIDTH - 11;
        if (cursor_y > HEIGHT - 17) cursor_y = HEIGHT - 17;
    }
    return changed;
}

static void restore_console(void)
{
    tty_raw(0);
    if (frame != NULL) memset(frame, 0, frame_bytes);
    if (frame != NULL) framebuffer_present(frame, frame_bytes);
    printf("\033[2J\033[H");
    fflush(stdout);
}

int main(void)
{
    if (kui_screen_size(&screen_width, &screen_height) < 0 ||
        screen_width < 320 || screen_height < 200 ||
        screen_width > 4096 || screen_height > 2160) {
        screen_width = KUI_SCREEN_WIDTH;
        screen_height = KUI_SCREEN_HEIGHT;
    }
    frame_bytes = (uint32_t)screen_width * screen_height * sizeof(uint32_t);
    frame = calloc((size_t)screen_width * screen_height, sizeof(uint32_t));
    if (frame == NULL) {
        fprintf(stderr, "desktop: cannot allocate framebuffer\n");
        return 1;
    }
    kui_surface_init(&surface, frame, WIDTH, HEIGHT);
    memset(windows, 0, sizeof(windows));
    layout_desktop_icons();
    cursor_x = WIDTH / 2;
    cursor_y = HEIGHT / 2;
    tty_raw(1);
    atexit(restore_console);
    open_finder("/");
    render();

    while (running) {
        int changed = handle_mouse();
        changed |= handle_keyboard();
        if (changed) render();
        else sleep_ms(10);
    }
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].visible) stop_window_process(&windows[i]);
    return 0;
}
