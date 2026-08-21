#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef KYLEOS_HOST
#include <poll.h>
#endif

#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"
#include "extras/terminal/terminal_ipc.h"

struct terminal_state {
    char path[TERMINAL_PATH_MAX];
    char terminal[TERMINAL_TEXT_MAX];
    int terminal_length;
    char command[TERMINAL_COMMAND_MAX];
    int command_length;
    int child_pid;
    int output_fd;
    int scroll_from_bottom;
    struct kui_scrollbar_state scrollbar;
};

static void append_text(struct terminal_state *state, const char *text,
                        int length)
{
    if (length < 0) length = strlen(text);
    if (length >= TERMINAL_TEXT_MAX) {
        text += length - (TERMINAL_TEXT_MAX - 1);
        length = TERMINAL_TEXT_MAX - 1;
    }
    if (state->terminal_length + length >= TERMINAL_TEXT_MAX) {
        int discard = state->terminal_length + length -
                      (TERMINAL_TEXT_MAX - 1);
        memmove(state->terminal, state->terminal + discard,
                state->terminal_length - discard);
        state->terminal_length -= discard;
    }
    memcpy(state->terminal + state->terminal_length, text, length);
    state->terminal_length += length;
    state->terminal[state->terminal_length] = '\0';
}

static void set_path(struct terminal_state *state, const char *path)
{
    if (path[0] == '/') {
        strncpy(state->path, path, sizeof(state->path) - 1);
    } else if (strcmp(path, "..") == 0) {
        char *slash;
        if (strcmp(state->path, "/") == 0) return;
        slash = strrchr(state->path, '/');
        if (slash == state->path) state->path[1] = '\0';
        else if (slash != NULL) *slash = '\0';
        return;
    } else if (strcmp(path, ".") == 0) {
        return;
    } else if (strcmp(state->path, "/") == 0) {
        snprintf(state->path, sizeof(state->path), "/%s", path);
    } else {
        int length = strlen(state->path);
        snprintf(state->path + length, sizeof(state->path) - length,
                 "/%s", path);
    }
    state->path[sizeof(state->path) - 1] = '\0';
}

static int launch_action(const char *command)
{
    if (strcmp(command, "solitaire") == 0 ||
        strcmp(command, "/bin/solitaire") == 0)
        return TERMINAL_ACTION_SOLITAIRE;
    if (strcmp(command, "calculator") == 0 ||
        strcmp(command, "/bin/calculator") == 0)
        return TERMINAL_ACTION_CALCULATOR;
    if (strcmp(command, "minesweeper") == 0 ||
        strcmp(command, "/bin/minesweeper") == 0)
        return TERMINAL_ACTION_MINESWEEPER;
    if (strcmp(command, "sysmon") == 0 ||
        strcmp(command, "/bin/sysmon") == 0)
        return TERMINAL_ACTION_SYSTEM_MONITOR;
    if (strcmp(command, "editor") == 0 ||
        strcmp(command, "/bin/editor") == 0)
        return TERMINAL_ACTION_EDITOR;
    return TERMINAL_ACTION_NONE;
}

static int execute(struct terminal_state *state)
{
    char input[TERMINAL_COMMAND_MAX];
    char display[TERMINAL_COMMAND_MAX + 4];
    char program[160];
    char *argv[16];
    char *cursor;
    int argc = 0;
    int output_pipe[2];
    int child;
    int action;

    if (state->child_pid > 0) return TERMINAL_ACTION_NONE;

    memcpy(input, state->command, state->command_length);
    input[state->command_length] = '\0';
    snprintf(display, sizeof(display), "$ %s\n", input);
    append_text(state, display, -1);
    state->command_length = 0;
    state->command[0] = '\0';
    cursor = input;
    while (*cursor != '\0' && argc < 15) {
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        if (*cursor == '\0') break;
        argv[argc++] = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t')
            cursor++;
        if (*cursor != '\0') *cursor++ = '\0';
    }
    argv[argc] = NULL;
    if (argc == 0) return TERMINAL_ACTION_NONE;
    if (strcmp(argv[0], "clear") == 0) {
        state->terminal_length = 0;
        state->terminal[0] = '\0';
        return TERMINAL_ACTION_NONE;
    }
    if (strcmp(argv[0], "pwd") == 0) {
        append_text(state, state->path, -1);
        append_text(state, "\n", 1);
        return TERMINAL_ACTION_NONE;
    }
    if (strcmp(argv[0], "cd") == 0) {
        const char *target = argc > 1 ? argv[1] : "/";
        if (chdir(target) == 0) set_path(state, target);
        else append_text(state, "cd: directory not found\n", -1);
        return TERMINAL_ACTION_NONE;
    }
    action = launch_action(argv[0]);
    if (action != TERMINAL_ACTION_NONE) return action;
    if (strchr(argv[0], '/') != NULL) {
        strncpy(program, argv[0], sizeof(program) - 1);
        program[sizeof(program) - 1] = '\0';
    } else {
        snprintf(program, sizeof(program), "/bin/%s", argv[0]);
    }
    if (pipe(output_pipe) < 0) {
        append_text(state, "terminal: cannot create output pipe\n", -1);
        return TERMINAL_ACTION_NONE;
    }
    child = fork();
    if (child == 0) {
        int nullfd = open("/dev/null", O_RDONLY);
        char *environment[] = { "PATH=/bin:/usr/bin", NULL };

        close(output_pipe[0]);
        if (nullfd >= 0) { dup2(nullfd, 0); close(nullfd); }
        dup2(output_pipe[1], 1);
        dup2(output_pipe[1], 2);
        close(output_pipe[1]);
        execve(program, argv, environment);
        if (strchr(argv[0], '/') == NULL) {
            snprintf(program, sizeof(program), "/usr/bin/%s", argv[0]);
            execve(program, argv, environment);
        }
        fprintf(stderr, "%s: command not found\n", argv[0]);
        _exit(127);
    }
    close(output_pipe[1]);
    if (child < 0) {
        close(output_pipe[0]);
        append_text(state, "terminal: fork failed\n", -1);
        return TERMINAL_ACTION_NONE;
    }
    state->child_pid = child;
    state->output_fd = output_pipe[0];
    return TERMINAL_ACTION_NONE;
}

static void drain_child_output(struct terminal_state *state)
{
    char buffer[512];
    struct kyleos_fd_status status;

    if (state->output_fd < 0) return;
    for (;;) {
        int count;

#ifdef KYLEOS_HOST
        struct pollfd descriptor;

        descriptor.fd = state->output_fd;
        descriptor.events = POLLIN;
        descriptor.revents = 0;
        if (poll(&descriptor, 1, 0) < 0) break;
        status.readable = (descriptor.revents & POLLIN) ? sizeof(buffer) : 0;
#else
        if (kui_fd_status(state->output_fd, &status) < 0) break;
#endif
        if (status.readable == 0) break;
        count = read(state->output_fd, buffer,
                     status.readable < sizeof(buffer) ?
                     (int)status.readable : (int)sizeof(buffer));
        if (count <= 0) return;
        append_text(state, buffer, count);
    }
}

static void poll_child(struct terminal_state *state)
{
    int status = 0;
    int result;

    if (state->child_pid <= 0) return;
    drain_child_output(state);
    result = waitpid(state->child_pid, &status, WNOHANG);
    if (result == 0) return;
    drain_child_output(state);
    if (state->output_fd >= 0) close(state->output_fd);
    state->output_fd = -1;
    state->child_pid = 0;
    if (result < 0)
        append_text(state, "terminal: wait failed\n", -1);
    if (state->terminal_length > 0 &&
        state->terminal[state->terminal_length - 1] != '\n')
        append_text(state, "\n", 1);
}

static int terminal_rows(const char *text, int length, int columns)
{
    int rows = 0;
    int column = 0;

    for (int i = 0; i < length; i++) {
        if (text[i] == '\r') continue;
        if (text[i] == '\n') { rows++; column = 0; }
        else if (++column >= columns) { rows++; column = 0; }
    }
    return rows;
}

static int terminal_view_text(const struct terminal_state *state,
                              char *visible)
{
    int length = state->terminal_length;

    memcpy(visible, state->terminal, length);
    if (state->child_pid <= 0) {
        visible[length++] = '$';
        visible[length++] = ' ';
        memcpy(visible + length, state->command, state->command_length);
        length += state->command_length;
    }
    return length;
}

static void terminal_scroll_metrics(const struct terminal_state *state,
                                    int width, int height,
                                    int *rows, int *total, int *maximum)
{
    char visible[TERMINAL_TEXT_MAX + TERMINAL_COMMAND_MAX + 8];
    int columns = (width - 14 - KUI_SCROLLBAR_SIZE) / 8;
    int visible_rows = (height - 8) / 16;
    int length;

    if (columns < 1) columns = 1;
    if (visible_rows < 1) visible_rows = 1;
    length = terminal_view_text(state, visible);
    *rows = visible_rows;
    *total = terminal_rows(visible, length, columns) + 1;
    *maximum = *total - *rows;
    if (*maximum < 0) *maximum = 0;
}

static void terminal_render(struct kui_client *client,
                            const struct kui_event *event,
                            const struct terminal_state *state)
{
    char visible[TERMINAL_TEXT_MAX + TERMINAL_COMMAND_MAX + 8];
    int columns = (event->width - 14 - KUI_SCROLLBAR_SIZE) / 8;
    int rows = (event->height - 8) / 16;
    int length = state->terminal_length;
    int first_row;
    int row = 0;
    int column = 0;

    if (columns < 1) columns = 1;
    if (rows < 1) rows = 1;
    length = terminal_view_text(state, visible);
    first_row = terminal_rows(visible, length, columns) + 1 - rows -
                state->scroll_from_bottom;
    if (first_row < 0) first_row = 0;
    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  KUI_BLACK);
    for (int i = 0; i < length; i++) {
        unsigned char ch = visible[i];
        if (ch == '\r') continue;
        if (ch == '\n') { row++; column = 0; continue; }
        if (ch == '\t') ch = ' ';
        if (row >= first_row && row < first_row + rows &&
            ch >= 32 && ch < 127)
            kui_draw_mono_char(&client->surface, 7 + column * 8,
                               4 + (row - first_row) * 16, ch,
                               0x0000ff80U, KUI_BLACK, 0);
        if (++column >= columns) { row++; column = 0; }
    }
    if (state->child_pid <= 0 && row >= first_row && row < first_row + rows)
        kui_fill_rect(&client->surface, 7 + column * 8,
                      4 + (row - first_row) * 16, 2, 16, 0x0000ff80U);
    kui_draw_vscrollbar(&client->surface,
                        event->width - KUI_SCROLLBAR_SIZE, 0,
                        event->height, first_row, rows,
                        terminal_rows(visible, length, columns) + 1,
                        &state->scrollbar);
}

static const char *terminal_action_path(int action)
{
    if (action == TERMINAL_ACTION_SOLITAIRE) return "/bin/solitaire";
    if (action == TERMINAL_ACTION_CALCULATOR) return "/bin/calculator";
    if (action == TERMINAL_ACTION_MINESWEEPER) return "/bin/minesweeper";
    if (action == TERMINAL_ACTION_SYSTEM_MONITOR) return "/bin/sysmon";
    if (action == TERMINAL_ACTION_EDITOR) return "/bin/editor";
    return NULL;
}

int main(void)
{
    struct terminal_state state;
    struct kui_client client;
    struct kui_event event;

    memset(&state, 0, sizeof(state));
    state.output_fd = -1;
    strcpy(state.path, "/");
    append_text(&state,
                "KyleOS Terminal\nType a command and press Enter.\n", -1);
    kui_client_init(&client);
    for (;;) {
        int action = TERMINAL_ACTION_NONE;
        const char *launch_path;

        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        {
            int rows;
            int total;
            int maximum;
            int first;

            terminal_scroll_metrics(&state, event.width, event.height,
                                    &rows, &total, &maximum);
            first = maximum - state.scroll_from_bottom;
            first = kui_vscrollbar_handle(
                &state.scrollbar, &event,
                event.width - KUI_SCROLLBAR_SIZE, 0, event.height,
                first, rows, total);
            state.scroll_from_bottom = maximum - first;
        }
        if (event.type == KUI_EVENT_INIT) {
            memset(&state, 0, sizeof(state));
            state.output_fd = -1;
            strcpy(state.path, "/");
            append_text(&state,
                        "KyleOS Terminal\nType a command and press Enter.\n",
                        -1);
            chdir("/");
        } else if (event.type == KUI_EVENT_TIMER && state.child_pid > 0) {
            poll_child(&state);
        } else if (event.type == KUI_EVENT_POINTER && event.wheel != 0) {
            int rows;
            int total;
            int maximum;
            int first;

            terminal_scroll_metrics(&state, event.width, event.height,
                                    &rows, &total, &maximum);
            first = kui_scrollbar_wheel(
                maximum - state.scroll_from_bottom, event.wheel,
                rows, total);
            state.scroll_from_bottom = maximum - first;
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1 &&
                   kui_in_rect(event.x, event.y,
                               event.width - KUI_SCROLLBAR_SIZE, 0,
                               KUI_SCROLLBAR_SIZE, event.height)) {
            /* The shared scrollbar owns this press and repeat timer. */
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   (event.modifiers & KUI_MOD_CTRL) &&
                   !event.key_extended && event.key_code == 38) {
            state.terminal_length = 0;
            state.terminal[0] = '\0';
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   state.child_pid <= 0) {
            char key = event.character;
            if (!event.key_extended && event.key_code == 14) key = '\b';
            if (!event.key_extended && event.key_code == 28) key = '\n';
            if (key == '\b' && state.command_length > 0)
                state.command[--state.command_length] = '\0';
            else if (key == '\n')
                action = execute(&state);
            else if (key >= 32 && key < 127 &&
                     state.command_length < TERMINAL_COMMAND_MAX - 1) {
                state.command[state.command_length++] = key;
                state.command[state.command_length] = '\0';
            }
        }
        {
            int rows;
            int total;
            int maximum;

            terminal_scroll_metrics(&state, event.width, event.height,
                                    &rows, &total, &maximum);
            (void)rows;
            (void)total;
            if (state.scroll_from_bottom < 0) state.scroll_from_bottom = 0;
            if (state.scroll_from_bottom > maximum)
                state.scroll_from_bottom = maximum;
        }
        kui_client_begin(&client, &event, "Terminal");
        kui_client_set_constraints(&client, 360, 180, 700, 481);
        terminal_render(&client, &event, &state);
        {
            uint32_t timer = state.child_pid > 0 ? 50U : 0U;
            uint32_t scroll_timer =
                kui_scrollbar_timer_ms(&state.scrollbar);

            if (scroll_timer != 0 && (timer == 0 || scroll_timer < timer))
                timer = scroll_timer;
            kui_client_set_timer(&client, timer);
        }
        launch_path = terminal_action_path(action);
        if (launch_path != NULL)
            kui_client_set_action(&client, KUI_ACTION_LAUNCH, launch_path);
        if (kui_client_send(&client) < 0) break;
    }
    if (state.output_fd >= 0) close(state.output_fd);
    kui_client_free(&client);
    return 0;
}
