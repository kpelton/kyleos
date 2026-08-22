#ifndef KYLEOS_SHELL_PROTOCOL_H
#define KYLEOS_SHELL_PROTOCOL_H

#include <stdint.h>

#include "extras/kui/kui_style.h"

#define KUI_SHELL_MAGIC 0x314c4853U
#define KUI_SHELL_VERSION 4U
#define KUI_SHELL_MAX_APPS 24
#define KUI_SHELL_MAX_WINDOWS 10
#define KUI_SHELL_ICON_PATH_MAX 40

enum kui_shell_request_type {
    KUI_SHELL_RENDER = 1,
    KUI_SHELL_POINTER
};

enum kui_shell_action_type {
    KUI_SHELL_ACTION_NONE,
    KUI_SHELL_ACTION_OPEN_APP,
    KUI_SHELL_ACTION_ACTIVATE_WINDOW,
    KUI_SHELL_ACTION_CLOSE_ACTIVE,
    KUI_SHELL_ACTION_QUIT,
    KUI_SHELL_ACTION_TOGGLE_PIN,
    KUI_SHELL_ACTION_OPEN_MENU_APP
};

struct kui_shell_profile {
    uint32_t magic;
    uint32_t version;
    char name[32];
    struct kui_style style;
};

struct kui_shell_item {
    char title[64];
    char icon_path[KUI_SHELL_ICON_PATH_MAX];
    uint32_t minimized;
    uint32_t active;
    int app_index;
};

struct kui_shell_request {
    uint32_t magic;
    uint32_t version;
    uint32_t serial;
    uint32_t type;
    int screen_width;
    int screen_height;
    uint32_t ticks_ms;
    int x;
    int y;
    int button;
    int app_count;
    int menu_app_count;
    int window_count;
    char active_application[32];
    struct kui_shell_item apps[KUI_SHELL_MAX_APPS];
    struct kui_shell_item menu_apps[KUI_SHELL_MAX_APPS];
    struct kui_shell_item windows[KUI_SHELL_MAX_WINDOWS];
};

struct kui_shell_response {
    uint32_t magic;
    uint32_t version;
    uint32_t serial;
    uint32_t consumed;
    uint32_t action;
    int index;
    uint32_t display_bytes;
};

#endif
