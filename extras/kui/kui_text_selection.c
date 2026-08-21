#include "extras/kui/kui_text_selection.h"

#include <string.h>

static int clamp_position(int position, int text_length)
{
    if (position < 0) return 0;
    if (position > text_length) return text_length;
    return position;
}

void kui_text_selection_init(struct kui_text_selection *selection)
{
    selection->anchor = 0;
    selection->focus = 0;
    selection->dragging = 0;
}

void kui_text_selection_collapse(struct kui_text_selection *selection,
                                 int position)
{
    selection->anchor = position;
    selection->focus = position;
    selection->dragging = 0;
}

void kui_text_selection_set(struct kui_text_selection *selection,
                            int anchor, int focus)
{
    selection->anchor = anchor;
    selection->focus = focus;
    selection->dragging = 0;
}

int kui_text_selection_has_range(const struct kui_text_selection *selection)
{
    return selection->anchor != selection->focus;
}

void kui_text_selection_range(const struct kui_text_selection *selection,
                              int *start, int *end)
{
    if (selection->anchor <= selection->focus) {
        if (start != 0) *start = selection->anchor;
        if (end != 0) *end = selection->focus;
    } else {
        if (start != 0) *start = selection->focus;
        if (end != 0) *end = selection->anchor;
    }
}

int kui_text_selection_contains(const struct kui_text_selection *selection,
                                int position)
{
    int start;
    int end;

    kui_text_selection_range(selection, &start, &end);
    return position >= start && position < end;
}

void kui_text_selection_select_all(struct kui_text_selection *selection,
                                   int text_length)
{
    if (text_length < 0) text_length = 0;
    selection->anchor = 0;
    selection->focus = text_length;
    selection->dragging = 0;
}

int kui_text_selection_copy(const struct kui_text_selection *selection,
                            const char *text, int text_length,
                            char *destination, int capacity)
{
    int start;
    int end;
    int count;

    if (destination == 0 || capacity <= 0) return 0;
    destination[0] = '\0';
    if (selection == 0 || text == 0 || text_length < 0 ||
        !kui_text_selection_has_range(selection))
        return 0;
    kui_text_selection_range(selection, &start, &end);
    start = clamp_position(start, text_length);
    end = clamp_position(end, text_length);
    count = end - start;
    if (count <= 0) return 0;
    if (count >= capacity) count = capacity - 1;
    memcpy(destination, text + start, (size_t)count);
    destination[count] = '\0';
    return count;
}

int kui_text_selection_delete(struct kui_text_selection *selection,
                              char *text, int *text_length,
                              int *cursor_position)
{
    int start;
    int end;

    if (selection == 0 || text == 0 || text_length == 0 ||
        !kui_text_selection_has_range(selection))
        return 0;
    kui_text_selection_range(selection, &start, &end);
    start = clamp_position(start, *text_length);
    end = clamp_position(end, *text_length);
    if (end <= start) {
        kui_text_selection_collapse(selection, start);
        if (cursor_position != 0) *cursor_position = start;
        return 0;
    }
    memmove(text + start, text + end, (size_t)(*text_length - end));
    *text_length -= end - start;
    text[*text_length] = '\0';
    if (cursor_position != 0) *cursor_position = start;
    kui_text_selection_collapse(selection, start);
    return 1;
}

int kui_text_selection_replace(struct kui_text_selection *selection,
                               char *text, int *text_length, int capacity,
                               int *cursor_position,
                               const char *replacement,
                               int replacement_length)
{
    int cursor;
    int available;
    int changed;

    if (selection == 0 || text == 0 || text_length == 0 || capacity <= 0)
        return 0;
    cursor = cursor_position != 0 ? *cursor_position : selection->focus;
    cursor = clamp_position(cursor, *text_length);
    changed = kui_text_selection_delete(selection, text, text_length,
                                        &cursor);
    if (replacement == 0 || replacement_length <= 0) {
        if (cursor_position != 0) *cursor_position = cursor;
        return changed;
    }
    available = capacity - 1 - *text_length;
    if (replacement_length > available) replacement_length = available;
    if (replacement_length <= 0) {
        if (cursor_position != 0) *cursor_position = cursor;
        return changed;
    }
    memmove(text + cursor + replacement_length, text + cursor,
            (size_t)(*text_length - cursor));
    memcpy(text + cursor, replacement, (size_t)replacement_length);
    *text_length += replacement_length;
    cursor += replacement_length;
    text[*text_length] = '\0';
    kui_text_selection_collapse(selection, cursor);
    if (cursor_position != 0) *cursor_position = cursor;
    return 1;
}

int kui_mono_text_position_at(const char *text, int text_length,
                              const struct kui_mono_text_layout *layout,
                              int x, int y)
{
    int target_row;
    int target_column;
    int row = 0;
    int column = 0;

    if (text == 0 || layout == 0 || text_length <= 0)
        return 0;
    if (layout->columns <= 0 || layout->visible_rows <= 0 ||
        layout->cell_width <= 0 || layout->row_height <= 0)
        return 0;

    if (x < layout->left) x = layout->left;
    if (y < layout->top) y = layout->top;
    if (x >= layout->left + layout->columns * layout->cell_width)
        x = layout->left + layout->columns * layout->cell_width - 1;
    if (y >= layout->top + layout->visible_rows * layout->row_height)
        y = layout->top + layout->visible_rows * layout->row_height - 1;

    target_row = layout->scroll_row +
                 (y - layout->top) / layout->row_height;
    target_column = (x - layout->left) / layout->cell_width;

    for (int position = 0; position <= text_length; position++) {
        if (row == target_row &&
            (column >= target_column || position == text_length ||
             text[position] == '\n'))
            return position;
        if (position == text_length) break;
        if (text[position] == '\n') {
            row++;
            column = 0;
        } else if (++column >= layout->columns) {
            row++;
            column = 0;
        }
    }
    return text_length;
}

static int pointer_in_layout(const struct kui_event *event,
                             const struct kui_mono_text_layout *layout)
{
    int width;
    int height;

    if (layout->columns <= 0 || layout->visible_rows <= 0 ||
        layout->cell_width <= 0 || layout->row_height <= 0)
        return 0;
    width = layout->columns * layout->cell_width;
    height = layout->visible_rows * layout->row_height;
    return event->x >= layout->left && event->x < layout->left + width &&
           event->y >= layout->top && event->y < layout->top + height;
}

int kui_text_selection_handle_pointer(
    struct kui_text_selection *selection,
    const struct kui_event *event,
    const char *text, int text_length,
    const struct kui_mono_text_layout *layout,
    int *cursor_position)
{
    int position;

    if (selection == 0 || event == 0 || layout == 0 ||
        event->type != KUI_EVENT_POINTER)
        return 0;
    if (text_length < 0) text_length = 0;

    if (event->button == 1) {
        if (!pointer_in_layout(event, layout)) return 0;
        position = kui_mono_text_position_at(text, text_length, layout,
                                             event->x, event->y);
        position = clamp_position(position, text_length);
        selection->anchor = position;
        selection->focus = position;
        selection->dragging = 1;
        if (cursor_position != 0) *cursor_position = position;
        return 1;
    }

    if (!selection->dragging) return 0;
    position = kui_mono_text_position_at(text, text_length, layout,
                                         event->x, event->y);
    position = clamp_position(position, text_length);
    selection->focus = position;
    if (cursor_position != 0) *cursor_position = position;
    if ((event->buttons & 1) == 0)
        selection->dragging = 0;
    return 1;
}
