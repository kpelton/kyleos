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
                    struct kui_response *response, uint32_t *largest_frame)
{
    void *display = NULL;
    if (write_all(input, event, sizeof(*event)) < 0 ||
        read_all(output, response, sizeof(*response)) < 0)
        return -1;
    if (response->magic != KUI_PROTOCOL_MAGIC ||
        response->version != KUI_PROTOCOL_VERSION ||
        response->serial != event->serial ||
        response->display_bytes > KUI_PROTOCOL_MAX_DISPLAY)
        return -1;
    if (response->display_bytes > *largest_frame)
        *largest_frame = response->display_bytes;
    if (response->display_bytes != 0) {
        display = malloc(response->display_bytes);
        if (display == NULL ||
            read_all(output, display, response->display_bytes) < 0) {
            free(display);
            return -1;
        }
        free(display);
    }
    return 0;
}

static int send_key(int input, int output, struct kui_event *event,
                    struct kui_response *response, uint32_t *largest_frame,
                    int code, int extended, uint32_t modifiers,
                    char character)
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
    return exchange(input, output, event, response, largest_frame);
}

static int send_text(int input, int output, struct kui_event *event,
                     struct kui_response *response, uint32_t *largest_frame,
                     const char *text)
{
    for (size_t index = 0; text[index] != '\0'; index++)
        if (send_key(input, output, event, response, largest_frame,
                     0, 0, 0, text[index]) < 0)
            return -1;
    return 0;
}

static int send_pointer(int input, int output, struct kui_event *event,
                        struct kui_response *response,
                        uint32_t *largest_frame, int x, int y,
                        int button, int buttons)
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
    return exchange(input, output, event, response, largest_frame);
}

static int click(int input, int output, struct kui_event *event,
                 struct kui_response *response, uint32_t *largest_frame,
                 int x, int y)
{
    return send_pointer(input, output, event, response, largest_frame,
                        x, y, 1, 1);
}

static int file_equals(const char *path, const char *expected)
{
    char data[256];
    FILE *file = fopen(path, "rb");
    size_t count;
    if (file == NULL) return 0;
    count = fread(data, 1, sizeof(data) - 1, file);
    fclose(file);
    data[count] = '\0';
    if (strcmp(data, expected) == 0) return 1;
    fprintf(stderr, "CSV mismatch: got %zu byte(s), expected %zu byte(s)\n",
            count, strlen(expected));
    return 0;
}

int main(int argc, char **argv)
{
    const char *path = "/tmp/os3ks.csv";
    const char *save_as_path = "/tmp/os3ks-saveas.csv";
    int to_child[2];
    int from_child[2];
    pid_t child;
    struct kui_event event;
    struct kui_response response;
    uint32_t largest_frame = 0;
    char final_expected[128];
    int failures = 0;
    int status = 0;
    FILE *file;

    if (argc != 2) return 2;
    file = fopen(path, "wb");
    if (file == NULL) return 2;
    fclose(file);
    remove(save_as_path);
    if (pipe(to_child) < 0 || pipe(from_child) < 0) return 2;
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
    event.width = 900;
    event.height = 600;
    kui_style_aqua(&event.style);
    strncpy(event.argument, path, sizeof(event.argument) - 1);
    failures += exchange(to_child[1], from_child[0], &event,
                         &response, &largest_frame) < 0;

    failures += send_text(to_child[1], from_child[0], &event,
                          &response, &largest_frame, "2") < 0;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame, 28, 0, 0, '\n') < 0;
    failures += send_text(to_child[1], from_child[0], &event,
                          &response, &largest_frame, "3") < 0;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame, 28, 0, 0, '\n') < 0;

    /* B1: row header 44, one 96-pixel column, grid begins at y=83. */
    failures += click(to_child[1], from_child[0], &event, &response,
                      &largest_frame, 145, 88) < 0;
    failures += send_text(to_child[1], from_child[0], &event,
                          &response, &largest_frame, "=A1+A2") < 0;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame, 28, 0, 0, '\n') < 0;

    failures += click(to_child[1], from_child[0], &event, &response,
                      &largest_frame, 145, 88) < 0;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         46, 0, KUI_MOD_CTRL, 0) < 0;
    if (response.action != KUI_ACTION_COPY_TEXT ||
        strcmp(response.action_argument, "=A1+A2") != 0)
        failures++;
    event.clipboard_kind = KUI_CLIPBOARD_TEXT;
    strncpy(event.clipboard, response.action_argument,
            sizeof(event.clipboard) - 1);
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         45, 0, KUI_MOD_CTRL, 0) < 0;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         47, 0, KUI_MOD_CTRL, 0) < 0;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         31, 0, KUI_MOD_CTRL, 0) < 0;
    if (response.action != KUI_ACTION_FILE_SAVED ||
        !file_equals(path, "2,=A1+A2\n3\n"))
        failures++;

    /* Shift+arrow extends from the active cell without moving the anchor. */
    failures += click(to_child[1], from_child[0], &event, &response,
                      &largest_frame, 50, 88) < 0;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         77, 1, KUI_MOD_SHIFT, 0) < 0;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         80, 1, KUI_MOD_SHIFT, 0) < 0;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         46, 0, KUI_MOD_CTRL, 0) < 0;
    if (response.action != KUI_ACTION_COPY_TEXT ||
        strcmp(response.action_argument, "2\t=A1+A2\n3\t") != 0)
        failures++;

    /* Drag A1:B2, then exercise rectangular copy, cut, and paste. */
    failures += send_pointer(to_child[1], from_child[0], &event, &response,
                             &largest_frame, 50, 88, 1, 1) < 0;
    failures += send_pointer(to_child[1], from_child[0], &event, &response,
                             &largest_frame, 145, 110, 0, 1) < 0;
    failures += send_pointer(to_child[1], from_child[0], &event, &response,
                             &largest_frame, 145, 110, 0, 0) < 0;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         46, 0, KUI_MOD_CTRL, 0) < 0;
    if (response.action != KUI_ACTION_COPY_TEXT ||
        strcmp(response.action_argument, "2\t=A1+A2\n3\t") != 0)
        failures++;
    event.clipboard_kind = KUI_CLIPBOARD_TEXT;
    strncpy(event.clipboard, response.action_argument,
            sizeof(event.clipboard) - 1);
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         45, 0, KUI_MOD_CTRL, 0) < 0;
    if (response.action != KUI_ACTION_COPY_TEXT ||
        strcmp(response.action_argument, "2\t=A1+A2\n3\t") != 0)
        failures++;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         47, 0, KUI_MOD_CTRL, 0) < 0;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         31, 0, KUI_MOD_CTRL, 0) < 0;
    if (response.action != KUI_ACTION_FILE_SAVED ||
        !file_equals(path, "2,=A1+A2\n3\n"))
        failures++;

    /* Right-clicking inside the range preserves it; Delete clears it all. */
    failures += send_pointer(to_child[1], from_child[0], &event, &response,
                             &largest_frame, 50, 88, 2, 2) < 0;
    failures += send_pointer(to_child[1], from_child[0], &event, &response,
                             &largest_frame, 60, 167, 1, 1) < 0;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         31, 0, KUI_MOD_CTRL, 0) < 0;
    if (response.action != KUI_ACTION_FILE_SAVED || !file_equals(path, ""))
        failures++;

    /* The visible grid grows beyond Z and preserves AA in CSV output. */
    failures += click(to_child[1], from_child[0], &event, &response,
                      &largest_frame, 50, 88) < 0;
    for (int column = 0; column < 26; column++)
        failures += send_key(to_child[1], from_child[0], &event,
                             &response, &largest_frame,
                             77, 1, 0, 0) < 0;
    failures += send_text(to_child[1], from_child[0], &event,
                          &response, &largest_frame, "7") < 0;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         31, 0, KUI_MOD_CTRL, 0) < 0;
    {
        char expected[32];

        memset(expected, ',', 26);
        expected[26] = '7';
        expected[27] = '\n';
        expected[28] = '\0';
        if (response.action != KUI_ACTION_FILE_SAVED ||
            !file_equals(path, expected))
            failures++;
    }

    /* Vertical navigation similarly grows beyond the original row 100. */
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         83, 1, 0, 0) < 0;
    for (int column = 0; column < 26; column++)
        failures += send_key(to_child[1], from_child[0], &event,
                             &response, &largest_frame,
                             75, 1, 0, 0) < 0;
    for (int row = 0; row < 100; row++)
        failures += send_key(to_child[1], from_child[0], &event,
                             &response, &largest_frame,
                             80, 1, 0, 0) < 0;
    failures += send_text(to_child[1], from_child[0], &event,
                          &response, &largest_frame, "8") < 0;
    failures += send_key(to_child[1], from_child[0], &event,
                         &response, &largest_frame,
                         31, 0, KUI_MOD_CTRL, 0) < 0;
    {
        char expected[128];

        memset(expected, '\n', 100);
        expected[100] = '8';
        expected[101] = '\n';
        expected[102] = '\0';
        memcpy(final_expected, expected, sizeof(final_expected));
        if (response.action != KUI_ACTION_FILE_SAVED ||
            !file_equals(path, expected))
            failures++;
    }

    /* Save As must keep the dialog alive while typing, then write the new
     * path after Enter. This covers the crash-prone dialog transition. */
    {
        int save_as_failure = 0;
        int open_result;
        int text_result;
        int enter_result;
        int action_result;
        int file_result;

        open_result = send_key(to_child[1], from_child[0], &event,
                               &response, &largest_frame, 31, 0,
                               KUI_MOD_CTRL | KUI_MOD_SHIFT, 0);
        text_result = send_text(to_child[1], from_child[0], &event,
                                &response, &largest_frame,
                                "os3ks-saveas.csv");
        enter_result = send_key(to_child[1], from_child[0], &event,
                                &response, &largest_frame, 28, 0, 0, '\n');
        action_result = response.action != KUI_ACTION_FILE_SAVED;
        file_result = !file_equals(save_as_path, final_expected);
        save_as_failure = (open_result < 0) + (text_result < 0) +
                          (enter_result < 0) + action_result + file_result;
        if (save_as_failure != 0)
            fprintf(stderr, "Save As checks: %d open=%d text=%d enter=%d "
                    "action=%d file=%d response=%u status=%u path=%s\n",
                    save_as_failure, open_result, text_result, enter_result,
                    action_result, file_result, response.action,
                    response.status, save_as_path);
        failures += save_as_failure;

        /* Existing Save As target must enter overwrite confirmation, then
         * accept the replacement without crashing. */
        open_result = send_key(to_child[1], from_child[0], &event,
                               &response, &largest_frame, 31, 0,
                               KUI_MOD_CTRL | KUI_MOD_SHIFT, 0);
        text_result = send_text(to_child[1], from_child[0], &event,
                                &response, &largest_frame,
                                "os3ks-saveas.csv");
        enter_result = send_key(to_child[1], from_child[0], &event,
                                &response, &largest_frame, 28, 0, 0, '\n');
        int confirm_result = send_key(to_child[1], from_child[0], &event,
                                      &response, &largest_frame, 28, 0, 0,
                                      '\n');
        failures += (open_result < 0) + (text_result < 0) +
                    (enter_result < 0) + (confirm_result < 0) +
                    (response.action != KUI_ACTION_FILE_SAVED);
    }

    event.type = KUI_EVENT_CLOSE;
    event.serial++;
    write_all(to_child[1], &event, sizeof(event));
    close(to_child[1]);
    close(from_child[0]);
    waitpid(child, &status, 0);
    remove(path);
    remove(save_as_path);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) failures++;
    if (largest_frame == 0 || largest_frame > KUI_PROTOCOL_MAX_DISPLAY)
        failures++;

    if (failures != 0) {
        fprintf(stderr, "FAIL KSheet UI: %d failure(s), frame=%u bytes\n",
                failures, largest_frame);
        return 1;
    }
    printf("PASS KSheet UI and clipboard (%u-byte largest frame)\n",
           largest_frame);
    return 0;
}
