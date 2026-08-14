#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "extras/kui/kui.h"
#include "extras/kui/kui_protocol.h"

int main(int argc, char **argv)
{
    struct kui_event event;
    struct kui_response response;
    struct kui_surface surface;
    unsigned char *display;
    uint32_t *pixels;
    uint32_t checksum = 0;
    int to_child[2];
    int from_child[2];
    int child;
    int status;

    if ((argc != 2 && argc != 3) ||
        pipe(to_child) < 0 || pipe(from_child) < 0) return 2;
    child = fork();
    if (child == 0) {
        char *child_argv[] = { argv[1], "--kui-client", NULL };
        close(to_child[1]);
        close(from_child[0]);
        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        close(to_child[0]);
        close(from_child[1]);
        execv(argv[1], child_argv);
        _exit(127);
    }
    close(to_child[0]);
    close(from_child[1]);
    memset(&event, 0, sizeof(event));
    event.magic = KUI_PROTOCOL_MAGIC;
    event.version = KUI_PROTOCOL_VERSION;
    event.type = KUI_EVENT_INIT;
    event.serial = 1;
    event.width = 399;
    event.height = 521;
    if (argc == 3)
        strncpy(event.argument, argv[2], sizeof(event.argument) - 1);
    if (kui_write_all(to_child[1], &event, sizeof(event)) < 0 ||
        kui_read_all(from_child[0], &response, sizeof(response)) < 0 ||
        response.magic != KUI_PROTOCOL_MAGIC || response.serial != 1 ||
        response.display_bytes == 0 ||
        response.display_bytes > KUI_PROTOCOL_MAX_DISPLAY)
        return 3;
    display = malloc(response.display_bytes);
    pixels = calloc((size_t)event.width * event.height, sizeof(uint32_t));
    if (display == NULL || pixels == NULL ||
        kui_read_all(from_child[0], display, response.display_bytes) < 0)
        return 4;
    kui_surface_init(&surface, pixels, event.width, event.height);
    if (kui_display_replay(&surface, display, response.display_bytes) < 0)
        return 5;
    for (int i = 0; i < event.width * event.height; i++) checksum ^= pixels[i];
    if (checksum == 0) return 6;
    event.type = KUI_EVENT_KEY;
    event.serial = 2;
    event.key_pressed = 1;
    event.character = '1';
    if (kui_write_all(to_child[1], &event, sizeof(event)) < 0 ||
        kui_read_all(from_child[0], &response, sizeof(response)) < 0 ||
        response.magic != KUI_PROTOCOL_MAGIC || response.serial != 2 ||
        response.display_bytes == 0 ||
        response.display_bytes > KUI_PROTOCOL_MAX_DISPLAY)
        return 7;
    display = realloc(display, response.display_bytes);
    if (display == NULL ||
        kui_read_all(from_child[0], display, response.display_bytes) < 0 ||
        kui_display_replay(&surface, display, response.display_bytes) < 0)
        return 8;
    event.type = KUI_EVENT_CLOSE;
    event.serial = 3;
    if (kui_write_all(to_child[1], &event, sizeof(event)) < 0) return 7;
    close(to_child[1]);
    close(from_child[0]);
    waitpid(child, &status, 0);
    free(display);
    free(pixels);
    return !WIFEXITED(status) || WEXITSTATUS(status) != 0;
}
