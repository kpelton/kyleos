#include "extras/citybuilder/city_sim.h"

#include <string.h>

static uint32_t city_random(struct city_state *city)
{
    city->seed = city->seed * 1664525U + 1013904223U;
    return city->seed;
}

void city_reset(struct city_state *city, uint32_t seed)
{
    memset(city, 0, sizeof(*city));
    city->money = 10000;
    city->happiness = 55;
    city->seed = seed != 0 ? seed : 1;
    for (int index = 0; index < 18; index++) {
        int x = (int)(city_random(city) % CITY_WIDTH);
        int y = (int)(city_random(city) % CITY_HEIGHT);

        if ((x < 4 || x >= CITY_WIDTH - 4) &&
            (y < 4 || y >= CITY_HEIGHT - 4))
            city->tiles[y][x].type = CITY_WATER;
    }
}

int city_tool_cost(int tool)
{
    static const int costs[CITY_TOOL_COUNT] = { 25, 10, 100, 150, 150, 250 };

    return tool >= 0 && tool < CITY_TOOL_COUNT ? costs[tool] : 0;
}

int city_place(struct city_state *city, int x, int y, int tool)
{
    struct city_tile *tile;
    int type;
    int cost;

    if (x < 0 || x >= CITY_WIDTH || y < 0 || y >= CITY_HEIGHT ||
        tool < 0 || tool >= CITY_TOOL_COUNT)
        return 0;
    tile = &city->tiles[y][x];
    if (tile->type == CITY_WATER) return 0;
    if (tool == CITY_TOOL_BULLDOZE) {
        if (tile->type == CITY_EMPTY || city->money < city_tool_cost(tool))
            return 0;
        city->money -= city_tool_cost(tool);
        tile->type = CITY_EMPTY;
        tile->level = 0;
        return 1;
    }
    type = tool == CITY_TOOL_ROAD ? CITY_ROAD :
           tool == CITY_TOOL_RESIDENTIAL ? CITY_RESIDENTIAL :
           tool == CITY_TOOL_COMMERCIAL ? CITY_COMMERCIAL :
           tool == CITY_TOOL_INDUSTRIAL ? CITY_INDUSTRIAL : CITY_PARK;
    if (tile->type != CITY_EMPTY || city->money < city_tool_cost(tool))
        return 0;
    cost = city_tool_cost(tool);
    city->money -= cost;
    tile->type = (uint8_t)type;
    tile->level = 0;
    return 1;
}

int city_has_road(const struct city_state *city, int x, int y)
{
    return (x > 0 && city->tiles[y][x - 1].type == CITY_ROAD) ||
           (x + 1 < CITY_WIDTH && city->tiles[y][x + 1].type == CITY_ROAD) ||
           (y > 0 && city->tiles[y - 1][x].type == CITY_ROAD) ||
           (y + 1 < CITY_HEIGHT && city->tiles[y + 1][x].type == CITY_ROAD);
}

static void city_totals(struct city_state *city)
{
    int population = 0;
    int jobs = 0;
    int parks = 0;
    int industry = 0;

    for (int y = 0; y < CITY_HEIGHT; y++)
        for (int x = 0; x < CITY_WIDTH; x++) {
            const struct city_tile *tile = &city->tiles[y][x];

            if (tile->type == CITY_RESIDENTIAL)
                population += tile->level * 12;
            else if (tile->type == CITY_COMMERCIAL)
                jobs += tile->level * 10;
            else if (tile->type == CITY_INDUSTRIAL) {
                jobs += tile->level * 16;
                industry += tile->level;
            } else if (tile->type == CITY_PARK) parks++;
        }
    city->population = population;
    city->jobs = jobs;
    city->happiness = 55 + parks * 2 - industry;
    if (population > jobs * 2 + 24) city->happiness -= 10;
    if (jobs > population * 2 + 32) city->happiness -= 5;
    if (city->happiness < 10) city->happiness = 10;
    if (city->happiness > 95) city->happiness = 95;
}

void city_tick(struct city_state *city)
{
    int roads = 0;
    int parks = 0;

    if (city->paused) return;
    city->month++;
    for (int y = 0; y < CITY_HEIGHT; y++)
        for (int x = 0; x < CITY_WIDTH; x++) {
            struct city_tile *tile = &city->tiles[y][x];
            int grow = (city->month + x * 7 + y * 11) % 3 == 0;

            if (tile->type == CITY_ROAD) roads++;
            if (tile->type == CITY_PARK) parks++;
            if (tile->type < CITY_RESIDENTIAL ||
                tile->type > CITY_INDUSTRIAL)
                continue;
            if (!city_has_road(city, x, y)) {
                if (tile->level > 0 && (city->month + x + y) % 8 == 0)
                    tile->level--;
                continue;
            }
            if (!grow || tile->level >= 3) continue;
            if (tile->type == CITY_RESIDENTIAL ||
                tile->type == CITY_INDUSTRIAL || city->population >= 12)
                tile->level++;
        }
    city_totals(city);
    city->money += city->population / 8 + city->jobs / 12;
    city->money -= roads / 3 + parks * 2;
}
