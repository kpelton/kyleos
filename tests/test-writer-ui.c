#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "extras/kui/kui_protocol.h"
#include "extras/kui/kui_style.h"
#include "extras/wordprocessor/writer_document.h"

#define WRITER_TEST_PATH "/tmp/os3-writer-ui.kwp"
#define WRITER_LARGE_LENGTH 20000

struct display_result {
    struct kui_response response;
    unsigned char *display;
};

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
                    struct display_result *result)
{
    free(result->display);
    result->display = NULL;
    if (write_all(input, event, sizeof(*event)) < 0 ||
        read_all(output, &result->response, sizeof(result->response)) < 0)
        return -1;
    if (result->response.magic != KUI_PROTOCOL_MAGIC ||
        result->response.version != KUI_PROTOCOL_VERSION ||
        result->response.serial != event->serial ||
        result->response.display_bytes > KUI_PROTOCOL_MAX_DISPLAY)
        return -1;
    if (result->response.display_bytes != 0) {
        result->display = malloc(result->response.display_bytes);
        if (result->display == NULL ||
            read_all(output, result->display,
                     result->response.display_bytes) < 0)
            return -1;
    }
    return 0;
}

static int send_key(int input, int output, struct kui_event *event,
                    struct display_result *result, int code,
                    uint32_t modifiers, char character)
{
    event->type = KUI_EVENT_KEY;
    event->serial++;
    event->key_code = code;
    event->key_extended = 0;
    event->key_pressed = 1;
    event->modifiers = modifiers;
    event->character = character;
    event->button = 0;
    event->buttons = 0;
    return exchange(input, output, event, result);
}

static int send_pointer(int input, int output, struct kui_event *event,
                        struct display_result *result, int x, int y)
{
    event->type = KUI_EVENT_POINTER;
    event->serial++;
    event->x = x;
    event->y = y;
    event->button = 1;
    event->buttons = 1;
    event->key_pressed = 0;
    event->modifiers = 0;
    event->character = 0;
    return exchange(input, output, event, result);
}

static int display_has_styled_a(const struct display_result *result,
                                int scale, int bold)
{
    uint32_t remaining = result->response.display_bytes;
    const unsigned char *cursor = result->display;

    while (remaining >= sizeof(struct kui_display_command)) {
        const struct kui_display_command *command =
            (const struct kui_display_command *)cursor;

        if (command->size < sizeof(*command) || command->size > remaining)
            return 0;
        if (command->type == KUI_DISPLAY_TEXT &&
            command->size > sizeof(*command)) {
            const char *text = (const char *)(command + 1);
            int command_scale =
                (int)((command->flags & KUI_TEXT_SCALE_MASK) >>
                      KUI_TEXT_SCALE_SHIFT) + 1;

            if (strcmp(text, "a") == 0 &&
                (command->flags & KUI_TEXT_MONOSPACE) != 0 &&
                ((command->flags & KUI_TEXT_BOLD) != 0) == bold &&
                command_scale == scale)
                return 1;
        }
        remaining -= command->size;
        cursor += command->size;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int to_child[2];
    int from_child[2];
    pid_t child;
    struct kui_event event;
    struct display_result result;
    struct writer_document loaded;
    int status = 0;
    int failures = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: %s WRITER-HOST\n", argv[0]);
        return 2;
    }
    unlink(WRITER_TEST_PATH);
    if (pipe(to_child) < 0 || pipe(from_child) < 0) return 2;
    child = fork();
    if (child == 0) {
        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        close(to_child[0]);
        close(to_child[1]);
        close(from_child[0]);
        close(from_child[1]);
        execl(argv[1], argv[1], (char *)NULL);
        _exit(127);
    }
    if (child < 0) return 2;
    close(to_child[0]);
    close(from_child[1]);

    memset(&event, 0, sizeof(event));
    memset(&result, 0, sizeof(result));
    writer_document_init(&loaded);
    event.magic = KUI_PROTOCOL_MAGIC;
    event.version = KUI_PROTOCOL_VERSION;
    event.type = KUI_EVENT_INIT;
    event.serial = 1;
    event.width = 760;
    event.height = 540;
    kui_style_win95(&event.style);
    strncpy(event.argument, WRITER_TEST_PATH, sizeof(event.argument) - 1);
    failures += exchange(to_child[1], from_child[0], &event, &result) < 0;
    failures += send_key(to_child[1], from_child[0], &event, &result,
                         30, 0, 'a') < 0;
    failures += !display_has_styled_a(&result, 1, 0);
    failures += send_pointer(to_child[1], from_child[0], &event, &result,
                             230, 20) < 0;
    failures += !display_has_styled_a(&result, 1, 1);
    failures += send_pointer(to_child[1], from_child[0], &event, &result,
                             310, 20) < 0;
    failures += !display_has_styled_a(&result, 2, 1);
    failures += send_key(to_child[1], from_child[0], &event, &result,
                         31, KUI_MOD_CTRL, 0) < 0;
    failures += result.response.action != KUI_ACTION_FILE_SAVED;
    failures += writer_document_load(&loaded, WRITER_TEST_PATH) < 0;
    failures += strcmp(loaded.text, "a") != 0 ||
                loaded.font_scale != 2 || loaded.bold != 1;

    failures += writer_document_reserve(
        &loaded, WRITER_LARGE_LENGTH + 1) < 0;
    if (loaded.text_capacity > WRITER_LARGE_LENGTH) {
        for (int index = 0; index < WRITER_LARGE_LENGTH; index++)
            loaded.text[index] = (char)('a' + index % 26);
        loaded.text[WRITER_LARGE_LENGTH] = '\0';
        loaded.text_length = WRITER_LARGE_LENGTH;
        failures += writer_document_save(&loaded, WRITER_TEST_PATH) < 0;

        event.type = KUI_EVENT_INIT;
        event.serial++;
        strncpy(event.argument, WRITER_TEST_PATH,
                sizeof(event.argument) - 1);
        event.argument[sizeof(event.argument) - 1] = '\0';
        failures += exchange(to_child[1], from_child[0], &event, &result) < 0;
        failures += send_key(to_child[1], from_child[0], &event, &result,
                             44, 0, 'Z') < 0;
        failures += send_key(to_child[1], from_child[0], &event, &result,
                             31, KUI_MOD_CTRL, 0) < 0;
        failures += writer_document_load(&loaded, WRITER_TEST_PATH) < 0;
        failures += loaded.text_length != WRITER_LARGE_LENGTH + 1 ||
                    loaded.text[0] != 'Z' ||
                    memcmp(loaded.text + 1, "abcdefghijklmnopqrstuvwxyz", 26)
                        != 0;
    }

    event.type = KUI_EVENT_CLOSE;
    event.serial++;
    write_all(to_child[1], &event, sizeof(event));
    close(to_child[1]);
    close(from_child[0]);
    waitpid(child, &status, 0);
    free(result.display);
    writer_document_destroy(&loaded);
    unlink(WRITER_TEST_PATH);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) failures++;
    if (failures != 0) {
        fprintf(stderr, "FAIL Writer UI integration: %d failure(s)\n",
                failures);
        return 1;
    }
    puts("Writer UI integration passed");
    return 0;
}
