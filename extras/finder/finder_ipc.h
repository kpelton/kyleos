#ifndef KYLEOS_FINDER_IPC_H
#define KYLEOS_FINDER_IPC_H

#define FINDER_MAX_ITEMS 48
#define FINDER_NAME_MAX 32
#define FINDER_PATH_MAX 128
#define FINDER_TITLE_MAX 32
#define FINDER_ITEM_DIRECTORY 40000

enum finder_command {
    FINDER_COMMAND_LOAD = 1,
    FINDER_COMMAND_ACTIVATE,
    FINDER_COMMAND_BACK,
    FINDER_COMMAND_FORWARD,
    FINDER_COMMAND_UP,
    FINDER_COMMAND_QUIT
};

enum finder_action {
    FINDER_ACTION_NONE,
    FINDER_ACTION_OPEN_FILE
};

struct finder_request {
    int command;
    int item;
    char path[FINDER_PATH_MAX];
};

struct finder_response {
    int status;
    int action;
    int can_back;
    int can_forward;
    int can_up;
    char activated_path[FINDER_PATH_MAX];
    char path[FINDER_PATH_MAX];
    char title[FINDER_TITLE_MAX];
    int item_count;
    char item_names[FINDER_MAX_ITEMS][FINDER_NAME_MAX];
    int item_types[FINDER_MAX_ITEMS];
};

#endif
