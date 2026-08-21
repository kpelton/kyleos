#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "extras/kui/kui.h"

static int write_all_fd(int fd, const void *buffer, size_t size)
{
    const unsigned char *bytes = buffer;

    while (size != 0) {
        ssize_t amount = write(fd, bytes, size);

        if (amount <= 0) return -1;
        bytes += amount;
        size -= (size_t)amount;
    }
    return 0;
}

static int read_all_fd(int fd, void *buffer, size_t size)
{
    unsigned char *bytes = buffer;

    while (size != 0) {
        ssize_t amount = read(fd, bytes, size);

        if (amount <= 0) return -1;
        bytes += amount;
        size -= (size_t)amount;
    }
    return 0;
}

static int exchange(int input, int output, struct kui_event *event,
                    struct kui_surface *surface, uint32_t *display_bytes)
{
    struct kui_response response;
    unsigned char *display;
    int result = -1;

    if (write_all_fd(input, event, sizeof(*event)) < 0 ||
        read_all_fd(output, &response, sizeof(response)) < 0 ||
        response.magic != KUI_PROTOCOL_MAGIC ||
        response.version != KUI_PROTOCOL_VERSION ||
        response.serial != event->serial ||
        response.display_bytes == 0 ||
        response.display_bytes > KUI_PROTOCOL_MAX_DISPLAY)
        return -1;
    display = malloc(response.display_bytes);
    if (display == NULL) return -1;
    if (read_all_fd(output, display, response.display_bytes) == 0 &&
        kui_display_replay(surface, display, response.display_bytes) == 0) {
        *display_bytes = response.display_bytes;
        result = 0;
    }
    free(display);
    return result;
}

int main(int argc, char **argv)
{
    struct kui_surface surface;
    struct kui_event event;
    struct kui_style style;
    uint32_t *pixels;
    uint32_t display_bytes = 0;
    int to_game[2];
    int from_game[2];
    int status;
    int failures = 0;
    pid_t child;

    if (argc != 2) return 2;
    if (pipe(to_game) < 0 || pipe(from_game) < 0) return 2;
    child = fork();
    if (child < 0) return 2;
    if (child == 0) {
        dup2(to_game[0], STDIN_FILENO);
        dup2(from_game[1], STDOUT_FILENO);
        close(to_game[0]);
        close(to_game[1]);
        close(from_game[0]);
        close(from_game[1]);
        execl(argv[1], argv[1], (char *)NULL);
        _exit(127);
    }
    close(to_game[0]);
    close(from_game[1]);
    pixels = calloc(450U * 380U, sizeof(*pixels));
    if (pixels == NULL) return 2;
    kui_surface_init(&surface, pixels, 450, 380);
    kui_style_aqua(&style);
    kui_style_apply(&style);
    memset(&event, 0, sizeof(event));
    event.magic = KUI_PROTOCOL_MAGIC;
    event.version = KUI_PROTOCOL_VERSION;
    event.serial = 1;
    event.type = KUI_EVENT_INIT;
    event.width = 450;
    event.height = 380;
    event.seed = 12345;
    event.style = style;
    failures += exchange(to_game[1], from_game[0], &event,
                         &surface, &display_bytes) < 0;
    failures += display_bytes < 180000U;
    failures += display_bytes >= KUI_PROTOCOL_MAX_DISPLAY;

    event.serial++;
    event.type = KUI_EVENT_KEY;
    event.key_pressed = 1;
    event.key_extended = 1;
    event.key_code = 75;
    failures += exchange(to_game[1], from_game[0], &event,
                         &surface, &display_bytes) < 0;
    event.serial++;
    event.key_extended = 0;
    event.key_code = 57;
    event.character = ' ';
    failures += exchange(to_game[1], from_game[0], &event,
                         &surface, &display_bytes) < 0;
    event.serial++;
    event.type = KUI_EVENT_TIMER;
    event.key_pressed = 0;
    event.character = '\0';
    failures += exchange(to_game[1], from_game[0], &event,
                         &surface, &display_bytes) < 0;

    close(to_game[1]);
    close(from_game[0]);
    waitpid(child, &status, 0);
    failures += !WIFEXITED(status) || WEXITSTATUS(status) != 0;
    free(pixels);
    if (failures != 0) {
        fprintf(stderr, "FAIL tetris UI: %d failure(s)\n", failures);
        return 1;
    }
    printf("Tetris UI integration passed (%u-byte frame)\n", display_bytes);
    return 0;
}
