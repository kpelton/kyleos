#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "extras/kui/kui_protocol.h"
#include "extras/kui/kui_style.h"

static int transfer(int fd, void *buffer, size_t bytes, int writing)
{
    unsigned char *cursor = buffer;

    while (bytes != 0) {
        ssize_t count = writing ? write(fd, cursor, bytes) :
                                  read(fd, cursor, bytes);

        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -1;
        cursor += count;
        bytes -= (size_t)count;
    }
    return 0;
}

static int exchange(int input, int output, struct kui_event *event,
                    struct kui_response *response)
{
    unsigned char *display = NULL;

    if (transfer(input, event, sizeof(*event), 1) < 0 ||
        transfer(output, response, sizeof(*response), 0) < 0 ||
        response->magic != KUI_PROTOCOL_MAGIC ||
        response->version != KUI_PROTOCOL_VERSION ||
        response->serial != event->serial ||
        response->display_bytes > KUI_PROTOCOL_MAX_DISPLAY)
        return -1;
    if (response->display_bytes != 0) {
        display = malloc(response->display_bytes);
        if (display == NULL ||
            transfer(output, display, response->display_bytes, 0) < 0) {
            free(display);
            return -1;
        }
        free(display);
    }
    return 0;
}

static int send_pointer(int input, int output, struct kui_event *event,
                        struct kui_response *response,
                        int x, int y, int button, int buttons)
{
    event->type = KUI_EVENT_POINTER;
    event->serial++;
    event->x = x;
    event->y = y;
    event->button = button;
    event->buttons = buttons;
    event->wheel = 0;
    event->key_pressed = 0;
    return exchange(input, output, event, response);
}

int main(int argc, char **argv)
{
    struct kui_event event;
    struct kui_response response;
    int to_child[2];
    int from_child[2];
    int failures = 0;
    int status = 0;
    pid_t child;

    if (argc != 2 || pipe(to_child) < 0 || pipe(from_child) < 0) return 2;
    child = fork();
    if (child == 0) {
        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        execl(argv[1], argv[1], "--kui-client", (char *)NULL);
        _exit(127);
    }
    if (child < 0) return 2;
    close(to_child[0]);
    close(from_child[1]);

    memset(&event, 0, sizeof(event));
    event.magic = KUI_PROTOCOL_MAGIC;
    event.version = KUI_PROTOCOL_VERSION;
    event.type = KUI_EVENT_INIT;
    event.serial = 1;
    event.width = 760;
    event.height = 540;
    kui_style_aqua(&event.style);
    failures += exchange(to_child[1], from_child[0], &event, &response) < 0;
    failures += strcmp(response.title, "KyleOS Help") != 0;
    failures += response.min_width != 650 || response.min_height != 500;
    failures += response.display_bytes == 0;

    /* Select Terminal in the sidebar. */
    failures += send_pointer(to_child[1], from_child[0], &event, &response,
                             50, 38 + 6 * 25 + 5, 1, 1) < 0;
    failures += response.display_bytes == 0;

    /* Aqua's down arrow is the last 15 pixels of the trailing arrow pair. */
    failures += send_pointer(to_child[1], from_child[0], &event, &response,
                             752, 532, 1, 1) < 0;
    failures += response.timer_ms != 320U;
    failures += send_pointer(to_child[1], from_child[0], &event, &response,
                             752, 532, 0, 0) < 0;
    failures += response.timer_ms != 0;

    event.type = KUI_EVENT_CLOSE;
    event.serial++;
    (void)transfer(to_child[1], &event, sizeof(event), 1);
    close(to_child[1]);
    close(from_child[0]);
    waitpid(child, &status, 0);
    failures += !WIFEXITED(status) || WEXITSTATUS(status) != 0;
    if (failures != 0) {
        fprintf(stderr, "Help UI test failed: %d check(s)\n", failures);
        return 1;
    }
    puts("Help UI integration passed");
    return 0;
}
