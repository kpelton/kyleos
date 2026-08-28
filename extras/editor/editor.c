#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
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
#include "extras/kui/kui_text_selection.h"

#define CIDE_BINARY_PATH "/tmp/cide.out"
#define EDITOR_TEXT_INITIAL_CAPACITY 8192

#ifdef NOTES_APP
#define EDITOR_DEFAULT_PATH "/NOTES.TXT"
#define EDITOR_WINDOW_TITLE "Notes - %s%s"
#else
#define EDITOR_DEFAULT_PATH "/NOTE.TXT"
#define EDITOR_WINDOW_TITLE "%s%s"
#endif

#ifdef C_IDE
#define EDITOR_TOOLBAR_HEIGHT 34
#define EDITOR_STATUS_HEIGHT 22
#define EDITOR_TEXT_TOP (KUI_MENU_BAR_HEIGHT + EDITOR_TOOLBAR_HEIGHT)
#define EDITOR_TEXT_LEFT 49
#define EDITOR_MENU_COUNT 3
#define CIDE_LOG_PATH "/tmp/cide.log"
#else
#define EDITOR_STATUS_HEIGHT 0
#define EDITOR_TEXT_TOP (KUI_MENU_BAR_HEIGHT + 5)
#define EDITOR_TEXT_LEFT 7
#define EDITOR_MENU_COUNT 2
#endif

enum editor_menu_action {
    EDITOR_MENU_OPEN = 1,
    EDITOR_MENU_SAVE,
    EDITOR_MENU_SAVE_AS,
    EDITOR_MENU_CUT,
    EDITOR_MENU_COPY,
    EDITOR_MENU_PASTE,
    EDITOR_MENU_DELETE,
    EDITOR_MENU_SELECT_ALL,
#ifdef C_IDE
    EDITOR_MENU_BUILD,
    EDITOR_MENU_RUN
#endif
};

enum editor_edit_item {
    EDITOR_EDIT_CUT,
    EDITOR_EDIT_COPY,
    EDITOR_EDIT_PASTE,
    EDITOR_EDIT_DELETE,
    EDITOR_EDIT_SELECT_ALL,
    EDITOR_EDIT_ITEM_COUNT
};

static const struct kui_menu_item editor_file_items[] = {
    { "Open...", "Ctrl+O", EDITOR_MENU_OPEN, 1 },
    { "Save", "Ctrl+S", EDITOR_MENU_SAVE, 1 },
    { "Save As...", "Ctrl+Shift+S", EDITOR_MENU_SAVE_AS, 1 }
};

static struct kui_menu_item editor_edit_items[] = {
    { "Cut", "Ctrl+X", EDITOR_MENU_CUT, 0 },
    { "Copy", "Ctrl+C", EDITOR_MENU_COPY, 0 },
    { "Paste", "Ctrl+V", EDITOR_MENU_PASTE, 0 },
    { "Delete", "Del", EDITOR_MENU_DELETE, 0 },
    { "Select All", "Ctrl+A", EDITOR_MENU_SELECT_ALL, 1 }
};

struct editor_state;
static void update_edit_items(const struct editor_state *state,
                              const struct kui_event *event);

#ifdef C_IDE
static const struct kui_menu_item editor_build_items[] = {
    { "Build", "F5", EDITOR_MENU_BUILD, 1 },
    { "Build and Run", "F6", EDITOR_MENU_RUN, 1 }
};
#endif

static const struct kui_menu editor_menus[] = {
    { "File", editor_file_items, 3 },
    { "Edit", editor_edit_items, EDITOR_EDIT_ITEM_COUNT },
#ifdef C_IDE
    { "Build", editor_build_items, 2 }
#endif
};

struct editor_state {
    char path[EDITOR_PATH_MAX];
    char *text;
    int text_capacity;
    int text_length;
    int text_cursor;
    struct kui_text_selection selection;
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

static int editor_reserve(struct editor_state *state, int capacity)
{
    char *grown;
    int new_capacity;

    if (capacity <= 0) return -1;
    if (capacity <= state->text_capacity) return 0;
    new_capacity = state->text_capacity > 0 ? state->text_capacity :
                                              EDITOR_TEXT_INITIAL_CAPACITY;
    while (new_capacity < capacity) {
        if (new_capacity > INT_MAX / 2) {
            new_capacity = capacity;
            break;
        }
        new_capacity *= 2;
    }
    grown = realloc(state->text, (size_t)new_capacity);
    if (grown == NULL) return -1;
    state->text = grown;
    state->text_capacity = new_capacity;
    if (state->text_length == 0) state->text[0] = '\0';
    return 0;
}

static void update_edit_items(const struct editor_state *state,
                              const struct kui_event *event)
{
    int selected = kui_text_selection_has_range(&state->selection);

    editor_edit_items[EDITOR_EDIT_CUT].enabled = selected;
    editor_edit_items[EDITOR_EDIT_COPY].enabled = selected;
    editor_edit_items[EDITOR_EDIT_DELETE].enabled = selected;
    editor_edit_items[EDITOR_EDIT_PASTE].enabled =
        event->clipboard_kind == KUI_CLIPBOARD_TEXT &&
        event->clipboard[0] != '\0';
}

#ifdef C_IDE
static unsigned char *syntax_classes;
static int syntax_capacity;

static int syntax_reserve(int capacity)
{
    unsigned char *grown;
    int new_capacity = syntax_capacity > 0 ? syntax_capacity :
                                             EDITOR_TEXT_INITIAL_CAPACITY;

    if (capacity <= syntax_capacity) return 0;
    while (new_capacity < capacity) {
        if (new_capacity > INT_MAX / 2) {
            new_capacity = capacity;
            break;
        }
        new_capacity *= 2;
    }
    grown = realloc(syntax_classes, (size_t)new_capacity);
    if (grown == NULL) return -1;
    syntax_classes = grown;
    syntax_capacity = new_capacity;
    return 0;
}
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

static int delete_selection(struct editor_state *state)
{
    if (!kui_text_selection_delete(&state->selection, state->text,
                                   &state->text_length,
                                   &state->text_cursor))
        return 0;
    state->dirty = 1;
    state->save_failed = 0;
    return 1;
}

static void insert_character(struct editor_state *state, char ch)
{
    if (state->text_length == INT_MAX - 1 ||
        editor_reserve(state, state->text_length + 2) < 0)
        return;
    if (!kui_text_selection_replace(&state->selection, state->text,
                                    &state->text_length,
                                    state->text_capacity,
                                    &state->text_cursor, &ch, 1))
        return;
    state->dirty = 1;
    state->save_failed = 0;
}

static int replace_selection(struct editor_state *state,
                             const char *text, int length)
{
    if (length < 0 || state->text_length > INT_MAX - length - 1 ||
        editor_reserve(state, state->text_length + length + 1) < 0)
        return 0;
    if (!kui_text_selection_replace(&state->selection, state->text,
                                    &state->text_length,
                                    state->text_capacity,
                                    &state->text_cursor, text, length))
        return 0;
    state->dirty = 1;
    state->save_failed = 0;
    return 1;
}

static int editor_edit_action(struct editor_state *state, int action,
                              const struct kui_event *event,
                              char clipboard[KUI_PROTOCOL_CLIPBOARD_MAX],
                              int *copied)
{
    int changed = 0;

    *copied = 0;
    if (action == EDITOR_MENU_COPY || action == EDITOR_MENU_CUT) {
        *copied = kui_text_selection_copy(
            &state->selection, state->text, state->text_length,
            clipboard, KUI_PROTOCOL_CLIPBOARD_MAX) > 0;
        if (action == EDITOR_MENU_CUT && *copied)
            changed = delete_selection(state);
    } else if (action == EDITOR_MENU_PASTE) {
        if (event->clipboard_kind == KUI_CLIPBOARD_TEXT &&
            event->clipboard[0] != '\0')
            changed = replace_selection(state, event->clipboard,
                                        (int)strlen(event->clipboard));
    } else if (action == EDITOR_MENU_DELETE) {
        changed = delete_selection(state);
    } else if (action == EDITOR_MENU_SELECT_ALL) {
        kui_text_selection_select_all(&state->selection, state->text_length);
        state->text_cursor = state->text_length;
    }
    return changed;
}

static int load_document(struct editor_state *state, const char *path)
{
    char *text = NULL;
    int capacity = EDITOR_TEXT_INITIAL_CAPACITY;
    int length = 0;
    int fd;
    int count;

    fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    text = malloc((size_t)capacity);
    if (text == NULL) {
        close(fd);
        return -1;
    }
    for (;;) {
        char *grown;

        if (length == capacity - 1) {
            if (capacity > INT_MAX / 2) {
                free(text);
                close(fd);
                return -1;
            }
            capacity *= 2;
            grown = realloc(text, (size_t)capacity);
            if (grown == NULL) {
                free(text);
                close(fd);
                return -1;
            }
            text = grown;
        }
        count = read(fd, text + length, (size_t)(capacity - length - 1));
        if (count <= 0) break;
        length += count;
    }
    close(fd);
    if (count < 0) {
        free(text);
        return -1;
    }
    text[length] = '\0';
    free(state->text);
    state->text = text;
    state->text_capacity = capacity;
    state->text_length = length;
    state->text_cursor = 0;
    kui_text_selection_init(&state->selection);
    strncpy(state->path, path, sizeof(state->path) - 1);
    state->path[sizeof(state->path) - 1] = '\0';
    state->dirty = 0;
    state->save_failed = 0;
#ifdef C_IDE
    state->build_ok = 0;
    state->build_pid = 0;
    state->build_queued = 0;
    state->run_after_build = 0;
    state->build_status[0] = '\0';
#endif
    return 0;
}

static void open_document(struct editor_state *state, const char *path)
{
    if (load_document(state, path) == 0) return;
    state->text_length = 0;
    state->text_cursor = 0;
    state->text[0] = '\0';
    kui_text_selection_init(&state->selection);
    strncpy(state->path, path, sizeof(state->path) - 1);
    state->path[sizeof(state->path) - 1] = '\0';
    state->dirty = 1;
    state->save_failed = 0;
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
        size_t length = strlen(path);

        if (length >= sizeof(state->path)) length = sizeof(state->path) - 1;
        memmove(state->path, path, length);
        state->path[length] = '\0';
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
        if (!delete_selection(state) && state->text_cursor > 0) {
            memmove(state->text + state->text_cursor - 1,
                    state->text + state->text_cursor,
                    state->text_length - state->text_cursor);
            state->text_cursor--;
            state->text_length--;
            state->dirty = 1;
            state->save_failed = 0;
            kui_text_selection_collapse(&state->selection,
                                         state->text_cursor);
        }
        break;
    case EDITOR_COMMAND_DELETE:
        if (!delete_selection(state) &&
            state->text_cursor < state->text_length) {
            memmove(state->text + state->text_cursor,
                    state->text + state->text_cursor + 1,
                    state->text_length - state->text_cursor - 1);
            state->text_length--;
            state->dirty = 1;
            state->save_failed = 0;
            kui_text_selection_collapse(&state->selection,
                                         state->text_cursor);
        }
        break;
    case EDITOR_COMMAND_LEFT:
        if (kui_text_selection_has_range(&state->selection)) {
            int start;
            kui_text_selection_range(&state->selection, &start, 0);
            state->text_cursor = start;
        } else if (state->text_cursor > 0) {
            state->text_cursor--;
        }
        kui_text_selection_collapse(&state->selection, state->text_cursor);
        break;
    case EDITOR_COMMAND_RIGHT:
        if (kui_text_selection_has_range(&state->selection)) {
            int end;
            kui_text_selection_range(&state->selection, 0, &end);
            state->text_cursor = end;
        } else if (state->text_cursor < state->text_length) {
            state->text_cursor++;
        }
        kui_text_selection_collapse(&state->selection, state->text_cursor);
        break;
    case EDITOR_COMMAND_UP:
    case EDITOR_COMMAND_PAGE_UP:
        for (int i = 0; i < repeat; i++)
            state->text_cursor = move_vertical(state, -1);
        kui_text_selection_collapse(&state->selection, state->text_cursor);
        break;
    case EDITOR_COMMAND_DOWN:
    case EDITOR_COMMAND_PAGE_DOWN:
        for (int i = 0; i < repeat; i++)
            state->text_cursor = move_vertical(state, 1);
        kui_text_selection_collapse(&state->selection, state->text_cursor);
        break;
    case EDITOR_COMMAND_SET_CURSOR:
        state->text_cursor = request->value;
        if (state->text_cursor < 0) state->text_cursor = 0;
        if (state->text_cursor > state->text_length)
            state->text_cursor = state->text_length;
        kui_text_selection_collapse(&state->selection, state->text_cursor);
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
    int columns = (width - EDITOR_TEXT_LEFT - KUI_SCROLLBAR_SIZE - 9) / 8;
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

static void editor_text_layout(int width, int height, int scroll_row,
                               struct kui_mono_text_layout *layout)
{
    layout->left = EDITOR_TEXT_LEFT;
    layout->top = EDITOR_TEXT_TOP;
    layout->columns = editor_columns(width);
    layout->visible_rows = editor_visible_rows(height);
    layout->cell_width = 8;
    layout->row_height = 16;
    layout->scroll_row = scroll_row;
}

static void editor_render(struct kui_client *client,
                          const struct kui_event *event,
                          const struct editor_state *state, int scroll_row,
                          const struct kui_scrollbar_state *scrollbar)
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
    int syntax_ready = syntax_reserve(state->text_length > 0 ?
                                      state->text_length : 1) == 0;

    if (syntax_ready)
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
        int selected = i < state->text_length &&
            kui_text_selection_contains(&state->selection, i);

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
        if (selected && visible_row >= 0 &&
            visible_row < editor_visible_rows(event->height))
            kui_fill_rect(&client->surface, EDITOR_TEXT_LEFT + column * 8,
                          EDITOR_TEXT_TOP + visible_row * 16,
                          8, 16, event->style.selection);
        if (!kui_text_selection_has_range(&state->selection) &&
            i == state->text_cursor && visible_row >= 0 &&
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
                          selected ? KUI_WHITE :
                          cide_syntax_color(syntax_ready ?
                                            syntax_classes[i] : 0),
#else
                          selected ? KUI_WHITE : KUI_BLACK,
#endif
                          selected ? event->style.selection : KUI_WHITE, 0);
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
    kui_draw_vscrollbar(
        &client->surface, event->width - KUI_SCROLLBAR_SIZE,
        EDITOR_TEXT_TOP,
        event->height - EDITOR_TEXT_TOP - EDITOR_STATUS_HEIGHT,
        scroll_row, editor_visible_rows(event->height),
        editor_max_scroll(state, event->width, event->height) +
        editor_visible_rows(event->height), scrollbar);
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
    struct kui_context_menu_state context_menu;
    struct kui_scrollbar_state scrollbar;
    struct kui_client client;
    struct kui_event event;
    char pending_open_path[EDITOR_PATH_MAX];
    int scroll_row = 0;

    memset(&state, 0, sizeof(state));
    if (editor_reserve(&state, EDITOR_TEXT_INITIAL_CAPACITY) < 0) return 1;
    memset(&file_dialog, 0, sizeof(file_dialog));
    memset(&discard_dialog, 0, sizeof(discard_dialog));
    memset(&scrollbar, 0, sizeof(scrollbar));
    pending_open_path[0] = '\0';
    kui_menu_init(&menu_state);
    kui_context_menu_init(&context_menu);
    kui_client_init(&client);
    for (;;) {
        struct editor_request request;
        int saved = 0;
        int menu_action = 0;
        int menu_consumed = 0;
        int context_consumed = 0;
        int edit_action = 0;
        int clipboard_copied = 0;
        int edit_changed = 0;
        int scrollbar_hit;
        int launch_requested = 0;
        char title[KUI_PROTOCOL_TITLE_MAX];
        char copied_text[KUI_PROTOCOL_CLIPBOARD_MAX];
#ifdef C_IDE
        int build_requested = 0;
        int run_requested = 0;
#endif

        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        scrollbar_hit = !file_dialog.active && !discard_dialog.active &&
            event.type == KUI_EVENT_POINTER && event.button == 1 &&
            kui_in_rect(event.x, event.y,
                        event.width - KUI_SCROLLBAR_SIZE,
                        EDITOR_TEXT_TOP, KUI_SCROLLBAR_SIZE,
                        event.height - EDITOR_TEXT_TOP -
                        EDITOR_STATUS_HEIGHT);
        if ((!file_dialog.active && !discard_dialog.active) ||
            scrollbar.active_direction != 0)
            scroll_row = kui_vscrollbar_handle(
                &scrollbar, &event,
                event.width - KUI_SCROLLBAR_SIZE, EDITOR_TEXT_TOP,
                event.height - EDITOR_TEXT_TOP - EDITOR_STATUS_HEIGHT,
                scroll_row, editor_visible_rows(event.height),
                editor_max_scroll(&state, event.width, event.height) +
                editor_visible_rows(event.height));
        /* Keep C IDE scrolling independent of text-selection dispatch.  Some
         * compositor paths deliver wheel events with a button flag set, so
         * handle the wheel before the toolbar/menu branches below. */
        if (!file_dialog.active && !discard_dialog.active &&
            event.type == KUI_EVENT_POINTER && event.wheel != 0)
            scroll_row = kui_scrollbar_wheel(
                scroll_row, event.wheel, editor_visible_rows(event.height),
                editor_max_scroll(&state, event.width, event.height) +
                editor_visible_rows(event.height));
        copied_text[0] = '\0';
        update_edit_items(&state, &event);
        memset(&request, 0, sizeof(request));
        request.repeat = 1;
        if (event.type == KUI_EVENT_INIT) {
            request.command = EDITOR_COMMAND_OPEN;
            strncpy(request.path,
                    event.argument[0] != '\0' ? event.argument :
#ifdef C_IDE
                    "/hello.c",
#else
                    EDITOR_DEFAULT_PATH,
#endif
                    sizeof(request.path) - 1);
            scroll_row = 0;
            kui_menu_close(&menu_state);
            kui_context_menu_close(&context_menu);
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
        } else if (event.type == KUI_EVENT_POINTER && event.button == 2) {
            struct kui_mono_text_layout layout;
            int position;

            editor_text_layout(event.width, event.height, scroll_row, &layout);
            if (event.x >= layout.left &&
                event.x < layout.left + layout.columns * layout.cell_width &&
                event.y >= layout.top &&
                event.y < layout.top +
                          layout.visible_rows * layout.row_height) {
                position = kui_mono_text_position_at(
                    state.text, state.text_length, &layout,
                    event.x, event.y);
                if (!kui_text_selection_contains(&state.selection,
                                                 position)) {
                    state.text_cursor = position;
                    kui_text_selection_collapse(&state.selection, position);
                }
                kui_menu_close(&menu_state);
                kui_context_menu_open(&context_menu, event.x, event.y);
            } else {
                kui_context_menu_close(&context_menu);
            }
        } else if ((edit_action = kui_context_menu_handle(
                        &context_menu, &event, editor_edit_items,
                        EDITOR_EDIT_ITEM_COUNT, &context_consumed)) != 0) {
            /* Applied after event dispatch with keyboard Edit actions. */
        } else if (context_consumed) {
            /* The shared context menu owns this event. */
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
            } else if (menu_action >= EDITOR_MENU_CUT &&
                       menu_action <= EDITOR_MENU_SELECT_ALL) {
                edit_action = menu_action;
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
        } else if (scrollbar_hit) {
            /* The shared scrollbar owns this press and repeat timer. */
        } else if (event.type == KUI_EVENT_POINTER && event.wheel != 0) {
            /* Wheel scrolling is applied before dispatch above. */
        } else if (event.type == KUI_EVENT_POINTER) {
            struct kui_mono_text_layout layout;

            editor_text_layout(event.width, event.height, scroll_row, &layout);
            kui_text_selection_handle_pointer(
                &state.selection, &event, state.text, state.text_length,
                &layout, &state.text_cursor);
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
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 46) {
                edit_action = EDITOR_MENU_COPY;
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 45) {
                edit_action = EDITOR_MENU_CUT;
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 47) {
                edit_action = EDITOR_MENU_PASTE;
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 30) {
                edit_action = EDITOR_MENU_SELECT_ALL;
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
        if (edit_action != 0)
            edit_changed = editor_edit_action(
                &state, edit_action, &event, copied_text,
                &clipboard_copied);
        if (edit_changed) kui_context_menu_close(&context_menu);
#ifdef C_IDE
        if (request.command == EDITOR_COMMAND_INSERT ||
            request.command == EDITOR_COMMAND_BACKSPACE ||
            request.command == EDITOR_COMMAND_DELETE || edit_changed) {
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
        update_edit_items(&state, &event);
        snprintf(title, sizeof(title),
#ifdef C_IDE
                 "C IDE - %s%s",
#else
                 EDITOR_WINDOW_TITLE,
#endif
                 editor_path_name(state.path), state.dirty ? " *" : "");
        kui_client_begin(&client, &event, title);
        kui_client_set_constraints(&client,
#ifdef C_IDE
                                   560, 360, 820, 560);
#else
                                   440, 300, 700, 481);
#endif
        editor_render(&client, &event, &state, scroll_row, &scrollbar);
        kui_menu_render(&client.surface, &menu_state, editor_menus,
                        EDITOR_MENU_COUNT);
        kui_context_menu_render(&client.surface, &context_menu,
                                editor_edit_items,
                                EDITOR_EDIT_ITEM_COUNT);
        kui_file_dialog_render(&client.surface, &event, &file_dialog);
        kui_confirm_dialog_render(&client.surface, &event, &discard_dialog);
        {
            uint32_t timer = kui_scrollbar_timer_ms(&scrollbar);
            uint32_t dialog_timer = kui_file_dialog_timer_ms(&file_dialog);
#ifdef C_IDE
            uint32_t build_timer = state.build_pid > 0 ? 50U :
                                   state.build_queued != 0 ? 1U : 0U;

            if (build_timer != 0 && (timer == 0 || build_timer < timer))
                timer = build_timer;
#endif
            if (dialog_timer != 0 && (timer == 0 || dialog_timer < timer))
                timer = dialog_timer;
            kui_client_set_timer(&client, timer);
        }
        if (clipboard_copied)
            kui_client_set_action(&client, KUI_ACTION_COPY_TEXT, copied_text);
        else if (launch_requested)
            kui_client_set_action(&client, KUI_ACTION_LAUNCH,
                                  CIDE_BINARY_PATH);
        else if (saved)
            kui_client_set_action(&client, KUI_ACTION_FILE_SAVED, state.path);
        if (kui_client_send(&client) < 0) break;
    }
    free(state.text);
#ifdef C_IDE
    free(syntax_classes);
#endif
    kui_client_free(&client);
    return 0;
}
