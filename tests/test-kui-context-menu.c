#include <stdio.h>
#include <string.h>

#include "extras/kui/kui_menu.h"

static int failures;

static void expect_int(const char *name, int actual, int expected)
{
    if (actual == expected) return;
    fprintf(stderr, "FAIL %s: got %d, expected %d\n",
            name, actual, expected);
    failures++;
}

static struct kui_event event_at(int type, int x, int y, int button)
{
    struct kui_event event;

    memset(&event, 0, sizeof(event));
    event.type = type;
    event.width = 500;
    event.height = 400;
    event.x = x;
    event.y = y;
    event.button = button;
    event.key_pressed = type == KUI_EVENT_KEY;
    return event;
}

int main(void)
{
    struct kui_context_menu_state state;
    struct kui_menu_item items[] = {
        { "Copy", "Ctrl+C", 10, 1 },
        { "Paste", "Ctrl+V", 20, 0 }
    };
    struct kui_event event;
    int consumed = -1;

    kui_context_menu_init(&state);
    expect_int("initially closed", state.open, 0);
    kui_context_menu_open(&state, 100, 100);
    event = event_at(KUI_EVENT_POINTER, 110, 108, 1);
    expect_int("enabled action",
               kui_context_menu_handle(&state, &event, items, 2, &consumed),
               10);
    expect_int("enabled consumed", consumed, 1);
    expect_int("enabled closes", state.open, 0);

    kui_context_menu_open(&state, 100, 100);
    event = event_at(KUI_EVENT_POINTER, 110, 132, 1);
    expect_int("disabled action",
               kui_context_menu_handle(&state, &event, items, 2, &consumed),
               0);
    expect_int("disabled consumed", consumed, 1);
    expect_int("disabled closes", state.open, 0);

    kui_context_menu_open(&state, 100, 100);
    event = event_at(KUI_EVENT_POINTER, 10, 10, 1);
    expect_int("outside action",
               kui_context_menu_handle(&state, &event, items, 2, &consumed),
               0);
    expect_int("outside consumed", consumed, 1);
    expect_int("outside closes", state.open, 0);

    kui_context_menu_open(&state, 490, 390);
    event = event_at(KUI_EVENT_POINTER, 360, 352, 1);
    expect_int("edge-clamped action",
               kui_context_menu_handle(&state, &event, items, 2, &consumed),
               10);

    kui_context_menu_open(&state, 100, 100);
    event = event_at(KUI_EVENT_KEY, 0, 0, 0);
    expect_int("key action",
               kui_context_menu_handle(&state, &event, items, 2, &consumed),
               0);
    expect_int("key consumed", consumed, 1);
    expect_int("key closes", state.open, 0);

    if (failures != 0) {
        fprintf(stderr, "FAIL KUI context menu: %d failure(s)\n", failures);
        return 1;
    }
    puts("PASS KUI context menu");
    return 0;
}
