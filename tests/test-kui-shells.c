#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "extras/kui/kui.h"
#include "extras/kuid/shell_protocol.h"

static int test_alpha_icon_commands(void)
{
    struct kui_command_buffer commands;
    struct kui_surface recording;
    struct kui_surface output;
    struct kui_image icon;
    uint32_t pixels[64 * 64];
    int failures = 0;

    memset(&icon, 0, sizeof(icon));
    if (kui_image_load("image/rootfs/usr/share/gui/calculator.kimg",
                       &icon) < 0)
        return 1;
    for (unsigned int i = 0; i < sizeof(pixels) / sizeof(pixels[0]); i++)
        pixels[i] = 0x00123456U;
    kui_command_buffer_init(&commands);
    kui_surface_init_commands(&recording, &commands, 64, 64);
    kui_blit_image_scaled(&recording, &icon, 10, 10, 42, 42);
    kui_surface_init(&output, pixels, 64, 64);
    failures += commands.failed || commands.length == 0;
    failures += kui_display_replay(&output, commands.data, commands.length) < 0;
    failures += pixels[10 * 64 + 10] != 0x00123456U;
    failures += pixels[31 * 64 + 31] == 0x00123456U;
    kui_command_buffer_reset(&commands);
    kui_surface_init_commands(&recording, &commands, 64, 64);
    kui_fill_rect_alpha(&recording, 0, 0, 1, 1, 0x800000ffU);
    pixels[0] = 0x00ff0000U;
    failures += commands.failed || commands.length == 0;
    failures += kui_display_replay(&output, commands.data,
                                   commands.length) < 0;
    failures += pixels[0] == 0x00ff0000U || pixels[0] == 0x000000ffU;
    kui_command_buffer_free(&commands);
    kui_image_free(&icon);
    return failures != 0;
}

static int test_font_metrics_and_commands(void)
{
    char clipped[32];
    struct kui_command_buffer commands;
    struct kui_display_command *command;
    struct kui_surface recording;
    struct kui_surface output;
    struct kui_style style;
    uint32_t pixels[240 * 40];
    int blended = 0;
    int failures = 0;

    kui_style_aqua(&style);
    kui_style_apply(&style);
    failures += kui_text_width("iiii") >= kui_text_width("WWWW");
    failures += kui_mono_text_width("iiii") != kui_mono_text_width("WWWW");
    failures += kui_text_ellipsize(clipped, sizeof(clipped), "readme.txt",
                                   200) != kui_text_width("readme.txt");
    failures += strcmp(clipped, "readme.txt") != 0;
    failures += kui_text_ellipsize(clipped, sizeof(clipped),
                                   "a-filename-that-is-far-too-long.txt",
                                   72) > 72;
    failures += strstr(clipped, "...") == NULL;
    failures += strcmp(clipped, "a-filename-that-is-far-too-long.txt") == 0;
    failures += kui_text_ellipsize(clipped, sizeof(clipped), "too long", 0) != 0;
    failures += clipped[0] != '\0';
    for (unsigned int i = 0; i < sizeof(pixels) / sizeof(pixels[0]); i++)
        pixels[i] = KUI_WHITE;
    kui_surface_init(&output, pixels, 240, 40);
    kui_draw_text(&output, 2, 2, "Aqua", KUI_BLACK, KUI_WHITE, 0);
    for (unsigned int i = 0; i < sizeof(pixels) / sizeof(pixels[0]); i++)
        if (pixels[i] != KUI_WHITE && pixels[i] != KUI_BLACK) blended++;
    failures += blended == 0;

    kui_command_buffer_init(&commands);
    kui_surface_init_commands(&recording, &commands, 240, 40);
    kui_draw_mono_text(&recording, 3, 4, "int main", KUI_BLACK,
                       KUI_WHITE, 0);
    command = (struct kui_display_command *)commands.data;
    failures += commands.failed || commands.length <= sizeof(*command) ||
                command->type != KUI_DISPLAY_TEXT ||
                !(command->flags & KUI_TEXT_MONOSPACE);
    failures += kui_display_replay(&output, commands.data,
                                   commands.length) < 0;
    kui_command_buffer_free(&commands);
    return failures != 0;
}

static int test_aqua_rounded_windows(void)
{
    struct kui_surface surface;
    struct kui_style style;
    uint32_t pixels[140 * 110];
    const uint32_t desktop = 0x002468acU;
    int failures = 0;

    for (unsigned int i = 0; i < sizeof(pixels) / sizeof(pixels[0]); i++)
        pixels[i] = desktop;
    kui_surface_init(&surface, pixels, 140, 110);
    kui_style_aqua(&style);
    kui_style_apply(&style);
    kui_draw_window(&surface, 16, 12, 100, 80, "Rounded", 1);
    failures += pixels[12 * 140 + 16] != desktop;
    failures += pixels[12 * 140 + 16 + 50] == desktop;
    failures += pixels[(12 + 79) * 140 + 16] != desktop;
    failures += pixels[(12 + 79) * 140 + 16 + 50] == desktop;

    for (unsigned int i = 0; i < sizeof(pixels) / sizeof(pixels[0]); i++)
        pixels[i] = desktop;
    kui_style_win95(&style);
    kui_style_apply(&style);
    kui_draw_window(&surface, 16, 12, 100, 80, "Square", 1);
    failures += pixels[12 * 140 + 16] == desktop;
    return failures != 0;
}

static int test_scrollbar_controls(void)
{
    struct kui_surface surface;
    struct kui_style style;
    struct kui_event event;
    struct kui_scrollbar_state vertical;
    struct kui_scrollbar_state horizontal;
    uint32_t pixels[120 * 80];
    int failures = 0;

    memset(pixels, 0, sizeof(pixels));
    memset(&event, 0, sizeof(event));
    memset(&vertical, 0, sizeof(vertical));
    memset(&horizontal, 0, sizeof(horizontal));
    kui_surface_init(&surface, pixels, 120, 80);
    kui_style_aqua(&style);
    kui_style_apply(&style);
    kui_draw_vscrollbar(&surface, 100, 0, 80, 20, 10, 100, &vertical);
    kui_draw_hscrollbar(&surface, 0, 60, 90, 5, 10, 40, &horizontal);
    failures += pixels[10 * 120 + 105] == 0;
    failures += pixels[65 * 120 + 40] == 0;
    event.type = KUI_EVENT_POINTER;
    event.button = 1;
    event.x = 105;
    event.y = 75;
    failures += kui_vscrollbar_handle(&vertical, &event, 100, 0, 80,
                                      20, 10, 100) != 21;
    event.x = 85;
    event.y = 65;
    failures += kui_hscrollbar_handle(&horizontal, &event, 0, 60, 90,
                                      5, 10, 40) != 6;
    failures += kui_scrollbar_timer_ms(&vertical) != 320U;
    event.type = KUI_EVENT_TIMER;
    failures += kui_vscrollbar_handle(&vertical, &event, 100, 0, 80,
                                      21, 10, 100) != 22;
    failures += kui_scrollbar_timer_ms(&vertical) != 65U;
    event.type = KUI_EVENT_POINTER;
    event.button = 0;
    event.buttons = 0;
    (void)kui_vscrollbar_handle(&vertical, &event, 100, 0, 80,
                                22, 10, 100);
    failures += kui_scrollbar_timer_ms(&vertical) != 0;
    failures += kui_scrollbar_wheel(10, 1, 5, 30) != 7;
    failures += kui_scrollbar_wheel(0, 1, 5, 30) != 0;
    event.button = 1;
    event.x = 105;
    event.y = 55;
    failures += kui_vscrollbar_handle(&vertical, &event, 100, 0, 80,
                                      22, 10, 100) != 21;
    failures += vertical.active_direction != -1;
    event.x = 65;
    event.y = 65;
    failures += kui_hscrollbar_handle(&horizontal, &event, 0, 60, 90,
                                      6, 10, 40) != 5;
    failures += horizontal.active_direction != -1;
    return failures != 0;
}

static int exchange(int input, int output, struct kui_shell_request *request,
                    struct kui_shell_response *response,
                    struct kui_surface *surface)
{
    unsigned char *display = NULL;

    if (kui_write_all(input, request, sizeof(*request)) < 0 ||
        kui_read_all(output, response, sizeof(*response)) < 0 ||
        response->magic != KUI_SHELL_MAGIC ||
        response->serial != request->serial ||
        response->display_bytes > KUI_PROTOCOL_MAX_DISPLAY)
        return -1;
    if (response->display_bytes != 0) {
        display = malloc(response->display_bytes);
        if (display == NULL ||
            kui_read_all(output, display, response->display_bytes) < 0 ||
            (surface != NULL && kui_display_replay(
                surface, display, response->display_bytes) < 0)) {
            free(display);
            return -1;
        }
    }
    free(display);
    return 0;
}

static int run_shell(const char *path, int aqua)
{
    struct kui_shell_profile profile;
    struct kui_shell_request request;
    struct kui_shell_response response;
    struct kui_surface surface;
    uint32_t *pixels = calloc(800U * 600U, sizeof(*pixels));
    int to_shell[2];
    int from_shell[2];
    int child;
    int status = 0;
    int failures = 0;

    if (pixels == NULL || pipe(to_shell) < 0 || pipe(from_shell) < 0)
        return 1;
    child = fork();
    if (child == 0) {
        dup2(to_shell[0], 0);
        dup2(from_shell[1], 1);
        close(to_shell[0]); close(to_shell[1]);
        close(from_shell[0]); close(from_shell[1]);
        execl(path, path, NULL);
        _exit(127);
    }
    close(to_shell[0]);
    close(from_shell[1]);
    if (child < 0 || kui_read_all(from_shell[0], &profile, sizeof(profile)) < 0)
        return 1;
    failures += profile.magic != KUI_SHELL_MAGIC;
    failures += !!(profile.style.flags & KUI_STYLE_AQUA) != aqua;
    kui_surface_init(&surface, pixels, 800, 600);
    memset(&request, 0, sizeof(request));
    request.magic = KUI_SHELL_MAGIC;
    request.version = KUI_SHELL_VERSION;
    request.serial = 1;
    request.type = KUI_SHELL_RENDER;
    request.screen_width = 800;
    request.screen_height = 600;
    request.ticks_ms = 3720000;
    strcpy(request.active_application, "Finder");
    request.app_count = aqua ? 9 : 2;
    request.menu_app_count = request.app_count;
    request.window_count = 1;
    strcpy(request.apps[0].title, "Finder");
    strcpy(request.apps[1].title, "Paint");
    strcpy(request.windows[0].title, "Documents");
    strcpy(request.apps[0].icon_path,
           "image/rootfs/usr/share/gui/editor.kimg");
    strcpy(request.apps[1].icon_path,
           "image/rootfs/usr/share/gui/editor.kimg");
    for (int app = 2; app < request.app_count; app++) {
        snprintf(request.apps[app].title,
                 sizeof(request.apps[app].title), "App %d", app);
        strcpy(request.apps[app].icon_path,
               "image/rootfs/usr/share/gui/editor.kimg");
    }
    strcpy(request.windows[0].icon_path,
           "image/rootfs/usr/share/gui/editor.kimg");
    request.windows[0].active = 1;
    request.windows[0].app_index = 0;
    failures += exchange(to_shell[1], from_shell[0], &request,
                         &response, &surface) < 0;
    failures += response.display_bytes == 0;
    if (aqua) {
        int dock_y = 600 - 86;
        int dock_x = (800 - (20 + request.app_count * 70)) / 2;
        int zoom_pixels = 0;

        failures += pixels[2 * 800 + 400] == pixels[3 * 800 + 400];
        failures += pixels[dock_y * 800 + 400] == 0;
        failures += pixels[(dock_y + 30) * 800 + 400] == 0;
        for (int redraw = 0; redraw < 24; redraw++) {
            request.serial++;
            failures += exchange(to_shell[1], from_shell[0], &request,
                                 &response, &surface) < 0;
            failures += response.display_bytes == 0;
        }
        memset(pixels, 0, 800U * 600U * sizeof(*pixels));
        request.serial++;
        request.x = dock_x + 10 + 29;
        request.y = dock_y + 30;
        failures += exchange(to_shell[1], from_shell[0], &request,
                             &response, &surface) < 0;
        for (int y = dock_y - 18; y < dock_y; y++)
            for (int x = dock_x; x < dock_x + 68; x++)
                if (pixels[y * 800 + x] != 0) zoom_pixels++;
        failures += zoom_pixels == 0;
    }
    if (aqua) {
        request.serial++;
        request.type = KUI_SHELL_POINTER;
        request.x = 10;
        request.y = 10;
        failures += exchange(to_shell[1], from_shell[0], &request,
                             &response, NULL) < 0;
        failures += !response.consumed;
        request.serial++;
        request.type = KUI_SHELL_RENDER;
        failures += exchange(to_shell[1], from_shell[0], &request,
                             &response, &surface) < 0;
        failures += response.display_bytes == 0;
        request.serial++;
        request.type = KUI_SHELL_POINTER;
        request.x = 20;
        request.y = 36;
        failures += exchange(to_shell[1], from_shell[0], &request,
                             &response, NULL) < 0;
        failures += response.action != KUI_SHELL_ACTION_OPEN_APP ||
                    response.index != 0;
    } else {
        int height = 12 + (request.app_count + 2) * 26;

        request.serial++;
        request.type = KUI_SHELL_POINTER;
        request.x = 10;
        request.y = 590;
        failures += exchange(to_shell[1], from_shell[0], &request,
                             &response, NULL) < 0;
        failures += !response.consumed;
        request.serial++;
        request.type = KUI_SHELL_RENDER;
        failures += exchange(to_shell[1], from_shell[0], &request,
                             &response, &surface) < 0;
        failures += response.display_bytes == 0;
        request.serial++;
        request.type = KUI_SHELL_POINTER;
        request.x = 20;
        request.y = 600 - 32 - height + 10;
        failures += exchange(to_shell[1], from_shell[0], &request,
                             &response, NULL) < 0;
        failures += response.action != KUI_SHELL_ACTION_OPEN_APP ||
                    response.index != 0;
    }
    close(to_shell[1]);
    close(from_shell[0]);
    waitpid(child, &status, 0);
    failures += !WIFEXITED(status) || WEXITSTATUS(status) != 0;
    free(pixels);
    return failures != 0;
}

int main(int argc, char **argv)
{
    if (argc != 3)
        return 2;
    if (test_alpha_icon_commands() || test_font_metrics_and_commands() ||
        test_aqua_rounded_windows() || test_scrollbar_controls() ||
        run_shell(argv[1], 0) || run_shell(argv[2], 1)) {
        fputs("FAIL KUI shell protocol integration\n", stderr);
        return 1;
    }
    puts("KUI shell protocol integration passed");
    return 0;
}
