#include <stdio.h>
#include <string.h>

#include "extras/terminal/vt100.h"

static int failures;

static void check(int condition, const char *message)
{
    if (condition) return;
    fprintf(stderr, "VT100 check failed: %s\n", message);
    failures++;
}

static const struct vt100_cell *cell(struct vt100_terminal *terminal,
                                     int line, int column)
{
    const struct vt100_cell *result = vt100_cell_at(terminal, line, column);

    check(result != NULL, "cell exists");
    return result;
}

int main(void)
{
    struct vt100_terminal terminal;
    const struct vt100_cell *value;

    check(vt100_init(&terminal, 10, 4) == 0, "initialization");
    vt100_write(&terminal, "abc\rZ", -1);
    check(cell(&terminal, 0, 0)->character == 'Z', "carriage return");
    check(cell(&terminal, 0, 1)->character == 'b', "cursor overwrite");

    vt100_write(&terminal, "\033[2J\033[2;3HX", -1);
    check(cell(&terminal, 1, 2)->character == 'X', "absolute cursor move");
    vt100_write(&terminal, "\033[s\033[4;8HY\033[uQ", -1);
    check(cell(&terminal, 1, 3)->character == 'Q', "save/restore cursor");
    check(cell(&terminal, 3, 7)->character == 'Y', "moved cursor write");

    vt100_clear_all(&terminal);
    vt100_write(&terminal,
                "\033[31;44mR\033[1;32mG\033[7mI\033[0mN", -1);
    value = cell(&terminal, 0, 0);
    check(value->foreground == 1 && value->background == 4,
          "standard foreground/background colors");
    value = cell(&terminal, 0, 1);
    check(value->foreground == 10 && value->background == 4 &&
          (value->attributes & VT100_ATTR_BOLD) != 0,
          "bold bright color");
    value = cell(&terminal, 0, 2);
    check(value->foreground == 4 && value->background == 10,
          "inverse video");
    value = cell(&terminal, 0, 3);
    check(value->foreground == 10 && value->background == 0,
          "attribute reset");
    vt100_write(&terminal, "\033[4mU\033[24m", -1);
    check((cell(&terminal, 0, 4)->attributes & VT100_ATTR_UNDERLINE) != 0,
          "underline attribute");

    vt100_clear_all(&terminal);
    vt100_write(&terminal, "abcdef\033[3D\033[K", -1);
    check(cell(&terminal, 0, 2)->character == 'c', "erase keeps prefix");
    check(cell(&terminal, 0, 3)->character == ' ', "erase line suffix");
    vt100_write(&terminal, "12\033[2D\033[@X", -1);
    check(cell(&terminal, 0, 3)->character == 'X', "insert character");

    vt100_resize(&terminal, 10, 3);
    vt100_reset(&terminal);
    vt100_write(&terminal, "1\n2\n3\n4", -1);
    check(vt100_screen_top(&terminal) == 1, "screen scroll position");
    check(vt100_line_count(&terminal) == 4, "scrollback line retained");
    check(cell(&terminal, 0, 0)->character == '1', "scrollback contents");
    check(cell(&terminal, 3, 0)->character == '4', "new bottom line");

    vt100_write(&terminal, "\033]0;ignored title\007\033[?25l", -1);
    check(!vt100_cursor_visible(&terminal), "cursor visibility mode");
    vt100_write(&terminal, "\033[?25h", -1);
    check(vt100_cursor_visible(&terminal), "cursor visibility restore");

    vt100_reset(&terminal);
    for (int index = 0; index < 400; index++)
        vt100_write(&terminal, "overflow\n", -1);
    check(vt100_line_count(&terminal) == VT100_MAX_LINES,
          "scrollback capacity bound");
    check(vt100_screen_top(&terminal) ==
          VT100_MAX_LINES - terminal.screen_rows,
          "scrollback ring remains aligned");
    check(vt100_cursor_line(&terminal) < VT100_MAX_LINES,
          "cursor remains in bounded storage");

    vt100_free(&terminal);
    if (failures != 0) {
        fprintf(stderr, "FAIL VT100 parser: %d failure(s)\n", failures);
        return 1;
    }
    puts("PASS VT100 cursor, erase, scrolling, and ANSI color operations");
    return 0;
}
