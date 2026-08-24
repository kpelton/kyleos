#ifndef KYLEOS_KUI_PROTOCOL_H
#define KYLEOS_KUI_PROTOCOL_H

#include <stdint.h>
#include "extras/kui/kui_style.h"

#define KUI_PROTOCOL_MAGIC 0x3149554bU /* "KUI1" */
#define KUI_PROTOCOL_VERSION 9U
#define KUI_PROTOCOL_TEXT_MAX 128
#define KUI_PROTOCOL_CLIPBOARD_MAX 1024
#define KUI_PROTOCOL_TITLE_MAX 64
#define KUI_PROTOCOL_MAX_DISPLAY (512U * 1024U)

enum kui_event_type {
    KUI_EVENT_INIT = 1,
    KUI_EVENT_POINTER,
    KUI_EVENT_KEY,
    KUI_EVENT_RESIZE,
    KUI_EVENT_TIMER,
    KUI_EVENT_REFRESH,
    KUI_EVENT_CLOSE
};

enum kui_modifier {
    KUI_MOD_SHIFT = 1U,
    KUI_MOD_CTRL = 2U,
    KUI_MOD_ALT = 4U
};

enum kui_response_flag {
    KUI_RESPONSE_DISPLAY = 1U,
    KUI_RESPONSE_TITLE = 2U,
    KUI_RESPONSE_CONSTRAINTS = 4U,
    KUI_RESPONSE_CLOSE = 8U,
    KUI_RESPONSE_TIMER = 16U,
    KUI_RESPONSE_PARTIAL = 32U
};

enum kui_shell_action {
    KUI_ACTION_NONE,
    KUI_ACTION_OPEN_PATH,
    KUI_ACTION_LAUNCH,
    KUI_ACTION_FILE_SAVED,
    KUI_ACTION_COPY_PATH,
    KUI_ACTION_COPY_TEXT,
    KUI_ACTION_SETTINGS_CHANGED
};

enum kui_clipboard_kind {
    KUI_CLIPBOARD_NONE,
    KUI_CLIPBOARD_PATH,
    KUI_CLIPBOARD_TEXT
};

struct kui_event {
    uint32_t magic;
    uint32_t version;
    uint32_t type;
    uint32_t serial;
    int width;
    int height;
    int x;
    int y;
    int button;
    int buttons;
    int wheel;
    int key_code;
    int key_extended;
    int key_pressed;
    uint32_t modifiers;
    uint32_t seed;
    char character;
    struct kui_style style;
    char argument[KUI_PROTOCOL_TEXT_MAX];
    uint32_t clipboard_kind;
    char clipboard[KUI_PROTOCOL_CLIPBOARD_MAX];
};

struct kui_response {
    uint32_t magic;
    uint32_t version;
    uint32_t serial;
    uint32_t flags;
    int status;
    int min_width;
    int min_height;
    int preferred_width;
    int preferred_height;
    uint32_t timer_ms;
    uint32_t display_bytes;
    int damage_x;
    int damage_y;
    int damage_width;
    int damage_height;
    uint32_t action;
    char title[KUI_PROTOCOL_TITLE_MAX];
    char action_argument[KUI_PROTOCOL_CLIPBOARD_MAX];
};

enum kui_display_command_type {
    KUI_DISPLAY_FILL_RECT = 1,
    KUI_DISPLAY_LINE,
    KUI_DISPLAY_TEXT,
    KUI_DISPLAY_BITMAP_ROW,
    KUI_DISPLAY_BITMAP_REGION_ROW,
    KUI_DISPLAY_BITMAP_ALPHA_ROW,
    KUI_DISPLAY_FILL_RECT_ALPHA,
    KUI_DISPLAY_INDEXED_BITMAP
};

enum kui_text_flag {
    KUI_TEXT_OPAQUE = 1U,
    KUI_TEXT_MONOSPACE = 2U,
    KUI_TEXT_BOLD = 4U,
    KUI_TEXT_SCALE_SHIFT = 8U,
    KUI_TEXT_SCALE_MASK = 3U << KUI_TEXT_SCALE_SHIFT
};

#define KUI_TEXT_MAX_SCALE 4

struct kui_display_command {
    uint16_t type;
    uint16_t size;
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
    uint32_t foreground;
    uint32_t background;
    uint32_t flags;
};

#endif
