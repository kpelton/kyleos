#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"
#include "extras/kui/kui_menu.h"
#include "extras/minesweeper/minesweeper.h"

enum { MINESWEEPER_MENU_NEW_GAME = 1 };

static const struct kui_menu_item minesweeper_game_items[] = {
    { "New Game", "R", MINESWEEPER_MENU_NEW_GAME, 1 }
};

static const struct kui_menu minesweeper_menus[] = {
    { "Game", minesweeper_game_items, 1 }
};

static uint32_t *framebuffer;
static uint32_t framebuffer_bytes;
static struct kui_surface surface;
static struct minesweeper_game game;
static int screen_width;
static int screen_height;
static int window_x;
static int window_y;
static int window_w;
static int window_h;
static int cursor_x;
static int cursor_y;
static uint8_t old_buttons;
static int running = 1;

static int embedded_main(void)
{
    struct minesweeper_game embedded_game;
    struct kui_menu_state menu_state;
    struct kui_client client;
    struct kui_event event;

    memset(&embedded_game, 0, sizeof(embedded_game));
    kui_menu_init(&menu_state);
    kui_client_init(&client);
    for (;;) {
        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        if (event.type == KUI_EVENT_INIT) {
            minesweeper_reset(&embedded_game, event.seed);
            kui_menu_close(&menu_state);
        } else {
            int consumed = 0;
            int action = kui_menu_handle(&menu_state, &event,
                                         minesweeper_menus, 1, &consumed);

            if (action == MINESWEEPER_MENU_NEW_GAME ||
                (!consumed && event.type == KUI_EVENT_KEY &&
                 event.key_pressed && !event.key_extended &&
                 event.key_code == 19)) {
                minesweeper_reset(&embedded_game, event.seed);
            } else if (!consumed && event.type == KUI_EVENT_POINTER &&
                       event.button != 0) {
                minesweeper_click(&embedded_game, 0, KUI_MENU_BAR_HEIGHT,
                                  event.width,
                                  event.height - KUI_MENU_BAR_HEIGHT,
                                  event.x, event.y, event.button == 2,
                                  event.seed);
            }
        }
        kui_client_begin(&client, &event, "Minesweeper");
        kui_client_set_constraints(&client, 239, 204, 519, 604);
        minesweeper_draw(&client.surface, &embedded_game,
                         0, KUI_MENU_BAR_HEIGHT, event.width,
                         event.height - KUI_MENU_BAR_HEIGHT);
        kui_menu_render(&client.surface, &menu_state,
                        minesweeper_menus, 1);
        if (kui_client_send(&client) < 0) break;
    }
    kui_client_free(&client);
    return 0;
}

static void draw_cursor(void)
{
    static const uint16_t shape[16] = {
        0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00,
        0xff80, 0xfc00, 0xdc00, 0x8e00, 0x0600, 0x0700, 0x0300, 0x0000
    };

    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 10; x++)
            if (shape[y] & (0x8000 >> x))
                kui_pixel(&surface, cursor_x + x, cursor_y + y,
                          x == 0 || y == 0 ? KUI_WHITE : KUI_BLACK);
}

static void render(void)
{
    kui_draw_desktop(&surface, 0);
    kui_draw_window(&surface, window_x, window_y, window_w, window_h,
                    "KyleOS Minesweeper", 1);
    kui_draw_button(&surface, window_x + 5, window_y + 4, 16, 14, "X", 0);
    minesweeper_draw(&surface, &game, window_x + 4, window_y + 22,
                     window_w - 8, window_h - 27);
    draw_cursor();
    kui_present(&surface);
}

static void click_at(int pointer_x, int pointer_y, int right_button)
{
    if (kui_in_rect(pointer_x, pointer_y,
                    window_x + 5, window_y + 4, 16, 14)) {
        running = 0;
        return;
    }
    minesweeper_click(&game, window_x + 4, window_y + 22,
                      window_w - 8, window_h - 27,
                      pointer_x, pointer_y, right_button,
                      kui_ticks_ms() ^ ((uint32_t)pointer_x << 16) ^
                      (uint32_t)pointer_y);
}

static void restore_console(void)
{
    kui_tty_raw(0);
    memset(framebuffer, 0, framebuffer_bytes);
    kui_present(&surface);
    printf("\033[2J\033[H");
    fflush(stdout);
}

int main(int argc, char **argv)
{
    if (argc > 1 && (strcmp(argv[1], "--embedded") == 0 ||
                     strcmp(argv[1], "--kui-client") == 0))
        return embedded_main();
    if (kui_screen_size(&screen_width, &screen_height) < 0) {
        screen_width = KUI_SCREEN_WIDTH;
        screen_height = KUI_SCREEN_HEIGHT;
    }
    framebuffer_bytes =
        (uint32_t)screen_width * screen_height * sizeof(uint32_t);
    framebuffer = calloc((size_t)screen_width * screen_height,
                         sizeof(uint32_t));
    if (framebuffer == NULL) return 1;
    window_w = screen_width >= 800 ? 440 : 330;
    window_h = screen_height >= 600 ? 500 : 348;
    if (window_w > screen_width - 20) window_w = screen_width - 20;
    if (window_h > screen_height - 20) window_h = screen_height - 20;
    window_x = (screen_width - window_w) / 2;
    window_y = (screen_height - window_h) / 2;
    cursor_x = screen_width / 2;
    cursor_y = screen_height / 2;
    kui_surface_init(&surface, framebuffer,
                     screen_width, screen_height);
    kui_tty_raw(1);
    atexit(restore_console);
    minesweeper_reset(&game, kui_ticks_ms());
    render();

    while (running) {
        int changed = 0;
        int dx;
        int dy;
        int pressed;
        uint8_t buttons;
        uint8_t code;
        uint8_t extended;

        while (kui_mouse_poll(&dx, &dy, &buttons) > 0) {
            cursor_x += kui_pointer_delta(dx);
            cursor_y -= kui_pointer_delta(dy);
            if (cursor_x < 0) cursor_x = 0;
            if (cursor_y < 0) cursor_y = 0;
            if (cursor_x > screen_width - 11)
                cursor_x = screen_width - 11;
            if (cursor_y > screen_height - 17)
                cursor_y = screen_height - 17;
            if ((buttons & 1) && !(old_buttons & 1))
                click_at(cursor_x, cursor_y, 0);
            if ((buttons & 2) && !(old_buttons & 2))
                click_at(cursor_x, cursor_y, 1);
            old_buttons = buttons;
            changed = 1;
        }
        while (kui_key_poll(&pressed, &code, &extended) > 0) {
            (void)extended;
            if (!pressed) continue;
            if (code == 1) running = 0;
            else if (code == 19)
                minesweeper_reset(&game, kui_ticks_ms());
            changed = 1;
        }
        if (changed && running) render();
        else kui_sleep_ms(10);
    }
    return 0;
}
