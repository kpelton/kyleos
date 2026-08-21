#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "extras/citybuilder/city_sim.h"
#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"
#include "extras/kui/kui_menu.h"

#define CITY_TOOLBAR_WIDTH 164
#define CITY_STATUS_HEIGHT 24

enum city_menu_action {
    CITY_MENU_NEW = 1,
    CITY_MENU_PAUSE,
    CITY_MENU_BULLDOZE,
    CITY_MENU_ROAD,
    CITY_MENU_RESIDENTIAL,
    CITY_MENU_COMMERCIAL,
    CITY_MENU_INDUSTRIAL,
    CITY_MENU_PARK
};

static const struct kui_menu_item game_items[] = {
    { "New City", "N", CITY_MENU_NEW, 1 },
    { "Pause / Resume", "Space", CITY_MENU_PAUSE, 1 }
};

static const struct kui_menu_item tools_items[] = {
    { "Bulldozer", "1", CITY_MENU_BULLDOZE, 1 },
    { "Road", "2", CITY_MENU_ROAD, 1 },
    { "Residential", "3", CITY_MENU_RESIDENTIAL, 1 },
    { "Commercial", "4", CITY_MENU_COMMERCIAL, 1 },
    { "Industrial", "5", CITY_MENU_INDUSTRIAL, 1 },
    { "Park", "6", CITY_MENU_PARK, 1 }
};

static const struct kui_menu menus[] = {
    { "Game", game_items, 2 },
    { "Tools", tools_items, 6 }
};

static const char *tool_names[CITY_TOOL_COUNT] = {
    "Bulldozer", "Road", "Residential", "Commercial", "Industrial", "Park"
};

static const char *tool_labels[CITY_TOOL_COUNT] = {
    "1  Bulldoze  $25", "2  Road       $10", "3  Residential $100",
    "4  Commercial  $150", "5  Industrial  $150", "6  Park       $250"
};

static uint32_t tile_color(int type)
{
    static const uint32_t colors[] = {
        0x0076b75bU, 0x00377fc1U, 0x00656b70U, 0x008bd476U,
        0x0079aee8U, 0x00d8aa66U, 0x004ca956U
    };

    return type >= 0 && type <= CITY_PARK ? colors[type] : KUI_BLACK;
}

static void draw_road(struct kui_surface *surface,
                      const struct city_state *city, int gx, int gy,
                      int x, int y, int size)
{
    int middle = size / 2;

    kui_fill_rect(surface, x, y, size, size, 0x00656b70U);
    kui_fill_rect(surface, x, y + middle - 1, size, 3, 0x00d9bd4cU);
    kui_fill_rect(surface, x + middle - 1, y, 3, size, 0x00d9bd4cU);
    if (gx == 0 || city->tiles[gy][gx - 1].type != CITY_ROAD)
        kui_fill_rect(surface, x, y + middle - 1, middle, 3, 0x00656b70U);
    if (gx + 1 == CITY_WIDTH ||
        city->tiles[gy][gx + 1].type != CITY_ROAD)
        kui_fill_rect(surface, x + middle, y + middle - 1,
                      size - middle, 3, 0x00656b70U);
    if (gy == 0 || city->tiles[gy - 1][gx].type != CITY_ROAD)
        kui_fill_rect(surface, x + middle - 1, y, 3, middle, 0x00656b70U);
    if (gy + 1 == CITY_HEIGHT ||
        city->tiles[gy + 1][gx].type != CITY_ROAD)
        kui_fill_rect(surface, x + middle - 1, y + middle, 3,
                      size - middle, 0x00656b70U);
}

static void draw_building(struct kui_surface *surface,
                          const struct city_tile *tile,
                          int x, int y, int size)
{
    int level = tile->level;
    int inset = level == 0 ? size / 3 : 4;
    uint32_t face;
    char label[2] = { '?', '\0' };

    if (level == 0) {
        label[0] = tile->type == CITY_RESIDENTIAL ? 'R' :
                   tile->type == CITY_COMMERCIAL ? 'C' : 'I';
        kui_draw_text(surface, x + (size - kui_text_width(label)) / 2,
                      y + size / 2 - 8,
                      label, 0x00434c55U, tile_color(tile->type), 0);
        return;
    }
    face = tile->type == CITY_RESIDENTIAL ? 0x00f1e4c8U :
           tile->type == CITY_COMMERCIAL ? 0x00c6e4f7U : 0x00c5a77eU;
    kui_fill_rect(surface, x + inset, y + inset,
                  size - inset * 2, size - inset * 2, 0x00444b52U);
    kui_fill_rect(surface, x + inset - level, y + inset - level,
                  size - inset * 2, size - inset * 2, face);
    if (tile->type == CITY_RESIDENTIAL)
        kui_fill_rect(surface, x + inset - level, y + inset - level,
                      size - inset * 2, 4, 0x00b05242U);
    else
        for (int row = y + inset + 3; row < y + size - inset - 2; row += 6)
            kui_fill_rect(surface, x + inset + 2, row,
                          size - inset * 2 - 4, 2, 0x005d86a2U);
}

static void draw_tile(struct kui_surface *surface,
                      const struct city_state *city, int gx, int gy,
                      int x, int y, int size, int hovered)
{
    const struct city_tile *tile = &city->tiles[gy][gx];

    kui_fill_rect(surface, x, y, size, size, tile_color(tile->type));
    if (tile->type == CITY_EMPTY && ((gx + gy) & 1))
        kui_fill_rect(surface, x + 2, y + 2, size - 4, size - 4, 0x007bbd60U);
    else if (tile->type == CITY_WATER) {
        for (int row = 5; row < size; row += 8)
            kui_fill_rect(surface, x + 3, y + row, size - 6, 1, 0x007bc7e8U);
    } else if (tile->type == CITY_ROAD)
        draw_road(surface, city, gx, gy, x, y, size);
    else if (tile->type >= CITY_RESIDENTIAL &&
             tile->type <= CITY_INDUSTRIAL)
        draw_building(surface, tile, x, y, size);
    else if (tile->type == CITY_PARK) {
        kui_fill_rect(surface, x + size / 2 - 2, y + size / 2, 4,
                      size / 3, 0x00734c2cU);
        kui_fill_rect(surface, x + size / 4, y + 4, size / 2,
                      size / 2, 0x002f7f38U);
    }
    kui_outline_rect(surface, x, y, size, size,
                     hovered ? 0x00fff064U : 0x004f8647U);
}

static int city_layout(const struct kui_event *event,
                       int *map_x, int *map_y, int *tile_size)
{
    int available_width = event->width - CITY_TOOLBAR_WIDTH - 12;
    int available_height = event->height - KUI_MENU_BAR_HEIGHT -
                           CITY_STATUS_HEIGHT - 12;
    int size_x = available_width / CITY_WIDTH;
    int size_y = available_height / CITY_HEIGHT;

    *tile_size = size_x < size_y ? size_x : size_y;
    if (*tile_size < 10) *tile_size = 10;
    *map_x = CITY_TOOLBAR_WIDTH + 6 +
             (available_width - *tile_size * CITY_WIDTH) / 2;
    *map_y = KUI_MENU_BAR_HEIGHT + 6 +
             (available_height - *tile_size * CITY_HEIGHT) / 2;
    return 0;
}

static void draw_city(struct kui_client *client, const struct kui_event *event,
                      const struct city_state *city, int tool,
                      int hover_x, int hover_y)
{
    char text[64];
    int map_x;
    int map_y;
    int size;

    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  0x00d7d9dcU);
    kui_fill_rect(&client->surface, 0, KUI_MENU_BAR_HEIGHT,
                  CITY_TOOLBAR_WIDTH, event->height - KUI_MENU_BAR_HEIGHT,
                  0x00eceff2U);
    snprintf(text, sizeof(text), "$%d", city->money);
    kui_draw_text(&client->surface, 12, KUI_MENU_BAR_HEIGHT + 10,
                  text, 0x000b6b36U, 0x00eceff2U, 0);
    snprintf(text, sizeof(text), "Population: %d", city->population);
    kui_draw_text(&client->surface, 12, KUI_MENU_BAR_HEIGHT + 30,
                  text, KUI_BLACK, 0x00eceff2U, 0);
    snprintf(text, sizeof(text), "Jobs: %d", city->jobs);
    kui_draw_text(&client->surface, 12, KUI_MENU_BAR_HEIGHT + 48,
                  text, KUI_BLACK, 0x00eceff2U, 0);
    snprintf(text, sizeof(text), "Happiness: %d%%", city->happiness);
    kui_draw_text(&client->surface, 12, KUI_MENU_BAR_HEIGHT + 66,
                  text, KUI_BLACK, 0x00eceff2U, 0);
    for (int index = 0; index < CITY_TOOL_COUNT; index++)
        kui_draw_button(&client->surface, 8,
                        KUI_MENU_BAR_HEIGHT + 96 + index * 38,
                        CITY_TOOLBAR_WIDTH - 16, 30,
                        tool_labels[index], index == tool);
    kui_draw_text(&client->surface, 12,
                  KUI_MENU_BAR_HEIGHT + 96 + CITY_TOOL_COUNT * 38 + 8,
                  "Space: pause", 0x00454b51U, 0x00eceff2U, 0);

    city_layout(event, &map_x, &map_y, &size);
    kui_fill_rect(&client->surface, map_x - 4, map_y - 4,
                  CITY_WIDTH * size + 8, CITY_HEIGHT * size + 8,
                  0x003d5640U);
    for (int y = 0; y < CITY_HEIGHT; y++)
        for (int x = 0; x < CITY_WIDTH; x++)
            draw_tile(&client->surface, city, x, y,
                      map_x + x * size, map_y + y * size, size,
                      x == hover_x && y == hover_y);
    kui_fill_rect(&client->surface, 0,
                  event->height - CITY_STATUS_HEIGHT,
                  event->width, CITY_STATUS_HEIGHT, 0x00eef1f4U);
    snprintf(text, sizeof(text), "%s%s  |  Year %d, month %d",
             city->paused ? "PAUSED - " : "", tool_names[tool],
             2000 + city->month / 12, city->month % 12 + 1);
    kui_draw_text(&client->surface, 8,
                  event->height - CITY_STATUS_HEIGHT + 4,
                  text, KUI_BLACK, 0x00eef1f4U, 0);
}

static int menu_tool(int action)
{
    if (action >= CITY_MENU_BULLDOZE && action <= CITY_MENU_PARK)
        return action - CITY_MENU_BULLDOZE;
    return -1;
}

int main(int argc, char **argv)
{
    struct city_state city;
    struct kui_menu_state menu;
    struct kui_client client;
    struct kui_event event;
    int tool = CITY_TOOL_ROAD;
    int hover_x = -1;
    int hover_y = -1;
    int last_x = -1;
    int last_y = -1;

    (void)argc;
    (void)argv;
    memset(&city, 0, sizeof(city));
    kui_menu_init(&menu);
    kui_client_init(&client);
    for (;;) {
        int consumed = 0;
        int action = 0;

        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        if (event.type == KUI_EVENT_INIT) {
            city_reset(&city, event.seed);
            kui_menu_close(&menu);
        } else {
            action = kui_menu_handle(&menu, &event, menus, 2, &consumed);
            if (action == CITY_MENU_NEW)
                city_reset(&city, event.seed);
            else if (action == CITY_MENU_PAUSE)
                city.paused = !city.paused;
            else if (menu_tool(action) >= 0)
                tool = menu_tool(action);
            else if (!consumed && event.type == KUI_EVENT_TIMER)
                city_tick(&city);
            else if (!consumed && event.type == KUI_EVENT_KEY &&
                     event.key_pressed) {
                if (event.character >= '1' && event.character <= '6')
                    tool = event.character - '1';
                else if (event.character == ' ')
                    city.paused = !city.paused;
                else if (event.character == 'n' || event.character == 'N')
                    city_reset(&city, event.seed);
            } else if (!consumed && event.type == KUI_EVENT_POINTER) {
                int map_x;
                int map_y;
                int size;
                int gx;
                int gy;

                city_layout(&event, &map_x, &map_y, &size);
                gx = (event.x - map_x) / size;
                gy = (event.y - map_y) / size;
                hover_x = event.x >= map_x ? gx : -1;
                hover_y = event.y >= map_y ? gy : -1;
                if (hover_x < 0 || hover_x >= CITY_WIDTH ||
                    hover_y < 0 || hover_y >= CITY_HEIGHT)
                    hover_x = hover_y = -1;
                if (event.button == 1 &&
                    event.x >= 8 && event.x < CITY_TOOLBAR_WIDTH - 8 &&
                    event.y >= KUI_MENU_BAR_HEIGHT + 96) {
                    int selected = (event.y - KUI_MENU_BAR_HEIGHT - 96) / 38;

                    if (selected >= 0 && selected < CITY_TOOL_COUNT)
                        tool = selected;
                } else if ((event.button == 1 || (event.buttons & 1)) &&
                           hover_x >= 0 &&
                           (hover_x != last_x || hover_y != last_y)) {
                    city_place(&city, hover_x, hover_y, tool);
                    last_x = hover_x;
                    last_y = hover_y;
                }
                if (!(event.buttons & 1)) last_x = last_y = -1;
            }
        }
        kui_client_begin(&client, &event, "City Builder");
        kui_client_set_constraints(&client, 700, 480, 900, 600);
        kui_client_set_timer(&client, 500);
        draw_city(&client, &event, &city, tool, hover_x, hover_y);
        kui_menu_render(&client.surface, &menu, menus, 2);
        if (kui_client_send(&client) < 0) break;
    }
    kui_client_free(&client);
    return 0;
}
