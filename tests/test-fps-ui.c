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
                    struct kui_surface *surface,
                    struct kui_response *response)
{
    unsigned char *display;

    if (write_all_fd(input, event, sizeof(*event)) < 0 ||
        read_all_fd(output, response, sizeof(*response)) < 0 ||
        response->magic != KUI_PROTOCOL_MAGIC ||
        response->version != KUI_PROTOCOL_VERSION ||
        response->serial != event->serial ||
        response->display_bytes == 0 ||
        response->display_bytes > KUI_PROTOCOL_MAX_DISPLAY)
        return -1;
    display = malloc(response->display_bytes);
    if (display == NULL) return -1;
    if (read_all_fd(output, display, response->display_bytes) < 0 ||
        kui_display_replay(surface, display, response->display_bytes) < 0) {
        free(display);
        return -1;
    }
    free(display);
    return 0;
}

int main(int argc, char **argv)
{
    struct kui_surface surface;
    struct kui_event event;
    struct kui_response response;
    uint32_t *pixels;
    int to_game[2];
    int from_game[2];
    int status;
    int failures = 0;
    pid_t child;

    if (argc != 2 || pipe(to_game) < 0 || pipe(from_game) < 0) return 2;
    child = fork();
    if (child < 0) return 2;
    if (child == 0) {
        dup2(to_game[0], STDIN_FILENO);
        dup2(from_game[1], STDOUT_FILENO);
        close(to_game[0]); close(to_game[1]);
        close(from_game[0]); close(from_game[1]);
        execl(argv[1], argv[1], (char *)NULL);
        _exit(127);
    }
    close(to_game[0]);
    close(from_game[1]);
    pixels = calloc(800U * 560U, sizeof(*pixels));
    if (pixels == NULL) return 2;
    kui_surface_init(&surface, pixels, 800, 560);
    memset(&event, 0, sizeof(event));
    event.magic = KUI_PROTOCOL_MAGIC;
    event.version = KUI_PROTOCOL_VERSION;
    event.type = KUI_EVENT_INIT;
    event.serial = 1;
    event.width = 800;
    event.height = 560;
    kui_style_aqua(&event.style);
    failures += exchange(to_game[1], from_game[0], &event,
                         &surface, &response) < 0;
    failures += strcmp(response.title, "KyleFPS") != 0;
    failures += response.min_width != 560 || response.min_height != 420;
    failures += response.timer_ms != 32;
    failures += response.display_bytes < 20000U;
    failures += pixels[10 * 800 + 10] == 0;
    failures += pixels[520 * 800 + 400] == 0;

    event.serial++;
    event.type = KUI_EVENT_KEY;
    event.key_code = 57;
    event.key_pressed = 1;
    failures += exchange(to_game[1], from_game[0], &event,
                         &surface, &response) < 0;
    event.serial++;
    event.key_code = 4;
    failures += exchange(to_game[1], from_game[0], &event,
                         &surface, &response) < 0;
    event.serial++;
    event.type = KUI_EVENT_TIMER;
    event.key_code = 0;
    event.key_pressed = 0;
    failures += exchange(to_game[1], from_game[0], &event,
                         &surface, &response) < 0;

    event.serial++;
    event.type = KUI_EVENT_CLOSE;
    (void)write_all_fd(to_game[1], &event, sizeof(event));
    close(to_game[1]);
    close(from_game[0]);
    waitpid(child, &status, 0);
    failures += !WIFEXITED(status) || WEXITSTATUS(status) != 0;
    free(pixels);
    if (failures != 0) {
        fprintf(stderr, "FPS UI failed: %d check(s)\n", failures);
        return 1;
    }
    printf("FPS UI passed (%u-byte frame)\n", response.display_bytes);
    return 0;
}
