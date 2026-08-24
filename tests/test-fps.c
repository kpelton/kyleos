#include <stdio.h>

#include "extras/fps/fps_game.h"

static int failures;

static void expect(const char *name, int condition)
{
    if (condition) return;
    fprintf(stderr, "FAIL FPS: %s\n", name);
    failures++;
}

static void put_target_in_sights(struct fps_game *game, int bot, double x,
                                  int health)
{
    game->bots[bot].x = x;
    game->bots[bot].y = game->player_y;
    game->bots[bot].team = FPS_TEAM_T;
    game->bots[bot].health = health;
    game->bots[bot].alive = 1;
    game->angle = 0.0;
}

int main(void)
{
    struct fps_game game;

    for (int y = 0; y < FPS_MAP_HEIGHT; y++) {
        expect("map boundary left", fps_game_cell(0, y) != 0);
        expect("map boundary right",
               fps_game_cell(FPS_MAP_WIDTH - 1, y) != 0);
    }

    fps_game_reset(&game);
    expect("round starts", game.round == 1 && game.player_alive);
    expect("initial teams", fps_game_team_alive(&game, FPS_TEAM_CT) == 4 &&
                            fps_game_team_alive(&game, FPS_TEAM_T) == 4);
    expect("initial rifle", game.weapon == FPS_WEAPON_RIFLE &&
                            game.weapon_ammo[FPS_WEAPON_RIFLE] == 30 &&
                            game.weapon_reserve[FPS_WEAPON_RIFLE] == 90);
    put_target_in_sights(&game, 3, 6.5, 34);
    expect("rifle shot hits", fps_game_fire(&game) == 1);
    expect("rifle target removed", !game.bots[3].alive);
    expect("kill scored", game.score == 100 && game.kills == 1);
    expect("rifle ammo consumed",
           game.weapon_ammo[FPS_WEAPON_RIFLE] == 29);
    expect("fire cooldown", fps_game_fire(&game) == 0 &&
                            game.weapon_ammo[FPS_WEAPON_RIFLE] == 29);

    fps_game_reset(&game);
    expect("select pistol",
           fps_game_select_weapon(&game, FPS_WEAPON_PISTOL) == 1 &&
           game.weapon_ammo[FPS_WEAPON_PISTOL] == 12);
    expect("select shotgun",
           fps_game_select_weapon(&game, FPS_WEAPON_SHOTGUN) == 1 &&
           game.weapon_ammo[FPS_WEAPON_SHOTGUN] == 8);
    for (int tick = 0; tick < 3; tick++) fps_game_step(&game, 0, 0, 0);
    put_target_in_sights(&game, 3, 4.0, 60);
    expect("shotgun close damage", fps_game_fire(&game) == 1 &&
                                     !game.bots[3].alive);

    fps_game_reset(&game);
    game.weapon_ammo[FPS_WEAPON_RIFLE] = 5;
    game.weapon_reserve[FPS_WEAPON_RIFLE] = 10;
    expect("reload starts", fps_game_reload(&game) == 1);
    for (int tick = 0; tick < 44; tick++) fps_game_step(&game, 0, 0, 0);
    expect("reload transfers reserve",
           game.weapon_ammo[FPS_WEAPON_RIFLE] == 15 &&
           game.weapon_reserve[FPS_WEAPON_RIFLE] == 0);

    fps_game_reset(&game);
    {
        double old_x = game.player_x;
        fps_game_step(&game, 1, 0, 0);
        expect("forward movement", game.player_x > old_x);
    }
    game.player_x = 1.25;
    game.player_y = 1.25;
    game.angle = 3.14159265358979323846;
    for (int step = 0; step < 20; step++)
        fps_game_step(&game, 1, 0, 0);
    expect("wall collision", game.player_x >= 1.19);

    fps_game_reset(&game);
    game.bots[0].x = 5.5;
    game.bots[0].y = 2.5;
    game.bots[3].x = 6.5;
    game.bots[3].y = 2.5;
    for (int i = 1; i < FPS_BOT_COUNT; i++)
        if (i != 3) game.bots[i].alive = 0;
    for (int tick = 0; tick < 120 && game.bots[0].alive &&
         game.bots[3].alive; tick++)
        fps_game_step(&game, 0, 0, 0);
    expect("bots fight each other",
           game.bots[0].health < 100 || game.bots[3].health < 100);

    fps_game_reset(&game);
    for (int i = 0; i < FPS_BOT_COUNT; i++)
        if (game.bots[i].team == FPS_TEAM_T) game.bots[i].alive = 0;
    fps_game_step(&game, 0, 0, 0);
    expect("counter team wins round",
           game.round_over && game.round_winner == FPS_TEAM_CT &&
           game.ct_score == 1);

    fps_game_reset(&game);
    game.ct_score = FPS_ROUNDS_TO_WIN - 1;
    for (int i = 0; i < FPS_BOT_COUNT; i++)
        if (game.bots[i].team == FPS_TEAM_T) game.bots[i].alive = 0;
    fps_game_step(&game, 0, 0, 0);
    expect("first to five wins match",
           game.match_over && game.match_winner == FPS_TEAM_CT);

    fps_game_reset(&game);
    for (int tick = 0; tick < 1600 && !game.round_over; tick++)
        fps_game_step(&game, 0, 0, 0);
    expect("autonomous bot round resolves", game.round_over);

    if (failures != 0) {
        fprintf(stderr, "FPS gameplay failed: %d check(s)\n", failures);
        return 1;
    }
    puts("FPS tactical bot gameplay passed");
    return 0;
}
