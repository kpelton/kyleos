#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extras/calculator/calculator.h"
#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"

static int embedded_main(void)
{
    struct calculator_state state;
    struct kui_client client;
    struct kui_event event;

    calculator_reset(&state);
    kui_client_init(&client);
    for (;;) {
        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        if (event.type == KUI_EVENT_INIT)
            calculator_reset(&state);
        else if (event.type == KUI_EVENT_POINTER && event.button == 1)
            calculator_click(&state, 0, 0, event.width, event.height,
                             event.x, event.y);
        else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                 event.character != 0)
            calculator_key(&state,
                           event.character == '\n' ? '=' : event.character);
        kui_client_begin(&client, &event, "Calculator");
        kui_client_set_constraints(&client, 279, 301, 399, 521);
        calculator_draw(&client.surface, &state, 0, 0,
                        event.width, event.height);
        if (kui_client_send(&client) < 0) break;
    }
    kui_client_free(&client);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && (strcmp(argv[1], "--embedded") == 0 ||
                     strcmp(argv[1], "--kui-client") == 0))
        return embedded_main();
    puts("Calculator is a desktop application. Run 'desktop' first.");
    return 0;
}
