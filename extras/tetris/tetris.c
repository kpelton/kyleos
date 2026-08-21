#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"
#include "extras/kui/kui_menu.h"
#include "extras/tetris/tetris_game.h"

#define TETRIS_TILE_SIZE 16
#define TETRIS_BOARD_PIXELS_WIDTH (TETRIS_WIDTH * TETRIS_TILE_SIZE)
#define TETRIS_BOARD_PIXELS_HEIGHT (TETRIS_HEIGHT * TETRIS_TILE_SIZE)

enum tetris_menu_action {
    TETRIS_MENU_NEW = 1,
    TETRIS_MENU_PAUSE
};

static const struct kui_menu_item game_items[] = {
    { "New Game", "R", TETRIS_MENU_NEW, 1 },
    { "Pause / Resume", "P", TETRIS_MENU_PAUSE, 1 }
};

static const struct kui_menu menus[] = {
    { "Game", game_items, 2 }
};

static const uint32_t fallback_colors[TETRIS_PIECES] = {
    0x002cc9e8U, 0x00f2cf3fU, 0x009d62d5U, 0x005dcf69U,
    0x00e85a55U, 0x004f79d8U, 0x00ef983cU
};

static void draw_fallback_tile(struct kui_surface *surface,
                               int x, int y, int piece)
{
    uint32_t color = fallback_colors[piece];

    kui_fill_rect(surface, x, y, TETRIS_TILE_SIZE, TETRIS_TILE_SIZE,
                  0x00131d2bU);
    kui_fill_rect(surface, x + 1, y + 1,
                  TETRIS_TILE_SIZE - 2, TETRIS_TILE_SIZE - 2, color);
    kui_fill_rect(surface, x + 2, y + 2,
                  TETRIS_TILE_SIZE - 4, 2, 0x00ffffffU);
    kui_fill_rect_alpha(surface, x + 3, y + 4,
                        TETRIS_TILE_SIZE - 6, TETRIS_TILE_SIZE - 8,
                        0x30000000U);
}

static void draw_tile(struct kui_surface *surface,
                      const struct kui_image tiles[TETRIS_PIECES],
                      int x, int y, int piece)
{
    if (piece >= 0 && piece < TETRIS_PIECES && tiles[piece].pixels != NULL)
        kui_blit_image_scaled(surface, &tiles[piece], x, y,
                              TETRIS_TILE_SIZE, TETRIS_TILE_SIZE);
    else
        draw_fallback_tile(surface, x, y, piece);
}

static void draw_shape(struct kui_surface *surface,
                       const struct kui_image tiles[TETRIS_PIECES],
                       int piece, int rotation, int origin_x, int origin_y)
{
    uint16_t mask = tetris_shape_mask(piece, rotation);

    for (int row = 0; row < 4; row++)
        for (int column = 0; column < 4; column++)
            if (mask & (1U << (row * 4 + column)))
                draw_tile(surface, tiles,
                          origin_x + column * TETRIS_TILE_SIZE,
                          origin_y + row * TETRIS_TILE_SIZE, piece);
}

static void compose_board(struct kui_surface *board,
                          const struct tetris_game *game,
                          const struct kui_image tiles[TETRIS_PIECES])
{
    int ghost_y = tetris_drop_y(game);
    uint16_t mask = tetris_shape_mask(game->piece, game->rotation);

    kui_fill_rect(board, 0, 0, board->width, board->height, 0x000b1524U);
    for (int row = 0; row < TETRIS_HEIGHT; row++)
        for (int column = 0; column < TETRIS_WIDTH; column++) {
            int x = column * TETRIS_TILE_SIZE;
            int y = row * TETRIS_TILE_SIZE;

            kui_outline_rect(board, x, y, TETRIS_TILE_SIZE,
                             TETRIS_TILE_SIZE, 0x001a2a3dU);
            if (game->board[row][column] != 0)
                draw_tile(board, tiles, x, y,
                          game->board[row][column] - 1);
        }
    if (!game->game_over) {
        for (int row = 0; row < 4; row++)
            for (int column = 0; column < 4; column++) {
                int board_x;
                int board_y;

                if (!(mask & (1U << (row * 4 + column)))) continue;
                board_x = game->x + column;
                board_y = ghost_y + row;
                if (board_y >= 0)
                    kui_outline_rect(board,
                        board_x * TETRIS_TILE_SIZE + 3,
                        board_y * TETRIS_TILE_SIZE + 3,
                        TETRIS_TILE_SIZE - 6, TETRIS_TILE_SIZE - 6,
                        0x008da2baU);
            }
        draw_shape(board, tiles, game->piece, game->rotation,
                   game->x * TETRIS_TILE_SIZE,
                   game->y * TETRIS_TILE_SIZE);
    }
}

static void draw_game(struct kui_client *client, const struct kui_event *event,
                      const struct tetris_game *game,
                      struct kui_surface *board,
                      const struct kui_image tiles[TETRIS_PIECES])
{
    char text[48];
    int board_x = 24;
    int board_y = KUI_MENU_BAR_HEIGHT + 12;
    int panel_x = board_x + TETRIS_BOARD_PIXELS_WIDTH + 24;

    compose_board(board, game, tiles);
    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  0x0017263aU);
    kui_fill_rect(&client->surface, board_x - 5, board_y - 5,
                  TETRIS_BOARD_PIXELS_WIDTH + 10,
                  TETRIS_BOARD_PIXELS_HEIGHT + 10, 0x003b526dU);
    kui_draw_bitmap_scaled(&client->surface, board->pixels,
                           board->width, board->height,
                           board_x, board_y,
                           TETRIS_BOARD_PIXELS_WIDTH,
                           TETRIS_BOARD_PIXELS_HEIGHT);
    kui_outline_rect(&client->surface, board_x - 1, board_y - 1,
                     TETRIS_BOARD_PIXELS_WIDTH + 2,
                     TETRIS_BOARD_PIXELS_HEIGHT + 2, 0x008ba4c0U);

    kui_draw_text(&client->surface, panel_x, board_y + 4,
                  "NEXT", 0x00e8f2ffU, 0x0017263aU, 0);
    kui_fill_rect(&client->surface, panel_x, board_y + 26,
                  82, 82, 0x000b1524U);
    kui_outline_rect(&client->surface, panel_x, board_y + 26,
                     82, 82, 0x005c7591U);
    draw_shape(&client->surface, tiles, game->next_piece, 0,
               panel_x + 9, board_y + 35);

    snprintf(text, sizeof(text), "SCORE  %d", game->score);
    kui_draw_text(&client->surface, panel_x, board_y + 128,
                  text, 0x00ffffffU, 0x0017263aU, 0);
    snprintf(text, sizeof(text), "LINES  %d", game->lines);
    kui_draw_text(&client->surface, panel_x, board_y + 150,
                  text, 0x00dce9f7U, 0x0017263aU, 0);
    snprintf(text, sizeof(text), "LEVEL  %d", game->level);
    kui_draw_text(&client->surface, panel_x, board_y + 172,
                  text, 0x00dce9f7U, 0x0017263aU, 0);
    kui_draw_text(&client->surface, panel_x, board_y + 214,
                  "Arrows: move", 0x00b6c9dcU, 0x0017263aU, 0);
    kui_draw_text(&client->surface, panel_x, board_y + 234,
                  "Up/Z: rotate", 0x00b6c9dcU, 0x0017263aU, 0);
    kui_draw_text(&client->surface, panel_x, board_y + 254,
                  "Space: drop", 0x00b6c9dcU, 0x0017263aU, 0);
    kui_draw_text(&client->surface, panel_x, board_y + 274,
                  "P: pause", 0x00b6c9dcU, 0x0017263aU, 0);
    kui_draw_text(&client->surface, panel_x, board_y + 294,
                  "R: restart", 0x00b6c9dcU, 0x0017263aU, 0);

    if (game->paused || game->game_over) {
        const char *message = game->game_over ? "GAME OVER" : "PAUSED";
        int width = kui_text_width(message);

        kui_fill_rect_alpha(&client->surface, board_x + 12,
                            board_y + 140,
                            TETRIS_BOARD_PIXELS_WIDTH - 24, 42,
                            0xd0000000U);
        kui_draw_text(&client->surface,
                      board_x + (TETRIS_BOARD_PIXELS_WIDTH - width) / 2,
                      board_y + 153, message,
                      0x00ffffffU, KUI_BLACK, 0);
    }
}

static void load_tiles(struct kui_image tiles[TETRIS_PIECES])
{
    char path[64];

    memset(tiles, 0, sizeof(struct kui_image) * TETRIS_PIECES);
    for (int piece = 0; piece < TETRIS_PIECES; piece++) {
        snprintf(path, sizeof(path), "/usr/share/gui/tetris-block-%d.kimg",
                 piece + 1);
        if (kui_image_load(path, &tiles[piece]) < 0) {
            snprintf(path, sizeof(path),
                     "image/rootfs/usr/share/gui/tetris-block-%d.kimg",
                     piece + 1);
            kui_image_load(path, &tiles[piece]);
        }
    }
}

int main(void)
{
    struct kui_image tiles[TETRIS_PIECES];
    struct tetris_game game;
    struct kui_menu_state menu;
    struct kui_client client;
    struct kui_event event;
    struct kui_surface board;
    uint32_t *board_pixels;

    board_pixels = calloc((size_t)TETRIS_BOARD_PIXELS_WIDTH *
                          TETRIS_BOARD_PIXELS_HEIGHT, sizeof(uint32_t));
    if (board_pixels == NULL) return 1;
    kui_surface_init(&board, board_pixels,
                     TETRIS_BOARD_PIXELS_WIDTH,
                     TETRIS_BOARD_PIXELS_HEIGHT);
    load_tiles(tiles);
    memset(&game, 0, sizeof(game));
    kui_menu_init(&menu);
    kui_client_init(&client);
    for (;;) {
        int consumed = 0;
        int action;

        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        if (event.type == KUI_EVENT_INIT) {
            tetris_reset(&game, event.seed);
            kui_menu_close(&menu);
        } else {
            action = kui_menu_handle(&menu, &event, menus, 1, &consumed);
            if (action == TETRIS_MENU_NEW)
                tetris_reset(&game, event.seed);
            else if (action == TETRIS_MENU_PAUSE && !game.game_over)
                game.paused = !game.paused;
            else if (!consumed && event.type == KUI_EVENT_TIMER)
                tetris_tick(&game);
            else if (!consumed && event.type == KUI_EVENT_KEY &&
                     event.key_pressed) {
                if (event.key_extended && event.key_code == 75)
                    tetris_move(&game, -1, 0);
                else if (event.key_extended && event.key_code == 77)
                    tetris_move(&game, 1, 0);
                else if (event.key_extended && event.key_code == 80)
                    tetris_soft_drop(&game);
                else if (event.key_extended && event.key_code == 72)
                    tetris_rotate(&game, 1);
                else if (event.character == 'z' || event.character == 'Z')
                    tetris_rotate(&game, -1);
                else if (event.character == ' ')
                    tetris_hard_drop(&game);
                else if ((event.character == 'p' || event.character == 'P') &&
                         !game.game_over)
                    game.paused = !game.paused;
                else if (event.character == 'r' || event.character == 'R')
                    tetris_reset(&game, event.seed);
            }
        }
        kui_client_begin(&client, &event, "Tetris");
        kui_client_set_constraints(&client, 410, 374, 450, 380);
        kui_client_set_timer(&client,
            (uint32_t)(game.level >= 11 ? 100 : 700 - (game.level - 1) * 60));
        draw_game(&client, &event, &game, &board, tiles);
        kui_menu_render(&client.surface, &menu, menus, 1);
        if (kui_client_send(&client) < 0) break;
    }
    kui_client_free(&client);
    for (int piece = 0; piece < TETRIS_PIECES; piece++)
        kui_image_free(&tiles[piece]);
    free(board_pixels);
    return 0;
}
