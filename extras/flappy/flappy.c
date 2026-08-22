#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"

#define FRAME_MS 32

struct game { int bird_y, velocity, pipe_x, gap_y, score, over; uint32_t seed; };

static int random_gap(struct game *g)
{
    g->seed = g->seed * 1664525U + 1013904223U;
    return 85 + (int)(g->seed % 180U);
}

static void reset(struct game *g, uint32_t seed)
{
    memset(g, 0, sizeof(*g)); g->bird_y = 190; g->pipe_x = 430;
    g->seed = seed; g->gap_y = random_gap(g);
}

static void flap(struct game *g) { if (g->over) reset(g, g->seed + 1); else g->velocity = -8; }

static void step(struct game *g, int width, int height)
{
    int floor = height - 34;
    g->velocity++; if (g->velocity > 9) g->velocity = 9;
    g->bird_y += g->velocity; g->pipe_x -= 4;
    if (g->pipe_x < -52) { g->pipe_x = width + 40; g->gap_y = random_gap(g); g->score++; }
    if (g->bird_y < 0 || g->bird_y + 24 >= floor) g->over = 1;
    if (g->pipe_x < 120 && g->pipe_x + 52 > 88 &&
        (g->bird_y < g->gap_y || g->bird_y + 24 > g->gap_y + 116)) g->over = 1;
}

static void draw(struct kui_client *c, const struct kui_event *e, const struct game *g)
{
    char score[32]; int floor = e->height - 34;
    kui_fill_rect(&c->surface, 0, 0, e->width, e->height, 0x0067c9efU);
    for (int x = 0; x < e->width; x += 80)
        kui_fill_rect_alpha(&c->surface, x, floor - 55, 65, 55, 0x40ffffffU);
    kui_fill_rect(&c->surface, 0, floor, e->width, 34, 0x00d8c95aU);
    kui_fill_rect(&c->surface, g->pipe_x, 0, 52, g->gap_y, 0x0047b64dU);
    kui_fill_rect(&c->surface, g->pipe_x, g->gap_y + 116, 52,
                  floor - g->gap_y - 116, 0x0047b64dU);
    kui_outline_rect(&c->surface, g->pipe_x, 0, 52, g->gap_y, 0x001c6c2bU);
    kui_outline_rect(&c->surface, g->pipe_x, g->gap_y + 116, 52,
                     floor - g->gap_y - 116, 0x001c6c2bU);
    kui_fill_rect(&c->surface, 88, g->bird_y, 32, 24, 0x00ffd642U);
    kui_fill_rect(&c->surface, 108, g->bird_y + 9, 18, 7, 0x00ef7138U);
    kui_fill_rect(&c->surface, 96, g->bird_y + 5, 4, 4, KUI_BLACK);
    snprintf(score, sizeof(score), "Score: %d", g->score);
    kui_draw_text(&c->surface, 12, 12, score, KUI_WHITE, 0x0067c9efU, 0);
    if (g->over) kui_draw_text(&c->surface, e->width / 2 - 72, e->height / 2,
                               "GAME OVER - click to restart", KUI_WHITE, 0x0067c9efU, 0);
}

int main(void)
{
    struct kui_client client; struct kui_event event; struct game game;
    kui_client_init(&client); reset(&game, 1);
    while (kui_client_read_event(&event) == 0) {
        if (event.type == KUI_EVENT_CLOSE) break;
        if (event.type == KUI_EVENT_INIT) reset(&game, event.seed);
        else if (event.type == KUI_EVENT_TIMER && !game.over) step(&game, event.width, event.height);
        else if ((event.type == KUI_EVENT_POINTER && event.button == 1) ||
                 (event.type == KUI_EVENT_KEY && event.key_pressed && event.character == ' ')) flap(&game);
        kui_client_begin(&client, &event, "Flappy Bird");
        kui_client_set_constraints(&client, 420, 360, 640, 500);
        draw(&client, &event, &game); kui_client_set_timer(&client, FRAME_MS);
        if (kui_client_send(&client) < 0) break;
    }
    kui_client_free(&client); return 0;
}
