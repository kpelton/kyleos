#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extras/ksheet/ksheet_model.h"
#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"
#include "extras/kui/kui_file_dialog.h"
#include "extras/kui/kui_menu.h"
#include "extras/kui/kui_text_selection.h"

#define KSHEET_PATH_MAX KUI_FILE_DIALOG_PATH_MAX
#define KSHEET_CSV_MAX (1024 * 1024)
#define KSHEET_FORMULA_TOP KUI_MENU_BAR_HEIGHT
#define KSHEET_FORMULA_HEIGHT 38
#define KSHEET_COLUMN_HEADER_TOP (KSHEET_FORMULA_TOP + KSHEET_FORMULA_HEIGHT)
#define KSHEET_COLUMN_HEADER_HEIGHT 22
#define KSHEET_GRID_TOP (KSHEET_COLUMN_HEADER_TOP + KSHEET_COLUMN_HEADER_HEIGHT)
#define KSHEET_ROW_HEADER_WIDTH 44
#define KSHEET_CELL_WIDTH 96
#define KSHEET_CELL_HEIGHT 22
#define KSHEET_STATUS_HEIGHT 23
#define KSHEET_FORMULA_FIELD_X 96

enum ksheet_menu_action {
    KSHEET_MENU_NEW = 1,
    KSHEET_MENU_OPEN,
    KSHEET_MENU_SAVE,
    KSHEET_MENU_SAVE_AS,
    KSHEET_MENU_CUT,
    KSHEET_MENU_COPY,
    KSHEET_MENU_PASTE,
    KSHEET_MENU_DELETE
};

enum ksheet_edit_item {
    KSHEET_EDIT_CUT,
    KSHEET_EDIT_COPY,
    KSHEET_EDIT_PASTE,
    KSHEET_EDIT_DELETE,
    KSHEET_EDIT_ITEM_COUNT
};

struct ksheet_state {
    struct ksheet sheet;
    char path[KSHEET_PATH_MAX];
    char status[96];
    int selected_row;
    int selected_column;
    int anchor_row;
    int anchor_column;
    int cell_dragging;
    int scroll_row;
    int scroll_column;
    int dirty;
    int editing;
    char edit_text[KSHEET_CELL_TEXT_MAX];
    int edit_length;
    int edit_cursor;
    struct kui_text_selection edit_selection;
};

static char csv_buffer[KSHEET_CSV_MAX];

static const struct kui_menu_item file_items[] = {
    { "New", "Ctrl+N", KSHEET_MENU_NEW, 1 },
    { "Open...", "Ctrl+O", KSHEET_MENU_OPEN, 1 },
    { "Save", "Ctrl+S", KSHEET_MENU_SAVE, 1 },
    { "Save As...", "Ctrl+Shift+S", KSHEET_MENU_SAVE_AS, 1 }
};

static struct kui_menu_item edit_items[] = {
    { "Cut", "Ctrl+X", KSHEET_MENU_CUT, 0 },
    { "Copy", "Ctrl+C", KSHEET_MENU_COPY, 0 },
    { "Paste", "Ctrl+V", KSHEET_MENU_PASTE, 0 },
    { "Delete", "Del", KSHEET_MENU_DELETE, 0 }
};

static const struct kui_menu menus[] = {
    { "File", file_items, 4 },
    { "Edit", edit_items, KSHEET_EDIT_ITEM_COUNT }
};

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static void copy_text(char *destination, size_t capacity, const char *source)
{
    size_t length;

    if (capacity == 0) return;
    if (source == NULL) source = "";
    length = strlen(source);
    if (length >= capacity) length = capacity - 1;
    memmove(destination, source, length);
    destination[length] = '\0';
}

static int visible_columns(int width)
{
    int count = (width - KSHEET_ROW_HEADER_WIDTH - KUI_SCROLLBAR_SIZE) /
                KSHEET_CELL_WIDTH;
    return count > 0 ? count : 1;
}

static int visible_rows(int height)
{
    int count = (height - KSHEET_GRID_TOP - KSHEET_STATUS_HEIGHT -
                 KUI_SCROLLBAR_SIZE) / KSHEET_CELL_HEIGHT;
    return count > 0 ? count : 1;
}

static void ensure_selection_visible(struct ksheet_state *state,
                                     int width, int height)
{
    int columns = visible_columns(width);
    int rows = visible_rows(height);
    int sheet_columns;
    int sheet_rows;

    ksheet_ensure_size(&state->sheet, state->selected_row + 1,
                       state->selected_column + 1);
    sheet_columns = ksheet_column_count(&state->sheet);
    sheet_rows = ksheet_row_count(&state->sheet);

    if (state->selected_column < state->scroll_column)
        state->scroll_column = state->selected_column;
    if (state->selected_column >= state->scroll_column + columns)
        state->scroll_column = state->selected_column - columns + 1;
    if (state->selected_row < state->scroll_row)
        state->scroll_row = state->selected_row;
    if (state->selected_row >= state->scroll_row + rows)
        state->scroll_row = state->selected_row - rows + 1;
    if (state->scroll_column < 0) state->scroll_column = 0;
    if (state->scroll_row < 0) state->scroll_row = 0;
    if (state->scroll_column > sheet_columns - columns)
        state->scroll_column = sheet_columns - columns;
    if (state->scroll_row > sheet_rows - rows)
        state->scroll_row = sheet_rows - rows;
    if (state->scroll_column < 0) state->scroll_column = 0;
    if (state->scroll_row < 0) state->scroll_row = 0;
}

static int cell_at(const struct ksheet_state *state,
                   int width, int height, int x, int y,
                   int *row, int *column)
{
    int local_row;
    int local_column;

    if (x < KSHEET_ROW_HEADER_WIDTH || y < KSHEET_GRID_TOP ||
        x >= width - KUI_SCROLLBAR_SIZE ||
        y >= height - KSHEET_STATUS_HEIGHT - KUI_SCROLLBAR_SIZE)
        return 0;
    local_column = state->scroll_column +
                   (x - KSHEET_ROW_HEADER_WIDTH) / KSHEET_CELL_WIDTH;
    local_row = state->scroll_row +
                (y - KSHEET_GRID_TOP) / KSHEET_CELL_HEIGHT;
    if (local_column < 0 ||
        local_column >= ksheet_column_count(&state->sheet) ||
        local_row < 0 || local_row >= ksheet_row_count(&state->sheet))
        return 0;
    *row = local_row;
    *column = local_column;
    return 1;
}

static int cell_at_clamped(const struct ksheet_state *state,
                           int width, int height, int x, int y,
                           int *row, int *column)
{
    int grid_bottom = height - KSHEET_STATUS_HEIGHT -
                      KUI_SCROLLBAR_SIZE - 1;

    if (width <= KSHEET_ROW_HEADER_WIDTH || grid_bottom < KSHEET_GRID_TOP)
        return 0;
    if (x < KSHEET_ROW_HEADER_WIDTH) x = KSHEET_ROW_HEADER_WIDTH;
    if (x >= width - KUI_SCROLLBAR_SIZE)
        x = width - KUI_SCROLLBAR_SIZE - 1;
    if (y < KSHEET_GRID_TOP) y = KSHEET_GRID_TOP;
    if (y > grid_bottom) y = grid_bottom;
    return cell_at(state, width, height, x, y, row, column);
}

static void selection_range(const struct ksheet_state *state,
                            int *first_row, int *first_column,
                            int *last_row, int *last_column)
{
    int row0 = state->anchor_row;
    int row1 = state->selected_row;
    int column0 = state->anchor_column;
    int column1 = state->selected_column;

    if (row0 > row1) { int swap = row0; row0 = row1; row1 = swap; }
    if (column0 > column1) {
        int swap = column0; column0 = column1; column1 = swap;
    }
    if (first_row != NULL) *first_row = row0;
    if (first_column != NULL) *first_column = column0;
    if (last_row != NULL) *last_row = row1;
    if (last_column != NULL) *last_column = column1;
}

static int cell_is_selected(const struct ksheet_state *state,
                            int row, int column)
{
    int row0, column0, row1, column1;

    selection_range(state, &row0, &column0, &row1, &column1);
    return row >= row0 && row <= row1 &&
           column >= column0 && column <= column1;
}

static void select_one_cell(struct ksheet_state *state, int row, int column)
{
    state->selected_row = row;
    state->selected_column = column;
    state->anchor_row = row;
    state->anchor_column = column;
}

static int selection_has_content(const struct ksheet_state *state)
{
    int row0, column0, row1, column1;

    selection_range(state, &row0, &column0, &row1, &column1);
    for (int row = row0; row <= row1; row++)
        for (int column = column0; column <= column1; column++)
            if (ksheet_cell_input(&state->sheet, row, column)[0] != '\0')
                return 1;
    return 0;
}

static int formula_field_width(int width)
{
    int field_width = width - KSHEET_FORMULA_FIELD_X - 6;
    return field_width > 30 ? field_width : 30;
}

static void formula_text_layout(int width,
                                struct kui_mono_text_layout *layout)
{
    layout->left = KSHEET_FORMULA_FIELD_X + 5;
    layout->top = KSHEET_FORMULA_TOP + 10;
    layout->columns = (formula_field_width(width) - 10) / 8;
    if (layout->columns < 1) layout->columns = 1;
    layout->visible_rows = 1;
    layout->cell_width = 8;
    layout->row_height = 16;
    layout->scroll_row = 0;
}

static int pointer_in_formula(int width, int x, int y)
{
    return kui_in_rect(x, y, KSHEET_FORMULA_FIELD_X,
                       KSHEET_FORMULA_TOP + 5,
                       formula_field_width(width), 27);
}

static void begin_edit(struct ksheet_state *state, int replace)
{
    const char *input;

    if (state->editing && !replace) return;
    state->anchor_row = state->selected_row;
    state->anchor_column = state->selected_column;
    state->cell_dragging = 0;
    input = ksheet_cell_input(&state->sheet, state->selected_row,
                              state->selected_column);
    if (replace) state->edit_text[0] = '\0';
    else copy_text(state->edit_text, sizeof(state->edit_text), input);
    state->edit_length = (int)strlen(state->edit_text);
    state->edit_cursor = state->edit_length;
    kui_text_selection_collapse(&state->edit_selection, state->edit_cursor);
    state->editing = 1;
    copy_text(state->status, sizeof(state->status),
              "Editing cell; Enter accepts, Esc cancels");
}

static int commit_edit(struct ksheet_state *state)
{
    const char *old_input;
    int changed;
    int failed = 0;

    if (!state->editing) return 0;
    old_input = ksheet_cell_input(&state->sheet, state->selected_row,
                                  state->selected_column);
    changed = strcmp(old_input, state->edit_text) != 0;
    if (changed) {
        if (ksheet_set_cell(&state->sheet, state->selected_row,
                            state->selected_column,
                            state->edit_text) < 0) {
            changed = 0;
            failed = 1;
            copy_text(state->status, sizeof(state->status),
                      "Sheet size or memory limit reached");
        } else {
            state->dirty = 1;
        }
    }
    state->editing = 0;
    kui_text_selection_collapse(&state->edit_selection, 0);
    if (!failed)
        copy_text(state->status, sizeof(state->status),
                  changed ? "Cell updated" : "Ready");
    return changed;
}

static void cancel_edit(struct ksheet_state *state)
{
    state->editing = 0;
    kui_text_selection_collapse(&state->edit_selection, 0);
    copy_text(state->status, sizeof(state->status), "Edit cancelled");
}

static int edit_replace(struct ksheet_state *state,
                        const char *text, int length)
{
    return kui_text_selection_replace(
        &state->edit_selection, state->edit_text, &state->edit_length,
        sizeof(state->edit_text), &state->edit_cursor, text, length);
}

static int edit_delete_selection(struct ksheet_state *state)
{
    return kui_text_selection_delete(
        &state->edit_selection, state->edit_text, &state->edit_length,
        &state->edit_cursor);
}

static void edit_backspace(struct ksheet_state *state)
{
    if (edit_delete_selection(state)) return;
    if (state->edit_cursor <= 0) return;
    kui_text_selection_set(&state->edit_selection,
                           state->edit_cursor - 1, state->edit_cursor);
    edit_delete_selection(state);
}

static void edit_delete_forward(struct ksheet_state *state)
{
    if (edit_delete_selection(state)) return;
    if (state->edit_cursor >= state->edit_length) return;
    kui_text_selection_set(&state->edit_selection,
                           state->edit_cursor, state->edit_cursor + 1);
    edit_delete_selection(state);
}

static void move_selection(struct ksheet_state *state, int dx, int dy,
                           int width, int height, int extend)
{
    int sheet_rows;
    int sheet_columns;

    commit_edit(state);
    state->selected_column += dx;
    state->selected_row += dy;
    if (state->selected_column < 0) state->selected_column = 0;
    if (state->selected_row < 0) state->selected_row = 0;
    if (state->selected_column >= KSHEET_MAX_COLS)
        state->selected_column = KSHEET_MAX_COLS - 1;
    if (state->selected_row >= KSHEET_MAX_ROWS)
        state->selected_row = KSHEET_MAX_ROWS - 1;
    sheet_rows = ksheet_row_count(&state->sheet);
    sheet_columns = ksheet_column_count(&state->sheet);
    if (state->selected_row >= sheet_rows)
        ksheet_ensure_size(&state->sheet,
                           state->selected_row + KSHEET_DEFAULT_ROWS <
                           KSHEET_MAX_ROWS ?
                           state->selected_row + KSHEET_DEFAULT_ROWS :
                           KSHEET_MAX_ROWS,
                           sheet_columns);
    if (state->selected_column >= sheet_columns)
        ksheet_ensure_size(&state->sheet,
                           ksheet_row_count(&state->sheet),
                           state->selected_column + KSHEET_DEFAULT_COLS <
                           KSHEET_MAX_COLS ?
                           state->selected_column + KSHEET_DEFAULT_COLS :
                           KSHEET_MAX_COLS);
    if (!extend) {
        state->anchor_row = state->selected_row;
        state->anchor_column = state->selected_column;
    }
    ensure_selection_visible(state, width, height);
}

static int read_csv_file(struct ksheet_state *state, const char *path)
{
    int fd = open(path, O_RDONLY);
    int total = 0;

    if (fd < 0) return -1;
    while (total < (int)sizeof(csv_buffer) - 1) {
        int count = read(fd, csv_buffer + total,
                         (int)sizeof(csv_buffer) - 1 - total);
        if (count < 0) { close(fd); return -1; }
        if (count == 0) break;
        total += count;
    }
    if (total == (int)sizeof(csv_buffer) - 1) {
        char extra;
        int count = read(fd, &extra, 1);

        if (count != 0) { close(fd); return -1; }
    }
    close(fd);
    csv_buffer[total] = '\0';
    if (ksheet_import_csv(&state->sheet, csv_buffer, (size_t)total) < 0)
        return -1;
    copy_text(state->path, sizeof(state->path), path);
    state->selected_row = 0;
    state->selected_column = 0;
    state->anchor_row = 0;
    state->anchor_column = 0;
    state->cell_dragging = 0;
    state->scroll_row = 0;
    state->scroll_column = 0;
    state->dirty = 0;
    state->editing = 0;
    copy_text(state->status, sizeof(state->status), "CSV opened");
    return 0;
}

static int write_csv_file(struct ksheet_state *state, const char *path)
{
    size_t length;
    size_t total = 0;
    int fd;

    if (ksheet_export_csv(&state->sheet, csv_buffer, sizeof(csv_buffer),
                          &length) < 0)
        return -1;
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    while (total < length) {
        int count = write(fd, csv_buffer + total, (int)(length - total));
        if (count <= 0) break;
        total += (size_t)count;
    }
    close(fd);
    if (total != length) return -1;
    copy_text(state->path, sizeof(state->path), path);
    state->dirty = 0;
    copy_text(state->status, sizeof(state->status), "CSV saved");
    return 0;
}

static void new_sheet(struct ksheet_state *state)
{
    ksheet_clear(&state->sheet);
    copy_text(state->path, sizeof(state->path), "/SHEET.CSV");
    state->selected_row = 0;
    state->selected_column = 0;
    state->anchor_row = 0;
    state->anchor_column = 0;
    state->cell_dragging = 0;
    state->scroll_row = 0;
    state->scroll_column = 0;
    state->dirty = 0;
    state->editing = 0;
    copy_text(state->status, sizeof(state->status), "New spreadsheet");
}

static void update_edit_items(const struct ksheet_state *state,
                              const struct kui_event *event)
{
    int can_copy;

    if (state->editing)
        can_copy = kui_text_selection_has_range(&state->edit_selection);
    else
        can_copy = selection_has_content(state);
    edit_items[KSHEET_EDIT_CUT].enabled = can_copy;
    edit_items[KSHEET_EDIT_COPY].enabled = can_copy;
    edit_items[KSHEET_EDIT_DELETE].enabled = can_copy;
    edit_items[KSHEET_EDIT_PASTE].enabled =
        event->clipboard_kind == KUI_CLIPBOARD_TEXT &&
        event->clipboard[0] != '\0';
}

static int copy_selected_cells(const struct ksheet_state *state,
                               char *output, size_t capacity)
{
    int row0, column0, row1, column1;
    size_t length = 0;

    selection_range(state, &row0, &column0, &row1, &column1);
    for (int row = row0; row <= row1; row++) {
        for (int column = column0; column <= column1; column++) {
            const char *input = ksheet_cell_input(&state->sheet, row, column);
            size_t input_length = strlen(input);

            if (length + input_length + 1 > capacity) return 0;
            memcpy(output + length, input, input_length);
            length += input_length;
            if (column != column1) output[length++] = '\t';
        }
        if (row != row1) {
            if (length + 2 > capacity) return 0;
            output[length++] = '\n';
        }
    }
    output[length] = '\0';
    return 1;
}

static int clear_selected_cells(struct ksheet_state *state)
{
    int row0, column0, row1, column1;
    int changed = 0;

    selection_range(state, &row0, &column0, &row1, &column1);
    for (int row = row0; row <= row1; row++) {
        for (int column = column0; column <= column1; column++) {
            if (ksheet_cell_input(&state->sheet, row, column)[0] == '\0')
                continue;
            ksheet_set_cell(&state->sheet, row, column, "");
            changed = 1;
        }
    }
    return changed;
}

static int paste_cells(struct ksheet_state *state, const char *text)
{
    int start_row, start_column;
    int row, column;
    int last_row, last_column;
    const char *field;
    const char *cursor;

    selection_range(state, &start_row, &start_column, NULL, NULL);
    row = last_row = start_row;
    column = last_column = start_column;
    field = text;
    for (cursor = text;; cursor++) {
        if (*cursor == '\t' || *cursor == '\n' || *cursor == '\0') {
            char value[KSHEET_CELL_TEXT_MAX];
            size_t length = (size_t)(cursor - field);
            size_t clean = 0;

            if (!(*cursor == '\0' && cursor == field && cursor != text &&
                  cursor[-1] == '\n')) {
                for (size_t index = 0;
                     index < length && clean + 1 < sizeof(value); index++)
                    if (field[index] != '\r') value[clean++] = field[index];
                value[clean] = '\0';
                if (row < KSHEET_MAX_ROWS && column < KSHEET_MAX_COLS &&
                    ksheet_ensure_size(&state->sheet, row + 1,
                                       column + 1) == 0 &&
                    ksheet_set_cell(&state->sheet, row, column, value) == 0) {
                    last_row = row;
                    last_column = column;
                }
            }
            if (*cursor == '\0') break;
            if (*cursor == '\t') {
                column++;
            } else {
                row++;
                column = start_column;
            }
            field = cursor + 1;
        }
    }
    state->anchor_row = start_row;
    state->anchor_column = start_column;
    state->selected_row = last_row;
    state->selected_column = last_column;
    return 1;
}

static int perform_edit_action(struct ksheet_state *state, int action,
                               const struct kui_event *event,
                               char copied[KUI_PROTOCOL_CLIPBOARD_MAX],
                               int *copied_valid)
{
    int changed = 0;

    *copied_valid = 0;
    if (state->editing) {
        if (action == KSHEET_MENU_COPY || action == KSHEET_MENU_CUT) {
            *copied_valid = kui_text_selection_copy(
                &state->edit_selection, state->edit_text,
                state->edit_length, copied,
                KUI_PROTOCOL_CLIPBOARD_MAX) > 0;
            if (action == KSHEET_MENU_CUT && *copied_valid)
                changed = edit_delete_selection(state);
        } else if (action == KSHEET_MENU_PASTE &&
                   event->clipboard_kind == KUI_CLIPBOARD_TEXT) {
            changed = edit_replace(state, event->clipboard,
                                   (int)strlen(event->clipboard));
        } else if (action == KSHEET_MENU_DELETE) {
            changed = edit_delete_selection(state);
        }
        return changed;
    }

    if (action == KSHEET_MENU_COPY || action == KSHEET_MENU_CUT) {
        if (selection_has_content(state)) {
            *copied_valid = copy_selected_cells(
                state, copied, KUI_PROTOCOL_CLIPBOARD_MAX);
            if (!*copied_valid) {
                copy_text(state->status, sizeof(state->status),
                          "Selection is too large to copy");
            } else if (action == KSHEET_MENU_CUT) {
                changed = clear_selected_cells(state);
            }
        }
    } else if (action == KSHEET_MENU_PASTE &&
               event->clipboard_kind == KUI_CLIPBOARD_TEXT) {
        changed = paste_cells(state, event->clipboard);
    } else if (action == KSHEET_MENU_DELETE) {
        changed = clear_selected_cells(state);
    }
    if (changed) {
        state->dirty = 1;
        copy_text(state->status, sizeof(state->status), "Cells updated");
    }
    return changed;
}

static void clip_mono(char *output, size_t capacity,
                      const char *input, int maximum_characters)
{
    int count = (int)strlen(input);

    if (count > maximum_characters) count = maximum_characters;
    if (count >= (int)capacity) count = (int)capacity - 1;
    if (count < 0) count = 0;
    memcpy(output, input, (size_t)count);
    output[count] = '\0';
}

static int numeric_input(const char *input, const char *formatted)
{
    char *end;

    if (input[0] == '=') return formatted[0] != '#';
    if (input[0] == '\0') return 0;
    strtod(input, &end);
    while (*end == ' ' || *end == '\t') end++;
    return *end == '\0';
}

static void render_formula_bar(struct kui_surface *surface,
                               const struct kui_event *event,
                               const struct ksheet_state *state)
{
    char cell_name[16];
    char shown[KSHEET_CELL_TEXT_MAX];
    char clipped[KSHEET_CELL_TEXT_MAX];
    int field_width = formula_field_width(event->width);
    int maximum = (field_width - 10) / 8;
    int start = 0;

    ksheet_cell_name(state->selected_row, state->selected_column,
                     cell_name, sizeof(cell_name));
    copy_text(shown, sizeof(shown), state->editing ? state->edit_text :
              ksheet_cell_input(&state->sheet, state->selected_row,
                                state->selected_column));
    kui_fill_rect(surface, 0, KSHEET_FORMULA_TOP, event->width,
                  KSHEET_FORMULA_HEIGHT, KUI_FACE);
    kui_fill_rect(surface, 5, KSHEET_FORMULA_TOP + 5, 53, 27, KUI_WHITE);
    kui_outline_rect(surface, 5, KSHEET_FORMULA_TOP + 5, 53, 27, KUI_SHADOW);
    kui_draw_mono_text(surface, 11, KSHEET_FORMULA_TOP + 11,
                       cell_name, KUI_BLACK, KUI_WHITE, 0);
    kui_draw_text(surface, 68, KSHEET_FORMULA_TOP + 10,
                  "fx", KUI_TITLE_BLUE, KUI_FACE, 0);
    kui_fill_rect(surface, KSHEET_FORMULA_FIELD_X,
                  KSHEET_FORMULA_TOP + 5, field_width, 27, KUI_WHITE);
    kui_outline_rect(surface, KSHEET_FORMULA_FIELD_X,
                     KSHEET_FORMULA_TOP + 5, field_width, 27, KUI_SHADOW);

    if (state->editing && state->edit_cursor >= maximum)
        start = state->edit_cursor - maximum + 1;
    clip_mono(clipped, sizeof(clipped), shown + start, maximum);
    if (state->editing) {
        for (int index = 0; clipped[index] != '\0'; index++) {
            int absolute = start + index;
            int selected = kui_text_selection_contains(
                &state->edit_selection, absolute);
            int x = KSHEET_FORMULA_FIELD_X + 5 + index * 8;

            if (selected)
                kui_fill_rect(surface, x, KSHEET_FORMULA_TOP + 10,
                              8, 16, event->style.selection);
            kui_draw_mono_char(surface, x, KSHEET_FORMULA_TOP + 10,
                               (unsigned char)clipped[index],
                               selected ? KUI_WHITE : KUI_BLACK,
                               selected ? event->style.selection : KUI_WHITE,
                               0);
        }
        if (!kui_text_selection_has_range(&state->edit_selection)) {
            int cursor_x = KSHEET_FORMULA_FIELD_X + 5 +
                           (state->edit_cursor - start) * 8;
            kui_fill_rect(surface, cursor_x, KSHEET_FORMULA_TOP + 9,
                          2, 18, KUI_TITLE_BLUE);
        }
    } else {
        kui_draw_mono_text(surface, KSHEET_FORMULA_FIELD_X + 5,
                           KSHEET_FORMULA_TOP + 10,
                           clipped, KUI_BLACK, KUI_WHITE, 0);
    }
}

static void render_sheet(struct kui_client *client,
                         const struct kui_event *event,
                         const struct ksheet_state *state,
                         const struct kui_scrollbar_state *vertical_scrollbar,
                         const struct kui_scrollbar_state *horizontal_scrollbar)
{
    int columns = visible_columns(event->width);
    int rows = visible_rows(event->height);
    int grid_bottom = KSHEET_GRID_TOP + rows * KSHEET_CELL_HEIGHT;
    int selected_row0, selected_column0, selected_row1, selected_column1;

    selection_range(state, &selected_row0, &selected_column0,
                    &selected_row1, &selected_column1);

    kui_fill_rect(&client->surface, 0, 0,
                  event->width, event->height, KUI_FACE);
    render_formula_bar(&client->surface, event, state);
    kui_fill_rect(&client->surface, 0, KSHEET_COLUMN_HEADER_TOP,
                  event->width, KSHEET_COLUMN_HEADER_HEIGHT, KUI_LIGHT);
    kui_fill_rect(&client->surface, 0, KSHEET_GRID_TOP,
                  KSHEET_ROW_HEADER_WIDTH,
                  rows * KSHEET_CELL_HEIGHT, KUI_LIGHT);

    for (int visible_column = 0; visible_column < columns; visible_column++) {
        int column = state->scroll_column + visible_column;
        int x = KSHEET_ROW_HEADER_WIDTH + visible_column * KSHEET_CELL_WIDTH;
        char label[16];
        int selected = column >= selected_column0 &&
                       column <= selected_column1;

        if (column >= ksheet_column_count(&state->sheet)) break;
        ksheet_column_name(column, label, sizeof(label));
        kui_fill_rect(&client->surface, x, KSHEET_COLUMN_HEADER_TOP,
                      KSHEET_CELL_WIDTH, KSHEET_COLUMN_HEADER_HEIGHT,
                      selected ?
                      event->style.selection : KUI_LIGHT);
        kui_outline_rect(&client->surface, x, KSHEET_COLUMN_HEADER_TOP,
                         KSHEET_CELL_WIDTH + 1,
                         KSHEET_COLUMN_HEADER_HEIGHT + 1, KUI_SHADOW);
        kui_draw_text(&client->surface,
                      x + (KSHEET_CELL_WIDTH - kui_text_width(label)) / 2,
                      KSHEET_COLUMN_HEADER_TOP + 3, label,
                      selected ? KUI_WHITE : KUI_BLACK,
                      selected ? event->style.selection : KUI_LIGHT, 0);
    }

    for (int visible_row = 0; visible_row < rows; visible_row++) {
        int row = state->scroll_row + visible_row;
        int y = KSHEET_GRID_TOP + visible_row * KSHEET_CELL_HEIGHT;
        char label[16];
        int row_selected = row >= selected_row0 && row <= selected_row1;

        if (row >= ksheet_row_count(&state->sheet)) break;
        snprintf(label, sizeof(label), "%d", row + 1);
        kui_fill_rect(&client->surface, 0, y, KSHEET_ROW_HEADER_WIDTH,
                      KSHEET_CELL_HEIGHT,
                      row_selected ?
                      event->style.selection : KUI_LIGHT);
        kui_outline_rect(&client->surface, 0, y,
                         KSHEET_ROW_HEADER_WIDTH + 1,
                         KSHEET_CELL_HEIGHT + 1, KUI_SHADOW);
        kui_draw_text(&client->surface,
                      KSHEET_ROW_HEADER_WIDTH - 6 - kui_text_width(label),
                      y + 3, label,
                      row_selected ? KUI_WHITE : KUI_BLACK,
                      row_selected ? event->style.selection : KUI_LIGHT, 0);

        for (int visible_column = 0;
             visible_column < columns; visible_column++) {
            int column = state->scroll_column + visible_column;
            int x = KSHEET_ROW_HEADER_WIDTH +
                    visible_column * KSHEET_CELL_WIDTH;
            int selected = cell_is_selected(state, row, column);
            const char *input;
            char formatted[KSHEET_CELL_TEXT_MAX];
            char clipped[KSHEET_CELL_TEXT_MAX];
            int text_x;
            uint32_t foreground;

            if (column >= ksheet_column_count(&state->sheet)) break;
            input = ksheet_cell_input(&state->sheet, row, column);
            ksheet_format_cell(&state->sheet, row, column,
                               formatted, sizeof(formatted));
            clip_mono(clipped, sizeof(clipped), formatted,
                      (KSHEET_CELL_WIDTH - 10) / 8);
            kui_fill_rect(&client->surface, x, y,
                          KSHEET_CELL_WIDTH, KSHEET_CELL_HEIGHT,
                          selected ? event->style.selection : KUI_WHITE);
            kui_outline_rect(&client->surface, x, y,
                             KSHEET_CELL_WIDTH + 1,
                             KSHEET_CELL_HEIGHT + 1, 0x00c3c8ccU);
            text_x = numeric_input(input, formatted) ?
                x + KSHEET_CELL_WIDTH - 5 - kui_mono_text_width(clipped) :
                x + 5;
            foreground = selected ? KUI_WHITE :
                         formatted[0] == '#' ? 0x00b00020U : KUI_BLACK;
            kui_draw_mono_text(&client->surface, text_x, y + 3,
                               clipped, foreground,
                               selected ? event->style.selection : KUI_WHITE,
                               0);
            if (row == state->selected_row &&
                column == state->selected_column)
                kui_outline_rect(&client->surface, x + 1, y + 1,
                                 KSHEET_CELL_WIDTH - 1,
                                 KSHEET_CELL_HEIGHT - 1, KUI_WHITE);
        }
    }
    if (grid_bottom < event->height - KSHEET_STATUS_HEIGHT)
        kui_fill_rect(&client->surface, 0, grid_bottom, event->width,
                      event->height - KSHEET_STATUS_HEIGHT - grid_bottom,
                      KUI_WHITE);
    kui_draw_vscrollbar(
        &client->surface, event->width - KUI_SCROLLBAR_SIZE,
        KSHEET_GRID_TOP,
        event->height - KSHEET_STATUS_HEIGHT - KUI_SCROLLBAR_SIZE -
        KSHEET_GRID_TOP,
        state->scroll_row, rows, ksheet_row_count(&state->sheet),
        vertical_scrollbar);
    kui_draw_hscrollbar(
        &client->surface, KSHEET_ROW_HEADER_WIDTH,
        event->height - KSHEET_STATUS_HEIGHT - KUI_SCROLLBAR_SIZE,
        event->width - KSHEET_ROW_HEADER_WIDTH - KUI_SCROLLBAR_SIZE,
        state->scroll_column, columns,
        ksheet_column_count(&state->sheet), horizontal_scrollbar);
    kui_fill_rect(&client->surface,
                  event->width - KUI_SCROLLBAR_SIZE,
                  event->height - KSHEET_STATUS_HEIGHT -
                  KUI_SCROLLBAR_SIZE,
                  KUI_SCROLLBAR_SIZE, KUI_SCROLLBAR_SIZE, KUI_FACE);
    kui_outline_rect(&client->surface,
                     event->width - KUI_SCROLLBAR_SIZE,
                     event->height - KSHEET_STATUS_HEIGHT -
                     KUI_SCROLLBAR_SIZE,
                     KUI_SCROLLBAR_SIZE, KUI_SCROLLBAR_SIZE, KUI_SHADOW);
    {
        int status_y = event->height - KSHEET_STATUS_HEIGHT;
        char first_cell[16];
        char last_cell[16];
        char selection[80];
        char message[192];

        ksheet_cell_name(selected_row0, selected_column0,
                         first_cell, sizeof(first_cell));
        ksheet_cell_name(selected_row1, selected_column1,
                         last_cell, sizeof(last_cell));
        if (selected_row0 == selected_row1 &&
            selected_column0 == selected_column1) {
            copy_text(selection, sizeof(selection), first_cell);
        } else {
            snprintf(selection, sizeof(selection), "%s:%s (%dx%d)",
                     first_cell, last_cell,
                     selected_column1 - selected_column0 + 1,
                     selected_row1 - selected_row0 + 1);
        }
        snprintf(message, sizeof(message), "%s   %s",
                 selection, state->status);
        kui_fill_rect(&client->surface, 0, status_y,
                      event->width, KSHEET_STATUS_HEIGHT, KUI_FACE);
        kui_fill_rect(&client->surface, 0, status_y,
                      event->width, 1, KUI_SHADOW);
        kui_draw_text(&client->surface, 7, status_y + 4,
                      message, KUI_BLACK, KUI_FACE, 0);
        if (event->width > 650)
            kui_draw_text(&client->surface, event->width - 310,
                          status_y + 4,
                          "Formulas: =A1+B1  SUM(A1:A10)",
                          KUI_SHADOW, KUI_FACE, 0);
    }
}

static int ksheet_client_main(void)
{
    static struct ksheet_state state;
    struct kui_file_dialog file_dialog;
    struct kui_menu_state menu_state;
    struct kui_context_menu_state context_menu;
    struct kui_scrollbar_state vertical_scrollbar;
    struct kui_scrollbar_state horizontal_scrollbar;
    struct kui_client client;
    struct kui_event event;

    memset(&state, 0, sizeof(state));
    memset(&file_dialog, 0, sizeof(file_dialog));
    memset(&vertical_scrollbar, 0, sizeof(vertical_scrollbar));
    memset(&horizontal_scrollbar, 0, sizeof(horizontal_scrollbar));
    ksheet_init(&state.sheet);
    kui_menu_init(&menu_state);
    kui_context_menu_init(&context_menu);
    kui_text_selection_init(&state.edit_selection);
    kui_client_init(&client);
    new_sheet(&state);

    for (;;) {
        int menu_action = 0;
        int menu_consumed = 0;
        int vertical_scrollbar_hit;
        int horizontal_scrollbar_hit;
        int context_consumed = 0;
        int edit_action = 0;
        int copied_valid = 0;
        int saved = 0;
        char copied[KUI_PROTOCOL_CLIPBOARD_MAX];
        char title[KUI_PROTOCOL_TITLE_MAX];

        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        vertical_scrollbar_hit = !file_dialog.active &&
            event.type == KUI_EVENT_POINTER && event.button == 1 &&
            kui_in_rect(event.x, event.y,
                        event.width - KUI_SCROLLBAR_SIZE,
                        KSHEET_GRID_TOP, KUI_SCROLLBAR_SIZE,
                        event.height - KSHEET_STATUS_HEIGHT -
                        KUI_SCROLLBAR_SIZE - KSHEET_GRID_TOP);
        horizontal_scrollbar_hit = !file_dialog.active &&
            event.type == KUI_EVENT_POINTER && event.button == 1 &&
            kui_in_rect(event.x, event.y, KSHEET_ROW_HEADER_WIDTH,
                        event.height - KSHEET_STATUS_HEIGHT -
                        KUI_SCROLLBAR_SIZE,
                        event.width - KSHEET_ROW_HEADER_WIDTH -
                        KUI_SCROLLBAR_SIZE, KUI_SCROLLBAR_SIZE);
        if (!file_dialog.active || vertical_scrollbar.active_direction != 0)
            state.scroll_row = kui_vscrollbar_handle(
                &vertical_scrollbar, &event,
                event.width - KUI_SCROLLBAR_SIZE, KSHEET_GRID_TOP,
                event.height - KSHEET_STATUS_HEIGHT -
                KUI_SCROLLBAR_SIZE - KSHEET_GRID_TOP,
                state.scroll_row, visible_rows(event.height),
                ksheet_row_count(&state.sheet));
        if (!file_dialog.active || horizontal_scrollbar.active_direction != 0)
            state.scroll_column = kui_hscrollbar_handle(
                &horizontal_scrollbar, &event, KSHEET_ROW_HEADER_WIDTH,
                event.height - KSHEET_STATUS_HEIGHT - KUI_SCROLLBAR_SIZE,
                event.width - KSHEET_ROW_HEADER_WIDTH - KUI_SCROLLBAR_SIZE,
                state.scroll_column, visible_columns(event.width),
                ksheet_column_count(&state.sheet));
        copied[0] = '\0';
        update_edit_items(&state, &event);

        if (event.type == KUI_EVENT_INIT) {
            const char *path = event.argument[0] != '\0' ?
                               event.argument : "/SHEET.CSV";

            new_sheet(&state);
            copy_text(state.path, sizeof(state.path), path);
            if (read_csv_file(&state, path) < 0)
                copy_text(state.status, sizeof(state.status),
                          "Blank sheet; type a value or formula");
            kui_file_dialog_close(&file_dialog);
            kui_menu_close(&menu_state);
            kui_context_menu_close(&context_menu);
        } else if (file_dialog.active) {
            char path[KSHEET_PATH_MAX];
            int result;

            path[0] = '\0';
            result = kui_file_dialog_handle(&file_dialog, &event,
                                            path, sizeof(path));
            if (result == KUI_FILE_DIALOG_ACCEPT) {
                if (file_dialog.mode == KUI_FILE_DIALOG_OPEN) {
                    if (read_csv_file(&state, path) == 0)
                        kui_file_dialog_close(&file_dialog);
                    else
                        kui_file_dialog_set_error(&file_dialog,
                                                  "Could not open this CSV");
                } else if (write_csv_file(&state, path) == 0) {
                    kui_file_dialog_close(&file_dialog);
                    saved = 1;
                } else {
                    kui_file_dialog_set_error(&file_dialog,
                                              "Could not save this CSV");
                }
            }
        } else if (event.type == KUI_EVENT_POINTER && event.button == 2) {
            int row;
            int column;

            state.cell_dragging = 0;
            if (pointer_in_formula(event.width, event.x, event.y)) {
                struct kui_mono_text_layout layout;
                int position;

                begin_edit(&state, 0);
                formula_text_layout(event.width, &layout);
                position = kui_mono_text_position_at(
                    state.edit_text, state.edit_length, &layout,
                    event.x, event.y);
                if (!kui_text_selection_contains(&state.edit_selection,
                                                 position)) {
                    state.edit_cursor = position;
                    kui_text_selection_collapse(&state.edit_selection,
                                                position);
                }
                kui_context_menu_open(&context_menu, event.x, event.y);
            } else if (cell_at(&state, event.width, event.height,
                               event.x, event.y, &row, &column)) {
                commit_edit(&state);
                if (!cell_is_selected(&state, row, column))
                    select_one_cell(&state, row, column);
                kui_context_menu_open(&context_menu, event.x, event.y);
            } else {
                kui_context_menu_close(&context_menu);
            }
            kui_menu_close(&menu_state);
        } else if ((edit_action = kui_context_menu_handle(
                        &context_menu, &event, edit_items,
                        KSHEET_EDIT_ITEM_COUNT, &context_consumed)) != 0) {
            /* Applied with Edit-menu and shortcut actions below. */
        } else if (context_consumed) {
            /* Context menu owns the event. */
        } else if ((menu_action = kui_menu_handle(
                        &menu_state, &event, menus, 2,
                        &menu_consumed)) != 0) {
            if (menu_action == KSHEET_MENU_NEW) {
                new_sheet(&state);
            } else if (menu_action == KSHEET_MENU_OPEN) {
                commit_edit(&state);
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_OPEN,
                                     state.path);
            } else if (menu_action == KSHEET_MENU_SAVE) {
                commit_edit(&state);
                if (write_csv_file(&state, state.path) == 0) saved = 1;
                else copy_text(state.status, sizeof(state.status),
                               "Save failed");
            } else if (menu_action == KSHEET_MENU_SAVE_AS) {
                commit_edit(&state);
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_SAVE,
                                     state.path);
            } else {
                edit_action = menu_action;
            }
        } else if (menu_consumed) {
            /* Menu bar owns the event. */
        } else if (vertical_scrollbar_hit) {
            commit_edit(&state);
            state.cell_dragging = 0;
        } else if (horizontal_scrollbar_hit) {
            commit_edit(&state);
            state.cell_dragging = 0;
        } else if (event.type == KUI_EVENT_POINTER && event.wheel != 0) {
            int sheet_rows;

            commit_edit(&state);
            state.cell_dragging = 0;
            state.scroll_row = kui_scrollbar_wheel(
                state.scroll_row, event.wheel, visible_rows(event.height),
                ksheet_row_count(&state.sheet));
            if (state.scroll_row < 0) state.scroll_row = 0;
            sheet_rows = ksheet_row_count(&state.sheet);
            if (state.scroll_row + visible_rows(event.height) >= sheet_rows &&
                event.wheel < 0 && sheet_rows < KSHEET_MAX_ROWS)
                ksheet_ensure_size(&state.sheet,
                                   sheet_rows + KSHEET_DEFAULT_ROWS <
                                   KSHEET_MAX_ROWS ?
                                   sheet_rows + KSHEET_DEFAULT_ROWS :
                                   KSHEET_MAX_ROWS,
                                   ksheet_column_count(&state.sheet));
            if (state.scroll_row >= ksheet_row_count(&state.sheet))
                state.scroll_row = ksheet_row_count(&state.sheet) - 1;
        } else if (event.type == KUI_EVENT_POINTER &&
                   state.cell_dragging && event.button == 0) {
            int row;
            int column;

            if (cell_at_clamped(&state, event.width, event.height,
                                event.x, event.y, &row, &column)) {
                state.selected_row = row;
                state.selected_column = column;
                ensure_selection_visible(&state, event.width, event.height);
            }
            if ((event.buttons & 1) == 0) state.cell_dragging = 0;
        } else if (event.type == KUI_EVENT_POINTER &&
                   (pointer_in_formula(event.width, event.x, event.y) ||
                    state.edit_selection.dragging)) {
            struct kui_mono_text_layout layout;

            if (!state.editing && event.button == 1) begin_edit(&state, 0);
            if (state.editing) {
                formula_text_layout(event.width, &layout);
                kui_text_selection_handle_pointer(
                    &state.edit_selection, &event,
                    state.edit_text, state.edit_length,
                    &layout, &state.edit_cursor);
            }
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1) {
            int row;
            int column;

            if (cell_at(&state, event.width, event.height,
                        event.x, event.y, &row, &column)) {
                commit_edit(&state);
                select_one_cell(&state, row, column);
                state.cell_dragging = 1;
                copy_text(state.status, sizeof(state.status), "Ready");
            }
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed) {
            int ctrl = (event.modifiers & KUI_MOD_CTRL) != 0;
            int extend = (event.modifiers & KUI_MOD_SHIFT) != 0;

            state.cell_dragging = 0;

            if (ctrl && !event.key_extended && event.key_code == 31 &&
                (event.modifiers & KUI_MOD_SHIFT)) {
                commit_edit(&state);
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_SAVE,
                                     state.path);
            } else if (ctrl && !event.key_extended && event.key_code == 31) {
                commit_edit(&state);
                if (write_csv_file(&state, state.path) == 0) saved = 1;
            } else if (ctrl && !event.key_extended && event.key_code == 24) {
                commit_edit(&state);
                kui_file_dialog_open(&file_dialog, KUI_FILE_DIALOG_OPEN,
                                     state.path);
            } else if (ctrl && !event.key_extended && event.key_code == 49) {
                new_sheet(&state);
            } else if (ctrl && !event.key_extended && event.key_code == 46) {
                edit_action = KSHEET_MENU_COPY;
            } else if (ctrl && !event.key_extended && event.key_code == 45) {
                edit_action = KSHEET_MENU_CUT;
            } else if (ctrl && !event.key_extended && event.key_code == 47) {
                edit_action = KSHEET_MENU_PASTE;
            } else if (state.editing) {
                if (!event.key_extended && event.key_code == 1) {
                    cancel_edit(&state);
                } else if (!event.key_extended && event.key_code == 28) {
                    commit_edit(&state);
                    move_selection(&state, 0, 1, event.width, event.height, 0);
                } else if (!event.key_extended && event.key_code == 15) {
                    commit_edit(&state);
                    move_selection(&state, 1, 0, event.width, event.height, 0);
                } else if (!event.key_extended && event.key_code == 14) {
                    edit_backspace(&state);
                } else if (event.key_extended && event.key_code == 83) {
                    edit_delete_forward(&state);
                } else if (event.key_extended && event.key_code == 75) {
                    if (kui_text_selection_has_range(&state.edit_selection)) {
                        int start;
                        kui_text_selection_range(&state.edit_selection,
                                                 &start, NULL);
                        state.edit_cursor = start;
                    } else if (state.edit_cursor > 0) state.edit_cursor--;
                    kui_text_selection_collapse(&state.edit_selection,
                                                state.edit_cursor);
                } else if (event.key_extended && event.key_code == 77) {
                    if (kui_text_selection_has_range(&state.edit_selection)) {
                        int end;
                        kui_text_selection_range(&state.edit_selection,
                                                 NULL, &end);
                        state.edit_cursor = end;
                    } else if (state.edit_cursor < state.edit_length)
                        state.edit_cursor++;
                    kui_text_selection_collapse(&state.edit_selection,
                                                state.edit_cursor);
                } else if (ctrl && !event.key_extended &&
                           event.key_code == 30) {
                    kui_text_selection_select_all(&state.edit_selection,
                                                  state.edit_length);
                    state.edit_cursor = state.edit_length;
                } else if (event.character != 0 && !ctrl) {
                    edit_replace(&state, &event.character, 1);
                }
            } else if (!event.key_extended && event.key_code == 60) {
                begin_edit(&state, 0);
            } else if (event.key_extended && event.key_code == 75) {
                move_selection(&state, -1, 0, event.width, event.height,
                               extend);
            } else if (event.key_extended && event.key_code == 77) {
                move_selection(&state, 1, 0, event.width, event.height,
                               extend);
            } else if (event.key_extended && event.key_code == 72) {
                move_selection(&state, 0, -1, event.width, event.height,
                               extend);
            } else if (event.key_extended && event.key_code == 80) {
                move_selection(&state, 0, 1, event.width, event.height,
                               extend);
            } else if (!event.key_extended && event.key_code == 15) {
                move_selection(&state, 1, 0, event.width, event.height, 0);
            } else if (!event.key_extended && event.key_code == 28) {
                move_selection(&state, 0, 1, event.width, event.height, 0);
            } else if (event.key_extended && event.key_code == 83) {
                edit_action = KSHEET_MENU_DELETE;
            } else if (event.character != 0 && !ctrl) {
                begin_edit(&state, 1);
                edit_replace(&state, &event.character, 1);
            }
        }

        if (edit_action != 0)
            perform_edit_action(&state, edit_action, &event,
                                copied, &copied_valid);
        update_edit_items(&state, &event);
        snprintf(title, sizeof(title), "KSheet - %s%s",
                 base_name(state.path), state.dirty ? " *" : "");
        kui_client_begin(&client, &event, title);
        kui_client_set_constraints(&client, 560, 360, 900, 600);
        render_sheet(&client, &event, &state,
                     &vertical_scrollbar, &horizontal_scrollbar);
        kui_menu_render(&client.surface, &menu_state, menus, 2);
        kui_context_menu_render(&client.surface, &context_menu,
                                edit_items, KSHEET_EDIT_ITEM_COUNT);
        kui_file_dialog_render(&client.surface, &event, &file_dialog);
        {
            uint32_t timer = kui_scrollbar_timer_ms(&vertical_scrollbar);
            uint32_t horizontal_timer =
                kui_scrollbar_timer_ms(&horizontal_scrollbar);
            uint32_t dialog_timer = kui_file_dialog_timer_ms(&file_dialog);

            if (horizontal_timer != 0 &&
                (timer == 0 || horizontal_timer < timer))
                timer = horizontal_timer;
            if (dialog_timer != 0 && (timer == 0 || dialog_timer < timer))
                timer = dialog_timer;
            kui_client_set_timer(&client, timer);
        }
        if (copied_valid)
            kui_client_set_action(&client, KUI_ACTION_COPY_TEXT, copied);
        else if (saved)
            kui_client_set_action(&client, KUI_ACTION_FILE_SAVED, state.path);
        if (kui_client_send(&client) < 0) break;
    }
    ksheet_destroy(&state.sheet);
    kui_client_free(&client);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && (strcmp(argv[1], "--embedded") == 0 ||
                     strcmp(argv[1], "--kui-client") == 0))
        return ksheet_client_main();
    puts("KSheet is a desktop application. Run 'desktop' or 'aqua' first.");
    return 0;
}
