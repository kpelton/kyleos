#ifndef KYLEOS_TETRIS_GAME_H
#define KYLEOS_TETRIS_GAME_H

#include <stdint.h>

#define TETRIS_WIDTH 10
#define TETRIS_HEIGHT 20
#define TETRIS_PIECES 7

enum tetris_piece {
    TETRIS_I,
    TETRIS_O,
    TETRIS_T,
    TETRIS_S,
    TETRIS_Z,
    TETRIS_J,
    TETRIS_L
};

struct tetris_game {
    uint8_t board[TETRIS_HEIGHT][TETRIS_WIDTH];
    int piece;
    int next_piece;
    int rotation;
    int x;
    int y;
    int score;
    int lines;
    int level;
    int paused;
    int game_over;
    uint32_t random_state;
    uint8_t bag[TETRIS_PIECES];
    int bag_index;
};

uint16_t tetris_shape_mask(int piece, int rotation);
void tetris_reset(struct tetris_game *game, uint32_t seed);
int tetris_move(struct tetris_game *game, int dx, int dy);
int tetris_rotate(struct tetris_game *game, int direction);
int tetris_tick(struct tetris_game *game);
int tetris_soft_drop(struct tetris_game *game);
int tetris_hard_drop(struct tetris_game *game);
int tetris_drop_y(const struct tetris_game *game);

#endif
