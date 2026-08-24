#include "extras/terminal/vt100.h"

#include <stdlib.h>
#include <string.h>

#define VT100_DEFAULT_FOREGROUND 10
#define VT100_DEFAULT_BACKGROUND 0

enum parser_state {
    PARSER_GROUND,
    PARSER_ESCAPE,
    PARSER_CSI,
    PARSER_OSC,
    PARSER_OSC_ESCAPE,
    PARSER_CHARSET
};

static int clamp(int value, int minimum, int maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static struct vt100_cell *cell_at(struct vt100_terminal *terminal,
                                  int line, int column)
{
    if (terminal == NULL || terminal->cells == NULL || line < 0 ||
        line >= VT100_MAX_LINES || column < 0 ||
        column >= VT100_MAX_COLUMNS)
        return NULL;
    return &terminal->cells[line * VT100_MAX_COLUMNS + column];
}

static void blank_cell(struct vt100_terminal *terminal,
                       struct vt100_cell *cell)
{
    uint8_t foreground = terminal->foreground;
    uint8_t background = terminal->background;

    if ((terminal->attributes & VT100_ATTR_BOLD) && foreground < 8)
        foreground += 8;
    if (terminal->inverse) {
        uint8_t swap = foreground;
        foreground = background;
        background = swap;
    }
    cell->character = ' ';
    cell->foreground = foreground;
    cell->background = background;
    cell->attributes = terminal->attributes;
}

static void clear_line(struct vt100_terminal *terminal, int line)
{
    for (int column = 0; column < VT100_MAX_COLUMNS; column++)
        blank_cell(terminal, cell_at(terminal, line, column));
}

static void discard_oldest_line(struct vt100_terminal *terminal)
{
    size_t line_bytes = VT100_MAX_COLUMNS * sizeof(struct vt100_cell);

    memmove(terminal->cells, terminal->cells + VT100_MAX_COLUMNS,
            (VT100_MAX_LINES - 1) * line_bytes);
    terminal->line_count--;
    if (terminal->screen_top > 0) terminal->screen_top--;
    if (terminal->cursor_line > 0) terminal->cursor_line--;
    if (terminal->saved_row > 0) terminal->saved_row--;
    clear_line(terminal, VT100_MAX_LINES - 1);
}

static void append_blank_line(struct vt100_terminal *terminal)
{
    if (terminal->line_count >= VT100_MAX_LINES)
        discard_oldest_line(terminal);
    clear_line(terminal, terminal->line_count);
    terminal->line_count++;
}

static void scroll_up(struct vt100_terminal *terminal, int amount)
{
    int relative_row = terminal->cursor_line - terminal->screen_top;

    while (amount-- > 0) {
        append_blank_line(terminal);
        terminal->screen_top++;
        terminal->cursor_line = terminal->screen_top + relative_row;
    }
}

static void scroll_down(struct vt100_terminal *terminal, int amount)
{
    size_t line_bytes = VT100_MAX_COLUMNS * sizeof(struct vt100_cell);
    int top = terminal->screen_top;
    int bottom = top + terminal->screen_rows - 1;

    while (amount-- > 0) {
        if (bottom > top)
            memmove(cell_at(terminal, top + 1, 0),
                    cell_at(terminal, top, 0),
                    (size_t)(bottom - top) * line_bytes);
        clear_line(terminal, top);
    }
}

static void linefeed(struct vt100_terminal *terminal)
{
    terminal->wrap_pending = 0;
    if (terminal->cursor_line >=
        terminal->screen_top + terminal->screen_rows - 1)
        scroll_up(terminal, 1);
    else
        terminal->cursor_line++;
}

static int parameter(const struct vt100_terminal *terminal, int index,
                     int fallback)
{
    if (index < 0 || index >= terminal->parameter_count ||
        terminal->parameters[index] < 0)
        return fallback;
    return terminal->parameters[index];
}

static void erase_line(struct vt100_terminal *terminal, int mode)
{
    int first = mode == 0 ? terminal->cursor_column : 0;
    int last = mode == 1 ? terminal->cursor_column : terminal->columns - 1;

    if (mode == 2) {
        first = 0;
        last = terminal->columns - 1;
    }
    for (int column = first; column <= last; column++)
        blank_cell(terminal, cell_at(terminal, terminal->cursor_line, column));
}

static void erase_display(struct vt100_terminal *terminal, int mode)
{
    int top = terminal->screen_top;
    int bottom = top + terminal->screen_rows - 1;

    if (mode == 2 || mode == 3) {
        for (int line = top; line <= bottom; line++)
            clear_line(terminal, line);
        if (mode == 3) {
            size_t bytes = (size_t)terminal->screen_rows *
                           VT100_MAX_COLUMNS * sizeof(struct vt100_cell);

            memmove(terminal->cells, cell_at(terminal, top, 0), bytes);
            terminal->line_count = terminal->screen_rows;
            terminal->screen_top = 0;
            terminal->cursor_line = clamp(terminal->cursor_line - top, 0,
                                          terminal->screen_rows - 1);
        }
        return;
    }
    if (mode == 0) {
        erase_line(terminal, 0);
        for (int line = terminal->cursor_line + 1; line <= bottom; line++)
            clear_line(terminal, line);
    } else if (mode == 1) {
        for (int line = top; line < terminal->cursor_line; line++)
            clear_line(terminal, line);
        erase_line(terminal, 1);
    }
}

static void sgr(struct vt100_terminal *terminal)
{
    int count = terminal->parameter_count == 0 ? 1 :
                terminal->parameter_count;

    for (int index = 0; index < count; index++) {
        int value = parameter(terminal, index, 0);

        if (value == 0) {
            terminal->foreground = VT100_DEFAULT_FOREGROUND;
            terminal->background = VT100_DEFAULT_BACKGROUND;
            terminal->attributes = 0;
            terminal->inverse = 0;
        } else if (value == 1) {
            terminal->attributes |= VT100_ATTR_BOLD;
        } else if (value == 22) {
            terminal->attributes &= (uint8_t)~VT100_ATTR_BOLD;
        } else if (value == 4) {
            terminal->attributes |= VT100_ATTR_UNDERLINE;
        } else if (value == 24) {
            terminal->attributes &= (uint8_t)~VT100_ATTR_UNDERLINE;
        } else if (value == 7) {
            terminal->inverse = 1;
        } else if (value == 27) {
            terminal->inverse = 0;
        } else if (value == 8) {
            terminal->attributes |= VT100_ATTR_CONCEAL;
        } else if (value == 28) {
            terminal->attributes &= (uint8_t)~VT100_ATTR_CONCEAL;
        } else if (value >= 30 && value <= 37) {
            terminal->foreground = (uint8_t)(value - 30);
        } else if (value == 39) {
            terminal->foreground = VT100_DEFAULT_FOREGROUND;
        } else if (value >= 40 && value <= 47) {
            terminal->background = (uint8_t)(value - 40);
        } else if (value == 49) {
            terminal->background = VT100_DEFAULT_BACKGROUND;
        } else if (value >= 90 && value <= 97) {
            terminal->foreground = (uint8_t)(value - 90 + 8);
        } else if (value >= 100 && value <= 107) {
            terminal->background = (uint8_t)(value - 100 + 8);
        }
    }
}

static void insert_lines(struct vt100_terminal *terminal, int amount)
{
    size_t line_bytes = VT100_MAX_COLUMNS * sizeof(struct vt100_cell);
    int bottom = terminal->screen_top + terminal->screen_rows - 1;
    int line = terminal->cursor_line;

    amount = clamp(amount, 1, bottom - line + 1);
    if (bottom - line + 1 - amount > 0)
        memmove(cell_at(terminal, line + amount, 0),
                cell_at(terminal, line, 0),
                (size_t)(bottom - line + 1 - amount) * line_bytes);
    for (int index = 0; index < amount; index++)
        clear_line(terminal, line + index);
}

static void delete_lines(struct vt100_terminal *terminal, int amount)
{
    size_t line_bytes = VT100_MAX_COLUMNS * sizeof(struct vt100_cell);
    int bottom = terminal->screen_top + terminal->screen_rows - 1;
    int line = terminal->cursor_line;

    amount = clamp(amount, 1, bottom - line + 1);
    if (bottom - line + 1 - amount > 0)
        memmove(cell_at(terminal, line, 0),
                cell_at(terminal, line + amount, 0),
                (size_t)(bottom - line + 1 - amount) * line_bytes);
    for (int index = bottom - amount + 1; index <= bottom; index++)
        clear_line(terminal, index);
}

static void insert_characters(struct vt100_terminal *terminal, int amount)
{
    struct vt100_cell *line = cell_at(terminal, terminal->cursor_line, 0);
    int column = terminal->cursor_column;

    amount = clamp(amount, 1, terminal->columns - column);
    memmove(line + column + amount, line + column,
            (size_t)(terminal->columns - column - amount) * sizeof(*line));
    for (int index = 0; index < amount; index++)
        blank_cell(terminal, &line[column + index]);
}

static void delete_characters(struct vt100_terminal *terminal, int amount)
{
    struct vt100_cell *line = cell_at(terminal, terminal->cursor_line, 0);
    int column = terminal->cursor_column;

    amount = clamp(amount, 1, terminal->columns - column);
    memmove(line + column, line + column + amount,
            (size_t)(terminal->columns - column - amount) * sizeof(*line));
    for (int index = terminal->columns - amount;
         index < terminal->columns; index++)
        blank_cell(terminal, &line[index]);
}

static void csi_execute(struct vt100_terminal *terminal, unsigned char command)
{
    int amount = parameter(terminal, 0, 1);
    int row;
    int column;

    if (amount < 1) amount = 1;
    if (command == 'A')
        terminal->cursor_line = clamp(terminal->cursor_line - amount,
                                      terminal->screen_top,
                                      terminal->screen_top +
                                      terminal->screen_rows - 1);
    else if (command == 'B' || command == 'e')
        terminal->cursor_line = clamp(terminal->cursor_line + amount,
                                      terminal->screen_top,
                                      terminal->screen_top +
                                      terminal->screen_rows - 1);
    else if (command == 'C' || command == 'a')
        terminal->cursor_column = clamp(terminal->cursor_column + amount,
                                        0, terminal->columns - 1);
    else if (command == 'D')
        terminal->cursor_column = clamp(terminal->cursor_column - amount,
                                        0, terminal->columns - 1);
    else if (command == 'E') {
        terminal->cursor_line = clamp(terminal->cursor_line + amount,
                                      terminal->screen_top,
                                      terminal->screen_top +
                                      terminal->screen_rows - 1);
        terminal->cursor_column = 0;
    } else if (command == 'F') {
        terminal->cursor_line = clamp(terminal->cursor_line - amount,
                                      terminal->screen_top,
                                      terminal->screen_top +
                                      terminal->screen_rows - 1);
        terminal->cursor_column = 0;
    } else if (command == 'G' || command == '`') {
        terminal->cursor_column = clamp(parameter(terminal, 0, 1) - 1,
                                        0, terminal->columns - 1);
    } else if (command == 'H' || command == 'f') {
        row = parameter(terminal, 0, 1) - 1;
        column = parameter(terminal, 1, 1) - 1;
        terminal->cursor_line = terminal->screen_top +
            clamp(row, 0, terminal->screen_rows - 1);
        terminal->cursor_column = clamp(column, 0, terminal->columns - 1);
    } else if (command == 'd') {
        row = parameter(terminal, 0, 1) - 1;
        terminal->cursor_line = terminal->screen_top +
            clamp(row, 0, terminal->screen_rows - 1);
    } else if (command == 'J') {
        erase_display(terminal, parameter(terminal, 0, 0));
    } else if (command == 'K') {
        erase_line(terminal, parameter(terminal, 0, 0));
    } else if (command == 'L') {
        insert_lines(terminal, amount);
    } else if (command == 'M') {
        delete_lines(terminal, amount);
    } else if (command == '@') {
        insert_characters(terminal, amount);
    } else if (command == 'P') {
        delete_characters(terminal, amount);
    } else if (command == 'X') {
        for (int index = 0; index < amount &&
             terminal->cursor_column + index < terminal->columns; index++)
            blank_cell(terminal, cell_at(terminal, terminal->cursor_line,
                       terminal->cursor_column + index));
    } else if (command == 'S') {
        scroll_up(terminal, amount);
    } else if (command == 'T') {
        scroll_down(terminal, amount);
    } else if (command == 'm') {
        sgr(terminal);
    } else if (command == 's') {
        terminal->saved_row = terminal->cursor_line;
        terminal->saved_column = terminal->cursor_column;
    } else if (command == 'u') {
        terminal->cursor_line = clamp(terminal->saved_row,
                                      terminal->screen_top,
                                      terminal->screen_top +
                                      terminal->screen_rows - 1);
        terminal->cursor_column = clamp(terminal->saved_column, 0,
                                        terminal->columns - 1);
    } else if ((command == 'h' || command == 'l') &&
               terminal->private_sequence) {
        int enabled = command == 'h';

        if (parameter(terminal, 0, 0) == 25)
            terminal->cursor_visible = (uint8_t)enabled;
        else if (parameter(terminal, 0, 0) == 7)
            terminal->autowrap = (uint8_t)enabled;
    }
    terminal->wrap_pending = 0;
}

static void put_character(struct vt100_terminal *terminal,
                          unsigned char character)
{
    struct vt100_cell *cell;
    uint8_t foreground;
    uint8_t background;

    if (terminal->wrap_pending) {
        if (terminal->autowrap) {
            terminal->cursor_column = 0;
            linefeed(terminal);
        }
        terminal->wrap_pending = 0;
    }
    cell = cell_at(terminal, terminal->cursor_line,
                   terminal->cursor_column);
    foreground = terminal->foreground;
    background = terminal->background;
    if ((terminal->attributes & VT100_ATTR_BOLD) && foreground < 8)
        foreground += 8;
    if (terminal->inverse) {
        uint8_t swap = foreground;
        foreground = background;
        background = swap;
    }
    cell->character = character;
    cell->foreground = foreground;
    cell->background = background;
    cell->attributes = terminal->attributes;
    if (terminal->cursor_column == terminal->columns - 1)
        terminal->wrap_pending = terminal->autowrap;
    else
        terminal->cursor_column++;
}

static void feed(struct vt100_terminal *terminal, unsigned char character)
{
    if (terminal->parser_state == PARSER_OSC) {
        if (character == 7)
            terminal->parser_state = PARSER_GROUND;
        else if (character == 27)
            terminal->parser_state = PARSER_OSC_ESCAPE;
        return;
    }
    if (terminal->parser_state == PARSER_OSC_ESCAPE) {
        terminal->parser_state = character == '\\' ? PARSER_GROUND :
                                 PARSER_OSC;
        return;
    }
    if (terminal->parser_state == PARSER_CHARSET) {
        terminal->parser_state = PARSER_GROUND;
        return;
    }
    if (terminal->parser_state == PARSER_ESCAPE) {
        terminal->parser_state = PARSER_GROUND;
        if (character == '[') {
            terminal->parser_state = PARSER_CSI;
            terminal->parameter_count = 0;
            terminal->private_sequence = 0;
        } else if (character == ']') {
            terminal->parser_state = PARSER_OSC;
        } else if (character == '(' || character == ')') {
            terminal->parser_state = PARSER_CHARSET;
        } else if (character == '7') {
            terminal->saved_row = terminal->cursor_line;
            terminal->saved_column = terminal->cursor_column;
        } else if (character == '8') {
            terminal->cursor_line = clamp(terminal->saved_row,
                                          terminal->screen_top,
                                          terminal->screen_top +
                                          terminal->screen_rows - 1);
            terminal->cursor_column = clamp(terminal->saved_column, 0,
                                            terminal->columns - 1);
        } else if (character == 'D') {
            linefeed(terminal);
        } else if (character == 'E') {
            terminal->cursor_column = 0;
            linefeed(terminal);
        } else if (character == 'M') {
            if (terminal->cursor_line == terminal->screen_top)
                scroll_down(terminal, 1);
            else
                terminal->cursor_line--;
        } else if (character == 'c') {
            vt100_reset(terminal);
        }
        return;
    }
    if (terminal->parser_state == PARSER_CSI) {
        if (character == '?' && terminal->parameter_count == 0) {
            terminal->private_sequence = 1;
            return;
        }
        if (character >= '0' && character <= '9') {
            if (terminal->parameter_count == 0) {
                terminal->parameter_count = 1;
                terminal->parameters[0] = 0;
            }
            if (terminal->parameters[terminal->parameter_count - 1] < 0)
                terminal->parameters[terminal->parameter_count - 1] = 0;
            terminal->parameters[terminal->parameter_count - 1] =
                terminal->parameters[terminal->parameter_count - 1] * 10 +
                character - '0';
            return;
        }
        if (character == ';') {
            if (terminal->parameter_count == 0) {
                terminal->parameter_count = 1;
                terminal->parameters[0] = -1;
            }
            if (terminal->parameter_count < 16)
                terminal->parameters[terminal->parameter_count++] = -1;
            return;
        }
        if (character >= '@' && character <= '~') {
            csi_execute(terminal, character);
            terminal->parser_state = PARSER_GROUND;
        }
        return;
    }
    if (character == 27) {
        terminal->parser_state = PARSER_ESCAPE;
    } else if (character == '\r') {
        terminal->cursor_column = 0;
        terminal->wrap_pending = 0;
    } else if (character == '\n' || character == '\v' || character == '\f') {
        /* KyleOS PTYs do not yet implement termios OPOST/ONLCR, so perform
         * the conventional terminal newline translation here. */
        terminal->cursor_column = 0;
        linefeed(terminal);
    } else if (character == '\b') {
        terminal->wrap_pending = 0;
        if (terminal->cursor_column > 0)
            terminal->cursor_column--;
    } else if (character == '\t') {
        terminal->wrap_pending = 0;
        terminal->cursor_column = clamp((terminal->cursor_column + 8) & ~7,
                                        0, terminal->columns - 1);
    } else if (character >= 32 && character != 127) {
        put_character(terminal, character);
    }
}

int vt100_init(struct vt100_terminal *terminal, int columns, int rows)
{
    if (terminal == NULL)
        return -1;
    memset(terminal, 0, sizeof(*terminal));
    terminal->cells = calloc((size_t)VT100_MAX_LINES * VT100_MAX_COLUMNS,
                             sizeof(struct vt100_cell));
    if (terminal->cells == NULL)
        return -1;
    terminal->columns = clamp(columns, 1, VT100_MAX_COLUMNS);
    terminal->screen_rows = clamp(rows, 1, VT100_MAX_SCREEN_ROWS);
    vt100_reset(terminal);
    return 0;
}

void vt100_free(struct vt100_terminal *terminal)
{
    if (terminal == NULL)
        return;
    free(terminal->cells);
    memset(terminal, 0, sizeof(*terminal));
}

void vt100_resize(struct vt100_terminal *terminal, int columns, int rows)
{
    if (terminal == NULL || terminal->cells == NULL)
        return;
    columns = clamp(columns, 1, VT100_MAX_COLUMNS);
    rows = clamp(rows, 1, VT100_MAX_SCREEN_ROWS);
    if (columns == terminal->columns && rows == terminal->screen_rows)
        return;
    terminal->columns = columns;
    terminal->screen_rows = rows;
    while (terminal->line_count < rows)
        append_blank_line(terminal);
    terminal->screen_top = terminal->line_count > rows ?
                           terminal->line_count - rows : 0;
    terminal->cursor_line = clamp(terminal->cursor_line,
                                  terminal->screen_top,
                                  terminal->screen_top + rows - 1);
    terminal->cursor_column = clamp(terminal->cursor_column, 0,
                                    terminal->columns - 1);
    terminal->wrap_pending = 0;
}

void vt100_reset(struct vt100_terminal *terminal)
{
    if (terminal == NULL || terminal->cells == NULL)
        return;
    terminal->foreground = VT100_DEFAULT_FOREGROUND;
    terminal->background = VT100_DEFAULT_BACKGROUND;
    terminal->attributes = 0;
    terminal->inverse = 0;
    terminal->cursor_visible = 1;
    terminal->autowrap = 1;
    terminal->wrap_pending = 0;
    terminal->parser_state = PARSER_GROUND;
    terminal->parameter_count = 0;
    terminal->private_sequence = 0;
    terminal->line_count = terminal->screen_rows;
    terminal->screen_top = 0;
    terminal->cursor_line = 0;
    terminal->cursor_column = 0;
    terminal->saved_row = 0;
    terminal->saved_column = 0;
    for (int line = 0; line < terminal->line_count; line++)
        clear_line(terminal, line);
}

void vt100_clear_all(struct vt100_terminal *terminal)
{
    if (terminal == NULL || terminal->cells == NULL)
        return;
    terminal->line_count = terminal->screen_rows;
    terminal->screen_top = 0;
    terminal->cursor_line = 0;
    terminal->cursor_column = 0;
    terminal->wrap_pending = 0;
    for (int line = 0; line < terminal->line_count; line++)
        clear_line(terminal, line);
}

void vt100_write(struct vt100_terminal *terminal, const char *bytes,
                 int length)
{
    if (terminal == NULL || terminal->cells == NULL || bytes == NULL)
        return;
    if (length < 0)
        length = (int)strlen(bytes);
    for (int index = 0; index < length; index++)
        feed(terminal, (unsigned char)bytes[index]);
}

const struct vt100_cell *vt100_cell_at(const struct vt100_terminal *terminal,
                                       int line, int column)
{
    if (terminal == NULL || terminal->cells == NULL || line < 0 ||
        line >= terminal->line_count || column < 0 ||
        column >= terminal->columns)
        return NULL;
    return &terminal->cells[line * VT100_MAX_COLUMNS + column];
}

int vt100_line_count(const struct vt100_terminal *terminal)
{
    return terminal == NULL ? 0 : terminal->line_count;
}

int vt100_screen_top(const struct vt100_terminal *terminal)
{
    return terminal == NULL ? 0 : terminal->screen_top;
}

int vt100_cursor_line(const struct vt100_terminal *terminal)
{
    return terminal == NULL ? 0 : terminal->cursor_line;
}

int vt100_cursor_column(const struct vt100_terminal *terminal)
{
    return terminal == NULL ? 0 : terminal->cursor_column;
}

int vt100_cursor_visible(const struct vt100_terminal *terminal)
{
    return terminal != NULL && terminal->cursor_visible;
}

uint8_t vt100_current_foreground(const struct vt100_terminal *terminal)
{
    uint8_t color = terminal == NULL ? VT100_DEFAULT_FOREGROUND :
                    terminal->foreground;
    uint8_t background = terminal == NULL ? VT100_DEFAULT_BACKGROUND :
                         terminal->background;

    if (terminal != NULL &&
        (terminal->attributes & VT100_ATTR_BOLD) && color < 8)
        color += 8;
    if (terminal != NULL && terminal->inverse)
        color = background;
    return color;
}

uint8_t vt100_current_background(const struct vt100_terminal *terminal)
{
    uint8_t background = terminal == NULL ? VT100_DEFAULT_BACKGROUND :
                         terminal->background;

    if (terminal != NULL && terminal->inverse) {
        background = terminal->foreground;
        if ((terminal->attributes & VT100_ATTR_BOLD) && background < 8)
            background += 8;
    }
    return background;
}

uint8_t vt100_current_attributes(const struct vt100_terminal *terminal)
{
    return terminal == NULL ? 0 : terminal->attributes;
}

uint32_t vt100_palette_color(uint8_t color)
{
    static const uint32_t palette[16] = {
        0x00000000U, 0x00aa0000U, 0x0000aa00U, 0x00aa5500U,
        0x000000aaU, 0x00aa00aaU, 0x0000aaaaU, 0x00aaaaaaU,
        0x00555555U, 0x00ff5555U, 0x0055ff55U, 0x00ffff55U,
        0x005555ffU, 0x00ff55ffU, 0x0055ffffU, 0x00ffffffU
    };

    return palette[color & 15];
}
