#include <string.h>

#include "extras/tetris/tetris_game.h"

static const uint16_t shapes[TETRIS_PIECES][4] = {
    { 0x00f0, 0x4444, 0x0f00, 0x2222 },
    { 0x0066, 0x0066, 0x0066, 0x0066 },
    { 0x0072, 0x0262, 0x0270, 0x0232 },
    { 0x0036, 0x0462, 0x0360, 0x0231 },
    { 0x0063, 0x0264, 0x0630, 0x0132 },
    { 0x0071, 0x0226, 0x0470, 0x0322 },
    { 0x0074, 0x0622, 0x0170, 0x0223 }
};

uint16_t tetris_shape_mask(int piece, int rotation)
{
    if (piece < 0 || piece >= TETRIS_PIECES) return 0;
    return shapes[piece][rotation & 3];
}

static uint32_t tetris_random(struct tetris_game *game)
{
    game->random_state = game->random_state * 1664525U + 1013904223U;
    return game->random_state;
}

static void refill_bag(struct tetris_game *game)
{
    for (int index = 0; index < TETRIS_PIECES; index++)
        game->bag[index] = (uint8_t)index;
    for (int index = TETRIS_PIECES - 1; index > 0; index--) {
        int other = (int)(tetris_random(game) % (uint32_t)(index + 1));
        uint8_t piece = game->bag[index];

        game->bag[index] = game->bag[other];
        game->bag[other] = piece;
    }
    game->bag_index = 0;
}

static int take_piece(struct tetris_game *game)
{
    if (game->bag_index >= TETRIS_PIECES) refill_bag(game);
    return game->bag[game->bag_index++];
}

static int collides(const struct tetris_game *game, int x, int y,
                    int piece, int rotation)
{
    uint16_t mask = tetris_shape_mask(piece, rotation);

    for (int row = 0; row < 4; row++)
        for (int column = 0; column < 4; column++) {
            int board_x;
            int board_y;

            if (!(mask & (1U << (row * 4 + column)))) continue;
            board_x = x + column;
            board_y = y + row;
            if (board_x < 0 || board_x >= TETRIS_WIDTH ||
                board_y >= TETRIS_HEIGHT)
                return 1;
            if (board_y >= 0 && game->board[board_y][board_x] != 0)
                return 1;
        }
    return 0;
}

static void spawn_next(struct tetris_game *game)
{
    game->piece = game->next_piece;
    game->next_piece = take_piece(game);
    game->rotation = 0;
    game->x = 3;
    game->y = -1;
    if (collides(game, game->x, game->y,
                 game->piece, game->rotation))
        game->game_over = 1;
}

static int clear_lines(struct tetris_game *game)
{
    int cleared = 0;

    for (int row = TETRIS_HEIGHT - 1; row >= 0; row--) {
        int full = 1;

        for (int column = 0; column < TETRIS_WIDTH; column++)
            if (game->board[row][column] == 0) full = 0;
        if (!full) continue;
        for (int destination = row; destination > 0; destination--)
            memcpy(game->board[destination], game->board[destination - 1],
                   sizeof(game->board[destination]));
        memset(game->board[0], 0, sizeof(game->board[0]));
        cleared++;
        row++;
    }
    if (cleared != 0) {
        static const int points[] = { 0, 100, 300, 500, 800 };

        game->score += points[cleared] * game->level;
        game->lines += cleared;
        game->level = game->lines / 10 + 1;
    }
    return cleared;
}

static void lock_piece(struct tetris_game *game)
{
    uint16_t mask = tetris_shape_mask(game->piece, game->rotation);

    for (int row = 0; row < 4; row++)
        for (int column = 0; column < 4; column++) {
            int board_x;
            int board_y;

            if (!(mask & (1U << (row * 4 + column)))) continue;
            board_x = game->x + column;
            board_y = game->y + row;
            if (board_y < 0) {
                game->game_over = 1;
                return;
            }
            game->board[board_y][board_x] = (uint8_t)(game->piece + 1);
        }
    clear_lines(game);
    spawn_next(game);
}

void tetris_reset(struct tetris_game *game, uint32_t seed)
{
    memset(game, 0, sizeof(*game));
    game->random_state = seed != 0 ? seed : 0x4b59544cU;
    game->level = 1;
    game->bag_index = TETRIS_PIECES;
    game->piece = take_piece(game);
    game->next_piece = take_piece(game);
    game->rotation = 0;
    game->x = 3;
    game->y = -1;
}

int tetris_move(struct tetris_game *game, int dx, int dy)
{
    if (game->paused || game->game_over ||
        collides(game, game->x + dx, game->y + dy,
                 game->piece, game->rotation))
        return 0;
    game->x += dx;
    game->y += dy;
    return 1;
}

int tetris_rotate(struct tetris_game *game, int direction)
{
    static const int kicks[] = { 0, -1, 1, -2, 2 };
    int rotation;

    if (game->paused || game->game_over) return 0;
    rotation = (game->rotation + (direction < 0 ? 3 : 1)) & 3;
    for (unsigned int index = 0;
         index < sizeof(kicks) / sizeof(kicks[0]); index++)
        if (!collides(game, game->x + kicks[index], game->y,
                      game->piece, rotation)) {
            game->x += kicks[index];
            game->rotation = rotation;
            return 1;
        }
    return 0;
}

int tetris_tick(struct tetris_game *game)
{
    if (game->paused || game->game_over) return 0;
    if (tetris_move(game, 0, 1)) return 0;
    lock_piece(game);
    return 1;
}

int tetris_soft_drop(struct tetris_game *game)
{
    if (game->paused || game->game_over) return 0;
    if (tetris_move(game, 0, 1)) {
        game->score++;
        return 1;
    }
    lock_piece(game);
    return 0;
}

int tetris_drop_y(const struct tetris_game *game)
{
    int y = game->y;

    while (!collides(game, game->x, y + 1,
                     game->piece, game->rotation))
        y++;
    return y;
}

int tetris_hard_drop(struct tetris_game *game)
{
    int destination;
    int distance;

    if (game->paused || game->game_over) return 0;
    destination = tetris_drop_y(game);
    distance = destination - game->y;
    game->y = destination;
    game->score += distance * 2;
    lock_piece(game);
    return distance;
}
