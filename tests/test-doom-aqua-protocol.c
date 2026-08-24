#include <stdint.h>
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
                    struct kui_response *response, int expect_display)
{
    unsigned char *display = NULL;

    if (write_all_fd(input, event, sizeof(*event)) < 0 ||
        read_all_fd(output, response, sizeof(*response)) < 0 ||
        response->magic != KUI_PROTOCOL_MAGIC ||
        response->version != KUI_PROTOCOL_VERSION ||
        response->serial != event->serial ||
        response->display_bytes > KUI_PROTOCOL_MAX_DISPLAY)
        return -1;
    if (!!response->display_bytes != !!expect_display) return -1;
    if (response->display_bytes != 0) {
        display = malloc(response->display_bytes);
        if (display == NULL ||
            read_all_fd(output, display, response->display_bytes) < 0 ||
            kui_display_replay(surface, display, response->display_bytes) < 0) {
            free(display);
            return -1;
        }
    }
    free(display);
    return 0;
}

int main(int argc, char **argv)
{
    struct kui_event event;
    struct kui_response response;
    struct kui_surface surface;
    uint32_t *pixels;
    int to_game[2];
    int from_game[2];
    int status;
    int failures = 0;
    pid_t child;

    unlink("/tmp/doom-aqua.log");
    if (argc != 2 || pipe(to_game) < 0 || pipe(from_game) < 0) return 2;
    child = fork();
    if (child < 0) return 2;
    if (child == 0) {
        dup2(to_game[0], STDIN_FILENO);
        dup2(from_game[1], STDOUT_FILENO);
        close(to_game[0]); close(to_game[1]);
        close(from_game[0]); close(from_game[1]);
        execl(argv[1], argv[1], "--kui-client", (char *)NULL);
        _exit(127);
    }
    close(to_game[0]);
    close(from_game[1]);
    pixels = calloc(640U * 400U, sizeof(*pixels));
    if (pixels == NULL) return 2;
    kui_surface_init(&surface, pixels, 640, 400);

    memset(&event, 0, sizeof(event));
    event.magic = KUI_PROTOCOL_MAGIC;
    event.version = KUI_PROTOCOL_VERSION;
    event.type = KUI_EVENT_INIT;
    event.serial = 1;
    event.width = 640;
    event.height = 400;
    kui_style_aqua(&event.style);
    failures += exchange(to_game[1], from_game[0], &event,
                         &surface, &response, 1) < 0;
    failures += strcmp(response.title, "Doom") != 0;
    failures += response.timer_ms != 5000;

    event.serial++;
    event.type = KUI_EVENT_TIMER;
    failures += exchange(to_game[1], from_game[0], &event,
                         &surface, &response, 1) < 0;
    failures += strcmp(response.title, "DOOM Shareware") != 0;
    failures += response.timer_ms != 28;
    failures += response.display_bytes != 262432U;
    failures += pixels[200 * 640 + 320] != 0x00123456U;

    event.serial++;
    event.type = KUI_EVENT_KEY;
    event.key_code = 29;
    event.key_pressed = 1;
    failures += exchange(to_game[1], from_game[0], &event,
                         &surface, &response, 0) < 0;

    event.serial++;
    event.type = KUI_EVENT_TIMER;
    event.key_code = 0;
    event.key_pressed = 0;
    failures += exchange(to_game[1], from_game[0], &event,
                         &surface, &response, 1) < 0;
    failures += strcmp(response.title, "DOOM Shareware") != 0;
    failures += response.timer_ms != 28;
    failures += response.display_bytes != 262432U;
    failures += pixels[200 * 640 + 320] != 0x00aa0000U;

    event.serial++;
    event.type = KUI_EVENT_CLOSE;
    (void)write_all_fd(to_game[1], &event, sizeof(event));
    close(to_game[1]);
    close(from_game[0]);
    waitpid(child, &status, 0);
    failures += !WIFEXITED(status) || WEXITSTATUS(status) != 0;
    {
        FILE *log = fopen("/tmp/doom-aqua.log", "r");
        char text[128] = {0};

        failures += log == NULL;
        if (log != NULL) {
            (void)fread(text, 1, sizeof(text) - 1, log);
            fclose(log);
            failures += strstr(text, "Doom Aqua protocol diagnostic") == NULL;
        }
    }
    free(pixels);
    if (failures != 0) {
        fprintf(stderr, "Doom Aqua protocol failed: %d check(s)\n", failures);
        return 1;
    }
    puts("Doom Aqua protocol passed");
    return 0;
}
