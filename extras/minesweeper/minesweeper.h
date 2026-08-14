#ifndef KYLEOS_MINESWEEPER_H
#define KYLEOS_MINESWEEPER_H

#include <stdint.h>

#include "extras/kui/kui.h"

#define MINESWEEPER_COLS 9
#define MINESWEEPER_ROWS 9
#define MINESWEEPER_CELLS (MINESWEEPER_COLS * MINESWEEPER_ROWS)
#define MINESWEEPER_MINES 10

struct minesweeper_cell {
    uint8_t mine;
    uint8_t revealed;
    uint8_t flagged;
    uint8_t adjacent;
};

struct minesweeper_game {
    struct minesweeper_cell cells[MINESWEEPER_CELLS];
    uint32_t random_state;
    int mines_placed;
    int game_over;
    int won;
};

void minesweeper_reset(struct minesweeper_game *game, uint32_t seed);
void minesweeper_draw(struct kui_surface *surface,
                      const struct minesweeper_game *game,
                      int x, int y, int width, int height);
int minesweeper_click(struct minesweeper_game *game,
                      int x, int y, int width, int height,
                      int pointer_x, int pointer_y, int right_button,
                      uint32_t seed);

#endif
