#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#include "extras/kui/kui.h"
#include "extras/kuid/shell_protocol.h"

static int menu_open;
static int menu_hover = -1;

static int launch_desktop_from_terminal(void)
{
    char *argv[] = { "desktop", NULL };

    if (!isatty(0) || !isatty(1)) return 0;
    execve("/bin/desktop", argv, NULL);
    return 127;
}

static const struct kui_shell_app *start_app(
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

static int window_button_width(const struct kui_shell_request *request)
{
    int width = request->window_count != 0 ?
                (request->screen_width - 220) / request->window_count : 0;

    return width > 180 ? 180 : width;
}

static void render(struct kui_surface *surface,
                   const struct kui_shell_request *request)
{
    int y = request->screen_height - 32;
    int button_width = window_button_width(request);
    int x = 102;
    char clock[16];
    struct timeval now;
    uint32_t minutes;
    gettimeofday(&now, NULL);
    minutes = (uint32_t)((now.tv_sec / 60) % (24 * 60));

    kui_fill_rect(surface, 0, y, request->screen_width, 32, KUI_TITLE_BLUE);
    kui_fill_rect(surface, 0, y, request->screen_width, 2, 0x0085bcffU);
    kui_draw_button(surface, 3, y + 4, 94, 25, "start", menu_open);
    for (int rank = 0; rank < request->window_count; rank++) {
        uint32_t color = request->windows[rank].active ?
                         0x0015449bU : 0x00236bc1U;
        char title[24];
        int maximum = button_width / 8 - 2;

        if (maximum < 0) maximum = 0;
        if (maximum > (int)sizeof(title) - 1) maximum = sizeof(title) - 1;
        strncpy(title, request->windows[rank].title, maximum);
        title[maximum] = '\0';
        kui_fill_rect(surface, x, y + 4, button_width - 2, 25, color);
        kui_outline_rect(surface, x, y + 4, button_width - 2, 25,
                         0x00002271U);
        kui_draw_text(surface, x + 7, y + 8, title, KUI_WHITE, color, 0);
        x += button_width;
    }
    snprintf(clock, sizeof(clock), "%02u:%02u",
             (unsigned)((minutes / 60U) % 24U),
             (unsigned)(minutes % 60U));
    kui_draw_text(surface, request->screen_width - 68, y + 8,
                  clock, KUI_WHITE, KUI_TITLE_BLUE, 0);
    if (menu_open) {
        int height = 12 + (request->app_count + 2) * 26;
        int top = y - height;

        kui_fill_rect(surface, 3, top, 250, height, 0x00f5f5f5U);
        kui_outline_rect(surface, 3, top, 250, height, KUI_TITLE_BLUE);
        for (int rank = 0; rank < request->app_count; rank++) {
            const struct kui_shell_app *entry = start_app(request, rank);
            uint32_t background = rank == menu_hover ?
                                  KUI_TITLE_BLUE : 0x00f5f5f5U;

            if (rank == menu_hover)
                kui_fill_rect(surface, 5, top + 3 + rank * 26,
                              246, 26, background);
            kui_draw_text(surface, 17, top + 8 + rank * 26,
                          entry != NULL ? entry->title : "",
                          rank == menu_hover ? KUI_WHITE : KUI_BLACK,
                          background, 0);
        }
        if (menu_hover == request->app_count)
            kui_fill_rect(surface, 5,
                          top + 3 + request->app_count * 26,
                          246, 26, KUI_TITLE_BLUE);
        kui_draw_text(surface, 17, top + 8 + request->app_count * 26,
                      "Close Window",
                      menu_hover == request->app_count ?
                      KUI_WHITE : KUI_BLACK,
                      menu_hover == request->app_count ?
                      KUI_TITLE_BLUE : 0x00f5f5f5U, 0);
        if (menu_hover == request->app_count + 1)
            kui_fill_rect(surface, 5,
                          top + 3 + (request->app_count + 1) * 26,
                          246, 26, KUI_TITLE_BLUE);
        kui_draw_text(surface, 17, top + 8 +
                      (request->app_count + 1) * 26,
                      "Quit to Shell",
                      menu_hover == request->app_count + 1 ?
                      KUI_WHITE : KUI_BLACK,
                      menu_hover == request->app_count + 1 ?
                      KUI_TITLE_BLUE : 0x00f5f5f5U, 0);
    }
}

static void pointer(const struct kui_shell_request *request,
                    struct kui_shell_response *response)
{
    int panel_y = request->screen_height - 32;

    if (request->button == 0) {
        int hovered = -1;

        if (menu_open) {
            int height = 12 + (request->app_count + 2) * 26;
            int top = panel_y - height;

            if (kui_in_rect(request->x, request->y,
                            3, top, 250, height)) {
                hovered = (request->y - top - 5) / 26;
                if (hovered < 0 || hovered >= request->app_count + 2)
                    hovered = -1;
            }
        }
        if (hovered != menu_hover) {
            menu_hover = hovered;
            response->consumed = 1;
        }
        if (menu_open) response->consumed = 1;
        return;
    }
    if (menu_open) {
        int height = 12 + (request->app_count + 2) * 26;
        int top = panel_y - height;

        if (kui_in_rect(request->x, request->y, 3, top, 250, height)) {
            int rank = (request->y - top - 5) / 26;

            response->consumed = 1;
            menu_open = 0;
            menu_hover = -1;
            if (rank < request->app_count) {
                response->action = KUI_SHELL_ACTION_OPEN_MENU_APP;
                response->index = request->app_indices[rank];
            } else if (rank == request->app_count)
                response->action = KUI_SHELL_ACTION_CLOSE_ACTIVE;
            else
                response->action = KUI_SHELL_ACTION_QUIT;
            return;
        }
        menu_open = 0;
        menu_hover = -1;
        response->consumed = 1;
        return;
    }
    if (request->y < panel_y)
        return;
    response->consumed = 1;
    if (request->x < 100) {
        menu_open = !menu_open;
        menu_hover = -1;
    } else {
        int width = window_button_width(request);
        int rank = width > 0 ? (request->x - 102) / width : -1;

        if (rank >= 0 && rank < request->window_count) {
            response->action = KUI_SHELL_ACTION_ACTIVATE_WINDOW;
            response->index = rank;
        }
    }
}

int main(void)
{
    struct kui_shell_profile profile;
    struct kui_command_buffer display;

    /* win95-shell is normally a child of kuid.  Make the binary useful when
     * launched from the serial/virtual console too, instead of sending the
     * binary shell profile to a human-readable terminal and then blocking on
     * compositor requests that will never arrive. */
    if (launch_desktop_from_terminal() != 0) return 127;

    memset(&profile, 0, sizeof(profile));
    profile.magic = KUI_SHELL_MAGIC;
    profile.version = KUI_SHELL_VERSION;
    strcpy(profile.name, "KyleOS 95");
    kui_style_win95(&profile.style);
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
    return 0;
}
