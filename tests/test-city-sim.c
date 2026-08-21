#include <stdio.h>

#include "extras/citybuilder/city_sim.h"

static int check(int condition, const char *message)
{
    if (condition) return 0;
    fprintf(stderr, "city simulation failure: %s\n", message);
    return 1;
}

int main(void)
{
    struct city_state city;
    int failures = 0;
    int x = 8;
    int y = 7;

    city_reset(&city, 7);
    failures += check(city.money == 10000 && city.population == 0,
                      "new city starts with the expected budget");
    failures += check(city_place(&city, x, y, CITY_TOOL_ROAD),
                      "road can be placed");
    failures += check(city_place(&city, x + 1, y, CITY_TOOL_RESIDENTIAL),
                      "residential zone can be placed");
    failures += check(city_place(&city, x - 1, y, CITY_TOOL_INDUSTRIAL),
                      "industrial zone can be placed");
    failures += check(city_has_road(&city, x + 1, y),
                      "zone detects adjacent road");
    failures += check(!city_place(&city, x, y, CITY_TOOL_PARK),
                      "occupied tiles cannot be overwritten");
    for (int tick = 0; tick < 18; tick++) city_tick(&city);
    failures += check(city.population > 0 && city.jobs > 0,
                      "connected zones develop population and jobs");
    failures += check(city_place(&city, x + 1, y, CITY_TOOL_BULLDOZE),
                      "bulldozer clears developed zones");
    failures += check(city.tiles[y][x + 1].type == CITY_EMPTY,
                      "bulldozed tile becomes empty");
    if (failures == 0) puts("City simulation tests passed");
    return failures != 0;
}
