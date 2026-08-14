#ifndef KYLEOS_MINESWEEPER_IPC_H
#define KYLEOS_MINESWEEPER_IPC_H

#include "extras/minesweeper/minesweeper.h"

enum minesweeper_command {
    MINESWEEPER_COMMAND_RESET = 1,
    MINESWEEPER_COMMAND_CLICK,
    MINESWEEPER_COMMAND_QUIT
};

struct minesweeper_request {
    int command;
    int width;
    int height;
    int pointer_x;
    int pointer_y;
    int right_button;
    uint32_t seed;
};

struct minesweeper_response {
    int changed;
    struct minesweeper_game game;
};

#endif
