#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"
#include "extras/kui/kui_dialog.h"
#include "extras/kui/kui_file_dialog.h"
#include "extras/kui/kui_text_selection.h"
#include "extras/wordprocessor/writer_document.h"

#define WRITER_DEFAULT_PATH "/DOC.KWP"
#define WRITER_TOOLBAR_HEIGHT 42
#define WRITER_TEXT_TOP (WRITER_TOOLBAR_HEIGHT + 5)
#define WRITER_TEXT_LEFT 8
#define WRITER_STATUS_HEIGHT 22

enum writer_button {
    WRITER_BUTTON_NONE,
    WRITER_BUTTON_OPEN,
    WRITER_BUTTON_SAVE,
    WRITER_BUTTON_SAVE_AS,
    WRITER_BUTTON_BOLD,
    WRITER_BUTTON_SMALLER,
    WRITER_BUTTON_LARGER
};

struct writer_state {
    struct writer_document document;
    char path[WRITER_PATH_MAX];
    int cursor;
    struct kui_text_selection selection;
    int dirty;
    int save_failed;
};

static const char *path_name(const char *path)
{
    const char *slash = strrchr(path, '/');

    return slash != NULL ? slash + 1 : path;
}

static void writer_reset_view(struct writer_state *state)
{
    state->cursor = 0;
    kui_text_selection_init(&state->selection);
    state->save_failed = 0;
}

static void writer_new_at(struct writer_state *state, const char *path)
{
    writer_document_destroy(&state->document);
    writer_document_init(&state->document);
    strncpy(state->path, path, sizeof(state->path) - 1);
    state->path[sizeof(state->path) - 1] = '\0';
    state->dirty = 1;
    writer_reset_view(state);
}

static int writer_load(struct writer_state *state, const char *path)
{
    struct writer_document loaded;

    writer_document_init(&loaded);
    if (writer_document_load(&loaded, path) < 0) {
        writer_document_destroy(&loaded);
        return -1;
    }
    writer_document_destroy(&state->document);
    state->document = loaded;
    strncpy(state->path, path, sizeof(state->path) - 1);
    state->path[sizeof(state->path) - 1] = '\0';
    state->dirty = 0;
    writer_reset_view(state);
    return 0;
}

static int writer_save_to(struct writer_state *state, const char *path)
{
    if (writer_document_save(&state->document, path) < 0) {
        state->save_failed = 1;
        return -1;
    }
    strncpy(state->path, path, sizeof(state->path) - 1);
    state->path[sizeof(state->path) - 1] = '\0';
    state->dirty = 0;
    state->save_failed = 0;
    return 0;
}

static int writer_delete_selection(struct writer_state *state)
{
    if (!kui_text_selection_delete(
            &state->selection, state->document.text,
            &state->document.text_length, &state->cursor))
        return 0;
    state->dirty = 1;
    state->save_failed = 0;
    return 1;
}

static int writer_replace(struct writer_state *state,
                          const char *text, int length)
{
    int required;

    if (length < 0 || state->document.text_length > INT_MAX - length - 1)
        return 0;
    required = state->document.text_length + length + 1;
    if (writer_document_reserve(&state->document, required) < 0) return 0;
    if (!kui_text_selection_replace(
            &state->selection, state->document.text,
            &state->document.text_length, state->document.text_capacity,
            &state->cursor, text, length))
        return 0;
    state->dirty = 1;
    state->save_failed = 0;
    return 1;
}

static int writer_cell_width(const struct writer_state *state)
{
    return 8 * state->document.font_scale;
}

static int writer_row_height(const struct writer_state *state)
{
    return 16 * state->document.font_scale;
}

static int writer_columns(const struct writer_state *state, int width)
{
    int columns = (width - WRITER_TEXT_LEFT - KUI_SCROLLBAR_SIZE - 8) /
                  writer_cell_width(state);

    return columns > 0 ? columns : 1;
}

static int writer_visible_rows(const struct writer_state *state, int height)
{
    int rows = (height - WRITER_TEXT_TOP - WRITER_STATUS_HEIGHT - 4) /
               writer_row_height(state);

    return rows > 0 ? rows : 1;
}

static void writer_row_column(const char *text, int position, int columns,
                              int *row_out, int *column_out)
{
    int row = 0;
    int column = 0;

    for (int index = 0; index < position; index++) {
        if (text[index] == '\n') {
            row++;
            column = 0;
        } else if (++column >= columns) {
            row++;
            column = 0;
        }
    }
    if (row_out != NULL) *row_out = row;
    if (column_out != NULL) *column_out = column;
}

static int writer_position_at_row(const struct writer_state *state,
                                  int columns, int target_row,
                                  int target_column)
{
    int row = 0;
    int column = 0;

    for (int position = 0; position <= state->document.text_length;
         position++) {
        if (row == target_row &&
            (column >= target_column ||
             position == state->document.text_length ||
             state->document.text[position] == '\n'))
            return position;
        if (position == state->document.text_length) break;
        if (state->document.text[position] == '\n') {
            row++;
            column = 0;
        } else if (++column >= columns) {
            row++;
            column = 0;
        }
    }
    return state->document.text_length;
}

static int writer_max_scroll(const struct writer_state *state,
                             int width, int height)
{
    int last_row;
    int maximum;

    writer_row_column(state->document.text, state->document.text_length,
                      writer_columns(state, width), &last_row, NULL);
    maximum = last_row + 1 - writer_visible_rows(state, height);
    return maximum > 0 ? maximum : 0;
}

static void writer_layout(const struct writer_state *state,
                          int width, int height, int scroll_row,
                          struct kui_mono_text_layout *layout)
{
    layout->left = WRITER_TEXT_LEFT;
    layout->top = WRITER_TEXT_TOP;
    layout->columns = writer_columns(state, width);
    layout->visible_rows = writer_visible_rows(state, height);
    layout->cell_width = writer_cell_width(state);
    layout->row_height = writer_row_height(state);
    layout->scroll_row = scroll_row;
}

static int writer_button_at(int x, int y)
{
    if (y < 5 || y >= 35) return WRITER_BUTTON_NONE;
    if (x >= 6 && x < 64) return WRITER_BUTTON_OPEN;
    if (x >= 68 && x < 126) return WRITER_BUTTON_SAVE;
    if (x >= 130 && x < 206) return WRITER_BUTTON_SAVE_AS;
    if (x >= 218 && x < 250) return WRITER_BUTTON_BOLD;
    if (x >= 256 && x < 292) return WRITER_BUTTON_SMALLER;
    if (x >= 296 && x < 332) return WRITER_BUTTON_LARGER;
    return WRITER_BUTTON_NONE;
}

static void writer_change_scale(struct writer_state *state, int direction)
{
    int scale = state->document.font_scale + direction;

    if (scale < WRITER_FONT_MIN) scale = WRITER_FONT_MIN;
    if (scale > WRITER_FONT_MAX) scale = WRITER_FONT_MAX;
    if (scale == state->document.font_scale) return;
    state->document.font_scale = scale;
    state->dirty = 1;
    state->save_failed = 0;
}

static void writer_render_toolbar(struct kui_surface *surface,
                                  const struct writer_state *state,
                                  int width)
{
    char size[24];

    kui_fill_rect(surface, 0, 0, width, WRITER_TOOLBAR_HEIGHT, KUI_FACE);
    kui_fill_rect(surface, 0, WRITER_TOOLBAR_HEIGHT - 2, width, 1, KUI_LIGHT);
    kui_fill_rect(surface, 0, WRITER_TOOLBAR_HEIGHT - 1, width, 1, KUI_SHADOW);
    kui_draw_button(surface, 6, 5, 58, 30, "Open", 0);
    kui_draw_button(surface, 68, 5, 58, 30, "Save", 0);
    kui_draw_button(surface, 130, 5, 76, 30, "Save As", 0);
    kui_draw_button(surface, 218, 5, 32, 30, "B", state->document.bold);
    kui_draw_button(surface, 256, 5, 36, 30, "A-", 0);
    kui_draw_button(surface, 296, 5, 36, 30, "A+", 0);
    snprintf(size, sizeof(size), "Size %d", state->document.font_scale);
    kui_draw_text(surface, 342, 12, size, KUI_BLACK, KUI_FACE, 0);
}

static void writer_render(struct kui_client *client,
                          const struct kui_event *event,
                          const struct writer_state *state, int scroll_row,
                          const struct kui_scrollbar_state *scrollbar)
{
    int columns = writer_columns(state, event->width);
    int visible_rows = writer_visible_rows(state, event->height);
    int cell_width = writer_cell_width(state);
    int row_height = writer_row_height(state);
    int row = 0;
    int column = 0;
    char status[96];

    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  KUI_WHITE);
    writer_render_toolbar(&client->surface, state, event->width);
    for (int index = 0; index <= state->document.text_length; index++) {
        int visible_row = row - scroll_row;
        int selected = index < state->document.text_length &&
            kui_text_selection_contains(&state->selection, index);
        int cell_x = WRITER_TEXT_LEFT + column * cell_width;
        int cell_y = WRITER_TEXT_TOP + visible_row * row_height;

        if (selected && visible_row >= 0 && visible_row < visible_rows)
            kui_fill_rect(&client->surface, cell_x, cell_y,
                          cell_width, row_height, event->style.selection);
        if (!kui_text_selection_has_range(&state->selection) &&
            index == state->cursor && visible_row >= 0 &&
            visible_row < visible_rows)
            kui_fill_rect(&client->surface, cell_x - 1, cell_y,
                          2, row_height, KUI_TITLE_BLUE);
        if (index == state->document.text_length) break;
        if (state->document.text[index] == '\n') {
            row++;
            column = 0;
            continue;
        }
        if (visible_row >= 0 && visible_row < visible_rows)
            kui_draw_mono_char_styled(
                &client->surface, cell_x, cell_y,
                (unsigned char)state->document.text[index],
                selected ? KUI_WHITE : KUI_BLACK,
                selected ? event->style.selection : KUI_WHITE, 0,
                state->document.font_scale, state->document.bold);
        if (++column >= columns) {
            row++;
            column = 0;
        }
    }
    kui_draw_vscrollbar(
        &client->surface, event->width - KUI_SCROLLBAR_SIZE,
        WRITER_TEXT_TOP,
        event->height - WRITER_TEXT_TOP - WRITER_STATUS_HEIGHT,
        scroll_row, visible_rows,
        writer_max_scroll(state, event->width, event->height) + visible_rows,
        scrollbar);

    kui_fill_rect(&client->surface, 0,
                  event->height - WRITER_STATUS_HEIGHT,
                  event->width, WRITER_STATUS_HEIGHT, 0x00e8edf2U);
    kui_fill_rect(&client->surface, 0,
                  event->height - WRITER_STATUS_HEIGHT,
                  event->width, 1, 0x0099a3adU);
    snprintf(status, sizeof(status), "%d characters  |  %s  |  %s",
             state->document.text_length,
             state->document.bold ? "Bold" : "Regular",
             state->dirty ? "Modified" : "Saved");
    kui_draw_text(&client->surface, 7,
                  event->height - WRITER_STATUS_HEIGHT + 4,
                  state->save_failed ? "Save failed - use an 8.3 filename" :
                                       status,
                  state->save_failed ? 0x00b00000U : 0x0036424eU,
                  0x00e8edf2U, 0);
}

static void writer_move_horizontal(struct writer_state *state, int direction)
{
    if (kui_text_selection_has_range(&state->selection)) {
        int start;
        int end;

        kui_text_selection_range(&state->selection, &start, &end);
        state->cursor = direction < 0 ? start : end;
    } else {
        state->cursor += direction;
        if (state->cursor < 0) state->cursor = 0;
        if (state->cursor > state->document.text_length)
            state->cursor = state->document.text_length;
    }
    kui_text_selection_collapse(&state->selection, state->cursor);
}

static void writer_move_vertical(struct writer_state *state, int width,
                                 int direction)
{
    int row;
    int column;
    int columns = writer_columns(state, width);

    writer_row_column(state->document.text, state->cursor, columns,
                      &row, &column);
    row += direction;
    if (row < 0) row = 0;
    state->cursor = writer_position_at_row(state, columns, row, column);
    kui_text_selection_collapse(&state->selection, state->cursor);
}

static void writer_backspace(struct writer_state *state)
{
    if (writer_delete_selection(state) || state->cursor <= 0) return;
    memmove(state->document.text + state->cursor - 1,
            state->document.text + state->cursor,
            (size_t)(state->document.text_length - state->cursor));
    state->cursor--;
    state->document.text_length--;
    state->document.text[state->document.text_length] = '\0';
    kui_text_selection_collapse(&state->selection, state->cursor);
    state->dirty = 1;
    state->save_failed = 0;
}

static void writer_delete(struct writer_state *state)
{
    if (writer_delete_selection(state) ||
        state->cursor >= state->document.text_length)
        return;
    memmove(state->document.text + state->cursor,
            state->document.text + state->cursor + 1,
            (size_t)(state->document.text_length - state->cursor - 1));
    state->document.text_length--;
    state->document.text[state->document.text_length] = '\0';
    state->dirty = 1;
    state->save_failed = 0;
}

int main(void)
{
    struct writer_state state;
    struct kui_client client;
    struct kui_event event;
    struct kui_file_dialog file_dialog;
    struct kui_confirm_dialog discard_dialog;
    struct kui_scrollbar_state scrollbar;
    char pending_open[WRITER_PATH_MAX];
    int scroll_row = 0;

    memset(&state, 0, sizeof(state));
    memset(&file_dialog, 0, sizeof(file_dialog));
    memset(&discard_dialog, 0, sizeof(discard_dialog));
    memset(&scrollbar, 0, sizeof(scrollbar));
    pending_open[0] = '\0';
    writer_new_at(&state, WRITER_DEFAULT_PATH);
    kui_client_init(&client);

    for (;;) {
        int saved = 0;
        int copied = 0;
        int scrollbar_hit;
        char copied_text[KUI_PROTOCOL_CLIPBOARD_MAX];
        char title[KUI_PROTOCOL_TITLE_MAX];

        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        copied_text[0] = '\0';

        scrollbar_hit = !file_dialog.active && !discard_dialog.active &&
            event.type == KUI_EVENT_POINTER && event.button == 1 &&
            kui_in_rect(event.x, event.y,
                        event.width - KUI_SCROLLBAR_SIZE, WRITER_TEXT_TOP,
                        KUI_SCROLLBAR_SIZE,
                        event.height - WRITER_TEXT_TOP - WRITER_STATUS_HEIGHT);
        if ((!file_dialog.active && !discard_dialog.active) ||
            scrollbar.active_direction != 0)
            scroll_row = kui_vscrollbar_handle(
                &scrollbar, &event,
                event.width - KUI_SCROLLBAR_SIZE, WRITER_TEXT_TOP,
                event.height - WRITER_TEXT_TOP - WRITER_STATUS_HEIGHT,
                scroll_row, writer_visible_rows(&state, event.height),
                writer_max_scroll(&state, event.width, event.height) +
                writer_visible_rows(&state, event.height));
        if (!file_dialog.active && !discard_dialog.active &&
            event.type == KUI_EVENT_POINTER && event.wheel != 0)
            scroll_row = kui_scrollbar_wheel(
                scroll_row, event.wheel,
                writer_visible_rows(&state, event.height),
                writer_max_scroll(&state, event.width, event.height) +
                writer_visible_rows(&state, event.height));

        if (event.type == KUI_EVENT_INIT) {
            const char *path = event.argument[0] != '\0' ? event.argument :
                               WRITER_DEFAULT_PATH;

            if (writer_load(&state, path) < 0) writer_new_at(&state, path);
            scroll_row = 0;
        } else if (discard_dialog.active) {
            int result = kui_confirm_dialog_handle(&discard_dialog, &event);

            if (result == KUI_CONFIRM_DIALOG_ACCEPT) {
                if (writer_load(&state, pending_open) == 0) {
                    scroll_row = 0;
                } else {
                    kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_OPEN,
                                         pending_open);
                    kui_file_dialog_set_error(&file_dialog,
                                              "Could not open this file");
                }
                pending_open[0] = '\0';
                kui_confirm_dialog_close(&discard_dialog);
            } else if (result == KUI_CONFIRM_DIALOG_CANCEL) {
                pending_open[0] = '\0';
            }
        } else if (file_dialog.active) {
            char selected[WRITER_PATH_MAX];
            int result;

            selected[0] = '\0';
            result = kui_file_dialog_handle(&file_dialog, &event, selected,
                                            sizeof(selected));
            if (result == KUI_FILE_DIALOG_ACCEPT) {
                if (file_dialog.mode == KUI_FILE_DIALOG_OPEN) {
                    if (state.dirty) {
                        kui_file_dialog_close(&file_dialog);
                        strncpy(pending_open, selected,
                                sizeof(pending_open) - 1);
                        pending_open[sizeof(pending_open) - 1] = '\0';
                        kui_confirm_dialog_open(
                            &discard_dialog, "Unsaved Changes",
                            "Discard changes and open another document?",
                            "Open", "Cancel");
                    } else if (writer_load(&state, selected) == 0) {
                        kui_file_dialog_close(&file_dialog);
                        scroll_row = 0;
                    } else {
                        kui_file_dialog_set_error(&file_dialog,
                                                  "Could not open this file");
                    }
                } else if (writer_save_to(&state, selected) == 0) {
                    kui_file_dialog_close(&file_dialog);
                    saved = 1;
                } else {
                    kui_file_dialog_set_error(
                        &file_dialog, "Save failed (use an 8.3 filename)");
                }
            }
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1 &&
                   writer_button_at(event.x, event.y) != WRITER_BUTTON_NONE) {
            switch (writer_button_at(event.x, event.y)) {
            case WRITER_BUTTON_OPEN:
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_OPEN,
                                     state.path);
                break;
            case WRITER_BUTTON_SAVE:
                if (writer_save_to(&state, state.path) == 0) saved = 1;
                break;
            case WRITER_BUTTON_SAVE_AS:
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_SAVE,
                                     state.path);
                break;
            case WRITER_BUTTON_BOLD:
                state.document.bold = !state.document.bold;
                state.dirty = 1;
                state.save_failed = 0;
                break;
            case WRITER_BUTTON_SMALLER:
                writer_change_scale(&state, -1);
                scroll_row = 0;
                break;
            case WRITER_BUTTON_LARGER:
                writer_change_scale(&state, 1);
                scroll_row = 0;
                break;
            default:
                break;
            }
        } else if (scrollbar_hit ||
                   (event.type == KUI_EVENT_POINTER && event.wheel != 0)) {
            /* The scrollbar and wheel handlers above own these events. */
        } else if (event.type == KUI_EVENT_POINTER) {
            struct kui_mono_text_layout layout;

            writer_layout(&state, event.width, event.height, scroll_row,
                          &layout);
            kui_text_selection_handle_pointer(
                &state.selection, &event, state.document.text,
                state.document.text_length, &layout, &state.cursor);
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed) {
            if ((event.modifiers & KUI_MOD_CTRL) &&
                (event.modifiers & KUI_MOD_SHIFT) &&
                !event.key_extended && event.key_code == 31) {
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_SAVE,
                                     state.path);
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 31) {
                if (writer_save_to(&state, state.path) == 0) saved = 1;
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 24) {
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_OPEN,
                                     state.path);
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 48) {
                state.document.bold = !state.document.bold;
                state.dirty = 1;
                state.save_failed = 0;
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 13) {
                writer_change_scale(&state, 1);
                scroll_row = 0;
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 12) {
                writer_change_scale(&state, -1);
                scroll_row = 0;
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 46) {
                copied = kui_text_selection_copy(
                    &state.selection, state.document.text,
                    state.document.text_length, copied_text,
                    sizeof(copied_text)) > 0;
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 45) {
                copied = kui_text_selection_copy(
                    &state.selection, state.document.text,
                    state.document.text_length, copied_text,
                    sizeof(copied_text)) > 0;
                if (copied) writer_delete_selection(&state);
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 47) {
                if (event.clipboard_kind == KUI_CLIPBOARD_TEXT &&
                    event.clipboard[0] != '\0')
                    writer_replace(&state, event.clipboard,
                                   (int)strlen(event.clipboard));
            } else if ((event.modifiers & KUI_MOD_CTRL) &&
                       !event.key_extended && event.key_code == 30) {
                kui_text_selection_select_all(
                    &state.selection, state.document.text_length);
                state.cursor = state.document.text_length;
            } else if (event.key_extended && event.key_code == 75) {
                writer_move_horizontal(&state, -1);
            } else if (event.key_extended && event.key_code == 77) {
                writer_move_horizontal(&state, 1);
            } else if (event.key_extended && event.key_code == 72) {
                writer_move_vertical(&state, event.width, -1);
            } else if (event.key_extended && event.key_code == 80) {
                writer_move_vertical(&state, event.width, 1);
            } else if (event.key_extended && event.key_code == 73) {
                int rows = writer_visible_rows(&state, event.height);

                for (int count = 0; count < rows; count++)
                    writer_move_vertical(&state, event.width, -1);
            } else if (event.key_extended && event.key_code == 81) {
                int rows = writer_visible_rows(&state, event.height);

                for (int count = 0; count < rows; count++)
                    writer_move_vertical(&state, event.width, 1);
            } else if (!event.key_extended && event.key_code == 14) {
                writer_backspace(&state);
            } else if (event.key_extended && event.key_code == 83) {
                writer_delete(&state);
            } else if (event.character != 0 &&
                       !(event.modifiers & KUI_MOD_CTRL)) {
                if (event.character == '\t') {
                    writer_replace(&state, "    ", 4);
                } else {
                    writer_replace(&state, &event.character, 1);
                }
            }
        }

        {
            int cursor_row;
            int visible_rows = writer_visible_rows(&state, event.height);

            writer_row_column(state.document.text, state.cursor,
                              writer_columns(&state, event.width),
                              &cursor_row, NULL);
            if (cursor_row < scroll_row) scroll_row = cursor_row;
            if (cursor_row >= scroll_row + visible_rows)
                scroll_row = cursor_row - visible_rows + 1;
        }
        if (scroll_row < 0) scroll_row = 0;
        if (scroll_row > writer_max_scroll(&state, event.width, event.height))
            scroll_row = writer_max_scroll(&state, event.width, event.height);

        snprintf(title, sizeof(title), "Writer - %s%s",
                 path_name(state.path), state.dirty ? " *" : "");
        kui_client_begin(&client, &event, title);
        kui_client_set_constraints(&client, 500, 340, 760, 540);
        writer_render(&client, &event, &state, scroll_row, &scrollbar);
        kui_file_dialog_render(&client.surface, &event, &file_dialog);
        kui_confirm_dialog_render(&client.surface, &event, &discard_dialog);
        {
            uint32_t timer = kui_scrollbar_timer_ms(&scrollbar);
            uint32_t dialog_timer = kui_file_dialog_timer_ms(&file_dialog);

            if (dialog_timer != 0 && (timer == 0 || dialog_timer < timer))
                timer = dialog_timer;
            if (timer != 0) kui_client_set_timer(&client, timer);
        }
        if (copied)
            kui_client_set_action(&client, KUI_ACTION_COPY_TEXT, copied_text);
        else if (saved)
            kui_client_set_action(&client, KUI_ACTION_FILE_SAVED, state.path);
        if (kui_client_send(&client) < 0) break;
    }
    kui_client_free(&client);
    writer_document_destroy(&state.document);
    return 0;
}
