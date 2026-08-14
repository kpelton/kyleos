#ifndef KYLEOS_TERMINAL_IPC_H
#define KYLEOS_TERMINAL_IPC_H

#define TERMINAL_TEXT_MAX 4096
#define TERMINAL_COMMAND_MAX 128
#define TERMINAL_PATH_MAX 128

enum terminal_command {
    TERMINAL_COMMAND_RESET = 1,
    TERMINAL_COMMAND_KEY,
    TERMINAL_COMMAND_CLEAR,
    TERMINAL_COMMAND_QUIT
};

enum terminal_action {
    TERMINAL_ACTION_NONE,
    TERMINAL_ACTION_SOLITAIRE,
    TERMINAL_ACTION_CALCULATOR,
    TERMINAL_ACTION_MINESWEEPER,
    TERMINAL_ACTION_SYSTEM_MONITOR,
    TERMINAL_ACTION_EDITOR
};

struct terminal_request {
    int command;
    char key;
};

struct terminal_response {
    int status;
    int action;
    int terminal_length;
    int command_length;
    char path[TERMINAL_PATH_MAX];
    char terminal[TERMINAL_TEXT_MAX];
    char input[TERMINAL_COMMAND_MAX];
};

#endif
