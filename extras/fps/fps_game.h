#ifndef KYLEOS_FPS_GAME_H
#define KYLEOS_FPS_GAME_H

#include <stdint.h>

#define FPS_MAP_WIDTH 16
#define FPS_MAP_HEIGHT 16
#define FPS_BOT_COUNT 7
#define FPS_TEAM_CT 0
#define FPS_TEAM_T 1
#define FPS_ROUNDS_TO_WIN 5

enum fps_weapon {
    FPS_WEAPON_PISTOL,
    FPS_WEAPON_RIFLE,
    FPS_WEAPON_SHOTGUN,
    FPS_WEAPON_COUNT
};

struct fps_bot {
    double x;
    double y;
    int team;
    int health;
    int alive;
    int hurt_ticks;
    int fire_cooldown;
    int muzzle_ticks;
};

struct fps_game {
    double player_x;
    double player_y;
    double angle;
    int health;
    int weapon;
    int weapon_ammo[FPS_WEAPON_COUNT];
    int weapon_reserve[FPS_WEAPON_COUNT];
    int score;
    int kills;
    int deaths;
    int tick;
    int round_ticks;
    int muzzle_ticks;
    int fire_cooldown;
    int reload_ticks;
    int player_alive;
    int round;
    int round_over;
    int round_delay;
    int round_winner;
    int ct_score;
    int t_score;
    int match_over;
    int match_winner;
    uint32_t random_state;
    struct fps_bot bots[FPS_BOT_COUNT];
};

void fps_game_reset(struct fps_game *game);
int fps_game_cell(int x, int y);
void fps_game_step(struct fps_game *game, int forward, int strafe, int turn);
int fps_game_fire(struct fps_game *game);
int fps_game_reload(struct fps_game *game);
int fps_game_select_weapon(struct fps_game *game, int weapon);
int fps_game_weapon_magazine(int weapon);
const char *fps_game_weapon_name(int weapon);
int fps_game_team_alive(const struct fps_game *game, int team);

#endif
