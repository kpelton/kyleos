#include <stdio.h>
#include <string.h>

#include "extras/kui/kui_text_selection.h"

static int failures;

static void expect_int(const char *name, int actual, int expected)
{
    if (actual == expected) return;
    fprintf(stderr, "FAIL %s: got %d, expected %d\n",
            name, actual, expected);
    failures++;
}

static struct kui_event pointer_event(int x, int y, int button, int buttons)
{
    struct kui_event event;

    memset(&event, 0, sizeof(event));
    event.type = KUI_EVENT_POINTER;
    event.x = x;
    event.y = y;
    event.button = button;
    event.buttons = buttons;
    return event;
}

int main(void)
{
    const char *text = "abc\ndefgh";
    struct kui_mono_text_layout layout = {
        .left = 10,
        .top = 20,
        .columns = 4,
        .visible_rows = 3,
        .cell_width = 8,
        .row_height = 16,
        .scroll_row = 0
    };
    struct kui_text_selection selection;
    struct kui_event event;
    int cursor = -1;
    int start = -1;
    int end = -1;
    char edit_buffer[32];
    char copied[8];
    int edit_length;

    expect_int("first row column 2",
               kui_mono_text_position_at(text, 9, &layout, 26, 20), 2);
    expect_int("newline clamps row",
               kui_mono_text_position_at(text, 9, &layout, 42, 20), 3);
    expect_int("second row column 2",
               kui_mono_text_position_at(text, 9, &layout, 26, 36), 6);
    expect_int("below view clamps",
               kui_mono_text_position_at(text, 9, &layout, 42, 500), 9);
    expect_int("empty text",
               kui_mono_text_position_at("", 0, &layout, 10, 20), 0);

    layout.scroll_row = 1;
    expect_int("scrolled hit test",
               kui_mono_text_position_at(text, 9, &layout, 18, 20), 5);
    layout.scroll_row = 0;

    kui_text_selection_init(&selection);
    expect_int("selection starts collapsed",
               kui_text_selection_has_range(&selection), 0);

    event = pointer_event(18, 20, 1, 1);
    expect_int("press consumed",
               kui_text_selection_handle_pointer(&selection, &event,
                                                  text, 9, &layout,
                                                  &cursor), 1);
    expect_int("press cursor", cursor, 1);
    expect_int("drag active", selection.dragging, 1);

    event = pointer_event(26, 36, 0, 1);
    expect_int("drag consumed",
               kui_text_selection_handle_pointer(&selection, &event,
                                                  text, 9, &layout,
                                                  &cursor), 1);
    expect_int("drag cursor", cursor, 6);
    kui_text_selection_range(&selection, &start, &end);
    expect_int("forward range start", start, 1);
    expect_int("forward range end", end, 6);
    expect_int("range includes start",
               kui_text_selection_contains(&selection, 1), 1);
    expect_int("range excludes end",
               kui_text_selection_contains(&selection, 6), 0);

    event = pointer_event(26, 36, 0, 0);
    expect_int("release consumed",
               kui_text_selection_handle_pointer(&selection, &event,
                                                  text, 9, &layout,
                                                  &cursor), 1);
    expect_int("drag released", selection.dragging, 0);

    event = pointer_event(26, 36, 1, 1);
    kui_text_selection_handle_pointer(&selection, &event, text, 9,
                                      &layout, &cursor);
    event = pointer_event(18, 20, 0, 1);
    kui_text_selection_handle_pointer(&selection, &event, text, 9,
                                      &layout, &cursor);
    kui_text_selection_range(&selection, &start, &end);
    expect_int("reverse range start", start, 1);
    expect_int("reverse range end", end, 6);

    kui_text_selection_collapse(&selection, 4);
    expect_int("collapse anchor", selection.anchor, 4);
    expect_int("collapse focus", selection.focus, 4);
    expect_int("collapse range", kui_text_selection_has_range(&selection), 0);

    event = pointer_event(0, 0, 1, 1);
    expect_int("outside press ignored",
               kui_text_selection_handle_pointer(&selection, &event,
                                                  text, 9, &layout,
                                                  &cursor), 0);

    strcpy(edit_buffer, "hello");
    edit_length = 5;
    kui_text_selection_set(&selection, 1, 4);
    expect_int("copy count",
               kui_text_selection_copy(&selection, edit_buffer, edit_length,
                                       copied, sizeof(copied)), 3);
    expect_int("copy bytes", strcmp(copied, "ell"), 0);
    cursor = 4;
    expect_int("replace selection",
               kui_text_selection_replace(&selection, edit_buffer,
                                          &edit_length, sizeof(edit_buffer),
                                          &cursor, "i", 1), 1);
    expect_int("replace bytes", strcmp(edit_buffer, "hio"), 0);
    expect_int("replace length", edit_length, 3);
    expect_int("replace cursor", cursor, 2);

    kui_text_selection_select_all(&selection, edit_length);
    expect_int("select all", kui_text_selection_has_range(&selection), 1);
    expect_int("delete all",
               kui_text_selection_delete(&selection, edit_buffer,
                                         &edit_length, &cursor), 1);
    expect_int("delete all bytes", strcmp(edit_buffer, ""), 0);
    expect_int("delete all cursor", cursor, 0);

    strcpy(edit_buffer, "123456");
    edit_length = 6;
    cursor = 6;
    kui_text_selection_collapse(&selection, cursor);
    expect_int("capacity-limited paste",
               kui_text_selection_replace(&selection, edit_buffer,
                                          &edit_length, 9, &cursor,
                                          "abcd", 4), 1);
    expect_int("capacity-limited bytes", strcmp(edit_buffer, "123456ab"), 0);
    expect_int("capacity-limited length", edit_length, 8);

    if (failures != 0) {
        fprintf(stderr, "FAIL KUI text selection: %d failure(s)\n", failures);
        return 1;
    }
    puts("PASS KUI text selection");
    return 0;
}
