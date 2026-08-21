#ifndef KYLEOS_CITY_SIM_H
#define KYLEOS_CITY_SIM_H

#include <stdint.h>

#define CITY_WIDTH 20
#define CITY_HEIGHT 14

enum city_tile_type {
    CITY_EMPTY,
    CITY_WATER,
    CITY_ROAD,
    CITY_RESIDENTIAL,
    CITY_COMMERCIAL,
    CITY_INDUSTRIAL,
    CITY_PARK
};

enum city_tool {
    CITY_TOOL_BULLDOZE,
    CITY_TOOL_ROAD,
    CITY_TOOL_RESIDENTIAL,
    CITY_TOOL_COMMERCIAL,
    CITY_TOOL_INDUSTRIAL,
    CITY_TOOL_PARK,
    CITY_TOOL_COUNT
};

struct city_tile {
    uint8_t type;
    uint8_t level;
};

struct city_state {
    struct city_tile tiles[CITY_HEIGHT][CITY_WIDTH];
    int money;
    int population;
    int jobs;
    int happiness;
    int month;
    int paused;
    uint32_t seed;
};

void city_reset(struct city_state *city, uint32_t seed);
int city_tool_cost(int tool);
int city_place(struct city_state *city, int x, int y, int tool);
int city_has_road(const struct city_state *city, int x, int y);
void city_tick(struct city_state *city);

#endif
