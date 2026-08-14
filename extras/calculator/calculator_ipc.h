#ifndef KYLEOS_CALCULATOR_IPC_H
#define KYLEOS_CALCULATOR_IPC_H

#include <stdint.h>

#include "extras/calculator/calculator.h"

enum calculator_command {
    CALCULATOR_COMMAND_RESET = 1,
    CALCULATOR_COMMAND_CLICK,
    CALCULATOR_COMMAND_QUIT,
    CALCULATOR_COMMAND_KEY
};

struct calculator_request {
    uint32_t command;
    int width;
    int height;
    int pointer_x;
    int pointer_y;
    char key;
};

struct calculator_response {
    int changed;
    struct calculator_state state;
};

#endif
