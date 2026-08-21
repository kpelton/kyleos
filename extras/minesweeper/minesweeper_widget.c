#include "minesweeper.h"

#include <stdio.h>
#include <string.h>

#define RED 0x00c00000U
#define GREEN 0x00008000U
#define BLUE 0x000000ffU
#define CYAN 0x00008080U
#define BROWN 0x00804000U

struct minesweeper_layout {
    int board_x;
    int board_y;
    int cell_size;
    int restart_x;
    int restart_y;
    int restart_w;
    int restart_h;
};

static int cell_index(int col, int row)
{
    return row * MINESWEEPER_COLS + col;
}

static int valid_cell(int col, int row)
{
    return col >= 0 && col < MINESWEEPER_COLS &&
           row >= 0 && row < MINESWEEPER_ROWS;
}

static uint32_t next_random(struct minesweeper_game *game)
{
    game->random_state = game->random_state * 1664525U + 1013904223U;
    return game->random_state;
}

static struct minesweeper_layout calculate_layout(int x, int y,
                                                   int width, int height)
{
    struct minesweeper_layout layout;
    int horizontal = (width - 12) / MINESWEEPER_COLS;
    int vertical = (height - 58) / MINESWEEPER_ROWS;

    layout.cell_size = horizontal < vertical ? horizontal : vertical;
    if (layout.cell_size < 10) layout.cell_size = 10;
    if (layout.cell_size > 36) layout.cell_size = 36;
    layout.board_x = x +
        (width - layout.cell_size * MINESWEEPER_COLS) / 2;
    layout.board_y = y + 34;
    layout.restart_w = 104;
    layout.restart_h = 24;
    layout.restart_x = x + (width - layout.restart_w) / 2;
    layout.restart_y = y + 4;
    return layout;
}

void minesweeper_reset(struct minesweeper_game *game, uint32_t seed)
{
    memset(game, 0, sizeof(*game));
    game->random_state = seed ? seed : 1;
}

static void place_mines(struct minesweeper_game *game,
                        int safe_col, int safe_row)
{
    int placed = 0;

    while (placed < MINESWEEPER_MINES) {
        int index = next_random(game) % MINESWEEPER_CELLS;
        int col = index % MINESWEEPER_COLS;
        int row = index / MINESWEEPER_COLS;

        if (game->cells[index].mine ||
            (col >= safe_col - 1 && col <= safe_col + 1 &&
             row >= safe_row - 1 && row <= safe_row + 1))
            continue;
        game->cells[index].mine = 1;
        placed++;
    }

    for (int row = 0; row < MINESWEEPER_ROWS; row++) {
        for (int col = 0; col < MINESWEEPER_COLS; col++) {
            int count = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    if (valid_cell(col + dx, row + dy) &&
                        game->cells[cell_index(col + dx, row + dy)].mine)
                        count++;
            game->cells[cell_index(col, row)].adjacent = count;
        }
    }
    game->mines_placed = 1;
}

static void check_win(struct minesweeper_game *game)
{
    for (int i = 0; i < MINESWEEPER_CELLS; i++)
        if (!game->cells[i].mine && !game->cells[i].revealed) return;
    game->won = 1;
    game->game_over = 1;
    for (int i = 0; i < MINESWEEPER_CELLS; i++)
        if (game->cells[i].mine) game->cells[i].flagged = 1;
}

static void reveal_cell(struct minesweeper_game *game, int start_col,
                        int start_row)
{
    int stack[MINESWEEPER_CELLS];
    uint8_t queued[MINESWEEPER_CELLS];
    int stack_size = 0;
    int first;

    if (game->game_over || !valid_cell(start_col, start_row)) return;
    if (!game->mines_placed) place_mines(game, start_col, start_row);
    first = cell_index(start_col, start_row);
    if (game->cells[first].flagged || game->cells[first].revealed) return;
    if (game->cells[first].mine) {
        game->cells[first].revealed = 1;
        game->game_over = 1;
        for (int i = 0; i < MINESWEEPER_CELLS; i++)
            if (game->cells[i].mine) game->cells[i].revealed = 1;
        return;
    }

    memset(queued, 0, sizeof(queued));
    stack[stack_size++] = first;
    queued[first] = 1;
    while (stack_size > 0) {
        int index = stack[--stack_size];
        int col = index % MINESWEEPER_COLS;
        int row = index / MINESWEEPER_COLS;

        if (game->cells[index].revealed || game->cells[index].flagged)
            continue;
        game->cells[index].revealed = 1;
        if (game->cells[index].adjacent != 0) continue;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int next_col = col + dx;
                int next_row = row + dy;
                int next;
                if (!valid_cell(next_col, next_row)) continue;
                next = cell_index(next_col, next_row);
                if (!game->cells[next].mine &&
                    !game->cells[next].revealed &&
                    !game->cells[next].flagged && !queued[next] &&
                    stack_size < MINESWEEPER_CELLS) {
                    stack[stack_size++] = next;
                    queued[next] = 1;
                }
            }
        }
    }
    check_win(game);
}

static uint32_t number_color(int number)
{
    const uint32_t colors[] = {
        KUI_BLACK, BLUE, GREEN, RED, KUI_TITLE_BLUE, BROWN, CYAN,
        KUI_BLACK, KUI_SHADOW
    };
    return colors[number >= 0 && number <= 8 ? number : 0];
}

static void draw_flag(struct kui_surface *surface, int x, int y, int size)
{
    int center = size / 2;
    int flag_height = size / 3;
    kui_fill_rect(surface, x + center, y + size / 5, 2,
                  size * 3 / 5, KUI_BLACK);
    kui_fill_rect(surface, x + size / 4, y + size * 4 / 5,
                  size / 2, 2, KUI_BLACK);
    for (int row = 0; row < flag_height; row++)
        kui_fill_rect(surface, x + size / 4, y + size / 5 + row,
                      flag_height - row, 1, RED);
}

static void draw_mine(struct kui_surface *surface, int x, int y, int size)
{
    int center = size / 2;
    int radius = size / 5;
    kui_fill_rect(surface, x + center - radius, y + center - radius,
                  radius * 2 + 1, radius * 2 + 1, KUI_BLACK);
    kui_fill_rect(surface, x + center - 1, y + size / 5, 3,
                  size * 3 / 5, KUI_BLACK);
    kui_fill_rect(surface, x + size / 5, y + center - 1,
                  size * 3 / 5, 3, KUI_BLACK);
    kui_pixel(surface, x + center - 1, y + center - 1, KUI_WHITE);
}

static void draw_cell(struct kui_surface *surface,
                      const struct minesweeper_cell *cell,
                      int x, int y, int size, int won)
{
    if (!cell->revealed) {
        kui_draw_bevel(surface, x + 1, y + 1, size - 2, size - 2,
                       KUI_FACE, 0);
        if (cell->flagged) draw_flag(surface, x, y, size);
        return;
    }

    kui_fill_rect(surface, x, y, size, size, KUI_LIGHT);
    kui_outline_rect(surface, x, y, size + 1, size + 1, KUI_SHADOW);
    if (cell->mine) {
        kui_fill_rect(surface, x + 2, y + 2, size - 3, size - 3,
                      won ? KUI_LIGHT : RED);
        draw_mine(surface, x, y, size);
    } else if (cell->adjacent > 0 && size >= 16) {
        char label[2] = { (char)('0' + cell->adjacent), '\0' };
        kui_draw_text(surface, x + (size - kui_text_width(label)) / 2,
                      y + (size - 16) / 2,
                      label, number_color(cell->adjacent), KUI_LIGHT, 0);
    }
}

void minesweeper_draw(struct kui_surface *surface,
                      const struct minesweeper_game *game,
                      int x, int y, int width, int height)
{
    struct minesweeper_layout layout =
        calculate_layout(x, y, width, height);
    char status[64];
    int flags = 0;

    kui_fill_rect(surface, x, y, width, height, KUI_FACE);
    kui_draw_button(surface, layout.restart_x, layout.restart_y,
                    layout.restart_w, layout.restart_h,
                    game->game_over ? "New Game" : "Restart", 0);
    kui_draw_bevel(surface, layout.board_x - 3, layout.board_y - 3,
                   MINESWEEPER_COLS * layout.cell_size + 6,
                   MINESWEEPER_ROWS * layout.cell_size + 6,
                   KUI_FACE, 1);
    for (int row = 0; row < MINESWEEPER_ROWS; row++)
        for (int col = 0; col < MINESWEEPER_COLS; col++)
            draw_cell(surface, &game->cells[cell_index(col, row)],
                      layout.board_x + col * layout.cell_size,
                      layout.board_y + row * layout.cell_size,
                      layout.cell_size, game->won);

    for (int i = 0; i < MINESWEEPER_CELLS; i++)
        if (game->cells[i].flagged) flags++;
    if (game->won) strcpy(status, "You cleared the field!");
    else if (game->game_over) strcpy(status, "Boom! Start a new game");
    else snprintf(status, sizeof(status), "Mines: %d   Flags: %d",
                  MINESWEEPER_MINES, flags);
    kui_draw_text(surface, x + (width - kui_text_width(status)) / 2,
                  y + height - 18, status,
                  game->game_over && !game->won ? RED : KUI_BLACK,
                  KUI_FACE, 0);
}

int minesweeper_click(struct minesweeper_game *game,
                      int x, int y, int width, int height,
                      int pointer_x, int pointer_y, int right_button,
                      uint32_t seed)
{
    struct minesweeper_layout layout =
        calculate_layout(x, y, width, height);

    if (kui_in_rect(pointer_x, pointer_y, layout.restart_x, layout.restart_y,
                    layout.restart_w, layout.restart_h)) {
        minesweeper_reset(game, seed);
        return 1;
    }
    if (!kui_in_rect(pointer_x, pointer_y, layout.board_x, layout.board_y,
                     MINESWEEPER_COLS * layout.cell_size,
                     MINESWEEPER_ROWS * layout.cell_size))
        return 0;

    int col = (pointer_x - layout.board_x) / layout.cell_size;
    int row = (pointer_y - layout.board_y) / layout.cell_size;
    int index = cell_index(col, row);
    if (right_button) {
        if (!game->game_over && !game->cells[index].revealed)
            game->cells[index].flagged = !game->cells[index].flagged;
    } else {
        reveal_cell(game, col, row);
    }
    return 1;
}
