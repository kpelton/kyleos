#include "calculator.h"

#include <stdio.h>
#include <string.h>

#define DISPLAY_BG 0x00f4f0dcU
#define ERROR_RED 0x00c00000U

struct calculator_layout {
    int left;
    int top;
    int width;
    int display_h;
    int button_w;
    int button_h;
    int gap;
};

static const char *button_labels[5][4] = {
    { "C", "BS", "%", "/" },
    { "7", "8", "9", "*" },
    { "4", "5", "6", "-" },
    { "1", "2", "3", "+" },
    { "+/-", "0", ".", "=" }
};

static const char button_keys[5][4] = {
    { 'c', '\b', '%', '/' },
    { '7', '8', '9', '*' },
    { '4', '5', '6', '-' },
    { '1', '2', '3', '+' },
    { 'n', '0', '.', '=' }
};

static struct calculator_layout calculate_layout(int x, int y,
                                                  int width, int height)
{
    struct calculator_layout layout;
    int wanted_w = width - 24;
    int wanted_h = height - 20;

    if (wanted_w > 390) wanted_w = 390;
    if (wanted_h > 500) wanted_h = 500;
    layout.gap = 6;
    layout.display_h = wanted_h / 6;
    if (layout.display_h < 42) layout.display_h = 42;
    if (layout.display_h > 72) layout.display_h = 72;
    layout.button_h = (wanted_h - layout.display_h - 5 * layout.gap) / 5;
    if (layout.button_h < 28) layout.button_h = 28;
    if (layout.button_h > 68) layout.button_h = 68;
    layout.button_w = (wanted_w - 3 * layout.gap) / 4;
    layout.width = layout.button_w * 4 + layout.gap * 3;
    layout.left = x + (width - layout.width) / 2;
    layout.top = y + (height - (layout.display_h + layout.gap +
                 layout.button_h * 5 + layout.gap * 4)) / 2;
    return layout;
}

void calculator_reset(struct calculator_state *state)
{
    memset(state, 0, sizeof(*state));
    state->decimal_factor = 1.0;
    state->pressed_row = -1;
    state->pressed_col = -1;
}

static int apply_pending(struct calculator_state *state)
{
    double right = state->value;
    double result = state->accumulator;

    if (state->pending_operator == '+') result += right;
    else if (state->pending_operator == '-') result -= right;
    else if (state->pending_operator == '*') result *= right;
    else if (state->pending_operator == '/') {
        if (right == 0) {
            state->error = 1;
            state->value = 0;
            state->pending_operator = 0;
            return -1;
        }
        result /= right;
    } else {
        result = right;
    }
    state->value = result;
    state->accumulator = result;
    return 0;
}

int calculator_key(struct calculator_state *state, char key)
{
    if (key == 'C') key = 'c';
    if (key == '\r' || key == '\n') key = '=';
    if (state->error && key != 'c') return 0;

    if (key >= '0' && key <= '9') {
        int digit = key - '0';
        if (!state->entering) {
            state->value = 0;
            state->entering = 1;
            state->decimal_mode = 0;
            state->decimal_factor = 1.0;
        }
        if (state->decimal_mode) {
            state->decimal_factor *= 0.1;
            state->value += (state->value < 0 ? -digit : digit) *
                            state->decimal_factor;
        } else {
            state->value = state->value * 10 +
                           (state->value < 0 ? -digit : digit);
        }
        return 1;
    }
    if (key == 'c') {
        calculator_reset(state);
        return 1;
    }
    if (key == '\b') {
        if (state->decimal_mode && state->decimal_factor > 1e-12) {
            state->value = (double)((long long)(state->value * 10.0)) / 10.0;
            state->decimal_factor *= 10.0;
            if (state->decimal_factor >= 1.0) {
                state->decimal_mode = 0;
                state->decimal_factor = 1.0;
            }
        } else state->value = (double)((long long)(state->value / 10.0));
        return 1;
    }
    if (key == '.') {
        if (!state->entering) {
            state->value = 0;
            state->entering = 1;
        }
        state->decimal_mode = 1;
        return 1;
    }
    if (key == 'n') {
        state->value = -state->value;
        return 1;
    }
    if (key == '%') {
        state->value /= 100;
        state->entering = 0;
        state->decimal_mode = 0;
        state->decimal_factor = 1.0;
        return 1;
    }
    if (key == '+' || key == '-' || key == '*' || key == '/') {
        if (state->pending_operator && state->entering &&
            apply_pending(state) < 0)
            return 1;
        else if (!state->pending_operator)
            state->accumulator = state->value;
        state->pending_operator = key;
        state->entering = 0;
        state->decimal_mode = 0;
        state->decimal_factor = 1.0;
        return 1;
    }
    if (key == '=') {
        if (state->pending_operator) apply_pending(state);
        state->pending_operator = 0;
        state->entering = 0;
        state->decimal_mode = 0;
        state->decimal_factor = 1.0;
        return 1;
    }
    return 0;
}

static void value_text(const struct calculator_state *state,
                       char *text, int length)
{
    if (state->error) snprintf(text, length, "DIVIDE BY ZERO");
    else snprintf(text, length, "%.12g", state->value);
}

void calculator_draw(struct kui_surface *surface,
                     const struct calculator_state *state,
                     int x, int y, int width, int height)
{
    struct calculator_layout layout = calculate_layout(x, y, width, height);
    char display[40];
    int display_y = layout.top;
    int buttons_y = display_y + layout.display_h + layout.gap;

    kui_fill_rect(surface, x, y, width, height, KUI_FACE);
    kui_draw_bevel(surface, layout.left, display_y, layout.width,
                   layout.display_h, DISPLAY_BG, 1);
    value_text(state, display, sizeof(display));
    kui_draw_text(surface,
                  layout.left + layout.width - kui_text_width(display) - 10,
                  display_y + (layout.display_h - 16) / 2,
                  display, state->error ? ERROR_RED : KUI_BLACK,
                  DISPLAY_BG, 0);
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 4; col++) {
            int bx = layout.left + col * (layout.button_w + layout.gap);
            int by = buttons_y + row * (layout.button_h + layout.gap);
            int bw = layout.button_w;
            if (row == 4 && col == 2) bw = layout.button_w * 2 + layout.gap;
            if (row == 4 && col == 3) continue;
            kui_draw_button(surface, bx, by, bw, layout.button_h,
                            button_labels[row][col],
                            state->pressed_row == row &&
                            state->pressed_col == col);
        }
    }
    if (state->pending_operator) {
        char pending[2] = { state->pending_operator, 0 };
        kui_draw_text(surface, layout.left + 8, display_y + 4,
                      pending, KUI_SHADOW, DISPLAY_BG, 0);
    }
}

int calculator_click(struct calculator_state *state,
                     int x, int y, int width, int height,
                     int pointer_x, int pointer_y)
{
    struct calculator_layout layout = calculate_layout(x, y, width, height);
    int buttons_y = layout.top + layout.display_h + layout.gap;

    state->pressed_row = -1;
    state->pressed_col = -1;

    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 4; col++) {
            int bx = layout.left + col * (layout.button_w + layout.gap);
            int by = buttons_y + row * (layout.button_h + layout.gap);
            int bw = layout.button_w;
            if (row == 4 && col == 2) bw = layout.button_w * 2 + layout.gap;
            if (row == 4 && col == 3) continue;
            if (kui_in_rect(pointer_x, pointer_y, bx, by,
                            bw, layout.button_h))
            {
                state->pressed_row = row;
                state->pressed_col = col;
                return calculator_key(state, button_keys[row][col]);
            }
        }
    }
    return 0;
}
