#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "extras/desktop/desktop_config.h"
#include "extras/kui/kui.h"

static int exchange(int input, int output, struct kui_event *event,
                    struct kui_response *response)
{
    unsigned char *display = NULL;
    uint32_t *pixels = NULL;
    struct kui_surface surface;

    if (kui_write_all(input, event, sizeof(*event)) < 0 ||
        kui_read_all(output, response, sizeof(*response)) < 0 ||
        response->magic != KUI_PROTOCOL_MAGIC ||
        response->version != KUI_PROTOCOL_VERSION ||
        response->serial != event->serial ||
        response->display_bytes > KUI_PROTOCOL_MAX_DISPLAY)
        return -1;
    if (response->display_bytes != 0) {
        display = malloc(response->display_bytes);
        pixels = calloc((size_t)event->width * event->height,
                        sizeof(*pixels));
        if (display == NULL ||
            pixels == NULL ||
            kui_read_all(output, display, response->display_bytes) < 0) {
            free(display);
            free(pixels);
            return -1;
        }
        kui_surface_init(&surface, pixels, event->width, event->height);
        if (kui_display_replay(&surface, display,
                               response->display_bytes) < 0) {
            free(display);
            free(pixels);
            return -1;
        }
    }
    free(display);
    free(pixels);
    return 0;
}

static int click(int input, int output, struct kui_event *event,
                 struct kui_response *response, int x, int y)
{
    event->serial++;
    event->type = KUI_EVENT_POINTER;
    event->x = x;
    event->y = y;
    event->button = 1;
    event->buttons = 1;
    return exchange(input, output, event, response);
}

int main(int argc, char **argv)
{
    struct desktop_config config;
    struct kui_event event;
    struct kui_response response;
    struct kui_style style;
    int to_child[2];
    int from_child[2];
    int child;
    int status;
    int failures = 0;

    if (argc != 2) return 2;
    remove(DESKTOP_CONFIG_PATH);
    desktop_config_defaults(&config);
    if (desktop_config_save(&config, DESKTOP_CONFIG_PATH) < 0) return 2;
    if (pipe(to_child) < 0 || pipe(from_child) < 0) return 2;
    child = fork();
    if (child == 0) {
        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        execl(argv[1], argv[1], "--kui-client", NULL);
        _exit(127);
    }
    close(to_child[0]);
    close(from_child[1]);
    if (child < 0) return 2;

    memset(&event, 0, sizeof(event));
    kui_style_aqua(&style);
    event.magic = KUI_PROTOCOL_MAGIC;
    event.version = KUI_PROTOCOL_VERSION;
    event.type = KUI_EVENT_INIT;
    event.serial = 1;
    event.width = 620;
    event.height = 430;
    event.style = style;
    failures += exchange(to_child[1], from_child[0], &event, &response) < 0;
    failures += response.display_bytes == 0;

    failures += click(to_child[1], from_child[0], &event, &response,
                      370, 250) < 0;
    failures += click(to_child[1], from_child[0], &event, &response,
                      550, 400) < 0;
    failures += response.action != KUI_ACTION_SETTINGS_CHANGED;
    failures += desktop_config_load(&config, DESKTOP_CONFIG_PATH) < 0;
    failures += config.pointer_speed != 3;

    failures += click(to_child[1], from_child[0], &event, &response,
                      80, 150) < 0;
    failures += click(to_child[1], from_child[0], &event, &response,
                      550, 400) < 0;
    failures += desktop_config_load(&config, DESKTOP_CONFIG_PATH) < 0;
    failures += config.show_desktop_icons != 0;

    failures += click(to_child[1], from_child[0], &event, &response,
                      350, 90) < 0;
    failures += click(to_child[1], from_child[0], &event, &response,
                      550, 400) < 0;
    failures += desktop_config_load(&config, DESKTOP_CONFIG_PATH) < 0;
    failures += strcmp(config.aqua_wallpaper,
                       DESKTOP_WALLPAPER_WIN95) != 0;

    event.serial++;
    event.type = KUI_EVENT_CLOSE;
    kui_write_all(to_child[1], &event, sizeof(event));
    close(to_child[1]);
    close(from_child[0]);
    waitpid(child, &status, 0);
    failures += !WIFEXITED(status) || WEXITSTATUS(status) != 0;
    remove(DESKTOP_CONFIG_PATH);
    if (failures != 0) {
        fputs("FAIL Control Center UI integration\n", stderr);
        return 1;
    }
    puts("Control Center UI integration passed");
    return 0;
}
