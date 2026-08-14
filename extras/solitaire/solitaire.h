#ifndef KYLEOS_SOLITAIRE_H
#define KYLEOS_SOLITAIRE_H

#include <stdint.h>

#include "extras/kui/kui.h"

#define SOLITAIRE_TABLEAUS 7
#define SOLITAIRE_FOUNDATIONS 4
#define SOLITAIRE_CARDS 52

struct solitaire_card {
    uint8_t rank;
    uint8_t suit;
    uint8_t face_up;
};

struct solitaire_game {
    struct solitaire_card stock[SOLITAIRE_CARDS];
    struct solitaire_card waste[SOLITAIRE_CARDS];
    struct solitaire_card tableau[SOLITAIRE_TABLEAUS][SOLITAIRE_CARDS];
    struct solitaire_card foundation[SOLITAIRE_FOUNDATIONS][13];
    int stock_count;
    int waste_count;
    int tableau_count[SOLITAIRE_TABLEAUS];
    int foundation_count[SOLITAIRE_FOUNDATIONS];
    int selected_source;
    int selected_index;
    int won;
    uint32_t random_state;
};

void solitaire_reset(struct solitaire_game *game, uint32_t seed);
void solitaire_draw(struct kui_surface *surface,
                    const struct solitaire_game *game,
                    int x, int y, int width, int height);
int solitaire_click(struct solitaire_game *game,
                    int x, int y, int width, int height,
                    int pointer_x, int pointer_y, uint32_t seed);

#endif
