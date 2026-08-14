#ifndef KYLEOS_SOLITAIRE_IPC_H
#define KYLEOS_SOLITAIRE_IPC_H

#include <stdint.h>

#include "extras/solitaire/solitaire.h"

enum solitaire_command {
    SOLITAIRE_COMMAND_RESET = 1,
    SOLITAIRE_COMMAND_CLICK,
    SOLITAIRE_COMMAND_QUIT
};

struct solitaire_request {
    uint32_t command;
    uint32_t seed;
    int width;
    int height;
    int pointer_x;
    int pointer_y;
};

struct solitaire_response {
    int changed;
    struct solitaire_game game;
};

#endif
