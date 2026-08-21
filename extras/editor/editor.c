#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef C_IDE
#include "extras/editor/c_syntax.h"
#endif
#include "extras/editor/editor_ipc.h"
#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"
#include "extras/kui/kui_dialog.h"
#include "extras/kui/kui_file_dialog.h"
#include "extras/kui/kui_menu.h"

#define CIDE_BINARY_PATH "/tmp/cide.out"

#ifdef C_IDE
#define EDITOR_TOOLBAR_HEIGHT 34
#define EDITOR_STATUS_HEIGHT 22
#define EDITOR_TEXT_TOP (KUI_MENU_BAR_HEIGHT + EDITOR_TOOLBAR_HEIGHT)
#define EDITOR_TEXT_LEFT 49
#define EDITOR_MENU_COUNT 2
#define CIDE_LOG_PATH "/tmp/cide.log"
#else
#define EDITOR_STATUS_HEIGHT 0
#define EDITOR_TEXT_TOP (KUI_MENU_BAR_HEIGHT + 5)
#define EDITOR_TEXT_LEFT 7
#define EDITOR_MENU_COUNT 1
#endif

enum editor_menu_action {
    EDITOR_MENU_OPEN = 1,
    EDITOR_MENU_SAVE,
    EDITOR_MENU_SAVE_AS,
#ifdef C_IDE
    EDITOR_MENU_BUILD,
    EDITOR_MENU_RUN
#endif
};

static const struct kui_menu_item editor_file_items[] = {
    { "Open...", "Ctrl+O", EDITOR_MENU_OPEN, 1 },
    { "Save", "Ctrl+S", EDITOR_MENU_SAVE, 1 },
    { "Save As...", "Ctrl+Shift+S", EDITOR_MENU_SAVE_AS, 1 }
};

#ifdef C_IDE
static const struct kui_menu_item editor_build_items[] = {
    { "Build", "F5", EDITOR_MENU_BUILD, 1 },
    { "Build and Run", "F6", EDITOR_MENU_RUN, 1 }
};
#endif

static const struct kui_menu editor_menus[] = {
    { "File", editor_file_items, 3 },
#ifdef C_IDE
    { "Build", editor_build_items, 2 }
#endif
};

struct editor_state {
    char path[EDITOR_PATH_MAX];
    char text[EDITOR_TEXT_MAX];
    int text_length;
    int text_cursor;
    int dirty;
    int save_failed;
#ifdef C_IDE
    int build_ok;
    int build_pid;
    int build_queued;
    int run_after_build;
    char build_status[128];
#endif
};

#ifdef C_IDE
static unsigned char syntax_classes[EDITOR_TEXT_MAX];
#endif

static int move_vertical(const struct editor_state *state, int direction)
{
    int start = state->text_cursor;
    int column;
    int target_start;
    int target_end;

    while (start > 0 && state->text[start - 1] != '\n') start--;
    column = state->text_cursor - start;
    if (direction < 0) {
        if (start == 0) return state->text_cursor;
        target_end = start - 1;
        target_start = target_end;
        while (target_start > 0 && state->text[target_start - 1] != '\n')
            target_start--;
    } else {
        target_start = state->text_cursor;
        while (target_start < state->text_length &&
               state->text[target_start] != '\n')
            target_start++;
        if (target_start == state->text_length) return state->text_cursor;
        target_start++;
        target_end = target_start;
        while (target_end < state->text_length &&
               state->text[target_end] != '\n')
            target_end++;
    }
    return target_start +
        (column < target_end - target_start ? column : target_end - target_start);
}

static void insert_character(struct editor_state *state, char ch)
{
    if (state->text_length >= EDITOR_TEXT_MAX - 1) return;
    memmove(state->text + state->text_cursor + 1,
            state->text + state->text_cursor,
            state->text_length - state->text_cursor);
    state->text[state->text_cursor++] = ch;
    state->text_length++;
    state->text[state->text_length] = '\0';
    state->dirty = 1;
    state->save_failed = 0;
}

static int load_document(struct editor_state *state, const char *path)
{
    struct editor_state loaded;
    int fd;
    int count;

    memset(&loaded, 0, sizeof(loaded));
    strncpy(loaded.path, path, sizeof(loaded.path) - 1);
    fd = open(loaded.path, O_RDONLY);
    if (fd < 0) return -1;
    count = read(fd, loaded.text, EDITOR_TEXT_MAX - 1);
    close(fd);
    if (count < 0) return -1;
    loaded.text_length = count;
    loaded.text[loaded.text_length] = '\0';
    *state = loaded;
    return 0;
}

static void open_document(struct editor_state *state, const char *path)
{
    if (load_document(state, path) == 0) return;
    memset(state, 0, sizeof(*state));
    strncpy(state->path, path, sizeof(state->path) - 1);
    state->dirty = 1;
}

static int save_document_to(struct editor_state *state, const char *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int total = 0;

    if (fd >= 0) {
        while (total < state->text_length) {
            int count = write(fd, state->text + total,
                              state->text_length - total);
            if (count <= 0) break;
            total += count;
        }
        close(fd);
    }
    state->save_failed = fd < 0 || total != state->text_length;
    if (!state->save_failed) {
        strncpy(state->path, path, sizeof(state->path) - 1);
        state->path[sizeof(state->path) - 1] = '\0';
        state->dirty = 0;
        return 0;
    }
    return -1;
}

static int save_document(struct editor_state *state)
{
    return save_document_to(state, state->path);
}

#ifdef C_IDE
static void cide_read_build_log(struct editor_state *state)
{
    int fd = open(CIDE_LOG_PATH, O_RDONLY);
    int count = -1;

    if (fd >= 0) {
        count = read(fd, state->build_status,
                     (int)sizeof(state->build_status) - 1);
        close(fd);
    }
    if (count <= 0) {
        strcpy(state->build_status, "Build failed without diagnostics");
        return;
    }
    state->build_status[count] = '\0';
    for (int index = 0; index < count; index++)
        if (state->build_status[index] == '\n' ||
            state->build_status[index] == '\r' ||
            state->build_status[index] == '\t')
            state->build_status[index] = ' ';
}

static int cide_start_build(struct editor_state *state, int run_after_build)
{
    int child;

    state->build_ok = 0;
    if (state->build_pid > 0) {
        strcpy(state->build_status, "A build is already running");
        return -1;
    }
    if (state->path[0] == '\0') {
        strcpy(state->build_status, "Save the source file before building");
        return -1;
    }
    if (state->dirty && save_document(state) < 0) {
        strcpy(state->build_status, "Save failed; build cancelled");
        return -1;
    }
    child = fork();
    if (child == 0) {
        int log = open(CIDE_LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        char *arguments[] = {
            "cc", state->path, "-o", CIDE_BINARY_PATH, NULL
        };

        if (log >= 0) {
            dup2(log, 1);
            dup2(log, 2);
            close(log);
        }
        execve("/bin/cc", arguments, NULL);
        _exit(127);
    }
    if (child < 0) {
        strcpy(state->build_status, "Could not start the compiler");
        return -1;
    }
    state->build_pid = child;
    state->run_after_build = run_after_build;
    strcpy(state->build_status, "Building...");
    return 0;
}

static int cide_request_build(struct editor_state *state,
                              int run_after_build, int *saved)
{
    *saved = 0;
    state->build_ok = 0;
    if (state->build_pid > 0 || state->build_queued != 0) {
        strcpy(state->build_status, "A build is already running");
        return -1;
    }
    if (state->path[0] == '\0') {
        strcpy(state->build_status, "Save the source file before building");
        return -1;
    }
    if (!state->dirty)
        return cide_start_build(state, run_after_build);
    if (save_document(state) < 0) {
        strcpy(state->build_status, "Save failed; build cancelled");
        return -1;
    }

    /* Let KUID process the file-saved refresh before forking TinyCC. Aqua has
     * more shell work per frame, and doing both at once can make otherwise
     * healthy clients miss the compositor's protocol deadline. */
    state->build_queued = run_after_build ? 2 : 1;
    strcpy(state->build_status, "Saved; waiting to build...");
    *saved = 1;
    return 0;
}

static int cide_poll_build(struct editor_state *state, int *launch)
{
    int status = 0;
    int result;

    *launch = 0;
    if (state->build_pid <= 0) return 0;
    result = waitpid(state->build_pid, &status, WNOHANG);
    if (result == 0) return 0;
    state->build_pid = 0;
    if (result < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        state->run_after_build = 0;
        cide_read_build_log(state);
        return -1;
    }
    state->build_ok = 1;
    strcpy(state->build_status, "Build succeeded: /tmp/cide.out");
    if (state->run_after_build) *launch = 1;
    state->run_after_build = 0;
    return 1;
}

static uint32_t cide_syntax_color(unsigned char syntax)
{
    static const uint32_t colors[] = {
        0x001d2530U,
        0x008321a3U,
        0x00005fb8U,
        0x00008a7aU,
        0x00a31515U,
        0x00047736U,
        0x00875200U
    };

    return syntax < sizeof(colors) / sizeof(colors[0]) ?
           colors[syntax] : colors[0];
}

static void cide_cursor_location(const struct editor_state *state,
                                 int *line, int *column)
{
    *line = 1;
    *column = 1;
    for (int position = 0; position < state->text_cursor; position++) {
        if (state->text[position] == '\n') {
            (*line)++;
            *column = 1;
        } else {
            (*column)++;
        }
    }
}
#endif

static void apply_request(struct editor_state *state,
                          const struct editor_request *request)
{
    int repeat = request->repeat > 0 ? request->repeat : 1;

    switch (request->command) {
    case EDITOR_COMMAND_OPEN:
        open_document(state, request->path);
        break;
    case EDITOR_COMMAND_INSERT:
        for (int i = 0; i < repeat; i++)
            insert_character(state, (char)request->value);
        break;
    case EDITOR_COMMAND_BACKSPACE:
        if (state->text_cursor > 0) {
            memmove(state->text + state->text_cursor - 1,
                    state->text + state->text_cursor,
                    state->text_length - state->text_cursor);
            state->text_cursor--;
            state->text_length--;
            state->dirty = 1;
            state->save_failed = 0;
        }
        break;
    case EDITOR_COMMAND_DELETE:
        if (state->text_cursor < state->text_length) {
            memmove(state->text + state->text_cursor,
                    state->text + state->text_cursor + 1,
                    state->text_length - state->text_cursor - 1);
            state->text_length--;
            state->dirty = 1;
            state->save_failed = 0;
        }
        break;
    case EDITOR_COMMAND_LEFT:
        if (state->text_cursor > 0) state->text_cursor--;
        break;
    case EDITOR_COMMAND_RIGHT:
        if (state->text_cursor < state->text_length) state->text_cursor++;
        break;
    case EDITOR_COMMAND_UP:
    case EDITOR_COMMAND_PAGE_UP:
        for (int i = 0; i < repeat; i++)
            state->text_cursor = move_vertical(state, -1);
        break;
    case EDITOR_COMMAND_DOWN:
    case EDITOR_COMMAND_PAGE_DOWN:
        for (int i = 0; i < repeat; i++)
            state->text_cursor = move_vertical(state, 1);
        break;
    case EDITOR_COMMAND_SET_CURSOR:
        state->text_cursor = request->value;
        if (state->text_cursor < 0) state->text_cursor = 0;
        if (state->text_cursor > state->text_length)
            state->text_cursor = state->text_length;
        break;
    case EDITOR_COMMAND_SAVE:
        save_document(state);
        break;
    default:
        break;
    }
    state->text[state->text_length] = '\0';
}

static int editor_columns(int width)
{
    int columns = (width - EDITOR_TEXT_LEFT - 9) / 8;
    return columns > 0 ? columns : 1;
}

static int editor_visible_rows(int height)
{
    int rows = (height - EDITOR_TEXT_TOP - EDITOR_STATUS_HEIGHT - 4) / 16;
    return rows > 0 ? rows : 1;
}

static int text_rows_for_position(const char *text, int position, int columns)
{
    int row = 0;
    int column = 0;

    for (int i = 0; i < position; i++) {
        if (text[i] == '\n') { row++; column = 0; }
        else if (++column >= columns) { row++; column = 0; }
    }
    return row;
}

static int editor_max_scroll(const struct editor_state *state,
                             int width, int height)
{
    int rows = text_rows_for_position(state->text, state->text_length,
                                      editor_columns(width)) + 1;
    int maximum = rows - editor_visible_rows(height);
    return maximum > 0 ? maximum : 0;
}

static int editor_position_at(const struct editor_state *state,
                              int width, int scroll_row, int x, int y)
{
    int columns = editor_columns(width);
    int target_row = scroll_row + (y - EDITOR_TEXT_TOP) / 16;
    int target_column = (x - EDITOR_TEXT_LEFT) / 8;
    int row = 0;
    int column = 0;

    if (target_column < 0) target_column = 0;
    for (int position = 0; position <= state->text_length; position++) {
        if (row == target_row &&
            (column >= target_column || position == state->text_length ||
             state->text[position] == '\n'))
            return position;
        if (position == state->text_length) break;
        if (state->text[position] == '\n') { row++; column = 0; }
        else if (++column >= columns) { row++; column = 0; }
    }
    return state->text_length;
}

static void editor_render(struct kui_client *client,
                          const struct kui_event *event,
                          const struct editor_state *state, int scroll_row)
{
    int columns = editor_columns(event->width);
    int row = 0;
    int column = 0;
#ifdef C_IDE
    int source_line = 1;
#endif

    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  KUI_WHITE);
#ifdef C_IDE
    c_syntax_highlight(state->text, state->text_length, syntax_classes);
    kui_fill_rect(&client->surface, 0, KUI_MENU_BAR_HEIGHT,
                  event->width, EDITOR_TOOLBAR_HEIGHT, KUI_FACE);
    kui_fill_rect(&client->surface, 0,
                  KUI_MENU_BAR_HEIGHT + EDITOR_TOOLBAR_HEIGHT - 1,
                  event->width, 1, KUI_SHADOW);
    kui_draw_button(&client->surface, 6, KUI_MENU_BAR_HEIGHT + 4,
                    58, 25, "Open", 0);
    kui_draw_button(&client->surface, 69, KUI_MENU_BAR_HEIGHT + 4,
                    58, 25, "Save", 0);
    kui_draw_button(&client->surface, 132, KUI_MENU_BAR_HEIGHT + 4,
                    58, 25, "Build", 0);
    kui_draw_button(&client->surface, 195, KUI_MENU_BAR_HEIGHT + 4,
                    58, 25, "Run", 0);
    kui_fill_rect(&client->surface, 0, EDITOR_TEXT_TOP, 45,
                  event->height - EDITOR_TEXT_TOP - EDITOR_STATUS_HEIGHT,
                  0x00eef1f4U);
    kui_fill_rect(&client->surface, 44, EDITOR_TEXT_TOP, 1,
                  event->height - EDITOR_TEXT_TOP - EDITOR_STATUS_HEIGHT,
                  0x00bbc2c9U);
#endif
    for (int i = 0; i <= state->text_length; i++) {
        int visible_row = row - scroll_row;

#ifdef C_IDE
        if (column == 0 &&
            (i == 0 || (i > 0 && state->text[i - 1] == '\n')) &&
            visible_row >= 0 &&
            visible_row < editor_visible_rows(event->height)) {
            char number[8];

            snprintf(number, sizeof(number), "%4d", source_line);
            kui_draw_mono_text(&client->surface, 5,
                               EDITOR_TEXT_TOP + visible_row * 16,
                               number, 0x006b737bU, 0x00eef1f4U, 0);
        }
#endif
        if (i == state->text_cursor && visible_row >= 0 &&
            visible_row < editor_visible_rows(event->height))
            kui_fill_rect(&client->surface, EDITOR_TEXT_LEFT - 1 + column * 8,
                          EDITOR_TEXT_TOP + visible_row * 16,
                          2, 16, KUI_TITLE_BLUE);
        if (i == state->text_length) break;
        if (state->text[i] == '\n') {
            row++;
            column = 0;
#ifdef C_IDE
            source_line++;
#endif
            continue;
        }
        if (visible_row >= 0 &&
            visible_row < editor_visible_rows(event->height))
            kui_draw_mono_char(&client->surface,
                               EDITOR_TEXT_LEFT + column * 8,
                               EDITOR_TEXT_TOP + visible_row * 16,
                               (unsigned char)state->text[i],
#ifdef C_IDE
                          cide_syntax_color(syntax_classes[i]),
#else
                          KUI_BLACK,
#endif
                          KUI_WHITE, 0);
        if (++column >= columns) { row++; column = 0; }
    }
#ifdef C_IDE
    {
        int line;
        int cursor_column;
        char location[48];
        int status_y = event->height - EDITOR_STATUS_HEIGHT;

        cide_cursor_location(state, &line, &cursor_column);
        kui_fill_rect(&client->surface, 0, status_y, event->width,
                      EDITOR_STATUS_HEIGHT, 0x00e8edf2U);
        kui_fill_rect(&client->surface, 0, status_y, event->width, 1,
                      0x0099a3adU);
        snprintf(location, sizeof(location), "Ln %d  Col %d  C99%s",
                 line, cursor_column, state->dirty ? "  Modified" : "");
        kui_draw_text(&client->surface, 7, status_y + 4, location,
                      0x0036424eU, 0x00e8edf2U, 0);
        if (state->build_status[0] != '\0')
            kui_draw_text(&client->surface, 220, status_y + 4,
                          state->build_status,
                          state->build_ok ? 0x00006f35U : 0x00a31515U,
                          0x00e8edf2U, 0);
    }
#endif
    if (state->save_failed)
        kui_draw_text(&client->surface, EDITOR_TEXT_LEFT,
                      event->height - 18 - EDITOR_STATUS_HEIGHT,
                      "Save failed", 0x00b00000U, KUI_WHITE, 0);
}

static const char *editor_path_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

int main(void)
{
    struct editor_state state;
    struct kui_file_dialog file_dialog;
    struct kui_confirm_dialog discard_dialog;
    struct kui_menu_state menu_state;
    struct kui_client client;
    struct kui_event event;
    char pending_open_path[EDITOR_PATH_MAX];
    int scroll_row = 0;

    memset(&state, 0, sizeof(state));
    memset(&file_dialog, 0, sizeof(file_dialog));
    memset(&discard_dialog, 0, sizeof(discard_dialog));
    pending_open_path[0] = '\0';
    kui_menu_init(&menu_state);
    kui_client_init(&client);
    for (;;) {
        struct editor_request request;
        int saved = 0;
        int menu_action = 0;
        int menu_consumed = 0;
        int launch_requested = 0;
        char title[KUI_PROTOCOL_TITLE_MAX];
#ifdef C_IDE
        int build_requested = 0;
        int run_requested = 0;
#endif

        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        memset(&request, 0, sizeof(request));
        request.repeat = 1;
        if (event.type == KUI_EVENT_INIT) {
            request.command = EDITOR_COMMAND_OPEN;
            strncpy(request.path,
                    event.argument[0] != '\0' ? event.argument :
#ifdef C_IDE
                    "/hello.c",
#else
                    "/NOTE.TXT",
#endif
                    sizeof(request.path) - 1);
            scroll_row = 0;
            kui_menu_close(&menu_state);
            pending_open_path[0] = '\0';
            kui_file_dialog_close(&file_dialog);
            kui_confirm_dialog_close(&discard_dialog);
#ifdef C_IDE
        } else if (event.type == KUI_EVENT_TIMER &&
                   (state.build_pid > 0 || state.build_queued != 0)) {
            if (state.build_queued != 0) {
                int queued = state.build_queued;

                state.build_queued = 0;
                cide_start_build(&state, queued == 2);
            } else {
                cide_poll_build(&state, &launch_requested);
            }
#endif
        } else if (discard_dialog.active) {
            int result = kui_confirm_dialog_handle(&discard_dialog, &event);

            if (result == KUI_CONFIRM_DIALOG_ACCEPT) {
                if (load_document(&state, pending_open_path) == 0) {
                    scroll_row = 0;
                } else {
                    kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_OPEN,
                                         pending_open_path);
                    kui_file_dialog_set_error(&file_dialog,
                                              "Could not open this file");
                }
                pending_open_path[0] = '\0';
                kui_confirm_dialog_close(&discard_dialog);
            } else if (result == KUI_CONFIRM_DIALOG_CANCEL) {
                pending_open_path[0] = '\0';
            }
        } else if (file_dialog.active) {
            char selected_path[EDITOR_PATH_MAX];
            int result;

            selected_path[0] = '\0';
            result = kui_file_dialog_handle(&file_dialog, &event,
                                            selected_path,
                                            sizeof(selected_path));
            if (result == KUI_FILE_DIALOG_ACCEPT) {
                if (file_dialog.mode == KUI_FILE_DIALOG_OPEN) {
                    if (state.dirty) {
                        kui_file_dialog_close(&file_dialog);
                        strncpy(pending_open_path, selected_path,
                                sizeof(pending_open_path) - 1);
                        pending_open_path[sizeof(pending_open_path) - 1] = '\0';
                        kui_confirm_dialog_open(
                            &discard_dialog, "Unsaved Changes",
                            "Discard changes and open another file?",
                            "Open", "Cancel");
                    } else if (load_document(&state, selected_path) == 0) {
                        kui_file_dialog_close(&file_dialog);
                        scroll_row = 0;
                    } else {
                        kui_file_dialog_set_error(&file_dialog,
                                                  "Could not open this file");
                    }
                } else if (save_document_to(&state, selected_path) == 0) {
                    kui_file_dialog_close(&file_dialog);
                    saved = 1;
                } else {
                    kui_file_dialog_set_error(&file_dialog,
                                              "Save failed (try an 8.3 name)");
                }
            }
        } else if ((menu_action = kui_menu_handle(
                        &menu_state, &event, editor_menus, EDITOR_MENU_COUNT,
                        &menu_consumed)) != 0) {
            if (menu_action == EDITOR_MENU_OPEN) {
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_OPEN,
                                     state.path);
            } else if (menu_action == EDITOR_MENU_SAVE) {
                if (save_document(&state) == 0) saved = 1;
            } else if (menu_action == EDITOR_MENU_SAVE_AS) {
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_SAVE,
                                     state.path);
#ifdef C_IDE
            } else if (menu_action == EDITOR_MENU_BUILD) {
                build_requested = 1;
            } else if (menu_action == EDITOR_MENU_RUN) {
                run_requested = 1;
#endif
            }
        } else if (menu_consumed) {
            /* The shared menu owns this event. */
#ifdef C_IDE
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1 &&
                   event.y >= KUI_MENU_BAR_HEIGHT &&
                   event.y < KUI_MENU_BAR_HEIGHT + EDITOR_TOOLBAR_HEIGHT) {
            if (event.x < 66) {
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_OPEN,
                                     state.path);
            } else if (event.x < 130) {
                if (save_document(&state) == 0) saved = 1;
            } else if (event.x < 193) {
                build_requested = 1;
            } else if (event.x < 257) {
                run_requested = 1;
            }
#endif
        } else if (event.type == KUI_EVENT_POINTER && event.wheel != 0) {
            scroll_row -= event.wheel * 3;
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1 &&
                   event.y >= EDITOR_TEXT_TOP) {
            request.command = EDITOR_COMMAND_SET_CURSOR;
            request.value = editor_position_at(&state, event.width,
                                               scroll_row, event.x, event.y);
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed) {
#ifdef C_IDE
            if (!event.key_extended && event.key_code == 63) {
                build_requested = 1;
            } else if (!event.key_extended && event.key_code == 64) {
                run_requested = 1;
            } else
#endif
            if ((event.modifiers & KUI_MOD_CTRL) &&
                (event.modifiers & KUI_MOD_SHIFT) &&
                !event.key_extended && event.key_code == 31) {
                kui_menu_close(&menu_state);
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_SAVE,
                                     state.path);
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 31) {
                if (save_document(&state) == 0) saved = 1;
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 24) {
                kui_menu_close(&menu_state);
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_OPEN,
                                     state.path);
            } else if (event.key_extended && event.key_code == 73) {
                request.command = EDITOR_COMMAND_PAGE_UP;
                request.repeat = editor_visible_rows(event.height);
            } else if (event.key_extended && event.key_code == 81) {
                request.command = EDITOR_COMMAND_PAGE_DOWN;
                request.repeat = editor_visible_rows(event.height);
            } else if (event.key_extended && event.key_code == 75)
                request.command = EDITOR_COMMAND_LEFT;
            else if (event.key_extended && event.key_code == 77)
                request.command = EDITOR_COMMAND_RIGHT;
            else if (event.key_extended && event.key_code == 72)
                request.command = EDITOR_COMMAND_UP;
            else if (event.key_extended && event.key_code == 80)
                request.command = EDITOR_COMMAND_DOWN;
            else if (!event.key_extended && event.key_code == 14)
                request.command = EDITOR_COMMAND_BACKSPACE;
            else if (event.key_extended && event.key_code == 83)
                request.command = EDITOR_COMMAND_DELETE;
            else if (event.character != 0 &&
                     !(event.modifiers & KUI_MOD_CTRL)) {
                request.command = EDITOR_COMMAND_INSERT;
                request.value = event.character;
                if (event.character == '\t') {
                    request.value = ' ';
                    request.repeat = 4;
                }
            }
        }
        if (request.command != 0) apply_request(&state, &request);
#ifdef C_IDE
        if (request.command == EDITOR_COMMAND_INSERT ||
            request.command == EDITOR_COMMAND_BACKSPACE ||
            request.command == EDITOR_COMMAND_DELETE) {
            state.build_ok = 0;
            state.build_status[0] = '\0';
        }
        if (build_requested || run_requested) {
            int build_saved = 0;

            cide_request_build(&state, run_requested, &build_saved);
            if (build_saved) saved = 1;
        }
#endif
        {
            int cursor_row = text_rows_for_position(
                state.text, state.text_cursor, editor_columns(event.width));
            if (cursor_row < scroll_row) scroll_row = cursor_row;
            if (cursor_row >= scroll_row + editor_visible_rows(event.height))
                scroll_row = cursor_row - editor_visible_rows(event.height) + 1;
        }
        if (scroll_row < 0) scroll_row = 0;
        if (scroll_row > editor_max_scroll(&state, event.width, event.height))
            scroll_row = editor_max_scroll(&state, event.width, event.height);
        snprintf(title, sizeof(title),
#ifdef C_IDE
                 "C IDE - %s%s",
#else
                 "%s%s",
#endif
                 editor_path_name(state.path), state.dirty ? " *" : "");
        kui_client_begin(&client, &event, title);
        kui_client_set_constraints(&client,
#ifdef C_IDE
                                   560, 360, 820, 560);
#else
                                   440, 300, 700, 481);
#endif
        editor_render(&client, &event, &state, scroll_row);
        kui_menu_render(&client.surface, &menu_state, editor_menus,
                        EDITOR_MENU_COUNT);
        kui_file_dialog_render(&client.surface, &event, &file_dialog);
        kui_confirm_dialog_render(&client.surface, &event, &discard_dialog);
#ifdef C_IDE
        kui_client_set_timer(&client, state.build_pid > 0 ? 50 :
                                      state.build_queued != 0 ? 1 : 0);
#endif
        if (launch_requested)
            kui_client_set_action(&client, KUI_ACTION_LAUNCH,
                                  CIDE_BINARY_PATH);
        else if (saved)
            kui_client_set_action(&client, KUI_ACTION_FILE_SAVED, state.path);
        if (kui_client_send(&client) < 0) break;
    }
    kui_client_free(&client);
    return 0;
}
