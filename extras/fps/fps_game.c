#include "extras/fps/fps_game.h"

#include <math.h>
#include <string.h>

#define FPS_PI 3.14159265358979323846
#define FPS_MOVE_SPEED 0.105
#define FPS_TURN_SPEED 0.072
#define FPS_ACTOR_RADIUS 0.20
#define FPS_ROUND_LIMIT_TICKS 1500
#define FPS_ROUND_DELAY_TICKS 78

static const int weapon_magazines[FPS_WEAPON_COUNT] = { 12, 30, 8 };
static const int weapon_reserves[FPS_WEAPON_COUNT] = { 48, 90, 32 };
static const int weapon_reload_ticks[FPS_WEAPON_COUNT] = { 30, 44, 52 };
static const int weapon_fire_ticks[FPS_WEAPON_COUNT] = { 9, 4, 18 };
static const int weapon_damage[FPS_WEAPON_COUNT] = { 40, 34, 72 };
static const char *const weapon_names[FPS_WEAPON_COUNT] = {
    "PISTOL", "RIFLE", "SHOTGUN"
};

static const char *const fps_map[FPS_MAP_HEIGHT] = {
    "1111111111111111",
    "1..............1",
    "1..............1",
    "1..22.....33...1",
    "1..2......3....1",
    "1..2..444.3....1",
    "1.....4........1",
    "1.....4..222...1",
    "1........2.....1",
    "1.333....2.....1",
    "1...3..........1",
    "1...3..1111....1",
    "1......1.......1",
    "1......1.......1",
    "1..............1",
    "1111111111111111"
};

static const double bot_spawns[FPS_BOT_COUNT][2] = {
    { 1.6, 1.5 }, { 2.5, 1.5 }, { 1.5, 2.5 },
    { 13.5, 14.3 }, { 12.5, 14.3 }, { 14.2, 13.4 }, { 10.5, 14.3 }
};

static const int bot_teams[FPS_BOT_COUNT] = {
    FPS_TEAM_CT, FPS_TEAM_CT, FPS_TEAM_CT,
    FPS_TEAM_T, FPS_TEAM_T, FPS_TEAM_T, FPS_TEAM_T
};

int fps_game_cell(int x, int y)
{
    char cell;

    if (x < 0 || y < 0 || x >= FPS_MAP_WIDTH || y >= FPS_MAP_HEIGHT)
        return 1;
    cell = fps_map[y][x];
    return cell >= '1' && cell <= '4' ? cell - '0' : 0;
}

int fps_game_weapon_magazine(int weapon)
{
    if (weapon < 0 || weapon >= FPS_WEAPON_COUNT) return 0;
    return weapon_magazines[weapon];
}

const char *fps_game_weapon_name(int weapon)
{
    if (weapon < 0 || weapon >= FPS_WEAPON_COUNT) return "UNKNOWN";
    return weapon_names[weapon];
}

static uint32_t random_next(struct fps_game *game)
{
    game->random_state = game->random_state * 1664525U + 1013904223U;
    return game->random_state;
}

static int can_stand(double x, double y)
{
    return fps_game_cell((int)(x - FPS_ACTOR_RADIUS),
                         (int)(y - FPS_ACTOR_RADIUS)) == 0 &&
           fps_game_cell((int)(x + FPS_ACTOR_RADIUS),
                         (int)(y - FPS_ACTOR_RADIUS)) == 0 &&
           fps_game_cell((int)(x - FPS_ACTOR_RADIUS),
                         (int)(y + FPS_ACTOR_RADIUS)) == 0 &&
           fps_game_cell((int)(x + FPS_ACTOR_RADIUS),
                         (int)(y + FPS_ACTOR_RADIUS)) == 0;
}

static double normalize_angle(double angle)
{
    while (angle > FPS_PI) angle -= FPS_PI * 2.0;
    while (angle < -FPS_PI) angle += FPS_PI * 2.0;
    return angle;
}

static int clear_path(double x0, double y0, double x1, double y1)
{
    double dx = x1 - x0;
    double dy = y1 - y0;
    double distance = sqrt(dx * dx + dy * dy);
    int steps = (int)(distance / 0.07);

    if (steps < 1) return 1;
    for (int step = 1; step < steps; step++) {
        double amount = (double)step / steps;

        if (fps_game_cell((int)(x0 + dx * amount),
                          (int)(y0 + dy * amount)) != 0)
            return 0;
    }
    return 1;
}

static void begin_round(struct fps_game *game)
{
    int ct_score = game->ct_score;
    int t_score = game->t_score;
    int round = game->round + 1;
    int score = game->score;
    int kills = game->kills;
    int deaths = game->deaths;
    uint32_t random_state = game->random_state;

    memset(game, 0, sizeof(*game));
    game->ct_score = ct_score;
    game->t_score = t_score;
    game->round = round;
    game->score = score;
    game->kills = kills;
    game->deaths = deaths;
    game->random_state = random_state != 0 ? random_state : 0x4b594c45U;
    game->player_x = 2.5;
    game->player_y = 2.5;
    game->angle = 0.0;
    game->health = 100;
    game->weapon = FPS_WEAPON_RIFLE;
    for (int weapon = 0; weapon < FPS_WEAPON_COUNT; weapon++) {
        game->weapon_ammo[weapon] = weapon_magazines[weapon];
        game->weapon_reserve[weapon] = weapon_reserves[weapon];
    }
    game->player_alive = 1;
    game->round_winner = -1;
    game->match_winner = -1;
    for (int i = 0; i < FPS_BOT_COUNT; i++) {
        game->bots[i].x = bot_spawns[i][0];
        game->bots[i].y = bot_spawns[i][1];
        game->bots[i].team = bot_teams[i];
        game->bots[i].health = 100;
        game->bots[i].alive = 1;
        game->bots[i].fire_cooldown = 8 + i * 2;
    }
}

void fps_game_reset(struct fps_game *game)
{
    memset(game, 0, sizeof(*game));
    game->round_winner = -1;
    game->match_winner = -1;
    game->random_state = 0x4b594c45U;
    begin_round(game);
}

int fps_game_team_alive(const struct fps_game *game, int team)
{
    int alive = team == FPS_TEAM_CT && game->player_alive ? 1 : 0;

    for (int i = 0; i < FPS_BOT_COUNT; i++)
        if (game->bots[i].alive && game->bots[i].team == team) alive++;
    return alive;
}

static void finish_round(struct fps_game *game, int winner)
{
    if (game->round_over || game->match_over) return;
    game->round_over = 1;
    game->round_winner = winner;
    game->round_delay = FPS_ROUND_DELAY_TICKS;
    if (winner == FPS_TEAM_CT) game->ct_score++;
    else game->t_score++;
    if (game->ct_score >= FPS_ROUNDS_TO_WIN ||
        game->t_score >= FPS_ROUNDS_TO_WIN) {
        game->match_over = 1;
        game->match_winner = winner;
        game->round_delay = 0;
    }
}

static void check_round_end(struct fps_game *game)
{
    if (fps_game_team_alive(game, FPS_TEAM_T) == 0)
        finish_round(game, FPS_TEAM_CT);
    else if (fps_game_team_alive(game, FPS_TEAM_CT) == 0)
        finish_round(game, FPS_TEAM_T);
    else if (game->round_ticks >= FPS_ROUND_LIMIT_TICKS)
        finish_round(game, FPS_TEAM_CT);
}

static void move_player(struct fps_game *game, int forward, int strafe,
                        int turn)
{
    double length = forward != 0 && strafe != 0 ? 0.70710678118 : 1.0;
    double direction_x;
    double direction_y;
    double right_x;
    double right_y;
    double next_x;
    double next_y;

    game->angle = normalize_angle(game->angle + turn * FPS_TURN_SPEED);
    direction_x = cos(game->angle);
    direction_y = sin(game->angle);
    right_x = -direction_y;
    right_y = direction_x;
    next_x = game->player_x +
             (direction_x * forward + right_x * strafe) *
             FPS_MOVE_SPEED * length;
    next_y = game->player_y +
             (direction_y * forward + right_y * strafe) *
             FPS_MOVE_SPEED * length;

    if (can_stand(next_x, game->player_y)) game->player_x = next_x;
    if (can_stand(game->player_x, next_y)) game->player_y = next_y;
}

static int path_next_cell(int start_x, int start_y, int target_x, int target_y,
                          int *next_x, int *next_y)
{
    int queue_x[FPS_MAP_WIDTH * FPS_MAP_HEIGHT];
    int queue_y[FPS_MAP_WIDTH * FPS_MAP_HEIGHT];
    int first_x[FPS_MAP_HEIGHT][FPS_MAP_WIDTH];
    int first_y[FPS_MAP_HEIGHT][FPS_MAP_WIDTH];
    unsigned char visited[FPS_MAP_HEIGHT][FPS_MAP_WIDTH];
    static const int direction_x[4] = { 1, -1, 0, 0 };
    static const int direction_y[4] = { 0, 0, 1, -1 };
    int head = 0;
    int tail = 0;

    memset(visited, 0, sizeof(visited));
    if (start_x == target_x && start_y == target_y) return 0;
    queue_x[tail] = start_x;
    queue_y[tail++] = start_y;
    visited[start_y][start_x] = 1;
    first_x[start_y][start_x] = start_x;
    first_y[start_y][start_x] = start_y;

    while (head < tail) {
        int x = queue_x[head];
        int y = queue_y[head++];

        for (int direction = 0; direction < 4; direction++) {
            int nx = x + direction_x[direction];
            int ny = y + direction_y[direction];

            if (nx < 0 || ny < 0 || nx >= FPS_MAP_WIDTH ||
                ny >= FPS_MAP_HEIGHT || visited[ny][nx] ||
                fps_game_cell(nx, ny) != 0)
                continue;
            visited[ny][nx] = 1;
            if (x == start_x && y == start_y) {
                first_x[ny][nx] = nx;
                first_y[ny][nx] = ny;
            } else {
                first_x[ny][nx] = first_x[y][x];
                first_y[ny][nx] = first_y[y][x];
            }
            if (nx == target_x && ny == target_y) {
                *next_x = first_x[ny][nx];
                *next_y = first_y[ny][nx];
                return 1;
            }
            queue_x[tail] = nx;
            queue_y[tail++] = ny;
        }
    }
    return 0;
}

static void move_bot(struct fps_bot *bot, double target_x, double target_y,
                     double speed)
{
    double dx = target_x - bot->x;
    double dy = target_y - bot->y;
    double distance = sqrt(dx * dx + dy * dy);
    double next_x;
    double next_y;

    if (distance < 0.01) return;
    next_x = bot->x + dx / distance * speed;
    next_y = bot->y + dy / distance * speed;
    if (can_stand(next_x, bot->y)) bot->x = next_x;
    if (can_stand(bot->x, next_y)) bot->y = next_y;
}

static int nearest_target(const struct fps_game *game, int shooter,
                          double *target_x, double *target_y)
{
    const struct fps_bot *bot = &game->bots[shooter];
    double best_distance = 1.0e30;
    int target = -2;

    if (bot->team == FPS_TEAM_T && game->player_alive) {
        double dx = game->player_x - bot->x;
        double dy = game->player_y - bot->y;

        best_distance = dx * dx + dy * dy;
        target = -1;
        *target_x = game->player_x;
        *target_y = game->player_y;
    }
    for (int i = 0; i < FPS_BOT_COUNT; i++) {
        double dx;
        double dy;
        double distance;

        if (!game->bots[i].alive || game->bots[i].team == bot->team)
            continue;
        dx = game->bots[i].x - bot->x;
        dy = game->bots[i].y - bot->y;
        distance = dx * dx + dy * dy;
        if (distance < best_distance) {
            best_distance = distance;
            target = i;
            *target_x = game->bots[i].x;
            *target_y = game->bots[i].y;
        }
    }
    return target;
}

static void bot_damage_target(struct fps_game *game, int shooter, int target,
                              double distance)
{
    struct fps_bot *bot = &game->bots[shooter];
    int accuracy = distance < 2.0 ? 82 : distance < 5.0 ? 62 : 43;
    int damage;

    bot->fire_cooldown = 8 + (int)(random_next(game) % 8U);
    bot->muzzle_ticks = 2;
    if ((int)(random_next(game) % 100U) >= accuracy) return;
    damage = 15 + (int)(random_next(game) % 11U);
    if (target < 0) {
        game->health -= damage;
        if (game->health <= 0) {
            game->health = 0;
            game->player_alive = 0;
            game->deaths++;
        }
        return;
    }
    game->bots[target].health -= damage;
    game->bots[target].hurt_ticks = 3;
    if (game->bots[target].health <= 0) {
        game->bots[target].health = 0;
        game->bots[target].alive = 0;
    }
}

static void update_bots(struct fps_game *game)
{
    for (int i = 0; i < FPS_BOT_COUNT; i++) {
        struct fps_bot *bot = &game->bots[i];
        double target_x = 0.0;
        double target_y = 0.0;
        double dx;
        double dy;
        double distance;
        int target;

        if (!bot->alive) continue;
        target = nearest_target(game, i, &target_x, &target_y);
        if (target == -2) continue;
        dx = target_x - bot->x;
        dy = target_y - bot->y;
        distance = sqrt(dx * dx + dy * dy);
        if (clear_path(bot->x, bot->y, target_x, target_y)) {
            if (distance < 8.5 && bot->fire_cooldown == 0)
                bot_damage_target(game, i, target, distance);
            if (distance > 3.2)
                move_bot(bot, target_x, target_y, 0.034);
            else if (distance < 1.25)
                move_bot(bot, bot->x - dx, bot->y - dy, 0.028);
            else {
                double strafe = ((i + game->tick / 24) & 1) ? 0.022 : -0.022;
                move_bot(bot, bot->x - dy * strafe,
                         bot->y + dx * strafe, 0.025);
            }
        } else {
            int next_x;
            int next_y;

            if (path_next_cell((int)bot->x, (int)bot->y,
                               (int)target_x, (int)target_y,
                               &next_x, &next_y))
                move_bot(bot, next_x + 0.5, next_y + 0.5, 0.038);
        }
    }
}

static void complete_reload(struct fps_game *game)
{
    int weapon = game->weapon;
    int needed = weapon_magazines[weapon] - game->weapon_ammo[weapon];
    int amount = needed < game->weapon_reserve[weapon] ? needed :
                                                         game->weapon_reserve[weapon];

    game->weapon_ammo[weapon] += amount;
    game->weapon_reserve[weapon] -= amount;
}

int fps_game_reload(struct fps_game *game)
{
    int weapon = game->weapon;

    if (!game->player_alive || game->round_over || game->match_over ||
        game->reload_ticks != 0 ||
        game->weapon_ammo[weapon] >= weapon_magazines[weapon] ||
        game->weapon_reserve[weapon] <= 0)
        return 0;
    game->reload_ticks = weapon_reload_ticks[weapon];
    return 1;
}

int fps_game_select_weapon(struct fps_game *game, int weapon)
{
    if (weapon < 0 || weapon >= FPS_WEAPON_COUNT ||
        !game->player_alive || game->round_over || game->match_over)
        return 0;
    game->weapon = weapon;
    game->reload_ticks = 0;
    game->fire_cooldown = 3;
    return 1;
}

void fps_game_step(struct fps_game *game, int forward, int strafe, int turn)
{
    game->tick++;
    if (game->muzzle_ticks > 0) game->muzzle_ticks--;
    if (game->fire_cooldown > 0) game->fire_cooldown--;
    for (int i = 0; i < FPS_BOT_COUNT; i++) {
        if (game->bots[i].hurt_ticks > 0) game->bots[i].hurt_ticks--;
        if (game->bots[i].fire_cooldown > 0) game->bots[i].fire_cooldown--;
        if (game->bots[i].muzzle_ticks > 0) game->bots[i].muzzle_ticks--;
    }
    if (game->match_over) return;
    if (game->round_over) {
        if (game->round_delay > 0) game->round_delay--;
        if (game->round_delay == 0) begin_round(game);
        return;
    }
    if (game->reload_ticks > 0) {
        game->reload_ticks--;
        if (game->reload_ticks == 0) complete_reload(game);
    }
    if (game->player_alive)
        move_player(game, forward, strafe, turn);
    update_bots(game);
    game->round_ticks++;
    check_round_end(game);
}

int fps_game_fire(struct fps_game *game)
{
    int weapon = game->weapon;
    int target = -1;
    double target_distance = 1000000.0;

    if (!game->player_alive || game->round_over || game->match_over ||
        game->weapon_ammo[weapon] <= 0 || game->fire_cooldown != 0 ||
        game->reload_ticks != 0)
        return 0;
    game->weapon_ammo[weapon]--;
    game->fire_cooldown = weapon_fire_ticks[weapon];
    game->muzzle_ticks = 3;

    for (int i = 0; i < FPS_BOT_COUNT; i++) {
        struct fps_bot *bot = &game->bots[i];
        double dx;
        double dy;
        double distance;
        double angle;
        double aim_width;

        if (!bot->alive || bot->team != FPS_TEAM_T) continue;
        dx = bot->x - game->player_x;
        dy = bot->y - game->player_y;
        distance = sqrt(dx * dx + dy * dy);
        angle = fabs(normalize_angle(atan2(dy, dx) - game->angle));
        if (weapon == FPS_WEAPON_PISTOL)
            aim_width = 0.032 + 0.15 / distance;
        else if (weapon == FPS_WEAPON_SHOTGUN)
            aim_width = 0.13 + 0.28 / distance;
        else
            aim_width = 0.042 + 0.19 / distance;
        if (angle > aim_width || distance >= target_distance ||
            !clear_path(game->player_x, game->player_y, bot->x, bot->y))
            continue;
        target = i;
        target_distance = distance;
    }

    if (target >= 0) {
        struct fps_bot *bot = &game->bots[target];

        int damage = weapon_damage[weapon];

        if (weapon == FPS_WEAPON_SHOTGUN) {
            damage -= (int)(target_distance * 8.0);
            if (damage < 24) damage = 24;
        }
        bot->health -= damage;
        bot->hurt_ticks = 3;
        if (bot->health <= 0) {
            bot->health = 0;
            bot->alive = 0;
            game->score += 100;
            game->kills++;
        }
        return 1;
    }
    if (game->weapon_ammo[weapon] == 0) fps_game_reload(game);
    return 0;
}
