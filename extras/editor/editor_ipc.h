#ifndef KYLEOS_EDITOR_IPC_H
#define KYLEOS_EDITOR_IPC_H

#define EDITOR_TEXT_MAX 8192
#define EDITOR_PATH_MAX 128

enum editor_command {
    EDITOR_COMMAND_OPEN = 1,
    EDITOR_COMMAND_INSERT,
    EDITOR_COMMAND_BACKSPACE,
    EDITOR_COMMAND_DELETE,
    EDITOR_COMMAND_LEFT,
    EDITOR_COMMAND_RIGHT,
    EDITOR_COMMAND_UP,
    EDITOR_COMMAND_DOWN,
    EDITOR_COMMAND_PAGE_UP,
    EDITOR_COMMAND_PAGE_DOWN,
    EDITOR_COMMAND_SET_CURSOR,
    EDITOR_COMMAND_SAVE,
    EDITOR_COMMAND_QUIT
};

struct editor_request {
    int command;
    int value;
    int repeat;
    char path[EDITOR_PATH_MAX];
};

struct editor_response {
    int status;
    int text_length;
    int text_cursor;
    int dirty;
    int save_failed;
    char text[EDITOR_TEXT_MAX];
};

#endif
