#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "extras/editor/editor_ipc.h"
#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"
#include "extras/kui/kui_dialog.h"
#include "extras/kui/kui_file_dialog.h"

#define EDITOR_MENU_HEIGHT 23
#define EDITOR_TEXT_TOP 28
#define EDITOR_FILE_MENU_WIDTH 196
#define EDITOR_FILE_MENU_ROW_HEIGHT 24

struct editor_state {
    char path[EDITOR_PATH_MAX];
    char text[EDITOR_TEXT_MAX];
    int text_length;
    int text_cursor;
    int dirty;
    int save_failed;
};

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
    int columns = (width - 16) / 8;
    return columns > 0 ? columns : 1;
}

static int editor_visible_rows(int height)
{
    int rows = (height - EDITOR_TEXT_TOP - 4) / 16;
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
    int target_column = (x - 7) / 8;
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
                          const struct editor_state *state, int scroll_row,
                          int file_menu_open)
{
    int columns = editor_columns(event->width);
    int row = 0;
    int column = 0;

    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  KUI_WHITE);
    for (int i = 0; i <= state->text_length; i++) {
        int visible_row = row - scroll_row;

        if (i == state->text_cursor && visible_row >= 0 &&
            visible_row < editor_visible_rows(event->height))
            kui_fill_rect(&client->surface, 6 + column * 8,
                          EDITOR_TEXT_TOP + visible_row * 16,
                          2, 16, KUI_TITLE_BLUE);
        if (i == state->text_length) break;
        if (state->text[i] == '\n') { row++; column = 0; continue; }
        if (visible_row >= 0 &&
            visible_row < editor_visible_rows(event->height))
            kui_draw_char(&client->surface, 7 + column * 8,
                          EDITOR_TEXT_TOP + visible_row * 16,
                          (unsigned char)state->text[i],
                          KUI_BLACK, KUI_WHITE, 0);
        if (++column >= columns) { row++; column = 0; }
    }
    if (state->save_failed)
        kui_draw_text(&client->surface, 7, event->height - 18,
                      "Save failed", 0x00b00000U, KUI_WHITE, 0);
    kui_fill_rect(&client->surface, 0, 0, event->width,
                  EDITOR_MENU_HEIGHT, KUI_FACE);
    kui_fill_rect(&client->surface, 0, EDITOR_MENU_HEIGHT - 1,
                  event->width, 1, KUI_SHADOW);
    kui_draw_text(&client->surface, 8, 7, "File",
                  KUI_BLACK, KUI_FACE, 0);
    if (file_menu_open) {
        kui_fill_rect(&client->surface, 6, 5, 36, 17, 0x00d4d0c8U);
        kui_outline_rect(&client->surface, 6, 4, 38, 19, KUI_SHADOW);
        kui_draw_text(&client->surface, 8, 7, "File",
                      KUI_BLACK, 0x00d4d0c8U, 0);
        kui_fill_rect(&client->surface, 6, EDITOR_MENU_HEIGHT + 4,
                      EDITOR_FILE_MENU_WIDTH, 76, 0x00505050U);
        kui_fill_rect(&client->surface, 2, EDITOR_MENU_HEIGHT,
                      EDITOR_FILE_MENU_WIDTH, 76, KUI_FACE);
        kui_outline_rect(&client->surface, 2, EDITOR_MENU_HEIGHT,
                         EDITOR_FILE_MENU_WIDTH, 76, KUI_BLACK);
        kui_draw_text(&client->surface, 14, EDITOR_MENU_HEIGHT + 8,
                      "Open...", KUI_BLACK, KUI_FACE, 0);
        kui_draw_text(&client->surface, 117, EDITOR_MENU_HEIGHT + 8,
                      "Ctrl+O", KUI_SHADOW, KUI_FACE, 0);
        kui_draw_text(&client->surface, 14,
                      EDITOR_MENU_HEIGHT + EDITOR_FILE_MENU_ROW_HEIGHT + 8,
                      "Save", KUI_BLACK, KUI_FACE, 0);
        kui_draw_text(&client->surface, 117,
                      EDITOR_MENU_HEIGHT + EDITOR_FILE_MENU_ROW_HEIGHT + 8,
                      "Ctrl+S", KUI_SHADOW, KUI_FACE, 0);
        kui_draw_text(&client->surface, 14,
                      EDITOR_MENU_HEIGHT +
                      EDITOR_FILE_MENU_ROW_HEIGHT * 2 + 8,
                      "Save As...", KUI_BLACK, KUI_FACE, 0);
        kui_draw_text(&client->surface, 92,
                      EDITOR_MENU_HEIGHT +
                      EDITOR_FILE_MENU_ROW_HEIGHT * 2 + 8,
                      "Ctrl+Shift+S", KUI_SHADOW, KUI_FACE, 0);
    }
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
    struct kui_client client;
    struct kui_event event;
    char pending_open_path[EDITOR_PATH_MAX];
    int scroll_row = 0;
    int file_menu_open = 0;

    memset(&state, 0, sizeof(state));
    memset(&file_dialog, 0, sizeof(file_dialog));
    memset(&discard_dialog, 0, sizeof(discard_dialog));
    pending_open_path[0] = '\0';
    kui_client_init(&client);
    for (;;) {
        struct editor_request request;
        int saved = 0;
        char title[KUI_PROTOCOL_TITLE_MAX];

        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        memset(&request, 0, sizeof(request));
        request.repeat = 1;
        if (event.type == KUI_EVENT_INIT) {
            request.command = EDITOR_COMMAND_OPEN;
            strncpy(request.path,
                    event.argument[0] != '\0' ? event.argument : "/NOTE.TXT",
                    sizeof(request.path) - 1);
            scroll_row = 0;
            file_menu_open = 0;
            pending_open_path[0] = '\0';
            kui_file_dialog_close(&file_dialog);
            kui_confirm_dialog_close(&discard_dialog);
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
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1 &&
                   file_menu_open) {
            file_menu_open = 0;
            if (kui_in_rect(event.x, event.y, 2, EDITOR_MENU_HEIGHT,
                            EDITOR_FILE_MENU_WIDTH,
                            EDITOR_FILE_MENU_ROW_HEIGHT)) {
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_OPEN,
                                     state.path);
            } else if (kui_in_rect(event.x, event.y, 2,
                                   EDITOR_MENU_HEIGHT +
                                   EDITOR_FILE_MENU_ROW_HEIGHT,
                                   EDITOR_FILE_MENU_WIDTH,
                                   EDITOR_FILE_MENU_ROW_HEIGHT)) {
                if (save_document(&state) == 0) saved = 1;
            } else if (kui_in_rect(event.x, event.y, 2,
                                   EDITOR_MENU_HEIGHT +
                                   EDITOR_FILE_MENU_ROW_HEIGHT * 2,
                                   EDITOR_FILE_MENU_WIDTH,
                                   EDITOR_FILE_MENU_ROW_HEIGHT)) {
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_SAVE,
                                     state.path);
            }
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1 &&
                   event.y < EDITOR_MENU_HEIGHT && event.x < 52) {
            file_menu_open = 1;
        } else if (event.type == KUI_EVENT_POINTER && event.wheel != 0) {
            scroll_row -= event.wheel * 3;
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1 &&
                   event.y >= EDITOR_TEXT_TOP) {
            request.command = EDITOR_COMMAND_SET_CURSOR;
            request.value = editor_position_at(&state, event.width,
                                               scroll_row, event.x, event.y);
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed) {
            if ((event.modifiers & KUI_MOD_CTRL) &&
                (event.modifiers & KUI_MOD_SHIFT) &&
                !event.key_extended && event.key_code == 31) {
                file_menu_open = 0;
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_SAVE,
                                     state.path);
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 31) {
                if (save_document(&state) == 0) saved = 1;
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 24) {
                file_menu_open = 0;
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_OPEN,
                                     state.path);
            } else if (file_menu_open && !event.key_extended &&
                       event.key_code == 1) {
                file_menu_open = 0;
            } else if (file_menu_open) {
                /* The open menu owns keyboard focus. */
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
        snprintf(title, sizeof(title), "%s%s",
                 editor_path_name(state.path), state.dirty ? " *" : "");
        kui_client_begin(&client, &event, title);
        kui_client_set_constraints(&client, 440, 300, 700, 481);
        editor_render(&client, &event, &state, scroll_row, file_menu_open);
        kui_file_dialog_render(&client.surface, &event, &file_dialog);
        kui_confirm_dialog_render(&client.surface, &event, &discard_dialog);
        if (saved)
            kui_client_set_action(&client, KUI_ACTION_FILE_SAVED, state.path);
        if (kui_client_send(&client) < 0) break;
    }
    kui_client_free(&client);
    return 0;
}
