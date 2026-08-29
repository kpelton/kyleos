#include <stdio.h>
#include <sys/time.h>
#include <string.h>

#include "extras/kui/kui.h"
#include "extras/kuid/shell_protocol.h"

static int menu_open;
static int menu_submenu = -1;
static int menu_hover = -1;
static int submenu_hover = -1;
static int zoom_level;
static int dock_menu_rank = -1;
static int dock_menu_window = -1;
static int dock_menu_hover = -1;

#define DOCK_HEIGHT 74
#define DOCK_BOTTOM_GAP 0
#define DOCK_ICON_SIZE 56
#define DOCK_ITEM_STEP 62
#define DOCK_ZOOM_SIZE 88
#define DOCK_NEIGHBOR_SIZE 72

static int menu_is_game(const char *title)
{
    return strstr(title, "Tetris") != NULL || strstr(title, "Mines") != NULL ||
           strstr(title, "Solitaire") != NULL || strstr(title, "Flappy") != NULL ||
           strstr(title, "KyleKart") != NULL || strstr(title, "KyleFPS") != NULL ||
           strstr(title, "Duke") != NULL || strstr(title, "Doom") != NULL ||
           strstr(title, "Quake") != NULL;
}

static int menu_category_count(const struct kui_shell_request *request, int game)
{
    int count = 0;
    for (int i = 0; i < request->menu_app_count; i++)
        if (menu_is_game(request->menu_apps[i].title) == game) count++;
    return count;
}

static const struct kui_shell_app *dock_app(
    const struct kui_shell_request *request, int rank)
{
    unsigned int app;

    if (rank < 0 || rank >= request->app_count ||
        rank >= KUI_SHELL_MAX_APPS)
        return NULL;
    app = request->app_indices[rank];
    if (app >= (unsigned int)request->menu_app_count ||
        app >= KUI_SHELL_MAX_APPS)
        return NULL;
    return &request->menu_apps[app];
}

struct cached_icon {
    char path[KUI_SHELL_ICON_PATH_MAX];
    struct kui_image image;
    int attempted;
};

static struct cached_icon app_icons[KUI_SHELL_MAX_APPS];
static struct cached_icon window_icons[KUI_SHELL_MAX_WINDOWS];

static void copy_path(char *destination, const char *source)
{
    size_t length = strlen(source);

    if (length >= KUI_SHELL_ICON_PATH_MAX)
        length = KUI_SHELL_ICON_PATH_MAX - 1;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

static const struct kui_image *cached_image(struct cached_icon *cache,
                                             const char *path)
{
    if (strcmp(cache->path, path) != 0) {
        kui_image_free(&cache->image);
        copy_path(cache->path, path);
        cache->attempted = 0;
    }
    if (!cache->attempted && cache->path[0] != '\0') {
        cache->attempted = 1;
        kui_image_load(cache->path, &cache->image);
    }
    return cache->image.pixels != NULL ? &cache->image : NULL;
}

static uint32_t color_between(uint32_t top, uint32_t bottom,
                              int position, int maximum)
{
    uint32_t inverse = (uint32_t)(maximum - position);
    uint32_t red = (((top >> 16) & 0xffU) * inverse +
                    ((bottom >> 16) & 0xffU) * (uint32_t)position) /
                   (uint32_t)maximum;
    uint32_t green = (((top >> 8) & 0xffU) * inverse +
                      ((bottom >> 8) & 0xffU) * (uint32_t)position) /
                     (uint32_t)maximum;
    uint32_t blue = ((top & 0xffU) * inverse +
                     (bottom & 0xffU) * (uint32_t)position) /
                    (uint32_t)maximum;

    return (red << 16) | (green << 8) | blue;
}

static void draw_glass_dock(struct kui_surface *surface, int x, int y,
                            int width)
{
    kui_fill_rect_alpha(surface, x + 9, y + 10, width,
                        DOCK_HEIGHT - 2, 0x50000000U);
    for (int row = 0; row < DOCK_HEIGHT; row++) {
        int inset = row < 5 ? 5 - row :
                    row >= DOCK_HEIGHT - 5 ?
                    row - (DOCK_HEIGHT - 6) : 0;
        uint32_t rgb = row < 30 ?
            color_between(0x00ffffffU, 0x00d9e8f3U, row, 29) :
            color_between(0x00bfd2e2U, 0x005a7895U, row - 30,
                          DOCK_HEIGHT - 31);
        uint32_t color = (row < 24 ? 0xd0000000U : 0xc0000000U) | rgb;

        kui_fill_rect_alpha(surface, x + inset, y + row,
                            width - inset * 2, 1, color);
        if (row > 7 && row < DOCK_HEIGHT - 7 && (row & 1) == 0)
            kui_fill_rect_alpha(surface, x + 2, y + row, width - 4, 1,
                                0x24ffffffU);
    }
    kui_line(surface, x + 4, y, x + width - 5, y, 0x00ffffffU);
    kui_line(surface, x + 2, y + DOCK_HEIGHT - 2,
             x + width - 3, y + DOCK_HEIGHT - 2, 0x00445e79U);
}

static void draw_fallback_icon(struct kui_surface *surface, int x, int y,
                               int window)
{
    uint32_t face = window ? 0x00f4f6f8U : 0x0087caffU;
    uint32_t edge = window ? 0x00617487U : 0x000e62bbU;

    kui_fill_rect(surface, x + 5, y + 7, 32, 30, 0x003a536eU);
    kui_fill_rect(surface, x + 2, y + 2, 32, 31, edge);
    kui_fill_rect(surface, x + 4, y + 4, 28, 27, face);
    kui_fill_rect(surface, x + 4, y + 4, 28, 6,
                  window ? 0x0095b9dbU : 0x003889dcU);
    if (window) {
        kui_fill_rect(surface, x + 7, y + 13, 22, 2, 0x00bcc6d0U);
        kui_fill_rect(surface, x + 7, y + 18, 17, 2, 0x00bcc6d0U);
        kui_fill_rect(surface, x + 7, y + 23, 20, 2, 0x00bcc6d0U);
    } else {
        kui_fill_rect(surface, x + 9, y + 14, 18, 12, 0x00e7f5ffU);
        kui_outline_rect(surface, x + 9, y + 14, 18, 12, 0x004f8fc8U);
    }
    kui_line(surface, x + 5, y + 5, x + 30, y + 5, 0x00ffffffU);
}

static int dock_app_count(const struct kui_shell_request *request)
{
    return request->app_count > KUI_SHELL_MAX_APPS ?
           KUI_SHELL_MAX_APPS : request->app_count;
}

static int window_matches_app(const struct kui_shell_request *request,
                              int window, int app)
{
    const struct kui_shell_app *entry = dock_app(request, app);

    return request->windows[window].title[0] != '\0' &&
           entry != NULL &&
           strcmp(request->windows[window].title,
                  entry->title) == 0;
}

static int extra_window(const struct kui_shell_request *request, int window)
{
    return request->windows[window].app_index < 0;
}

static int extra_window_count(const struct kui_shell_request *request)
{
    int count = 0;

    for (int window = 0; window < request->window_count; window++)
        if (extra_window(request, window)) count++;
    return count;
}

static int extra_window_by_rank(const struct kui_shell_request *request,
                                int rank)
{
    for (int window = 0; window < request->window_count; window++)
        if (extra_window(request, window) && rank-- == 0) return window;
    return -1;
}

static int active_window_for_app(const struct kui_shell_request *request,
                                 int app)
{
    for (int window = 0; window < request->window_count; window++)
        if (request->windows[window].app_index == app) return window;
    for (int window = 0; window < request->window_count; window++)
        if (window_matches_app(request, window, app)) return window;
    return -1;
}

static int dock_item_count(const struct kui_shell_request *request)
{
    int count = dock_app_count(request) + extra_window_count(request);
    int maximum = (request->screen_width - 20) / DOCK_ITEM_STEP;

    /* Keep large alpha icons comfortably inside the compositor pipe. */
    if (maximum > KUI_SHELL_MAX_APPS) maximum = KUI_SHELL_MAX_APPS;

    return count > maximum ? maximum : count;
}

static int hovered_dock_item(const struct kui_shell_request *request,
                             int dock_x, int dock_y, int items)
{
    int rank;

    if (request->x < dock_x + 10 ||
        request->x >= dock_x + 10 + items * DOCK_ITEM_STEP ||
        request->y < dock_y - (DOCK_ZOOM_SIZE - DOCK_ICON_SIZE) ||
        request->y >= dock_y + DOCK_HEIGHT)
        return -1;
    rank = (request->x - dock_x - 10) / DOCK_ITEM_STEP;
    return rank >= 0 && rank < items ? rank : -1;
}

static int magnified_size(int rank, int hovered, int normal)
{
    int distance;

    if (hovered < 0) return normal;
    distance = rank > hovered ? rank - hovered : hovered - rank;
    if (distance == 0)
        return normal + ((DOCK_ZOOM_SIZE - DOCK_ICON_SIZE) * zoom_level) / 60;
    if (distance == 1)
        return normal + ((DOCK_NEIGHBOR_SIZE - DOCK_ICON_SIZE) * zoom_level) / 60;
    return normal;
}

static void draw_menu_bar(struct kui_surface *surface,
                          const struct kui_shell_request *request,
                          const char *clock)
{
    for (int row = 0; row < 24; row++)
        kui_fill_rect_alpha(surface, 0, row, request->screen_width, 1,
                            row & 1 ? 0xd8e3e9efU : 0xe8ffffffU);
    kui_fill_rect_alpha(surface, 0, 0, request->screen_width, 1,
                        0xf0ffffffU);
    kui_fill_rect(surface, 0, 23, request->screen_width, 1, 0x006c747dU);
    kui_fill_rect_alpha(surface, 15, 5, 12, 12, 0xe0246ac1U);
    kui_fill_rect_alpha(surface, 17, 3, 8, 16, 0xe0246ac1U);
    kui_draw_text(surface, 17, 4, "K", KUI_WHITE, 0x00246ac1U, 0);
    kui_draw_text(surface, 38, 4, request->active_application,
                  KUI_BLACK, 0x00fbfcfdU, 0);
    kui_draw_text(surface, request->screen_width - 68, 4, clock, KUI_BLACK,
                  0x00fbfcfdU, 0);
}

static void render(struct kui_surface *surface,
                   const struct kui_shell_request *request)
{
    int apps = dock_app_count(request);
    int menu_apps = request->menu_app_count > 0 ?
                    request->menu_app_count : request->app_count;
    int items = dock_item_count(request);
    int dock_width = 20 + items * DOCK_ITEM_STEP;
    int dock_x = (request->screen_width - dock_width) / 2;
    int dock_y = request->screen_height - DOCK_HEIGHT - DOCK_BOTTOM_GAP;
    int hovered = hovered_dock_item(request, dock_x, dock_y, items);
    char clock[16];
    struct timeval now;
    uint32_t minutes;

    gettimeofday(&now, NULL);
    minutes = (uint32_t)((now.tv_sec / 60) % (24 * 60));
    if (hovered >= 0) {
        if (zoom_level < 60) zoom_level++;
    } else if (zoom_level > 0) {
        zoom_level--;
    }

    snprintf(clock, sizeof(clock), "%02u:%02u",
             (unsigned)((minutes / 60U) % 24U),
             (unsigned)(minutes % 60U));
    draw_menu_bar(surface, request, clock);
    draw_glass_dock(surface, dock_x, dock_y, dock_width);
    for (int rank = 0; rank < apps && rank < items; rank++) {
        const struct kui_shell_app *entry = dock_app(request, rank);
        int x = dock_x + 10 + rank * DOCK_ITEM_STEP;
        int size = magnified_size(rank, hovered, DOCK_ICON_SIZE);
        int icon_x = x + 2 + (DOCK_ICON_SIZE - size) / 2;
        int icon_y = dock_y + 3 + DOCK_ICON_SIZE - size;
        int running_window = active_window_for_app(request, rank);
        const struct kui_image *icon = entry != NULL ?
            cached_image(&app_icons[rank], entry->icon_path) : NULL;

        if (icon != NULL)
            kui_blit_image_scaled(surface, icon, icon_x, icon_y, size, size);
        else
            draw_fallback_icon(surface, x + 10, dock_y + 10, 0);
        if (running_window >= 0) {
            uint32_t indicator = request->windows[running_window].active ?
                                 0x001d2a35U : 0x00637280U;

            kui_fill_rect(surface, x + 27, dock_y + 64, 10, 2, indicator);
            kui_fill_rect(surface, x + 29, dock_y + 62, 6, 2, indicator);
        }
    }
    for (int rank = 0; rank < extra_window_count(request) &&
         apps + rank < items;
         rank++) {
        int window = extra_window_by_rank(request, rank);
        int x = dock_x + 10 + (apps + rank) * DOCK_ITEM_STEP;
        int item_rank = apps + rank;
        int size = magnified_size(item_rank, hovered, DOCK_ICON_SIZE);
        int icon_x = x + 2 + (DOCK_ICON_SIZE - size) / 2;
        int icon_y = dock_y + 3 + DOCK_ICON_SIZE - size;
        const struct kui_image *icon;

        if (window < 0) continue;
        icon = cached_image(&window_icons[window],
                            request->windows[window].icon_path);

        if (icon != NULL)
            kui_blit_image_scaled(surface, icon, icon_x, icon_y, size, size);
        else
            draw_fallback_icon(surface, x + 11, dock_y + 12, 1);
        if (request->windows[window].active)
            kui_fill_rect(surface, x + 26, dock_y + 64, 10, 3,
                          0x002b485fU);
        if (request->windows[window].minimized)
            kui_fill_rect(surface, x + 23, dock_y + 64, 16, 3,
                          0x00f1a722U);
    }
    if (menu_open) {
        if (dock_menu_rank >= 0 && dock_menu_rank < apps) {
            int menu_x = dock_x + dock_menu_rank * DOCK_ITEM_STEP;
            int menu_y = dock_y - 58;
            kui_fill_rect_alpha(surface, menu_x, menu_y, 190, 52,
                                0xe8f5f5f5U);
            kui_outline_rect(surface, menu_x, menu_y, 190, 52,
                             0x00656565U);
            if (dock_menu_hover >= 0)
                kui_fill_rect(surface, menu_x + 2,
                              menu_y + 2 + dock_menu_hover * 26,
                              186, 24, 0x00366fc4U);
            kui_draw_text(surface, menu_x + 10, menu_y + 10, "Open",
                          dock_menu_hover == 0 ? KUI_WHITE : KUI_BLACK,
                          dock_menu_hover == 0 ? 0x00366fc4U : 0x00f5f5f5U,
                          0);
            kui_draw_text(surface, menu_x + 10, menu_y + 32,
                          "Keep/Remove from Dock",
                          dock_menu_hover == 1 ? KUI_WHITE : KUI_BLACK,
                          dock_menu_hover == 1 ? 0x00366fc4U : 0x00f5f5f5U,
                          0);
            return;
        }
        if (dock_menu_window >= 0) {
            int menu_x = dock_x + (apps + dock_menu_window) * DOCK_ITEM_STEP;
            int menu_y = dock_y - 58;
            kui_fill_rect_alpha(surface, menu_x, menu_y, 190, 52,
                                0xe8f5f5f5U);
            kui_outline_rect(surface, menu_x, menu_y, 190, 52,
                             0x00656565U);
            if (dock_menu_hover >= 0)
                kui_fill_rect(surface, menu_x + 2,
                              menu_y + 2 + dock_menu_hover * 26,
                              186, 24, 0x00366fc4U);
            kui_draw_text(surface, menu_x + 10, menu_y + 10, "Open",
                          dock_menu_hover == 0 ? KUI_WHITE : KUI_BLACK,
                          dock_menu_hover == 0 ? 0x00366fc4U : 0x00f5f5f5U,
                          0);
            kui_draw_text(surface, menu_x + 10, menu_y + 32,
                          "Keep in Dock",
                          dock_menu_hover == 1 ? KUI_WHITE : KUI_BLACK,
                          dock_menu_hover == 1 ? 0x00366fc4U : 0x00f5f5f5U,
                          0);
            return;
        }
        int category_count = menu_submenu >= 0 ?
                             menu_category_count(request, menu_submenu) : 0;
        int child_height = 12 + (category_count + 1) * 26;
        for (int row = 0; row < 90; row++)
            kui_fill_rect(surface, 4, 24 + row, 250, 1,
                          row & 1 ? 0x00e0e5e9U : 0x00f9fafbU);
        kui_outline_rect(surface, 4, 24, 250, 90, 0x00656565U);
        if (menu_hover >= 0)
            kui_fill_rect(surface, 6, 26 + menu_hover * 26,
                          246, 26, 0x00366fc4U);
        kui_draw_text(surface, 18, 32, "Applications   >",
                      menu_hover == 0 ? KUI_WHITE : KUI_BLACK,
                      menu_hover == 0 ? 0x00366fc4U : 0x00f9fafbU, 0);
        kui_draw_text(surface, 18, 58, "Games         >",
                      menu_hover == 1 ? KUI_WHITE : KUI_BLACK,
                      menu_hover == 1 ? 0x00366fc4U : 0x00e0e5e9U, 0);
        kui_draw_text(surface, 18, 84, "Quit Aqua",
                      menu_hover == 2 ? KUI_WHITE : KUI_BLACK,
                      menu_hover == 2 ? 0x00366fc4U : 0x00f9fafbU, 0);
        if (menu_submenu >= 0) {
            for (int row = 0; row < child_height; row++)
                kui_fill_rect(surface, 258, 24 + row, 300, 1,
                              row & 1 ? 0x00e0e5e9U : 0x00f9fafbU);
            kui_outline_rect(surface, 258, 24, 300, child_height, 0x00656565U);
            int row = 0;
            for (int rank = 0; rank < menu_apps; rank++) {
                const char *title = request->menu_apps[rank].title;
                if (menu_is_game(title) != menu_submenu) continue;
                if (row == submenu_hover)
                    kui_fill_rect(surface, 260, 26 + row * 26,
                                  296, 26, 0x00366fc4U);
                kui_draw_text(surface, 272, 32 + row * 26, title,
                              row == submenu_hover ? KUI_WHITE : KUI_BLACK,
                              row == submenu_hover ? 0x00366fc4U :
                              (row & 1 ? 0x00e0e5e9U : 0x00f9fafbU), 0);
                row++;
            }
            if (submenu_hover == category_count)
                kui_fill_rect(surface, 260,
                              26 + category_count * 26,
                              296, 26, 0x00366fc4U);
            kui_draw_text(surface, 272, 32 + category_count * 26,
                          "< Back",
                          submenu_hover == category_count ?
                          KUI_WHITE : KUI_BLACK,
                          submenu_hover == category_count ?
                          0x00366fc4U : 0x00f5f5f5U, 0);
        }
    }
}

static void pointer(const struct kui_shell_request *request,
                    struct kui_shell_response *response)
{
    int apps = dock_app_count(request);
    int menu_apps = request->menu_app_count > 0 ?
                    request->menu_app_count : request->app_count;
    int items = dock_item_count(request);
    int dock_width = 20 + items * DOCK_ITEM_STEP;
    int dock_x = (request->screen_width - dock_width) / 2;

    if (request->button == 0) {
        int hovered = -1;
        int child_hovered = -1;
        int dock_hovered = -1;

        if (menu_open && dock_menu_rank >= 0 && dock_menu_rank < apps) {
            int menu_x = dock_x + dock_menu_rank * DOCK_ITEM_STEP;
            int menu_y = request->screen_height - DOCK_HEIGHT -
                         DOCK_BOTTOM_GAP - 58;

            if (kui_in_rect(request->x, request->y,
                            menu_x, menu_y, 190, 52))
                dock_hovered = (request->y - menu_y) / 26;
        } else if (menu_open && dock_menu_window >= 0) {
            int menu_x = dock_x + (apps + dock_menu_window) * DOCK_ITEM_STEP;
            int menu_y = request->screen_height - DOCK_HEIGHT -
                         DOCK_BOTTOM_GAP - 58;

            if (kui_in_rect(request->x, request->y,
                            menu_x, menu_y, 190, 52))
                dock_hovered = (request->y - menu_y) / 26;
        } else if (menu_open) {
            int category_count = menu_submenu >= 0 ?
                menu_category_count(request, menu_submenu) : 0;

            if (kui_in_rect(request->x, request->y, 4, 29, 250, 78))
                hovered = (request->y - 29) / 26;
            if (menu_submenu >= 0 &&
                kui_in_rect(request->x, request->y, 258, 29, 300,
                            (category_count + 1) * 26))
                child_hovered = (request->y - 29) / 26;
        }
        if (hovered != menu_hover || child_hovered != submenu_hover ||
            dock_hovered != dock_menu_hover) {
            menu_hover = hovered;
            submenu_hover = child_hovered;
            dock_menu_hover = dock_hovered;
            response->consumed = 1;
        }
        if (menu_open) response->consumed = 1;
        return;
    }
    if (menu_open) {
        if (dock_menu_rank >= 0 && dock_menu_rank < apps) {
            int menu_x = dock_x + dock_menu_rank * DOCK_ITEM_STEP;
            int menu_y = request->screen_height - DOCK_HEIGHT -
                         DOCK_BOTTOM_GAP - 58;
            response->consumed = 1;
            if (kui_in_rect(request->x, request->y, menu_x, menu_y,
                            190, 52)) {
                int row = (request->y - menu_y) / 26;
                if (row == 0) {
                    response->action = KUI_SHELL_ACTION_OPEN_APP;
                    response->index = dock_menu_rank;
                } else {
                    response->action = KUI_SHELL_ACTION_TOGGLE_PIN;
                    response->index = dock_menu_rank;
                }
            }
            menu_open = 0;
            dock_menu_rank = -1;
            dock_menu_hover = -1;
            return;
        }
        if (dock_menu_window >= 0) {
            int menu_x = dock_x + (apps + dock_menu_window) * DOCK_ITEM_STEP;
            int menu_y = request->screen_height - DOCK_HEIGHT -
                         DOCK_BOTTOM_GAP - 58;
            response->consumed = 1;
            if (kui_in_rect(request->x, request->y, menu_x, menu_y,
                            190, 52) && (request->y - menu_y) / 26 == 1) {
                response->action = KUI_SHELL_ACTION_TOGGLE_PIN;
                response->index = 100 + extra_window_by_rank(request,
                                                              dock_menu_window);
            }
            menu_open = 0;
            dock_menu_window = -1;
            dock_menu_hover = -1;
            return;
        }
        int category_count = menu_submenu >= 0 ?
                             menu_category_count(request, menu_submenu) : 2;
        int height = 90;
        int child_height = 12 + (category_count + 1) * 26;

        if (menu_submenu >= 0 &&
            kui_in_rect(request->x, request->y, 258, 24, 300, child_height)) {
            int row = (request->y - 29) / 26;
            response->consumed = 1;
            if (row < category_count) {
                int found = 0;
                for (int app = 0; app < menu_apps; app++) {
                    if (menu_is_game(request->menu_apps[app].title) != menu_submenu) continue;
                    if (found++ == row) { response->action = KUI_SHELL_ACTION_OPEN_MENU_APP; response->index = app; break; }
                }
                menu_open = 0;
                menu_submenu = -1;
                menu_hover = -1;
                submenu_hover = -1;
            } else {
                menu_submenu = -1;
                submenu_hover = -1;
            }
            return;
        }

        if (kui_in_rect(request->x, request->y, 4, 24, 250, height)) {
            int rank = (request->y - 29) / 26;

            response->consumed = 1;
            if (menu_submenu >= 0) {
                if (rank < 2) {
                    menu_submenu = rank;
                    menu_open = 1;
                    submenu_hover = -1;
                } else {
                    menu_open = 0;
                    menu_submenu = -1;
                    menu_hover = -1;
                    submenu_hover = -1;
                    response->action = KUI_SHELL_ACTION_QUIT;
                }
                return;
            }
            menu_open = 0;
            menu_hover = -1;
            if (menu_submenu < 0) {
                if (rank < 2) {
                    menu_submenu = rank;
                    menu_open = 1;
                    submenu_hover = -1;
                    return;
                }
                response->action = KUI_SHELL_ACTION_QUIT;
            } else if (rank < category_count) {
                int found = 0;
                for (int app = 0; app < menu_apps; app++) {
                    if (menu_is_game(request->menu_apps[app].title) != menu_submenu) continue;
                    if (found++ == rank) { response->action = KUI_SHELL_ACTION_OPEN_MENU_APP; response->index = app; break; }
                }
            } else {
                menu_submenu = -1;
                menu_open = 1;
                return;
            }
            return;
        }
        menu_open = 0;
        menu_hover = -1;
        submenu_hover = -1;
        dock_menu_hover = -1;
        response->consumed = 1;
        return;
    }
    if (request->y < 24) {
        response->consumed = 1;
        if (request->x < 30) {
            menu_open = 1;
            menu_submenu = -1;
            menu_hover = -1;
            submenu_hover = -1;
        }
        return;
    }
    if (!kui_in_rect(request->x, request->y, dock_x,
                     request->screen_height - DOCK_HEIGHT - DOCK_BOTTOM_GAP,
                     dock_width, DOCK_HEIGHT))
        return;
    response->consumed = 1;
    {
        int rank = (request->x - dock_x - 10) / DOCK_ITEM_STEP;

        if (rank >= 0 && rank < apps) {
            if (request->button == 2) {
                menu_open = 1;
                dock_menu_rank = rank;
                dock_menu_hover = -1;
                return;
            }
            int window = active_window_for_app(request, rank);

            if (window >= 0) {
                response->action = KUI_SHELL_ACTION_ACTIVATE_WINDOW;
                response->index = window;
            } else {
                response->action = KUI_SHELL_ACTION_OPEN_APP;
                response->index = rank;
            }
        } else if (rank >= apps && rank < items) {
            int window = extra_window_by_rank(request, rank - apps);

            if (request->button == 2) {
                menu_open = 1;
                dock_menu_window = rank - apps;
                dock_menu_hover = -1;
                return;
            }

            response->action = KUI_SHELL_ACTION_ACTIVATE_WINDOW;
            response->index = window;
        }
    }
}

int main(void)
{
    struct kui_shell_profile profile;
    struct kui_command_buffer display;

    memset(&profile, 0, sizeof(profile));
    profile.magic = KUI_SHELL_MAGIC;
    profile.version = KUI_SHELL_VERSION;
    strcpy(profile.name, "Aqua");
    kui_style_aqua(&profile.style);
    kui_style_apply(&profile.style);
    if (kui_write_all(1, &profile, sizeof(profile)) < 0)
        return 1;
    kui_command_buffer_init(&display);
    for (;;) {
        struct kui_shell_request request;
        struct kui_shell_response response;
        struct kui_surface surface;

        if (kui_read_all(0, &request, sizeof(request)) < 0)
            break;
        if (request.magic != KUI_SHELL_MAGIC ||
            request.version != KUI_SHELL_VERSION)
            break;
        memset(&response, 0, sizeof(response));
        response.magic = KUI_SHELL_MAGIC;
        response.version = KUI_SHELL_VERSION;
        response.serial = request.serial;
        kui_command_buffer_reset(&display);
        if (request.type == KUI_SHELL_RENDER) {
            kui_surface_init_commands(&surface, &display,
                                      request.screen_width,
                                      request.screen_height);
            render(&surface, &request);
            response.display_bytes = display.length;
        } else if (request.type == KUI_SHELL_POINTER) {
            pointer(&request, &response);
        }
        if (display.failed || kui_write_all(1, &response, sizeof(response)) < 0 ||
            (display.length != 0 &&
             kui_write_all(1, display.data, display.length) < 0))
            break;
    }
    kui_command_buffer_free(&display);
    for (int i = 0; i < KUI_SHELL_MAX_APPS; i++)
        kui_image_free(&app_icons[i].image);
    for (int i = 0; i < KUI_SHELL_MAX_WINDOWS; i++)
        kui_image_free(&window_icons[i].image);
    return 0;
}
