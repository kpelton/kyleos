#ifndef KYLEOS_KUI_TEXT_SELECTION_H
#define KYLEOS_KUI_TEXT_SELECTION_H

#include "extras/kui/kui_protocol.h"

/*
 * Shared mouse-selection state for monospace, wrapped text views.
 *
 * Positions are byte offsets in the application's text buffer.  Selection
 * ranges are half-open: [start, end).  The layout must use the same column
 * count and row metrics as the renderer so hit testing and drawing agree.
 */
struct kui_text_selection {
    int anchor;
    int focus;
    int dragging;
};

struct kui_mono_text_layout {
    int left;
    int top;
    int columns;
    int visible_rows;
    int cell_width;
    int row_height;
    int scroll_row;
};

void kui_text_selection_init(struct kui_text_selection *selection);
void kui_text_selection_collapse(struct kui_text_selection *selection,
                                 int position);
void kui_text_selection_set(struct kui_text_selection *selection,
                            int anchor, int focus);
int kui_text_selection_has_range(const struct kui_text_selection *selection);
void kui_text_selection_range(const struct kui_text_selection *selection,
                              int *start, int *end);
int kui_text_selection_contains(const struct kui_text_selection *selection,
                                int position);
void kui_text_selection_select_all(struct kui_text_selection *selection,
                                   int text_length);
int kui_text_selection_copy(const struct kui_text_selection *selection,
                            const char *text, int text_length,
                            char *destination, int capacity);
int kui_text_selection_delete(struct kui_text_selection *selection,
                              char *text, int *text_length,
                              int *cursor_position);
int kui_text_selection_replace(struct kui_text_selection *selection,
                               char *text, int *text_length, int capacity,
                               int *cursor_position,
                               const char *replacement,
                               int replacement_length);

int kui_mono_text_position_at(const char *text, int text_length,
                              const struct kui_mono_text_layout *layout,
                              int x, int y);

/*
 * Handles left-button press, drag, and release events.  Returns non-zero when
 * the event belongs to an active selection gesture and updates cursor_position
 * to the selection focus when that pointer is non-NULL.
 */
int kui_text_selection_handle_pointer(
    struct kui_text_selection *selection,
    const struct kui_event *event,
    const char *text, int text_length,
    const struct kui_mono_text_layout *layout,
    int *cursor_position);

#endif
