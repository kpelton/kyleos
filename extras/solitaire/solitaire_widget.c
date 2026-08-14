#include "solitaire.h"

#include <stdio.h>
#include <string.h>

#define CARD_RED 0x00c00000U
#define FELT_GREEN 0x00006020U
#define FELT_LIGHT 0x00008030U
#define SELECT_YELLOW 0x00ffff00U
#define COURT_GOLD 0x00e0b020U
#define COURT_SKIN 0x00f0c090U
#define COURT_BLUE 0x000050c0U

#define SOURCE_NONE (-1)
#define SOURCE_WASTE 7
#define SOURCE_FOUNDATION 8

struct solitaire_layout {
    int card_w;
    int card_h;
    int gap;
    int left;
    int top;
    int tableau_y;
    int overlap;
    int new_x;
    int new_y;
    int new_w;
    int new_h;
};

static uint32_t next_random(struct solitaire_game *game)
{
    game->random_state = game->random_state * 1664525U + 1013904223U;
    return game->random_state;
}

static int red_suit(int suit)
{
    return suit == 1 || suit == 2;
}

static struct solitaire_layout calculate_layout(
    const struct solitaire_game *game, int x, int y, int width, int height)
{
    struct solitaire_layout layout;
    int maximum = 1;
    int available;

    layout.card_w = (width - 52) / SOLITAIRE_TABLEAUS;
    if (layout.card_w > 64) layout.card_w = 64;
    if (layout.card_w < 24) layout.card_w = 24;
    layout.card_h = layout.card_w * 4 / 3;
    layout.gap = (width - 16 - layout.card_w * SOLITAIRE_TABLEAUS) / 6;
    if (layout.gap < 2) layout.gap = 2;
    if (layout.gap > 16) layout.gap = 16;
    layout.left = x + (width - (layout.card_w * SOLITAIRE_TABLEAUS +
                  layout.gap * 6)) / 2;
    layout.top = y + 4;
    layout.tableau_y = y + layout.card_h + 13;
    for (int column = 0; column < SOLITAIRE_TABLEAUS; column++)
        if (game->tableau_count[column] > maximum)
            maximum = game->tableau_count[column];
    available = y + height - 19 - layout.tableau_y - layout.card_h;
    layout.overlap = maximum > 1 ? available / (maximum - 1) : 18;
    if (layout.overlap > 18) layout.overlap = 18;
    if (layout.overlap < 4) layout.overlap = 4;
    layout.new_w = 80;
    layout.new_h = 20;
    layout.new_x = x + (width - layout.new_w) / 2;
    layout.new_y = y + height - 20;
    return layout;
}

static int column_x(const struct solitaire_layout *layout, int column)
{
    return layout->left + column * (layout->card_w + layout->gap);
}

static void clear_selection(struct solitaire_game *game)
{
    game->selected_source = SOURCE_NONE;
    game->selected_index = -1;
}

void solitaire_reset(struct solitaire_game *game, uint32_t seed)
{
    struct solitaire_card deck[SOLITAIRE_CARDS];
    int next = 0;

    memset(game, 0, sizeof(*game));
    game->random_state = seed ? seed : 1;
    clear_selection(game);
    for (int suit = 0; suit < 4; suit++) {
        for (int rank = 1; rank <= 13; rank++) {
            int index = suit * 13 + rank - 1;
            deck[index].rank = rank;
            deck[index].suit = suit;
            deck[index].face_up = 0;
        }
    }
    for (int i = SOLITAIRE_CARDS - 1; i > 0; i--) {
        int other = next_random(game) % (i + 1);
        struct solitaire_card card = deck[i];
        deck[i] = deck[other];
        deck[other] = card;
    }
    for (int column = 0; column < SOLITAIRE_TABLEAUS; column++) {
        for (int row = 0; row <= column; row++) {
            game->tableau[column][row] = deck[next++];
            game->tableau[column][row].face_up = row == column;
        }
        game->tableau_count[column] = column + 1;
    }
    while (next < SOLITAIRE_CARDS)
        game->stock[game->stock_count++] = deck[next++];
}

static int can_stack(const struct solitaire_card *moving,
                     const struct solitaire_card *target)
{
    return moving->face_up && target->face_up &&
           moving->rank + 1 == target->rank &&
           red_suit(moving->suit) != red_suit(target->suit);
}

static struct solitaire_card *selected_card(struct solitaire_game *game)
{
    if (game->selected_source == SOURCE_WASTE && game->waste_count > 0)
        return &game->waste[game->waste_count - 1];
    if (game->selected_source >= 0 &&
        game->selected_source < SOLITAIRE_TABLEAUS &&
        game->selected_index >= 0 &&
        game->selected_index < game->tableau_count[game->selected_source])
        return &game->tableau[game->selected_source][game->selected_index];
    if (game->selected_source >= SOURCE_FOUNDATION &&
        game->selected_source < SOURCE_FOUNDATION + 4) {
        int pile = game->selected_source - SOURCE_FOUNDATION;
        if (game->foundation_count[pile] > 0)
            return &game->foundation[pile][game->foundation_count[pile] - 1];
    }
    return NULL;
}

static void remove_selected(struct solitaire_game *game, int count)
{
    if (game->selected_source == SOURCE_WASTE) {
        game->waste_count--;
    } else if (game->selected_source >= 0 &&
               game->selected_source < SOLITAIRE_TABLEAUS) {
        int source = game->selected_source;
        game->tableau_count[source] -= count;
        if (game->tableau_count[source] > 0)
            game->tableau[source][game->tableau_count[source] - 1].face_up = 1;
    } else if (game->selected_source >= SOURCE_FOUNDATION) {
        game->foundation_count[game->selected_source - SOURCE_FOUNDATION]--;
    }
}

static int move_to_tableau(struct solitaire_game *game, int destination)
{
    struct solitaire_card *moving = selected_card(game);
    int count = 1;

    if (moving == NULL || destination == game->selected_source) return 0;
    if (game->tableau_count[destination] == 0) {
        if (moving->rank != 13) return 0;
    } else if (!can_stack(moving,
               &game->tableau[destination]
                              [game->tableau_count[destination] - 1])) {
        return 0;
    }
    if (game->selected_source >= 0 &&
        game->selected_source < SOLITAIRE_TABLEAUS)
        count = game->tableau_count[game->selected_source] -
                game->selected_index;
    if (game->tableau_count[destination] + count > SOLITAIRE_CARDS)
        return 0;
    for (int i = 0; i < count; i++)
        game->tableau[destination][game->tableau_count[destination] + i] =
            moving[i];
    game->tableau_count[destination] += count;
    remove_selected(game, count);
    clear_selection(game);
    return 1;
}

static int move_to_foundation(struct solitaire_game *game, int destination)
{
    struct solitaire_card *moving = selected_card(game);
    int count = 1;

    if (moving == NULL) return 0;
    if (game->selected_source >= 0 &&
        game->selected_source < SOLITAIRE_TABLEAUS)
        count = game->tableau_count[game->selected_source] -
                game->selected_index;
    if (count != 1 || moving->suit != destination ||
        moving->rank != game->foundation_count[destination] + 1)
        return 0;
    game->foundation[destination][game->foundation_count[destination]++] =
        *moving;
    remove_selected(game, 1);
    clear_selection(game);
    game->won = 1;
    for (int pile = 0; pile < 4; pile++)
        if (game->foundation_count[pile] != 13) game->won = 0;
    return 1;
}

static void draw_slot(struct kui_surface *surface, int x, int y, int w, int h)
{
    kui_outline_rect(surface, x, y, w, h, FELT_LIGHT);
    kui_outline_rect(surface, x + 2, y + 2, w - 4, h - 4, FELT_LIGHT);
}

static void rank_label(const struct solitaire_card *card, char label[3])
{
    static const char ranks[] = " A23456789 JQK";

    if (card->rank == 10) {
        label[0] = '1';
        label[1] = '0';
        label[2] = '\0';
    } else {
        label[0] = ranks[card->rank];
        label[1] = '\0';
    }
}

static void draw_suit(struct kui_surface *surface, int x, int y,
                      int scale, int suit, uint32_t color)
{
    static const uint8_t images[4][7] = {
        { 0x1c, 0x1c, 0x6b, 0x7f, 0x1c, 0x3e, 0x00 }, /* club */
        { 0x08, 0x1c, 0x3e, 0x7f, 0x3e, 0x1c, 0x08 }, /* diamond */
        { 0x36, 0x7f, 0x7f, 0x7f, 0x3e, 0x1c, 0x08 }, /* heart */
        { 0x08, 0x1c, 0x3e, 0x7f, 0x6b, 0x1c, 0x3e }  /* spade */
    };

    if (scale < 1) scale = 1;
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 7; col++)
            if (images[suit][row] & (0x40 >> col))
                kui_fill_rect(surface, x + col * scale, y + row * scale,
                              scale, scale, color);
}

static void draw_pips(struct kui_surface *surface,
                      const struct solitaire_card *card,
                      int x, int y, int w, int h, uint32_t color)
{
    static const uint16_t masks[11] = {
        0,
        1U << 7,
        (1U << 1) | (1U << 13),
        (1U << 1) | (1U << 7) | (1U << 13),
        (1U << 0) | (1U << 2) | (1U << 12) | (1U << 14),
        (1U << 0) | (1U << 2) | (1U << 7) | (1U << 12) | (1U << 14),
        (1U << 0) | (1U << 2) | (1U << 6) | (1U << 8) |
            (1U << 12) | (1U << 14),
        (1U << 0) | (1U << 2) | (1U << 4) | (1U << 6) |
            (1U << 8) | (1U << 12) | (1U << 14),
        (1U << 0) | (1U << 2) | (1U << 4) | (1U << 6) |
            (1U << 8) | (1U << 10) | (1U << 12) | (1U << 14),
        (1U << 0) | (1U << 2) | (1U << 3) | (1U << 5) |
            (1U << 7) | (1U << 9) | (1U << 11) | (1U << 12) |
            (1U << 14),
        (1U << 0) | (1U << 2) | (1U << 3) | (1U << 5) |
            (1U << 6) | (1U << 8) | (1U << 9) | (1U << 11) |
            (1U << 12) | (1U << 14)
    };
    int area_y = y + 20;
    int area_h = h - 40;
    int scale = w >= 58 ? 2 : 1;

    if (card->rank == 1 && area_h >= 20) {
        int ace_scale = w >= 58 ? 3 : 2;
        draw_suit(surface,
                  x + w / 2 - 7 * ace_scale / 2,
                  area_y + area_h / 2 - 7 * ace_scale / 2,
                  ace_scale, card->suit, color);
    } else if (card->rank >= 2 && card->rank <= 10 && area_h >= 20) {
        uint16_t mask = masks[card->rank];
        for (int row = 0; row < 5; row++) {
            for (int col = 0; col < 3; col++) {
                int bit = row * 3 + col;
                if (mask & (1U << bit)) {
                    int cx = x + (col + 1) * w / 4;
                    int cy = area_y + (row + 1) * area_h / 6;
                    draw_suit(surface, cx - 7 * scale / 2,
                              cy - 7 * scale / 2,
                              scale, card->suit, color);
                }
            }
        }
    } else if (card->rank >= 11) {
        char face[2] = { card->rank == 11 ? 'J' :
                         card->rank == 12 ? 'Q' : 'K', '\0' };
        int portrait_x = x + w / 4;
        int portrait_y = y + h / 4;
        int portrait_w = w / 2;
        int portrait_h = h / 2;
        int center = portrait_x + portrait_w / 2;
        uint32_t robe = red_suit(card->suit) ? CARD_RED : COURT_BLUE;

        kui_fill_rect(surface, portrait_x, portrait_y,
                      portrait_w, portrait_h, 0x00f0e8c8U);
        kui_outline_rect(surface, portrait_x, portrait_y,
                         portrait_w, portrait_h, color);
        kui_fill_rect(surface, center - 5, portrait_y + 10,
                      10, 12, COURT_SKIN);
        kui_fill_rect(surface, center - 8, portrait_y + 22,
                      16, portrait_h - 24, robe);
        if (card->rank == 11) {
            kui_fill_rect(surface, center - 7, portrait_y + 7,
                          14, 4, COURT_BLUE);
            kui_fill_rect(surface, center + 5, portrait_y + 3,
                          2, 6, COURT_GOLD);
        } else {
            kui_fill_rect(surface, center - 7, portrait_y + 5,
                          14, 5, COURT_GOLD);
            kui_pixel(surface, center - 6, portrait_y + 3, COURT_GOLD);
            kui_pixel(surface, center, portrait_y + 2, COURT_GOLD);
            kui_pixel(surface, center + 6, portrait_y + 3, COURT_GOLD);
            if (card->rank == 13) {
                kui_fill_rect(surface, center - 5, portrait_y + 20,
                              10, 4, KUI_WHITE);
                kui_pixel(surface, center, portrait_y + 24, KUI_WHITE);
            }
        }
        kui_draw_text(surface, portrait_x + 2,
                      portrait_y + portrait_h - 18,
                      face, color, 0x00f0e8c8U, 0);
        draw_suit(surface, center - 3, portrait_y + portrait_h - 11,
                  1, card->suit, color);
    }
}

static void draw_card(struct kui_surface *surface,
                      const struct solitaire_card *card,
                      int x, int y, int w, int h, int selected)
{
    if (!card->face_up) {
        kui_fill_rect(surface, x, y, w, h, KUI_WHITE);
        kui_outline_rect(surface, x, y, w, h, KUI_BLACK);
        kui_fill_rect(surface, x + 3, y + 3, w - 6, h - 6,
                      KUI_TITLE_BLUE);
        for (int row = y + 5; row < y + h - 4; row += 5)
            for (int col = x + 5; col < x + w - 4; col += 5)
                kui_pixel(surface, col, row, KUI_WHITE);
    } else {
        char label[3];
        uint32_t color = red_suit(card->suit) ? CARD_RED : KUI_BLACK;
        kui_fill_rect(surface, x, y, w, h, KUI_WHITE);
        kui_outline_rect(surface, x, y, w, h, KUI_BLACK);
        rank_label(card, label);
        kui_draw_text(surface, x + 3, y + 2, label,
                      color, KUI_WHITE, 0);
        draw_suit(surface, x + w - 10, y + 4, 1, card->suit, color);
        if (h > 40) {
            kui_draw_text(surface, x + w - kui_text_width(label) - 3,
                          y + h - 18, label,
                          color, KUI_WHITE, 0);
            draw_pips(surface, card, x, y, w, h, color);
        }
    }
    if (selected)
        kui_outline_rect(surface, x - 2, y - 2, w + 4, h + 4,
                         SELECT_YELLOW);
}

void solitaire_draw(struct kui_surface *surface,
                    const struct solitaire_game *game,
                    int x, int y, int width, int height)
{
    struct solitaire_layout layout =
        calculate_layout(game, x, y, width, height);

    kui_fill_rect(surface, x, y, width, height, FELT_GREEN);
    draw_slot(surface, column_x(&layout, 0), layout.top,
              layout.card_w, layout.card_h);
    if (game->stock_count > 0)
        draw_card(surface, &game->stock[game->stock_count - 1],
                  column_x(&layout, 0), layout.top,
                  layout.card_w, layout.card_h, 0);
    draw_slot(surface, column_x(&layout, 1), layout.top,
              layout.card_w, layout.card_h);
    if (game->waste_count > 0)
        draw_card(surface, &game->waste[game->waste_count - 1],
                  column_x(&layout, 1), layout.top,
                  layout.card_w, layout.card_h,
                  game->selected_source == SOURCE_WASTE);
    for (int pile = 0; pile < 4; pile++) {
        int px = column_x(&layout, pile + 3);
        draw_slot(surface, px, layout.top, layout.card_w, layout.card_h);
        if (game->foundation_count[pile] > 0)
            draw_card(surface,
                      &game->foundation[pile]
                                       [game->foundation_count[pile] - 1],
                      px, layout.top, layout.card_w, layout.card_h,
                      game->selected_source == SOURCE_FOUNDATION + pile);
    }
    for (int column = 0; column < SOLITAIRE_TABLEAUS; column++) {
        int px = column_x(&layout, column);
        if (game->tableau_count[column] == 0)
            draw_slot(surface, px, layout.tableau_y,
                      layout.card_w, layout.card_h);
        for (int row = 0; row < game->tableau_count[column]; row++)
            draw_card(surface, &game->tableau[column][row], px,
                      layout.tableau_y + row * layout.overlap,
                      layout.card_w, layout.card_h,
                      game->selected_source == column &&
                      game->selected_index == row);
    }
    kui_draw_button(surface, layout.new_x, layout.new_y,
                    layout.new_w, layout.new_h,
                    game->won ? "You Win!" : "New Deal", 0);
}

static int point_in_card(int px, int py, int x, int y,
                         const struct solitaire_layout *layout)
{
    return kui_in_rect(px, py, x, y, layout->card_w, layout->card_h);
}

int solitaire_click(struct solitaire_game *game,
                    int x, int y, int width, int height,
                    int pointer_x, int pointer_y, uint32_t seed)
{
    struct solitaire_layout layout =
        calculate_layout(game, x, y, width, height);

    if (kui_in_rect(pointer_x, pointer_y, layout.new_x, layout.new_y,
                    layout.new_w, layout.new_h)) {
        solitaire_reset(game, seed);
        return 1;
    }
    if (point_in_card(pointer_x, pointer_y, column_x(&layout, 0),
                      layout.top, &layout)) {
        clear_selection(game);
        if (game->stock_count > 0) {
            struct solitaire_card card = game->stock[--game->stock_count];
            card.face_up = 1;
            game->waste[game->waste_count++] = card;
        } else {
            while (game->waste_count > 0) {
                struct solitaire_card card = game->waste[--game->waste_count];
                card.face_up = 0;
                game->stock[game->stock_count++] = card;
            }
        }
        return 1;
    }
    if (point_in_card(pointer_x, pointer_y, column_x(&layout, 1),
                      layout.top, &layout) && game->waste_count > 0) {
        if (game->selected_source == SOURCE_WASTE) clear_selection(game);
        else {
            game->selected_source = SOURCE_WASTE;
            game->selected_index = game->waste_count - 1;
        }
        return 1;
    }
    for (int pile = 0; pile < 4; pile++) {
        if (!point_in_card(pointer_x, pointer_y,
                           column_x(&layout, pile + 3), layout.top,
                           &layout)) continue;
        if (game->selected_source != SOURCE_NONE) {
            if (!move_to_foundation(game, pile)) clear_selection(game);
        } else if (game->foundation_count[pile] > 0) {
            game->selected_source = SOURCE_FOUNDATION + pile;
            game->selected_index = game->foundation_count[pile] - 1;
        }
        return 1;
    }
    for (int column = 0; column < SOLITAIRE_TABLEAUS; column++) {
        int px = column_x(&layout, column);
        int hit = -1;
        for (int row = game->tableau_count[column] - 1; row >= 0; row--) {
            if (point_in_card(pointer_x, pointer_y, px,
                              layout.tableau_y + row * layout.overlap,
                              &layout)) {
                hit = row;
                break;
            }
        }
        if (hit < 0 && game->tableau_count[column] == 0 &&
            point_in_card(pointer_x, pointer_y, px, layout.tableau_y,
                          &layout))
            hit = 0;
        if (hit < 0) continue;
        if (game->selected_source != SOURCE_NONE) {
            if (!move_to_tableau(game, column)) clear_selection(game);
        } else if (game->tableau_count[column] > 0) {
            struct solitaire_card *card = &game->tableau[column][hit];
            if (!card->face_up && hit == game->tableau_count[column] - 1)
                card->face_up = 1;
            else if (card->face_up) {
                game->selected_source = column;
                game->selected_index = hit;
            }
        }
        return 1;
    }
    clear_selection(game);
    return 0;
}
