#include "extras/kui/kui_menu.h"

#include <string.h>

static int menu_label_width(const char *label)
{
    int width = kui_text_width(label != NULL ? label : "") + 20;
    return width > 42 ? width : 42;
}

static int menu_x(const struct kui_menu *menus, int menu_index)
{
    int x = 2;

    for (int i = 0; i < menu_index; i++)
        x += menu_label_width(menus[i].label);
    return x;
}

static int dropdown_width(const struct kui_menu *menu)
{
    int width = 150;

    for (int i = 0; i < menu->item_count; i++) {
        int item_width = 24 + kui_text_width(menu->items[i].label);

        if (menu->items[i].shortcut != NULL)
            item_width += 20 + kui_text_width(menu->items[i].shortcut);
        if (item_width > width) width = item_width;
    }
    return width;
}

static int item_list_width(const struct kui_menu_item *items, int item_count)
{
    int width = 150;

    for (int i = 0; i < item_count; i++) {
        int item_width = 24 + kui_text_width(items[i].label);

        if (items[i].shortcut != NULL)
            item_width += 20 + kui_text_width(items[i].shortcut);
        if (item_width > width) width = item_width;
    }
    return width;
}

static void context_menu_layout(const struct kui_context_menu_state *state,
                                int surface_width, int surface_height,
                                const struct kui_menu_item *items,
                                int item_count, int *x, int *y,
                                int *width, int *height)
{
    *width = item_list_width(items, item_count);
    *height = item_count * KUI_MENU_ROW_HEIGHT + 4;
    *x = state->x;
    *y = state->y;
    if (*x + *width > surface_width) *x = surface_width - *width;
    if (*y + *height > surface_height) *y = surface_height - *height;
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
}

static int menu_at(const struct kui_menu *menus, int menu_count, int x)
{
    int left = 2;

    for (int i = 0; i < menu_count; i++) {
        int width = menu_label_width(menus[i].label);

        if (x >= left && x < left + width) return i;
        left += width;
    }
    return -1;
}

void kui_menu_init(struct kui_menu_state *state)
{
    state->open = 0;
    state->menu_index = 0;
}

void kui_menu_close(struct kui_menu_state *state)
{
    state->open = 0;
}

int kui_menu_handle(struct kui_menu_state *state,
                    const struct kui_event *event,
                    const struct kui_menu *menus, int menu_count,
                    int *consumed)
{
    if (consumed != NULL) *consumed = 0;
    if (event->type == KUI_EVENT_POINTER && event->button == 1) {
        if (event->y < KUI_MENU_BAR_HEIGHT) {
            int selected = menu_at(menus, menu_count, event->x);

            if (selected >= 0) {
                if (state->open && state->menu_index == selected)
                    state->open = 0;
                else {
                    state->open = 1;
                    state->menu_index = selected;
                }
            } else {
                state->open = 0;
            }
            if (consumed != NULL) *consumed = 1;
            return 0;
        }
        if (state->open && state->menu_index >= 0 &&
            state->menu_index < menu_count) {
            const struct kui_menu *menu = &menus[state->menu_index];
            int left = menu_x(menus, state->menu_index);
            int width = dropdown_width(menu);
            int row;

            if (left + width > event->width) left = event->width - width;
            if (left < 0) left = 0;
            row = kui_in_rect(event->x, event->y, left,
                              KUI_MENU_BAR_HEIGHT, width,
                              menu->item_count * KUI_MENU_ROW_HEIGHT) ?
                  (event->y - KUI_MENU_BAR_HEIGHT) / KUI_MENU_ROW_HEIGHT : -1;
            state->open = 0;
            if (consumed != NULL) *consumed = 1;
            if (row >= 0 && row < menu->item_count &&
                menu->items[row].enabled)
                return menu->items[row].action;
            return 0;
        }
    }
    if (state->open && event->type == KUI_EVENT_POINTER &&
        (event->button != 0 || event->wheel != 0)) {
        state->open = 0;
        if (consumed != NULL) *consumed = 1;
    } else if (state->open && event->type == KUI_EVENT_KEY &&
               event->key_pressed) {
        if (!event->key_extended && event->key_code == 1)
            state->open = 0;
        if (consumed != NULL) *consumed = 1;
    }
    return 0;
}

void kui_menu_render(struct kui_surface *surface,
                     const struct kui_menu_state *state,
                     const struct kui_menu *menus, int menu_count)
{
    const struct kui_style *style = kui_style_current();
    int left = 2;

    if (style->flags & KUI_STYLE_AQUA) {
        for (int row = 0; row < KUI_MENU_BAR_HEIGHT; row++)
            kui_fill_rect(surface, 0, row, surface->width, 1,
                          row & 1 ? 0x00e6ebf0U : 0x00fafbfcU);
        kui_fill_rect(surface, 0, 0, surface->width, 1, 0x00ffffffU);
    } else {
        kui_fill_rect(surface, 0, 0, surface->width,
                      KUI_MENU_BAR_HEIGHT, KUI_FACE);
    }
    kui_fill_rect(surface, 0, KUI_MENU_BAR_HEIGHT - 1,
                  surface->width, 1, KUI_SHADOW);
    for (int i = 0; i < menu_count; i++) {
        int width = menu_label_width(menus[i].label);
        uint32_t background = state->open && state->menu_index == i ?
                              style->selection :
                              (style->flags & KUI_STYLE_AQUA ?
                               0x00f4f4f4U : KUI_FACE);
        uint32_t text = state->open && state->menu_index == i ?
                        KUI_WHITE : style->text;

        if (state->open && state->menu_index == i) {
            kui_fill_rect(surface, left + 2, 4, width - 4, 18, background);
            kui_outline_rect(surface, left + 1, 3, width - 2, 20,
                             KUI_SHADOW);
        }
        kui_draw_text(surface, left + 7, 7, menus[i].label,
                      text, background, 0);
        left += width;
    }
    if (state->open && state->menu_index >= 0 &&
        state->menu_index < menu_count) {
        const struct kui_menu *menu = &menus[state->menu_index];
        int x = menu_x(menus, state->menu_index);
        int width = dropdown_width(menu);
        int height = menu->item_count * KUI_MENU_ROW_HEIGHT + 4;

        if (x + width > surface->width) x = surface->width - width;
        if (x < 0) x = 0;
        if (style->flags & KUI_STYLE_AQUA)
            kui_fill_rect_alpha(surface, x + 5, KUI_MENU_BAR_HEIGHT + 5,
                                width, height, 0x40000000U);
        else
            kui_fill_rect(surface, x + 4, KUI_MENU_BAR_HEIGHT + 4,
                          width, height, 0x00505050U);
        if (style->flags & KUI_STYLE_AQUA) {
            for (int stripe = 0; stripe < height; stripe++)
                kui_fill_rect(surface, x, KUI_MENU_BAR_HEIGHT + stripe,
                              width, 1,
                              stripe & 1 ? 0x00e0e5e9U : 0x00f9fafbU);
        } else {
            kui_fill_rect(surface, x, KUI_MENU_BAR_HEIGHT,
                          width, height, style->face);
        }
        kui_outline_rect(surface, x, KUI_MENU_BAR_HEIGHT,
                         width, height,
                         style->flags & KUI_STYLE_AQUA ? 0x006d747bU :
                                                        KUI_BLACK);
        for (int row = 0; row < menu->item_count; row++) {
            const struct kui_menu_item *item = &menu->items[row];
            uint32_t color = item->enabled ? KUI_BLACK : 0x00909090U;
            int y = KUI_MENU_BAR_HEIGHT + 8 + row * KUI_MENU_ROW_HEIGHT;

            kui_draw_text(surface, x + 12, y, item->label, color,
                          style->flags & KUI_STYLE_AQUA ?
                          (row & 1 ? 0x00e0e5e9U : 0x00f9fafbU) :
                          KUI_FACE, 0);
            if (item->shortcut != NULL)
                kui_draw_text(surface,
                              x + width - 10 - kui_text_width(item->shortcut),
                              y, item->shortcut, KUI_SHADOW, KUI_FACE, 0);
        }
    }
}

void kui_context_menu_init(struct kui_context_menu_state *state)
{
    state->open = 0;
    state->x = 0;
    state->y = 0;
}

void kui_context_menu_open(struct kui_context_menu_state *state,
                           int x, int y)
{
    state->open = 1;
    state->x = x;
    state->y = y;
}

void kui_context_menu_close(struct kui_context_menu_state *state)
{
    state->open = 0;
}

int kui_context_menu_handle(struct kui_context_menu_state *state,
                            const struct kui_event *event,
                            const struct kui_menu_item *items, int item_count,
                            int *consumed)
{
    int x;
    int y;
    int width;
    int height;
    int row;

    if (consumed != NULL) *consumed = 0;
    if (!state->open) return 0;
    if (event->type == KUI_EVENT_POINTER && event->button == 1) {
        context_menu_layout(state, event->width, event->height,
                            items, item_count, &x, &y, &width, &height);
        row = kui_in_rect(event->x, event->y, x + 1, y + 1,
                          width - 2, height - 2) ?
              (event->y - y - 2) / KUI_MENU_ROW_HEIGHT : -1;
        state->open = 0;
        if (consumed != NULL) *consumed = 1;
        if (row >= 0 && row < item_count && items[row].enabled)
            return items[row].action;
        return 0;
    }
    if (event->type == KUI_EVENT_POINTER &&
        (event->button != 0 || event->wheel != 0)) {
        state->open = 0;
        if (consumed != NULL) *consumed = 1;
    } else if (event->type == KUI_EVENT_KEY && event->key_pressed) {
        state->open = 0;
        if (consumed != NULL) *consumed = 1;
    }
    return 0;
}

void kui_context_menu_render(struct kui_surface *surface,
                             const struct kui_context_menu_state *state,
                             const struct kui_menu_item *items,
                             int item_count)
{
    const struct kui_style *style = kui_style_current();
    int x;
    int y;
    int width;
    int height;

    if (!state->open || item_count <= 0) return;
    context_menu_layout(state, surface->width, surface->height,
                        items, item_count, &x, &y, &width, &height);
    if (style->flags & KUI_STYLE_AQUA)
        kui_fill_rect_alpha(surface, x + 5, y + 5,
                            width, height, 0x40000000U);
    else
        kui_fill_rect(surface, x + 4, y + 4,
                      width, height, 0x00505050U);
    if (style->flags & KUI_STYLE_AQUA) {
        for (int stripe = 0; stripe < height; stripe++)
            kui_fill_rect(surface, x, y + stripe, width, 1,
                          stripe & 1 ? 0x00e0e5e9U : 0x00f9fafbU);
    } else {
        kui_fill_rect(surface, x, y, width, height, style->face);
    }
    kui_outline_rect(surface, x, y, width, height,
                     style->flags & KUI_STYLE_AQUA ? 0x006d747bU : KUI_BLACK);
    for (int row = 0; row < item_count; row++) {
        const struct kui_menu_item *item = &items[row];
        uint32_t color = item->enabled ? style->text : 0x00909090U;
        int item_y = y + 8 + row * KUI_MENU_ROW_HEIGHT;

        kui_draw_text(surface, x + 12, item_y, item->label, color,
                      style->flags & KUI_STYLE_AQUA ?
                      (row & 1 ? 0x00e0e5e9U : 0x00f9fafbU) : KUI_FACE,
                      0);
        if (item->shortcut != NULL)
            kui_draw_text(surface,
                          x + width - 10 - kui_text_width(item->shortcut),
                          item_y, item->shortcut,
                          item->enabled ? KUI_SHADOW : 0x00909090U,
                          KUI_FACE, 0);
    }
}
