#include <stdio.h>
#include <string.h>

#include "extras/tetris/tetris_game.h"

static int bit_count(uint16_t value)
{
    int count = 0;

    while (value != 0) {
        count += value & 1U;
        value >>= 1;
    }
    return count;
}

int main(void)
{
    struct tetris_game game;
    int failures = 0;

    for (int piece = 0; piece < TETRIS_PIECES; piece++)
        for (int rotation = 0; rotation < 4; rotation++)
            failures += bit_count(tetris_shape_mask(piece, rotation)) != 4;

    tetris_reset(&game, 1234);
    failures += game.level != 1 || game.game_over;
    for (int index = 0; index < 7; index++) {
        int seen = 0;

        for (int other = 0; other < 7; other++)
            seen += game.bag[other] == index;
        failures += seen != 1;
    }

    game.piece = TETRIS_T;
    game.rotation = 0;
    game.x = 0;
    game.y = 0;
    failures += tetris_move(&game, -1, 0) != 0;
    game.x = 7;
    failures += tetris_move(&game, 1, 0) != 0;

    tetris_reset(&game, 5678);
    game.piece = TETRIS_I;
    game.rotation = 0;
    game.x = 3;
    game.y = -1;
    for (int column = 0; column < TETRIS_WIDTH; column++)
        if (column < 3 || column > 6)
            game.board[TETRIS_HEIGHT - 1][column] = 1;
    failures += tetris_hard_drop(&game) <= 0;
    failures += game.lines != 1 || game.score < 100;
    for (int column = 0; column < TETRIS_WIDTH; column++)
        failures += game.board[TETRIS_HEIGHT - 1][column] != 0;

    tetris_reset(&game, 9012);
    game.piece = TETRIS_T;
    game.x = 3;
    game.y = 5;
    failures += !tetris_rotate(&game, 1);
    failures += game.rotation != 1;

    if (failures != 0) {
        fprintf(stderr, "FAIL tetris: %d failure(s)\n", failures);
        return 1;
    }
    puts("Tetris game logic passed");
    return 0;
}
