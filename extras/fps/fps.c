#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "extras/fps/fps_game.h"
#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"

#define FPS_FRAME_MS 32
#define FPS_HUD_HEIGHT 64
#define FPS_MAX_VIEW_WIDTH KUI_SCREEN_WIDTH
#define FPS_CAMERA_PLANE 0.6494075932

struct fps_input {
    int forward;
    int backward;
    int strafe_left;
    int strafe_right;
    int turn_left;
    int turn_right;
};

static double wall_depth[FPS_MAX_VIEW_WIDTH];

static uint32_t shade_color(uint32_t color, int light)
{
    uint32_t red = ((color >> 16) & 0xffU) * (uint32_t)light / 255U;
    uint32_t green = ((color >> 8) & 0xffU) * (uint32_t)light / 255U;
    uint32_t blue = (color & 0xffU) * (uint32_t)light / 255U;

    return (red << 16) | (green << 8) | blue;
}

static uint32_t wall_color(int wall)
{
    static const uint32_t colors[] = {
        0x00000000U, 0x00a84836U, 0x003f78b5U,
        0x004e9b58U, 0x00bd8a35U
    };

    return wall >= 1 && wall <= 4 ? colors[wall] : 0x00808080U;
}

static void draw_world(struct kui_surface *surface,
                       const struct fps_game *game, int width, int view_height,
                       double direction_x, double direction_y,
                       double plane_x, double plane_y)
{
    int horizon = view_height / 2;

    kui_fill_rect(surface, 0, 0, width, horizon / 2, 0x00253d63U);
    kui_fill_rect(surface, 0, horizon / 2, width, horizon / 2,
                  0x004f7392U);
    kui_fill_rect(surface, 0, horizon, width, view_height - horizon,
                  0x003d3a36U);
    for (int y = horizon + 18; y < view_height; y += 24)
        kui_fill_rect(surface, 0, y, width, 1, 0x004b4741U);

    for (int column = 0; column < width; column++) {
        double camera_x = 2.0 * column / width - 1.0;
        double ray_x = direction_x + plane_x * camera_x;
        double ray_y = direction_y + plane_y * camera_x;
        double delta_x = ray_x == 0.0 ? 1.0e30 : fabs(1.0 / ray_x);
        double delta_y = ray_y == 0.0 ? 1.0e30 : fabs(1.0 / ray_y);
        double side_x;
        double side_y;
        double distance;
        double texture;
        int map_x = (int)game->player_x;
        int map_y = (int)game->player_y;
        int step_x;
        int step_y;
        int side = 0;
        int wall = 0;
        int line_height;
        int top;
        int bottom;
        int light;
        uint32_t color;

        if (ray_x < 0.0) {
            step_x = -1;
            side_x = (game->player_x - map_x) * delta_x;
        } else {
            step_x = 1;
            side_x = (map_x + 1.0 - game->player_x) * delta_x;
        }
        if (ray_y < 0.0) {
            step_y = -1;
            side_y = (game->player_y - map_y) * delta_y;
        } else {
            step_y = 1;
            side_y = (map_y + 1.0 - game->player_y) * delta_y;
        }

        while (wall == 0) {
            if (side_x < side_y) {
                side_x += delta_x;
                map_x += step_x;
                side = 0;
            } else {
                side_y += delta_y;
                map_y += step_y;
                side = 1;
            }
            wall = fps_game_cell(map_x, map_y);
        }
        distance = side == 0 ? side_x - delta_x : side_y - delta_y;
        if (distance < 0.05) distance = 0.05;
        wall_depth[column] = distance;
        line_height = (int)(view_height / distance);
        if (line_height > view_height * 3) line_height = view_height * 3;
        top = (view_height - line_height) / 2;
        bottom = top + line_height;
        if (top < 0) top = 0;
        if (bottom > view_height) bottom = view_height;

        texture = side == 0 ? game->player_y + distance * ray_y :
                              game->player_x + distance * ray_x;
        texture -= (int)texture;
        light = 245 - (int)(distance * 17.0) - (side ? 28 : 0);
        if (((int)(texture * 10.0) & 1) != 0) light -= 18;
        if (light < 65) light = 65;
        color = shade_color(wall_color(wall), light);
        kui_fill_rect(surface, column, top, 1, bottom - top, color);
        if ((int)(texture * 16.0) == 0)
            kui_fill_rect(surface, column, top, 1, bottom - top,
                          shade_color(color, 150));
    }
}

static void sort_bots(const struct fps_game *game,
                      int order[FPS_BOT_COUNT])
{
    double distance[FPS_BOT_COUNT];

    for (int i = 0; i < FPS_BOT_COUNT; i++) {
        double dx = game->bots[i].x - game->player_x;
        double dy = game->bots[i].y - game->player_y;

        order[i] = i;
        distance[i] = dx * dx + dy * dy;
    }
    for (int i = 0; i < FPS_BOT_COUNT - 1; i++)
        for (int j = i + 1; j < FPS_BOT_COUNT; j++)
            if (distance[order[i]] < distance[order[j]]) {
                int swap = order[i];
                order[i] = order[j];
                order[j] = swap;
            }
}

static void draw_bot_stripe(struct kui_surface *surface, int x, int top,
                            int height, int relative, int team, int hurt)
{
    int absolute = relative < 0 ? -relative : relative;
    int head_top = top + height * 7 / 100;
    int head_bottom = top + height * 31 / 100;
    int body_top = top + height * 29 / 100;
    int body_bottom = top + height * 68 / 100;
    int leg_top = top + height * 66 / 100;
    int leg_bottom = top + height;
    uint32_t face = hurt ? 0x00fff0dcU : 0x00d5a071U;
    uint32_t uniform = team == FPS_TEAM_CT ? 0x003f6f9fU : 0x00a64235U;
    uint32_t uniform_dark = team == FPS_TEAM_CT ? 0x002a4b70U : 0x007b2d2dU;
    uint32_t legs = team == FPS_TEAM_CT ? 0x002b3445U : 0x00413931U;

    if (absolute < 39)
        kui_fill_rect(surface, x, head_top, 1, head_bottom - head_top,
                      face);
    if (absolute < 53)
        kui_fill_rect(surface, x, body_top, 1, body_bottom - body_top,
                      absolute < 13 ? 0x00d6c45cU : uniform);
    if (absolute >= 52 && absolute < 76)
        kui_fill_rect(surface, x, top + height * 37 / 100, 1,
                      height * 24 / 100, uniform_dark);
    if (absolute > 8 && absolute < 48)
        kui_fill_rect(surface, x, leg_top, 1, leg_bottom - leg_top,
                      legs);
}

static void draw_bots(struct kui_surface *surface,
                      const struct fps_game *game, int width,
                      int view_height, double direction_x,
                      double direction_y, double plane_x, double plane_y)
{
    int order[FPS_BOT_COUNT];
    double determinant = plane_x * direction_y - direction_x * plane_y;
    double inverse = 1.0 / determinant;

    sort_bots(game, order);
    for (int sorted = 0; sorted < FPS_BOT_COUNT; sorted++) {
        const struct fps_bot *bot = &game->bots[order[sorted]];
        double relative_x;
        double relative_y;
        double transform_x;
        double transform_y;
        int screen_x;
        int sprite_height;
        int sprite_width;
        int top;
        int left;
        int right;

        if (!bot->alive) continue;
        relative_x = bot->x - game->player_x;
        relative_y = bot->y - game->player_y;
        transform_x = inverse *
                      (direction_y * relative_x - direction_x * relative_y);
        transform_y = inverse *
                      (-plane_y * relative_x + plane_x * relative_y);
        if (transform_y <= 0.08) continue;
        screen_x = (int)((width / 2.0) *
                         (1.0 + transform_x / transform_y));
        sprite_height = (int)(view_height / transform_y);
        if (sprite_height > view_height * 2) sprite_height = view_height * 2;
        sprite_width = sprite_height / 2;
        if (sprite_width < 2) sprite_width = 2;
        top = (view_height - sprite_height) / 2;
        left = screen_x - sprite_width / 2;
        right = screen_x + sprite_width / 2;
        if (left < 0) left = 0;
        if (right > width) right = width;

        for (int stripe = left; stripe < right; stripe++) {
            int half = sprite_width / 2;
            int relative;

            if (transform_y >= wall_depth[stripe] || half == 0) continue;
            relative = (stripe - screen_x) * 100 / half;
            draw_bot_stripe(surface, stripe, top, sprite_height, relative,
                            bot->team, bot->hurt_ticks != 0);
        }
        if (screen_x >= 0 && screen_x < width &&
            transform_y < wall_depth[screen_x] && sprite_height > 32) {
            int eye_y = top + sprite_height * 18 / 100;
            int eye_size = sprite_height / 30;

            if (eye_size < 1) eye_size = 1;
            kui_fill_rect(surface, screen_x - sprite_width / 9,
                          eye_y, eye_size, eye_size, 0x00fff16aU);
            kui_fill_rect(surface, screen_x + sprite_width / 9,
                          eye_y, eye_size, eye_size, 0x00fff16aU);
            if (bot->muzzle_ticks > 0) {
                kui_fill_rect(surface, screen_x + sprite_width / 3,
                              top + sprite_height * 52 / 100,
                              eye_size * 3, eye_size * 2, 0x00ffd84fU);
            }
        }
    }
}

static void draw_weapon(struct kui_surface *surface,
                        const struct fps_game *game, int width,
                        int view_height)
{
    int center = width / 2;
    int base = view_height + (game->reload_ticks > 0 ? 12 : 0);

    kui_fill_rect(surface, center - 74, base - 34, 42, 34, 0x006c5749U);
    kui_fill_rect(surface, center + 32, base - 34, 42, 34, 0x006c5749U);
    if (game->weapon == FPS_WEAPON_PISTOL) {
        kui_fill_rect(surface, center - 24, base - 54, 48, 54, 0x002d3540U);
        kui_fill_rect(surface, center - 13, base - 92, 26, 45, 0x005d6874U);
        kui_fill_rect(surface, center - 8, base - 99, 42, 12, 0x00212830U);
        kui_fill_rect(surface, center - 4, base - 101, 36, 3, 0x008c98a3U);
    } else if (game->weapon == FPS_WEAPON_SHOTGUN) {
        kui_fill_rect(surface, center - 58, base - 45, 116, 45, 0x00422f26U);
        kui_fill_rect(surface, center - 42, base - 55, 84, 17, 0x00906a3cU);
        kui_fill_rect(surface, center - 12, base - 121, 24, 72, 0x00363f47U);
        kui_fill_rect(surface, center - 7, base - 127, 14, 76, 0x007b8791U);
        kui_fill_rect(surface, center - 10, base - 130, 20, 7, 0x0013171bU);
    } else {
        kui_fill_rect(surface, center - 46, base - 50, 92, 50, 0x00343b48U);
        kui_fill_rect(surface, center - 35, base - 44, 70, 9, 0x00636f7cU);
        kui_fill_rect(surface, center - 16, base - 92, 32, 48, 0x00232931U);
        kui_fill_rect(surface, center - 9, base - 88, 18, 43, 0x006a7580U);
        kui_fill_rect(surface, center - 5, base - 94, 10, 8, 0x0012171cU);
    }
    if (game->muzzle_ticks > 0) {
        int muzzle_y = base - (game->weapon == FPS_WEAPON_SHOTGUN ? 154 :
                               game->weapon == FPS_WEAPON_PISTOL ? 124 : 118);
        kui_fill_rect(surface, center - 5, muzzle_y, 10, 25,
                      0x00fff36aU);
        kui_fill_rect(surface, center - 16, muzzle_y + 9, 32, 8,
                      0x00f69b32U);
    }
}

static void draw_minimap(struct kui_surface *surface,
                         const struct fps_game *game, int width)
{
    int cell_size = 4;
    int map_size = FPS_MAP_WIDTH * cell_size;
    int origin_x = width - map_size - 12;
    int origin_y = 10;

    kui_fill_rect_alpha(surface, origin_x - 5, origin_y - 5,
                        map_size + 10, map_size + 10, 0xa0000000U);
    for (int y = 0; y < FPS_MAP_HEIGHT; y++)
        for (int x = 0; x < FPS_MAP_WIDTH; x++) {
            int wall = fps_game_cell(x, y);

            if (wall != 0)
                kui_fill_rect(surface, origin_x + x * cell_size,
                              origin_y + y * cell_size,
                              cell_size, cell_size,
                              shade_color(wall_color(wall), 185));
        }
    for (int i = 0; i < FPS_BOT_COUNT; i++)
        if (game->bots[i].alive)
            kui_fill_rect(surface,
                          origin_x + (int)(game->bots[i].x * cell_size) - 1,
                          origin_y + (int)(game->bots[i].y * cell_size) - 1,
                          3, 3, game->bots[i].team == FPS_TEAM_CT ?
                                0x004ca9ffU : 0x00ff4b4bU);
    {
        int player_x = origin_x + (int)(game->player_x * cell_size);
        int player_y = origin_y + (int)(game->player_y * cell_size);

        kui_fill_rect(surface, player_x - 1, player_y - 1, 3, 3,
                      KUI_WHITE);
        kui_line(surface, player_x, player_y,
                 player_x + (int)(cos(game->angle) * 7.0),
                 player_y + (int)(sin(game->angle) * 7.0),
                 0x00fff16aU);
    }
}

static void draw_hud(struct kui_surface *surface,
                     const struct fps_game *game, int width, int height)
{
    char text[96];
    int y = height - FPS_HUD_HEIGHT;

    kui_fill_rect(surface, 0, y, width, FPS_HUD_HEIGHT, 0x0012171dU);
    kui_fill_rect(surface, 0, y, width, 3, 0x006b7886U);
    snprintf(text, sizeof(text),
             "CT %d  %d T   RND %d   HP %3d   %s %02d/%02d   K %d",
             game->ct_score, game->t_score, game->round, game->health,
             fps_game_weapon_name(game->weapon),
             game->weapon_ammo[game->weapon],
             game->weapon_reserve[game->weapon], game->kills);
    kui_draw_mono_text_styled(surface, 18, y + 12, text,
                              game->health < 30 ? 0x00ff5b55U : 0x00e8edf2U,
                              0x0012171dU, 0, 1, 1);
    kui_draw_text(surface, 18, y + 37,
                  "WASD move  QE/arrows look  Space fire  1/2/3 weapon  R reload",
                  0x009eabb8U, 0x0012171dU, 0);
}

static void draw_overlay(struct kui_surface *surface,
                         const struct fps_game *game, int width,
                         int view_height)
{
    const char *title;
    const char *subtitle;
    int title_width;
    int box_width;
    int box_x;

    if (game->match_over) {
        title = game->match_winner == FPS_TEAM_CT ?
                "COUNTER TEAM WINS" : "RED TEAM WINS";
        subtitle = "Press N for a new match";
    } else if (game->round_over) {
        title = game->round_winner == FPS_TEAM_CT ?
                "COUNTER TEAM WINS ROUND" : "RED TEAM WINS ROUND";
        subtitle = "Next round starting...";
    } else if (!game->player_alive) {
        title = "YOU ARE DOWN";
        subtitle = "Your bots are still fighting";
    } else {
        return;
    }
    title_width = kui_text_width_scaled(title, 2);
    box_width = title_width + 40;
    if (box_width < 300) box_width = 300;
    if (box_width > width - 20) box_width = width - 20;
    box_x = (width - box_width) / 2;
    kui_fill_rect_alpha(surface, box_x, view_height / 2 - 44,
                        box_width, 88, 0xd0000000U);
    kui_outline_rect(surface, box_x, view_height / 2 - 44,
                     box_width, 88,
                     game->round_winner == FPS_TEAM_T ?
                     0x00e5574fU : 0x0069a7d4U);
    kui_draw_text_styled(surface, (width - title_width) / 2,
                         view_height / 2 - 26, title,
                         game->round_winner == FPS_TEAM_T ?
                         0x00ff726bU : 0x008cccf2U,
                         KUI_BLACK, 0, 2, 1);
    kui_draw_text(surface, (width - kui_text_width(subtitle)) / 2,
                  view_height / 2 + 15, subtitle,
                  KUI_WHITE, KUI_BLACK, 0);
}

static void draw_game(struct kui_client *client, const struct kui_event *event,
                      const struct fps_game *game)
{
    int width = event->width;
    int view_height = event->height - FPS_HUD_HEIGHT;
    double direction_x = cos(game->angle);
    double direction_y = sin(game->angle);
    double plane_x = -direction_y * FPS_CAMERA_PLANE;
    double plane_y = direction_x * FPS_CAMERA_PLANE;

    if (width > FPS_MAX_VIEW_WIDTH) width = FPS_MAX_VIEW_WIDTH;
    if (view_height < 1) return;
    draw_world(&client->surface, game, width, view_height,
               direction_x, direction_y, plane_x, plane_y);
    draw_bots(&client->surface, game, width, view_height,
              direction_x, direction_y, plane_x, plane_y);
    kui_fill_rect(&client->surface, width / 2 - 8, view_height / 2, 6, 1,
                  KUI_WHITE);
    kui_fill_rect(&client->surface, width / 2 + 3, view_height / 2, 6, 1,
                  KUI_WHITE);
    kui_fill_rect(&client->surface, width / 2, view_height / 2 - 8, 1, 6,
                  KUI_WHITE);
    kui_fill_rect(&client->surface, width / 2, view_height / 2 + 3, 1, 6,
                  KUI_WHITE);
    draw_weapon(&client->surface, game, width, view_height);
    draw_minimap(&client->surface, game, width);
    draw_overlay(&client->surface, game, width, view_height);
    draw_hud(&client->surface, game, event->width, event->height);
}

static void handle_key(struct fps_game *game, struct fps_input *input,
                       const struct kui_event *event)
{
    int down = event->key_pressed != 0;

    if (event->key_extended) {
        if (event->key_code == 72) input->forward = down;
        else if (event->key_code == 80) input->backward = down;
        else if (event->key_code == 75) input->turn_left = down;
        else if (event->key_code == 77) input->turn_right = down;
        return;
    }
    if (event->key_code == 17) input->forward = down;
    else if (event->key_code == 31) input->backward = down;
    else if (event->key_code == 30) input->strafe_left = down;
    else if (event->key_code == 32) input->strafe_right = down;
    else if (event->key_code == 16) input->turn_left = down;
    else if (event->key_code == 18) input->turn_right = down;
    else if (down && event->key_code == 2)
        fps_game_select_weapon(game, FPS_WEAPON_PISTOL);
    else if (down && event->key_code == 3)
        fps_game_select_weapon(game, FPS_WEAPON_RIFLE);
    else if (down && event->key_code == 4)
        fps_game_select_weapon(game, FPS_WEAPON_SHOTGUN);
    else if (down && event->key_code == 57) fps_game_fire(game);
    else if (down && event->key_code == 19) fps_game_reload(game);
    else if (down && event->key_code == 49) fps_game_reset(game);
}

int main(void)
{
    struct kui_client client;
    struct kui_event event;
    struct fps_game game;
    struct fps_input input;

    memset(&input, 0, sizeof(input));
    fps_game_reset(&game);
    kui_client_init(&client);
    while (kui_client_read_event(&event) == 0) {
        if (event.type == KUI_EVENT_CLOSE) break;
        if (event.type == KUI_EVENT_INIT) {
            memset(&input, 0, sizeof(input));
            fps_game_reset(&game);
        } else if (event.type == KUI_EVENT_KEY) {
            handle_key(&game, &input, &event);
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1) {
            fps_game_fire(&game);
        } else if (event.type == KUI_EVENT_TIMER) {
            fps_game_step(&game,
                          input.forward - input.backward,
                          input.strafe_right - input.strafe_left,
                          input.turn_right - input.turn_left);
        }

        kui_client_begin(&client, &event, "KyleFPS");
        kui_client_set_constraints(&client, 560, 420, 800, 560);
        draw_game(&client, &event, &game);
        kui_client_set_timer(&client, FPS_FRAME_MS);
        if (kui_client_send(&client) < 0) break;
    }
    kui_client_free(&client);
    return 0;
}
