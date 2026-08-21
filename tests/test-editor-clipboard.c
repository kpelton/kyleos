#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "extras/kui/kui_protocol.h"
#include "extras/kui/kui_style.h"

static int write_all(int fd, const void *buffer, size_t bytes)
{
    const unsigned char *cursor = buffer;

    while (bytes != 0) {
        ssize_t count = write(fd, cursor, bytes);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -1;
        cursor += count;
        bytes -= (size_t)count;
    }
    return 0;
}

static int read_all(int fd, void *buffer, size_t bytes)
{
    unsigned char *cursor = buffer;

    while (bytes != 0) {
        ssize_t count = read(fd, cursor, bytes);
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
    void *display = 0;

    if (write_all(input, event, sizeof(*event)) < 0 ||
        read_all(output, response, sizeof(*response)) < 0)
        return -1;
    if (response->magic != KUI_PROTOCOL_MAGIC ||
        response->version != KUI_PROTOCOL_VERSION ||
        response->serial != event->serial ||
        response->display_bytes > KUI_PROTOCOL_MAX_DISPLAY)
        return -1;
    if (response->display_bytes != 0) {
        display = malloc(response->display_bytes);
        if (display == 0 ||
            read_all(output, display, response->display_bytes) < 0) {
            free(display);
            return -1;
        }
        free(display);
    }
    return 0;
}

static int send_key(int input, int output, struct kui_event *event,
                    struct kui_response *response, int code,
                    int extended, uint32_t modifiers, char character)
{
    event->type = KUI_EVENT_KEY;
    event->serial++;
    event->key_code = code;
    event->key_extended = extended;
    event->key_pressed = 1;
    event->modifiers = modifiers;
    event->character = character;
    event->button = 0;
    event->buttons = 0;
    return exchange(input, output, event, response);
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
    event->key_pressed = 0;
    event->modifiers = 0;
    event->character = 0;
    return exchange(input, output, event, response);
}

static int file_equals(const char *path, const char *expected)
{
    char buffer[64];
    FILE *file = fopen(path, "rb");
    size_t count;

    if (file == 0) return 0;
    count = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[count] = '\0';
    return strcmp(buffer, expected) == 0;
}

int main(int argc, char **argv)
{
    const char *path = "/tmp/os3edui.txt";
    const char *save_as_path = "/tmp/os3edui-saveas.txt";
    int to_child[2];
    int from_child[2];
    pid_t child;
    struct kui_event event;
    struct kui_response response;
    FILE *file;
    int status = 0;
    int failures = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: %s EDITOR-HOST\n", argv[0]);
        return 2;
    }
    file = fopen(path, "wb");
    if (file == 0) return 2;
    fclose(file);
    remove(save_as_path);
    if (pipe(to_child) < 0 || pipe(from_child) < 0) return 2;
    child = fork();
    if (child == 0) {
        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        close(to_child[0]);
        close(to_child[1]);
        close(from_child[0]);
        close(from_child[1]);
        execl(argv[1], argv[1], (char *)0);
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
    event.width = 700;
    event.height = 481;
    kui_style_win95(&event.style);
    strncpy(event.argument, path, sizeof(event.argument) - 1);
    failures += exchange(to_child[1], from_child[0], &event, &response) < 0;

    failures += send_key(to_child[1], from_child[0], &event, &response,
                         30, 0, 0, 'a') < 0;
    failures += send_key(to_child[1], from_child[0], &event, &response,
                         48, 0, 0, 'b') < 0;
    failures += send_key(to_child[1], from_child[0], &event, &response,
                         46, 0, 0, 'c') < 0;

    failures += send_key(to_child[1], from_child[0], &event, &response,
                         30, 0, KUI_MOD_CTRL, 0) < 0;
    failures += send_key(to_child[1], from_child[0], &event, &response,
                         46, 0, KUI_MOD_CTRL, 0) < 0;
    if (response.action != KUI_ACTION_COPY_TEXT ||
        strcmp(response.action_argument, "abc") != 0)
        failures++;
    event.clipboard_kind = KUI_CLIPBOARD_TEXT;
    strncpy(event.clipboard, response.action_argument,
            sizeof(event.clipboard) - 1);

    failures += send_key(to_child[1], from_child[0], &event, &response,
                         77, 1, 0, 0) < 0;
    failures += send_key(to_child[1], from_child[0], &event, &response,
                         47, 0, KUI_MOD_CTRL, 0) < 0;
    failures += send_key(to_child[1], from_child[0], &event, &response,
                         31, 0, KUI_MOD_CTRL, 0) < 0;
    if (!file_equals(path, "abcabc")) failures++;

    failures += send_key(to_child[1], from_child[0], &event, &response,
                         31, 0, KUI_MOD_CTRL | KUI_MOD_SHIFT, 0) < 0;
    for (const char *name = "os3edui-saveas.txt"; *name != '\0'; name++)
        failures += send_key(to_child[1], from_child[0], &event, &response,
                             0, 0, 0, *name) < 0;
    failures += send_key(to_child[1], from_child[0], &event, &response,
                         28, 0, 0, '\n') < 0;
    if (response.action != KUI_ACTION_FILE_SAVED ||
        !file_equals(save_as_path, "abcabc")) failures++;

    failures += send_key(to_child[1], from_child[0], &event, &response,
                         30, 0, KUI_MOD_CTRL, 0) < 0;
    failures += send_key(to_child[1], from_child[0], &event, &response,
                         45, 0, KUI_MOD_CTRL, 0) < 0;
    if (response.action != KUI_ACTION_COPY_TEXT ||
        strcmp(response.action_argument, "abcabc") != 0)
        failures++;
    event.clipboard_kind = KUI_CLIPBOARD_TEXT;
    strncpy(event.clipboard, response.action_argument,
            sizeof(event.clipboard) - 1);
    failures += send_key(to_child[1], from_child[0], &event, &response,
                         47, 0, KUI_MOD_CTRL, 0) < 0;

    failures += send_key(to_child[1], from_child[0], &event, &response,
                         30, 0, KUI_MOD_CTRL, 0) < 0;
    failures += send_pointer(to_child[1], from_child[0], &event, &response,
                             8, 30, 2, 2) < 0;
    failures += send_pointer(to_child[1], from_child[0], &event, &response,
                             18, 109, 1, 1) < 0;
    failures += send_key(to_child[1], from_child[0], &event, &response,
                         31, 0, KUI_MOD_CTRL, 0) < 0;
    if (!file_equals(save_as_path, "")) failures++;

    event.type = KUI_EVENT_CLOSE;
    event.serial++;
    write_all(to_child[1], &event, sizeof(event));
    close(to_child[1]);
    close(from_child[0]);
    waitpid(child, &status, 0);
    remove(path);
    remove(save_as_path);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) failures++;

    if (failures != 0) {
        fprintf(stderr, "FAIL editor clipboard integration: %d failure(s)\n",
                failures);
        return 1;
    }
    puts("PASS editor clipboard integration");
    return 0;
}
