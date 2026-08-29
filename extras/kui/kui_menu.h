#ifndef KYLEOS_KUI_MENU_H
#define KYLEOS_KUI_MENU_H

#include "extras/kui/kui.h"

#define KUI_MENU_BAR_HEIGHT 23
#define KUI_MENU_ROW_HEIGHT 24

struct kui_menu_item {
    const char *label;
    const char *shortcut;
    int action;
    int enabled;
};

struct kui_menu {
    const char *label;
    const struct kui_menu_item *items;
    int item_count;
};

struct kui_menu_state {
    int open;
    int menu_index;
    int hovered_item;
};

struct kui_context_menu_state {
    int open;
    int x;
    int y;
    int hovered_item;
};

void kui_menu_init(struct kui_menu_state *state);
void kui_menu_close(struct kui_menu_state *state);
int kui_menu_handle(struct kui_menu_state *state,
                    const struct kui_event *event,
                    const struct kui_menu *menus, int menu_count,
                    int *consumed);
void kui_menu_render(struct kui_surface *surface,
                     const struct kui_menu_state *state,
                     const struct kui_menu *menus, int menu_count);

void kui_context_menu_init(struct kui_context_menu_state *state);
void kui_context_menu_open(struct kui_context_menu_state *state,
                           int x, int y);
void kui_context_menu_close(struct kui_context_menu_state *state);
int kui_context_menu_handle(struct kui_context_menu_state *state,
                            const struct kui_event *event,
                            const struct kui_menu_item *items, int item_count,
                            int *consumed);
void kui_context_menu_render(struct kui_surface *surface,
                             const struct kui_context_menu_state *state,
                             const struct kui_menu_item *items,
                             int item_count);

#endif
