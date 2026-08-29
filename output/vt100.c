#include <output/vt100.h>

#define KVT_DEFAULT_FOREGROUND 10
#define KVT_DEFAULT_BACKGROUND 0

enum kvt_parser_state {
    KVT_GROUND,
    KVT_ESCAPE,
    KVT_CSI,
    KVT_OSC,
    KVT_OSC_ESCAPE,
    KVT_CHARSET
};

static int clamp(int value, int minimum, int maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void copy_cells(struct kvt_cell destination[KVT_ROWS][KVT_COLUMNS],
                       struct kvt_cell source[KVT_ROWS][KVT_COLUMNS])
{
    for (int row = 0; row < KVT_ROWS; row++)
        for (int column = 0; column < KVT_COLUMNS; column++)
            destination[row][column] = source[row][column];
}

static void blank_cell(struct kvt_terminal *terminal, struct kvt_cell *cell)
{
    uint8_t foreground = terminal->foreground;
    uint8_t background = terminal->background;

    if ((terminal->attributes & KVT_ATTR_BOLD) && foreground < 8)
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

static void clear_line(struct kvt_terminal *terminal, int row)
{
    for (int column = 0; column < KVT_COLUMNS; column++)
        blank_cell(terminal, &terminal->cells[row][column]);
    terminal->dirty_rows[row] = true;
}

static void dirty_all(struct kvt_terminal *terminal)
{
    for (int row = 0; row < KVT_ROWS; row++)
        terminal->dirty_rows[row] = true;
}

void kvt_clear(struct kvt_terminal *terminal)
{
    terminal->cursor_row = 0;
    terminal->cursor_column = 0;
    terminal->wrap_pending = false;
    for (int row = 0; row < KVT_ROWS; row++) clear_line(terminal, row);
}

void kvt_init(struct kvt_terminal *terminal)
{
    terminal->foreground = KVT_DEFAULT_FOREGROUND;
    terminal->background = KVT_DEFAULT_BACKGROUND;
    terminal->attributes = 0;
    terminal->inverse = false;
    terminal->cursor_visible = true;
    terminal->autowrap = true;
    terminal->parser_state = KVT_GROUND;
    terminal->parameter_count = 0;
    terminal->private_sequence = false;
    terminal->alternate_active = false;
    terminal->saved_row = terminal->saved_column = 0;
    kvt_clear(terminal);
}

static void scroll_up(struct kvt_terminal *terminal, int amount)
{
    while (amount-- > 0) {
        for (int row = 1; row < KVT_ROWS; row++)
            for (int column = 0; column < KVT_COLUMNS; column++)
                terminal->cells[row - 1][column] =
                    terminal->cells[row][column];
        clear_line(terminal, KVT_ROWS - 1);
    }
    dirty_all(terminal);
}

static void scroll_down(struct kvt_terminal *terminal, int amount)
{
    while (amount-- > 0) {
        for (int row = KVT_ROWS - 1; row > 0; row--)
            for (int column = 0; column < KVT_COLUMNS; column++)
                terminal->cells[row][column] =
                    terminal->cells[row - 1][column];
        clear_line(terminal, 0);
    }
    dirty_all(terminal);
}

static void linefeed(struct kvt_terminal *terminal)
{
    terminal->wrap_pending = false;
    if (terminal->cursor_row == KVT_ROWS - 1)
        scroll_up(terminal, 1);
    else
        terminal->cursor_row++;
}

static int parameter(struct kvt_terminal *terminal, int index, int fallback)
{
    if (index < 0 || index >= terminal->parameter_count ||
        terminal->parameters[index] < 0)
        return fallback;
    return terminal->parameters[index];
}

static void erase_line(struct kvt_terminal *terminal, int mode)
{
    int first = mode == 0 ? terminal->cursor_column : 0;
    int last = mode == 1 ? terminal->cursor_column : KVT_COLUMNS - 1;
    if (mode == 2) { first = 0; last = KVT_COLUMNS - 1; }
    for (int column = first; column <= last; column++)
        blank_cell(terminal, &terminal->cells[terminal->cursor_row][column]);
    terminal->dirty_rows[terminal->cursor_row] = true;
}

static void erase_display(struct kvt_terminal *terminal, int mode)
{
    if (mode == 2 || mode == 3) {
        for (int row = 0; row < KVT_ROWS; row++) clear_line(terminal, row);
    } else if (mode == 0) {
        erase_line(terminal, 0);
        for (int row = terminal->cursor_row + 1; row < KVT_ROWS; row++)
            clear_line(terminal, row);
    } else if (mode == 1) {
        for (int row = 0; row < terminal->cursor_row; row++)
            clear_line(terminal, row);
        erase_line(terminal, 1);
    }
}

static void set_graphics(struct kvt_terminal *terminal)
{
    int count = terminal->parameter_count == 0 ? 1 :
                terminal->parameter_count;

    for (int index = 0; index < count; index++) {
        int value = parameter(terminal, index, 0);
        if (value == 0) {
            terminal->foreground = KVT_DEFAULT_FOREGROUND;
            terminal->background = KVT_DEFAULT_BACKGROUND;
            terminal->attributes = 0;
            terminal->inverse = false;
        } else if (value == 1) terminal->attributes |= KVT_ATTR_BOLD;
        else if (value == 22) terminal->attributes &= ~KVT_ATTR_BOLD;
        else if (value == 4) terminal->attributes |= KVT_ATTR_UNDERLINE;
        else if (value == 24) terminal->attributes &= ~KVT_ATTR_UNDERLINE;
        else if (value == 7) terminal->inverse = true;
        else if (value == 27) terminal->inverse = false;
        else if (value == 8) terminal->attributes |= KVT_ATTR_CONCEAL;
        else if (value == 28) terminal->attributes &= ~KVT_ATTR_CONCEAL;
        else if (value >= 30 && value <= 37)
            terminal->foreground = value - 30;
        else if (value == 39)
            terminal->foreground = KVT_DEFAULT_FOREGROUND;
        else if (value >= 40 && value <= 47)
            terminal->background = value - 40;
        else if (value == 49)
            terminal->background = KVT_DEFAULT_BACKGROUND;
        else if (value >= 90 && value <= 97)
            terminal->foreground = value - 90 + 8;
        else if (value >= 100 && value <= 107)
            terminal->background = value - 100 + 8;
    }
}

static void insert_lines(struct kvt_terminal *terminal, int amount)
{
    int row = terminal->cursor_row;
    amount = clamp(amount, 1, KVT_ROWS - row);
    for (int target = KVT_ROWS - 1; target >= row + amount; target--)
        for (int column = 0; column < KVT_COLUMNS; column++)
            terminal->cells[target][column] =
                terminal->cells[target - amount][column];
    for (int target = row; target < row + amount; target++)
        clear_line(terminal, target);
    for (int target = row; target < KVT_ROWS; target++)
        terminal->dirty_rows[target] = true;
}

static void delete_lines(struct kvt_terminal *terminal, int amount)
{
    int row = terminal->cursor_row;
    amount = clamp(amount, 1, KVT_ROWS - row);
    for (int target = row; target < KVT_ROWS - amount; target++)
        for (int column = 0; column < KVT_COLUMNS; column++)
            terminal->cells[target][column] =
                terminal->cells[target + amount][column];
    for (int target = KVT_ROWS - amount; target < KVT_ROWS; target++)
        clear_line(terminal, target);
    for (int target = row; target < KVT_ROWS; target++)
        terminal->dirty_rows[target] = true;
}

static void insert_characters(struct kvt_terminal *terminal, int amount)
{
    int column = terminal->cursor_column;
    amount = clamp(amount, 1, KVT_COLUMNS - column);
    for (int target = KVT_COLUMNS - 1; target >= column + amount; target--)
        terminal->cells[terminal->cursor_row][target] =
            terminal->cells[terminal->cursor_row][target - amount];
    for (int target = column; target < column + amount; target++)
        blank_cell(terminal, &terminal->cells[terminal->cursor_row][target]);
    terminal->dirty_rows[terminal->cursor_row] = true;
}

static void delete_characters(struct kvt_terminal *terminal, int amount)
{
    int column = terminal->cursor_column;
    amount = clamp(amount, 1, KVT_COLUMNS - column);
    for (int target = column; target < KVT_COLUMNS - amount; target++)
        terminal->cells[terminal->cursor_row][target] =
            terminal->cells[terminal->cursor_row][target + amount];
    for (int target = KVT_COLUMNS - amount; target < KVT_COLUMNS; target++)
        blank_cell(terminal, &terminal->cells[terminal->cursor_row][target]);
    terminal->dirty_rows[terminal->cursor_row] = true;
}

static void alternate_screen(struct kvt_terminal *terminal, bool enabled)
{
    if (enabled && !terminal->alternate_active) {
        copy_cells(terminal->saved_cells, terminal->cells);
        terminal->primary_cursor_row = terminal->cursor_row;
        terminal->primary_cursor_column = terminal->cursor_column;
        terminal->primary_saved_row = terminal->saved_row;
        terminal->primary_saved_column = terminal->saved_column;
        terminal->primary_foreground = terminal->foreground;
        terminal->primary_background = terminal->background;
        terminal->primary_attributes = terminal->attributes;
        terminal->primary_inverse = terminal->inverse;
        terminal->primary_cursor_visible = terminal->cursor_visible;
        terminal->primary_autowrap = terminal->autowrap;
        terminal->alternate_active = true;
        terminal->foreground = KVT_DEFAULT_FOREGROUND;
        terminal->background = KVT_DEFAULT_BACKGROUND;
        terminal->attributes = 0;
        terminal->inverse = false;
        terminal->cursor_visible = true;
        terminal->autowrap = true;
        kvt_clear(terminal);
    } else if (!enabled && terminal->alternate_active) {
        copy_cells(terminal->cells, terminal->saved_cells);
        terminal->cursor_row = terminal->primary_cursor_row;
        terminal->cursor_column = terminal->primary_cursor_column;
        terminal->saved_row = terminal->primary_saved_row;
        terminal->saved_column = terminal->primary_saved_column;
        terminal->foreground = terminal->primary_foreground;
        terminal->background = terminal->primary_background;
        terminal->attributes = terminal->primary_attributes;
        terminal->inverse = terminal->primary_inverse;
        terminal->cursor_visible = terminal->primary_cursor_visible;
        terminal->autowrap = terminal->primary_autowrap;
        terminal->wrap_pending = false;
        terminal->alternate_active = false;
        dirty_all(terminal);
    }
}

static void execute_csi(struct kvt_terminal *terminal, uint8_t command)
{
    int amount = parameter(terminal, 0, 1);
    int row;
    int column;
    if (amount < 1) amount = 1;

    if (command == 'A') terminal->cursor_row = clamp(terminal->cursor_row - amount, 0, KVT_ROWS - 1);
    else if (command == 'B' || command == 'e') terminal->cursor_row = clamp(terminal->cursor_row + amount, 0, KVT_ROWS - 1);
    else if (command == 'C' || command == 'a') terminal->cursor_column = clamp(terminal->cursor_column + amount, 0, KVT_COLUMNS - 1);
    else if (command == 'D') terminal->cursor_column = clamp(terminal->cursor_column - amount, 0, KVT_COLUMNS - 1);
    else if (command == 'E') { terminal->cursor_row = clamp(terminal->cursor_row + amount, 0, KVT_ROWS - 1); terminal->cursor_column = 0; }
    else if (command == 'F') { terminal->cursor_row = clamp(terminal->cursor_row - amount, 0, KVT_ROWS - 1); terminal->cursor_column = 0; }
    else if (command == 'G' || command == '`') terminal->cursor_column = clamp(parameter(terminal, 0, 1) - 1, 0, KVT_COLUMNS - 1);
    else if (command == 'H' || command == 'f') {
        row = parameter(terminal, 0, 1) - 1;
        column = parameter(terminal, 1, 1) - 1;
        terminal->cursor_row = clamp(row, 0, KVT_ROWS - 1);
        terminal->cursor_column = clamp(column, 0, KVT_COLUMNS - 1);
    } else if (command == 'd') terminal->cursor_row = clamp(parameter(terminal, 0, 1) - 1, 0, KVT_ROWS - 1);
    else if (command == 'J') erase_display(terminal, parameter(terminal, 0, 0));
    else if (command == 'K') erase_line(terminal, parameter(terminal, 0, 0));
    else if (command == 'L') insert_lines(terminal, amount);
    else if (command == 'M') delete_lines(terminal, amount);
    else if (command == '@') insert_characters(terminal, amount);
    else if (command == 'P') delete_characters(terminal, amount);
    else if (command == 'X') {
        for (int index = 0; index < amount && terminal->cursor_column + index < KVT_COLUMNS; index++)
            blank_cell(terminal, &terminal->cells[terminal->cursor_row][terminal->cursor_column + index]);
        terminal->dirty_rows[terminal->cursor_row] = true;
    } else if (command == 'S') scroll_up(terminal, amount);
    else if (command == 'T') scroll_down(terminal, amount);
    else if (command == 'm') set_graphics(terminal);
    else if (command == 's') { terminal->saved_row = terminal->cursor_row; terminal->saved_column = terminal->cursor_column; }
    else if (command == 'u') { terminal->cursor_row = clamp(terminal->saved_row, 0, KVT_ROWS - 1); terminal->cursor_column = clamp(terminal->saved_column, 0, KVT_COLUMNS - 1); }
    else if ((command == 'h' || command == 'l') && terminal->private_sequence) {
        bool enabled = command == 'h';
        if (parameter(terminal, 0, 0) == 25) terminal->cursor_visible = enabled;
        else if (parameter(terminal, 0, 0) == 7) terminal->autowrap = enabled;
        else if (parameter(terminal, 0, 0) == 1049) alternate_screen(terminal, enabled);
    }
    terminal->wrap_pending = false;
}

static void put_character(struct kvt_terminal *terminal, uint8_t character)
{
    struct kvt_cell *cell;
    uint8_t foreground = terminal->foreground;
    uint8_t background = terminal->background;

    if (terminal->wrap_pending) {
        if (terminal->autowrap) { terminal->cursor_column = 0; linefeed(terminal); }
        terminal->wrap_pending = false;
    }
    if ((terminal->attributes & KVT_ATTR_BOLD) && foreground < 8) foreground += 8;
    if (terminal->inverse) { uint8_t swap = foreground; foreground = background; background = swap; }
    cell = &terminal->cells[terminal->cursor_row][terminal->cursor_column];
    cell->character = character;
    cell->foreground = foreground;
    cell->background = background;
    cell->attributes = terminal->attributes;
    terminal->dirty_rows[terminal->cursor_row] = true;
    if (terminal->cursor_column == KVT_COLUMNS - 1) terminal->wrap_pending = terminal->autowrap;
    else terminal->cursor_column++;
}

static void feed(struct kvt_terminal *terminal, uint8_t character)
{
    if (terminal->parser_state == KVT_OSC) {
        if (character == 7) terminal->parser_state = KVT_GROUND;
        else if (character == 27) terminal->parser_state = KVT_OSC_ESCAPE;
        return;
    }
    if (terminal->parser_state == KVT_OSC_ESCAPE) { terminal->parser_state = character == '\\' ? KVT_GROUND : KVT_OSC; return; }
    if (terminal->parser_state == KVT_CHARSET) { terminal->parser_state = KVT_GROUND; return; }
    if (terminal->parser_state == KVT_ESCAPE) {
        terminal->parser_state = KVT_GROUND;
        if (character == '[') { terminal->parser_state = KVT_CSI; terminal->parameter_count = 0; terminal->private_sequence = false; }
        else if (character == ']') terminal->parser_state = KVT_OSC;
        else if (character == '(' || character == ')') terminal->parser_state = KVT_CHARSET;
        else if (character == '7') { terminal->saved_row = terminal->cursor_row; terminal->saved_column = terminal->cursor_column; }
        else if (character == '8') { terminal->cursor_row = clamp(terminal->saved_row, 0, KVT_ROWS - 1); terminal->cursor_column = clamp(terminal->saved_column, 0, KVT_COLUMNS - 1); }
        else if (character == 'D') linefeed(terminal);
        else if (character == 'E') { terminal->cursor_column = 0; linefeed(terminal); }
        else if (character == 'M') { if (terminal->cursor_row == 0) scroll_down(terminal, 1); else terminal->cursor_row--; }
        else if (character == 'c') kvt_init(terminal);
        return;
    }
    if (terminal->parser_state == KVT_CSI) {
        if (character == '?' && terminal->parameter_count == 0) { terminal->private_sequence = true; return; }
        if (character >= '0' && character <= '9') {
            if (terminal->parameter_count == 0) { terminal->parameter_count = 1; terminal->parameters[0] = 0; }
            if (terminal->parameters[terminal->parameter_count - 1] < 0) terminal->parameters[terminal->parameter_count - 1] = 0;
            terminal->parameters[terminal->parameter_count - 1] = terminal->parameters[terminal->parameter_count - 1] * 10 + character - '0';
            return;
        }
        if (character == ';') {
            if (terminal->parameter_count == 0) { terminal->parameter_count = 1; terminal->parameters[0] = -1; }
            if (terminal->parameter_count < 16) terminal->parameters[terminal->parameter_count++] = -1;
            return;
        }
        if (character >= '@' && character <= '~') { execute_csi(terminal, character); terminal->parser_state = KVT_GROUND; }
        return;
    }
    if (character == 27) terminal->parser_state = KVT_ESCAPE;
    else if (character == '\r') { terminal->cursor_column = 0; terminal->wrap_pending = false; }
    else if (character == '\n' || character == '\v' || character == '\f') { terminal->cursor_column = 0; linefeed(terminal); }
    else if (character == '\b') { terminal->wrap_pending = false; if (terminal->cursor_column > 0) terminal->cursor_column--; }
    else if (character == '\t') { terminal->wrap_pending = false; terminal->cursor_column = clamp((terminal->cursor_column + 8) & ~7, 0, KVT_COLUMNS - 1); }
    else if (character >= 32 && character != 127) put_character(terminal, character);
}

void kvt_write(struct kvt_terminal *terminal, const char *bytes,
               uint32_t length)
{
    if (terminal == NULL || bytes == NULL) return;
    for (uint32_t index = 0; index < length; index++)
        feed(terminal, (uint8_t)bytes[index]);
}

uint32_t kvt_palette_color(uint8_t color)
{
    static const uint32_t palette[16] = {
        0x00000000, 0x00aa0000, 0x0000aa00, 0x00aa5500,
        0x000000aa, 0x00aa00aa, 0x0000aaaa, 0x00aaaaaa,
        0x00555555, 0x00ff5555, 0x0055ff55, 0x00ffff55,
        0x005555ff, 0x00ff55ff, 0x0055ffff, 0x00ffffff
    };
    return palette[color & 15];
}
