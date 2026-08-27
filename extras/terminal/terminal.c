#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#ifndef KYLEOS_HOST
#include <pty.h>
#endif

#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"
#include "extras/terminal/terminal_ipc.h"
#include "extras/terminal/vt100.h"

#ifndef TERMINAL_SHELL_PATH
#define TERMINAL_SHELL_PATH "/bin/nushell"
#endif

#define TERMINAL_HISTORY_MAX 16

struct terminal_state {
    char path[TERMINAL_PATH_MAX];
    struct vt100_terminal vt;
    char command[TERMINAL_COMMAND_MAX];
    int command_length;
    int command_cursor;
    char history[TERMINAL_HISTORY_MAX][TERMINAL_COMMAND_MAX];
    int history_count;
    int history_position;
    char history_draft[TERMINAL_COMMAND_MAX];
    int history_draft_length;
    int child_pid;
    int input_fd;
    int output_fd;
    int scroll_from_bottom;
    int shell_prompt_ready;
    char previous_output;
    struct kui_scrollbar_state scrollbar;
};

static void append_text(struct terminal_state *state, const char *text,
                        int length)
{
    vt100_write(&state->vt, text, length);
}

static void note_output(struct terminal_state *state, const char *text,
                        int length)
{
#ifndef KYLEOS_HOST
    for (int index = 0; index < length; index++) {
        if (state->previous_output == '$' && text[index] == ' ')
            state->shell_prompt_ready = 1;
        state->previous_output = text[index];
    }
#else
    (void)state;
    (void)text;
    (void)length;
#endif
}

static void command_set(struct terminal_state *state, const char *command)
{
    size_t length = strlen(command);

    if (length >= sizeof(state->command))
        length = sizeof(state->command) - 1;
    memcpy(state->command, command, length);
    state->command[length] = '\0';
    state->command_length = (int)length;
    state->command_cursor = state->command_length;
}

static void history_add(struct terminal_state *state)
{
    if (state->command_length == 0)
        return;
    if (state->history_count > 0 &&
        strcmp(state->history[state->history_count - 1],
               state->command) == 0) {
        state->history_position = state->history_count;
        return;
    }
    if (state->history_count == TERMINAL_HISTORY_MAX) {
        memmove(state->history, state->history + 1,
                sizeof(state->history[0]) * (TERMINAL_HISTORY_MAX - 1));
        state->history_count--;
    }
    memcpy(state->history[state->history_count], state->command,
           (size_t)state->command_length + 1);
    state->history_count++;
    state->history_position = state->history_count;
    state->history_draft_length = 0;
    state->history_draft[0] = '\0';
}

static void history_previous(struct terminal_state *state)
{
    if (state->history_count == 0)
        return;
    if (state->history_position >= state->history_count) {
        memcpy(state->history_draft, state->command,
               (size_t)state->command_length + 1);
        state->history_draft_length = state->command_length;
        state->history_position = state->history_count;
    }
    if (state->history_position > 0)
        state->history_position--;
    command_set(state, state->history[state->history_position]);
}

static void history_next(struct terminal_state *state)
{
    if (state->history_position >= state->history_count)
        return;
    state->history_position++;
    if (state->history_position == state->history_count)
        command_set(state, state->history_draft);
    else
        command_set(state, state->history[state->history_position]);
}

static int terminal_editing_command(const struct terminal_state *state)
{
#ifdef KYLEOS_HOST
    return state->child_pid <= 0;
#else
    return state->shell_prompt_ready;
#endif
}

static const char *terminal_key_sequence(const struct kui_event *event,
                                         char *sequence, size_t size)
{
    const char *plain = NULL;
    char final = '\0';
    int modifier = 1;

    if (event->key_code == 72) { plain = "\033[A"; final = 'A'; }
    else if (event->key_code == 80) { plain = "\033[B"; final = 'B'; }
    else if (event->key_code == 77) { plain = "\033[C"; final = 'C'; }
    else if (event->key_code == 75) { plain = "\033[D"; final = 'D'; }
    else if (event->key_code == 71) { plain = "\033[H"; final = 'H'; }
    else if (event->key_code == 79) { plain = "\033[F"; final = 'F'; }
    else if (event->key_code == 73) plain = "\033[5~";
    else if (event->key_code == 81) plain = "\033[6~";
    else if (event->key_code == 82) plain = "\033[2~";
    else if (event->key_code == 83) plain = "\033[3~";
    if (plain == NULL)
        return NULL;
    if (event->modifiers == 0)
        return plain;
    if (event->modifiers & KUI_MOD_SHIFT) modifier += 1;
    if (event->modifiers & KUI_MOD_ALT) modifier += 2;
    if (event->modifiers & KUI_MOD_CTRL) modifier += 4;
    if (final != '\0')
        snprintf(sequence, size, "\033[1;%d%c", modifier, final);
    else
        snprintf(sequence, size, "\033[%d;%d~",
                 event->key_code == 73 ? 5 :
                 event->key_code == 81 ? 6 :
                 event->key_code == 82 ? 2 : 3,
                 modifier);
    return sequence;
}

#ifdef KYLEOS_HOST
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
#endif

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

#ifdef KYLEOS_HOST
static int command_needs_shell(const char *command)
{
    static const char *builtins[] = { "exit", "if", "repeat", "sleep" };
    const char *start = command;
    size_t length;

    if (strpbrk(command, "*?<>|&") != NULL || strstr(command, "$?") != NULL)
        return 1;
    while (*start == ' ' || *start == '\t') start++;
    length = strcspn(start, " \t\n");
    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
        if (strlen(builtins[i]) == length &&
            strncmp(start, builtins[i], length) == 0)
            return 1;
    return 0;
}

static int execute(struct terminal_state *state)
{
    char input[TERMINAL_COMMAND_MAX];
    char shell_input[TERMINAL_COMMAND_MAX + 1];
    char display[TERMINAL_COMMAND_MAX + 4];
    char program[160];
    char *argv[16];
    char *cursor;
    int argc = 0;
    int shell_input_length;
    int command_pipe[2];
    int output_pipe[2];
    int child;
    int action;
    int use_shell;

    if (state->child_pid > 0) return TERMINAL_ACTION_NONE;

    history_add(state);
    memcpy(input, state->command, state->command_length);
    input[state->command_length] = '\0';
    memcpy(shell_input, input, state->command_length);
    shell_input[state->command_length] = '\n';
    shell_input[state->command_length + 1] = '\0';
    shell_input_length = state->command_length + 1;
    snprintf(display, sizeof(display), "$ %s\n", input);
    append_text(state, display, -1);
    state->command_length = 0;
    state->command_cursor = 0;
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
        vt100_clear_all(&state->vt);
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
    use_shell = command_needs_shell(shell_input);
    if (!use_shell) {
        if (strchr(argv[0], '/') != NULL) {
            strncpy(program, argv[0], sizeof(program) - 1);
            program[sizeof(program) - 1] = '\0';
        } else {
            snprintf(program, sizeof(program), "/bin/%s", argv[0]);
        }
    }
    if (pipe(command_pipe) < 0) {
        append_text(state, "terminal: cannot create command pipe\n", -1);
        return TERMINAL_ACTION_NONE;
    }
    if (pipe(output_pipe) < 0) {
        close(command_pipe[0]);
        close(command_pipe[1]);
        append_text(state, "terminal: cannot create output pipe\n", -1);
        return TERMINAL_ACTION_NONE;
    }
    child = fork();
    if (child == 0) {
        char *environment[] = { "PATH=/bin:/usr/bin", NULL };

        close(command_pipe[1]);
        close(output_pipe[0]);
        dup2(command_pipe[0], 0);
        close(command_pipe[0]);
        dup2(output_pipe[1], 1);
        dup2(output_pipe[1], 2);
        close(output_pipe[1]);
        if (use_shell) {
            char *shell_argv[] = { "nushell", NULL };

            execve(TERMINAL_SHELL_PATH, shell_argv, environment);
            fprintf(stderr, "terminal: cannot start %s\n",
                    TERMINAL_SHELL_PATH);
        } else {
            execve(program, argv, environment);
            if (strchr(argv[0], '/') == NULL) {
                snprintf(program, sizeof(program), "/usr/bin/%s", argv[0]);
                execve(program, argv, environment);
            }
            fprintf(stderr, "%s: command not found\n", argv[0]);
        }
        _exit(127);
    }
    close(command_pipe[0]);
    close(output_pipe[1]);
    if (child < 0) {
        close(command_pipe[1]);
        close(output_pipe[0]);
        append_text(state, "terminal: fork failed\n", -1);
        return TERMINAL_ACTION_NONE;
    }
    if (use_shell) {
        if (write(command_pipe[1], shell_input, shell_input_length) !=
            shell_input_length)
            append_text(state, "terminal: cannot send command\n", -1);
        close(command_pipe[1]);
        state->input_fd = -1;
    } else {
        state->input_fd = command_pipe[1];
    }
    state->child_pid = child;
    state->output_fd = output_pipe[0];
    return TERMINAL_ACTION_NONE;
}
#else
static int start_shell(struct terminal_state *state)
{
    int master;
    int slave;
    int child;

    if (openpty(&master, &slave, NULL, NULL, NULL) < 0) {
        append_text(state, "terminal: cannot create pty\n", -1);
        return -1;
    }
    child = fork();
    if (child == 0) {
        char *shell_argv[] = { "nushell", NULL };
        char *environment[] = { "PATH=/bin:/usr/bin", NULL };

        close(master);
        if (setsid() < 0)
            _exit(126);
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);
        if (slave > 2)
            close(slave);
        execve(TERMINAL_SHELL_PATH, shell_argv, environment);
        fprintf(stderr, "terminal: cannot start %s\n", TERMINAL_SHELL_PATH);
        _exit(127);
    }
    close(slave);
    if (child < 0) {
        close(master);
        append_text(state, "terminal: fork failed\n", -1);
        return -1;
    }
    state->child_pid = child;
    state->input_fd = master;
    state->output_fd = master;
    return 0;
}
#endif

static void drain_child_output(struct terminal_state *state)
{
    char buffer[512];

    if (state->output_fd < 0) return;
    for (;;) {
        struct pollfd descriptor;
        int count;
        int ready;

        descriptor.fd = state->output_fd;
        descriptor.events = POLLIN;
        descriptor.revents = 0;
        ready = poll(&descriptor, 1, 0);
        if (ready <= 0 || (descriptor.revents & POLLIN) == 0)
            break;
        count = read(state->output_fd, buffer, sizeof(buffer));
        if (count <= 0) return;
        note_output(state, buffer, count);
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
    if (state->input_fd >= 0 && state->input_fd != state->output_fd)
        close(state->input_fd);
    state->output_fd = -1;
    state->input_fd = -1;
    state->child_pid = 0;
    state->command_length = 0;
    state->command_cursor = 0;
    state->command[0] = '\0';
    if (result < 0)
        append_text(state, "terminal: wait failed\n", -1);
    if (vt100_cursor_column(&state->vt) != 0)
        append_text(state, "\n", 1);
}

static int submit_child_input(struct terminal_state *state)
{
    char line[TERMINAL_COMMAND_MAX + 1];
    int length = state->command_length;

    if (state->input_fd < 0) return TERMINAL_ACTION_NONE;
#ifndef KYLEOS_HOST
    {
        char command[TERMINAL_COMMAND_MAX];
        int action;

        if (state->shell_prompt_ready)
            history_add(state);
        memcpy(command, state->command, state->command_length);
        command[state->command_length] = '\0';
        if (strcmp(command, "clear") == 0) {
            vt100_clear_all(&state->vt);
            append_text(state, "$ ", 2);
            state->command_length = 0;
            state->command_cursor = 0;
            state->command[0] = '\0';
            return TERMINAL_ACTION_NONE;
        }
        action = launch_action(command);
        if (action != TERMINAL_ACTION_NONE) {
            append_text(state, "\n$ ", 3);
            state->command_length = 0;
            state->command_cursor = 0;
            state->command[0] = '\0';
            return action;
        }
    }
#endif
    state->shell_prompt_ready = 0;
    state->previous_output = '\0';
    memcpy(line, state->command, length);
    line[length++] = '\n';
    if (write(state->input_fd, line, length) != length) {
        append_text(state, "terminal: cannot send input\n", -1);
        close(state->input_fd);
        state->input_fd = -1;
#ifndef KYLEOS_HOST
        state->output_fd = -1;
#endif
    } else {
#ifdef KYLEOS_HOST
        append_text(state, state->command, state->command_length);
        append_text(state, "\n", 1);
#endif
    }
    state->command_length = 0;
    state->command_cursor = 0;
    state->command[0] = '\0';
    return TERMINAL_ACTION_NONE;
}

static void terminal_dimensions(int width, int height,
                                int *columns, int *rows)
{
    *columns = (width - 14 - KUI_SCROLLBAR_SIZE) / 8;
    *rows = (height - 8) / 16;
    if (*columns < 1) *columns = 1;
    if (*columns > VT100_MAX_COLUMNS) *columns = VT100_MAX_COLUMNS;
    if (*rows < 1) *rows = 1;
    if (*rows > VT100_MAX_SCREEN_ROWS) *rows = VT100_MAX_SCREEN_ROWS;
}

static int terminal_overlay_length(const struct terminal_state *state)
{
    if (state->child_pid <= 0)
        return state->command_length + 2;
    if (state->input_fd >= 0)
        return state->command_length;
    return 0;
}

static unsigned char terminal_overlay_character(
    const struct terminal_state *state, int index)
{
    if (state->child_pid <= 0) {
        if (index == 0) return '$';
        if (index == 1) return ' ';
        index -= 2;
    }
    return (unsigned char)state->command[index];
}

static void terminal_overlay_position_at(const struct terminal_state *state,
                                         int columns, int length,
                                         int *line, int *column)
{
    int wrap = state->vt.wrap_pending;

    *line = vt100_cursor_line(&state->vt);
    *column = vt100_cursor_column(&state->vt);
    for (int index = 0; index < length; index++) {
        if (wrap) {
            (*line)++;
            *column = 0;
            wrap = 0;
        }
        if (*column == columns - 1)
            wrap = 1;
        else
            (*column)++;
    }
}

static void terminal_overlay_position(const struct terminal_state *state,
                                      int columns, int *line, int *column)
{
    terminal_overlay_position_at(state, columns,
                                 terminal_overlay_length(state),
                                 line, column);
}

static void terminal_scroll_metrics(const struct terminal_state *state,
                                    int width, int height,
                                    int *rows, int *total, int *maximum)
{
    int columns;
    int overlay_line;
    int overlay_column;

    terminal_dimensions(width, height, &columns, rows);
    terminal_overlay_position(state, columns, &overlay_line,
                              &overlay_column);
    (void)overlay_column;
    *total = vt100_line_count(&state->vt);
    if (overlay_line + 1 > *total)
        *total = overlay_line + 1;
    if (*total < *rows)
        *total = *rows;
    *maximum = *total - *rows;
    if (*maximum < 0) *maximum = 0;
}

static void terminal_render(struct kui_client *client,
                            const struct kui_event *event,
                            const struct terminal_state *state)
{
    int columns;
    int rows;
    int total;
    int maximum;
    int first_row;
    int overlay_line = vt100_cursor_line(&state->vt);
    int overlay_column = vt100_cursor_column(&state->vt);
    int overlay_wrap = state->vt.wrap_pending;
    int overlay_length = terminal_overlay_length(state);
    int cursor_line;
    int cursor_column;
    int cursor_index = state->command_cursor;
    uint32_t overlay_foreground = vt100_palette_color(
        vt100_current_foreground(&state->vt));
    uint32_t overlay_background = vt100_palette_color(
        vt100_current_background(&state->vt));
    uint8_t overlay_attributes = vt100_current_attributes(&state->vt);
    int overlay_bold = (overlay_attributes & VT100_ATTR_BOLD) != 0;

    terminal_dimensions(event->width, event->height, &columns, &rows);
    if (state->child_pid <= 0)
        cursor_index += 2;
    terminal_overlay_position_at(state, columns, cursor_index,
                                 &cursor_line, &cursor_column);
    terminal_scroll_metrics(state, event->width, event->height,
                            &rows, &total, &maximum);
    first_row = total - rows - state->scroll_from_bottom;
    if (first_row < 0) first_row = 0;
    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  KUI_BLACK);
    for (int line = first_row; line < first_row + rows; line++) {
        for (int column = 0; column < columns; column++) {
            const struct vt100_cell *cell =
                vt100_cell_at(&state->vt, line, column);
            uint32_t foreground;
            uint32_t background;

            if (cell == NULL) continue;
            foreground = vt100_palette_color(cell->foreground);
            background = vt100_palette_color(cell->background);
            if (background != KUI_BLACK)
                kui_fill_rect(&client->surface, 7 + column * 8,
                              4 + (line - first_row) * 16,
                              8, 16, background);
            if (cell->character >= 32 && cell->character != 127 &&
                cell->character != ' ' &&
                (cell->attributes & VT100_ATTR_CONCEAL) == 0)
                kui_draw_mono_char_styled(
                    &client->surface, 7 + column * 8,
                    4 + (line - first_row) * 16, cell->character,
                    foreground, background, 0, 1,
                    (cell->attributes & VT100_ATTR_BOLD) != 0);
            if ((cell->attributes & VT100_ATTR_UNDERLINE) != 0 &&
                (cell->attributes & VT100_ATTR_CONCEAL) == 0)
                kui_fill_rect(&client->surface, 7 + column * 8,
                              4 + (line - first_row) * 16 + 14,
                              8, 1, foreground);
        }
    }
    for (int index = 0; index < overlay_length; index++) {
        unsigned char character;

        if (overlay_wrap) {
            overlay_line++;
            overlay_column = 0;
            overlay_wrap = 0;
        }
        character = terminal_overlay_character(state, index);
        if (overlay_line >= first_row &&
            overlay_line < first_row + rows) {
            if (overlay_background != KUI_BLACK)
                kui_fill_rect(&client->surface, 7 + overlay_column * 8,
                              4 + (overlay_line - first_row) * 16,
                              8, 16, overlay_background);
            if (character != ' ' &&
                (overlay_attributes & VT100_ATTR_CONCEAL) == 0)
                kui_draw_mono_char_styled(
                    &client->surface, 7 + overlay_column * 8,
                    4 + (overlay_line - first_row) * 16, character,
                    overlay_foreground, overlay_background, 0, 1,
                    overlay_bold);
            if ((overlay_attributes & VT100_ATTR_UNDERLINE) != 0 &&
                (overlay_attributes & VT100_ATTR_CONCEAL) == 0)
                kui_fill_rect(&client->surface, 7 + overlay_column * 8,
                              4 + (overlay_line - first_row) * 16 + 14,
                              8, 1, overlay_foreground);
        }
        if (overlay_column == columns - 1)
            overlay_wrap = 1;
        else
            overlay_column++;
    }
    if ((state->child_pid <= 0 || state->input_fd >= 0) &&
        vt100_cursor_visible(&state->vt) &&
        cursor_line >= first_row && cursor_line < first_row + rows)
        kui_fill_rect(&client->surface, 7 + cursor_column * 8,
                      4 + (cursor_line - first_row) * 16,
                      2, 16, overlay_foreground);
    kui_draw_vscrollbar(&client->surface,
                        event->width - KUI_SCROLLBAR_SIZE, 0,
                        event->height, first_row, rows,
                        total, &state->scrollbar);
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
    char copy_buffer[KUI_PROTOCOL_CLIPBOARD_MAX];

    memset(&state, 0, sizeof(state));
    state.input_fd = -1;
    state.output_fd = -1;
    strcpy(state.path, "/");
    kui_client_init(&client);
    copy_buffer[0] = '\0';
    for (;;) {
        int action = TERMINAL_ACTION_NONE;
        const char *launch_path;

        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        if (event.type != KUI_EVENT_INIT && state.vt.cells != NULL) {
            int columns;
            int rows;

            terminal_dimensions(event.width, event.height, &columns, &rows);
            vt100_resize(&state.vt, columns, rows);
        }
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
            int columns;
            int rows;

            if (state.input_fd >= 0) close(state.input_fd);
            if (state.output_fd >= 0 && state.output_fd != state.input_fd)
                close(state.output_fd);
            vt100_free(&state.vt);
            memset(&state, 0, sizeof(state));
            state.input_fd = -1;
            state.output_fd = -1;
            strcpy(state.path, "/");
            terminal_dimensions(event.width, event.height, &columns, &rows);
            if (vt100_init(&state.vt, columns, rows) < 0)
                break;
            append_text(&state,
                        "KyleOS Terminal\nType a command and press Enter.\n",
                        -1);
            chdir("/");
#ifndef KYLEOS_HOST
            (void)start_shell(&state);
#endif
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
                   (event.modifiers & KUI_MOD_SHIFT) &&
                   !event.key_extended && event.key_code == 46) {
            size_t length = state.command_length;
            if (length >= sizeof(copy_buffer)) length = sizeof(copy_buffer) - 1;
            memcpy(copy_buffer, state.command, length);
            copy_buffer[length] = '\0';
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   (event.modifiers & KUI_MOD_CTRL) &&
                   (event.modifiers & KUI_MOD_SHIFT) &&
                   !event.key_extended && event.key_code == 47) {
            if (event.clipboard_kind == KUI_CLIPBOARD_TEXT) {
                size_t length = strlen(event.clipboard);
                if (length > TERMINAL_COMMAND_MAX - 1 -
                            (size_t)state.command_length)
                    length = TERMINAL_COMMAND_MAX - 1 -
                             (size_t)state.command_length;
                memmove(state.command + state.command_cursor + length,
                        state.command + state.command_cursor,
                        (size_t)(state.command_length -
                                 state.command_cursor) + 1);
                memcpy(state.command + state.command_cursor,
                       event.clipboard, length);
                state.command_length += length;
                state.command_cursor += (int)length;
            }
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   (event.modifiers & KUI_MOD_CTRL) &&
                   !event.key_extended && event.key_code == 38) {
            vt100_clear_all(&state.vt);
#ifndef KYLEOS_HOST
            if (state.child_pid > 0)
                append_text(&state, "$ ", 2);
#endif
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   (event.modifiers & KUI_MOD_CTRL) &&
                   !(event.modifiers & KUI_MOD_SHIFT) &&
                   !event.key_extended && event.key_code == 46) {
            if (terminal_editing_command(&state)) {
#ifdef KYLEOS_HOST
                append_text(&state, "$ ", 2);
#endif
                append_text(&state, state.command, state.command_length);
                append_text(&state, "^C\n", 3);
#ifndef KYLEOS_HOST
                append_text(&state, "$ ", 2);
#endif
            } else if (state.child_pid > 0) {
#ifdef KYLEOS_HOST
                append_text(&state, "^C\n", 3);
                if (kill(state.child_pid, SIGINT) < 0)
                    append_text(&state,
                                "terminal: cannot interrupt command\n", -1);
#else
                unsigned char interrupt = 3;

                if (write(state.input_fd, &interrupt, 1) != 1)
                    append_text(&state,
                                "terminal: cannot interrupt command\n", -1);
#endif
            }
            state.command_length = 0;
            state.command_cursor = 0;
            state.command[0] = '\0';
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   (event.modifiers & KUI_MOD_CTRL) &&
                   !(event.modifiers & KUI_MOD_SHIFT) &&
                   !event.key_extended &&
                   (event.key_code == 44 || event.key_code == 43)) {
            if (!terminal_editing_command(&state) && state.input_fd >= 0) {
#ifdef KYLEOS_HOST
                int signal_number = event.key_code == 44 ? SIGTSTP : SIGQUIT;

                if (kill(state.child_pid, signal_number) < 0)
                    append_text(&state,
                                "terminal: cannot signal command\n", -1);
#else
                unsigned char control = event.key_code == 44 ? 26 : 28;

                if (write(state.input_fd, &control, 1) != 1)
                    append_text(&state,
                                "terminal: cannot signal command\n", -1);
#endif
            }
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   (event.modifiers & KUI_MOD_CTRL) &&
                   !event.key_extended && event.key_code == 32 &&
                   state.child_pid > 0 && state.input_fd >= 0) {
#ifdef KYLEOS_HOST
            close(state.input_fd);
            state.input_fd = -1;
#else
            {
                char eof = 4;

                if (write(state.input_fd, &eof, 1) != 1)
                    append_text(&state, "terminal: cannot send EOF\n", -1);
            }
#endif
            state.command_length = 0;
            state.command_cursor = 0;
            state.command[0] = '\0';
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   event.key_extended &&
                   (state.child_pid <= 0 || state.input_fd >= 0)) {
            if (terminal_editing_command(&state)) {
                if (event.key_code == 75 && state.command_cursor > 0) {
                    state.command_cursor--;
                } else if (event.key_code == 77 &&
                           state.command_cursor < state.command_length) {
                    state.command_cursor++;
                } else if (event.key_code == 71) {
                    state.command_cursor = 0;
                } else if (event.key_code == 79) {
                    state.command_cursor = state.command_length;
                } else if (event.key_code == 83 &&
                           state.command_cursor < state.command_length) {
                    memmove(state.command + state.command_cursor,
                            state.command + state.command_cursor + 1,
                            (size_t)(state.command_length -
                                     state.command_cursor));
                    state.command_length--;
                } else if (event.key_code == 72) {
                    history_previous(&state);
                } else if (event.key_code == 80) {
                    history_next(&state);
                } else if (event.key_code == 73) {
                    int columns;
                    int rows;

                    terminal_dimensions(event.width, event.height,
                                        &columns, &rows);
                    (void)columns;
                    state.scroll_from_bottom += rows;
                } else if (event.key_code == 81) {
                    int columns;
                    int rows;

                    terminal_dimensions(event.width, event.height,
                                        &columns, &rows);
                    (void)columns;
                    state.scroll_from_bottom -= rows;
                }
            } else {
                char sequence_buffer[16];
                const char *sequence = terminal_key_sequence(
                    &event, sequence_buffer, sizeof(sequence_buffer));

                if (sequence != NULL &&
                    write(state.input_fd, sequence, strlen(sequence)) !=
                        (ssize_t)strlen(sequence))
                    append_text(&state,
                                "terminal: cannot send special key\n", -1);
            }
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   (state.child_pid <= 0 || state.input_fd >= 0)) {
            char key = event.character;
            if (!event.key_extended && event.key_code == 14) key = '\b';
            if (!event.key_extended && event.key_code == 28) key = '\n';
            if (key == '\b' && state.command_cursor > 0) {
                memmove(state.command + state.command_cursor - 1,
                        state.command + state.command_cursor,
                        (size_t)(state.command_length -
                                 state.command_cursor) + 1);
                state.command_cursor--;
                state.command_length--;
            }
            else if (key == '\n') {
                if (state.child_pid > 0)
                    action = submit_child_input(&state);
#ifdef KYLEOS_HOST
                else
                    action = execute(&state);
#endif
            }
            else if (key >= 32 && key < 127 &&
                     state.command_length < TERMINAL_COMMAND_MAX - 1) {
                memmove(state.command + state.command_cursor + 1,
                        state.command + state.command_cursor,
                        (size_t)(state.command_length -
                                 state.command_cursor) + 1);
                state.command[state.command_cursor++] = key;
                state.command_length++;
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
        if (copy_buffer[0] != '\0') {
            kui_client_set_action(&client, KUI_ACTION_COPY_TEXT, copy_buffer);
            copy_buffer[0] = '\0';
        }
        if (kui_client_send(&client) < 0) break;
    }
    if (state.input_fd >= 0) close(state.input_fd);
    if (state.output_fd >= 0 && state.output_fd != state.input_fd)
        close(state.output_fd);
    vt100_free(&state.vt);
    kui_client_free(&client);
    return 0;
}
