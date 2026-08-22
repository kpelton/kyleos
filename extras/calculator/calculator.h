#ifndef KYLEOS_CALCULATOR_H
#define KYLEOS_CALCULATOR_H

#include "extras/kui/kui.h"

struct calculator_state {
    double value;
    double accumulator;
    char pending_operator;
    int entering;
    int decimal_mode;
    double decimal_factor;
    int pressed_row;
    int pressed_col;
    int error;
};

void calculator_reset(struct calculator_state *state);
int calculator_key(struct calculator_state *state, char key);
void calculator_draw(struct kui_surface *surface,
                     const struct calculator_state *state,
                     int x, int y, int width, int height);
int calculator_click(struct calculator_state *state,
                     int x, int y, int width, int height,
                     int pointer_x, int pointer_y);

#endif
