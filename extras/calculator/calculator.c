#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extras/calculator/calculator.h"
#include "extras/calculator/calculator_ipc.h"
#include "extras/kui/kui.h"

static int read_all(int fd, void *buffer, int length)
{
    int total = 0;
    while (total < length) {
        int count = read(fd, (char *)buffer + total, length - total);
        if (count <= 0) return -1;
        total += count;
    }
    return 0;
}

static int write_all(int fd, const void *buffer, int length)
{
    int total = 0;
    while (total < length) {
        int count = write(fd, (const char *)buffer + total, length - total);
        if (count <= 0) return -1;
        total += count;
    }
    return 0;
}

static int embedded_main(void)
{
    struct calculator_request request;
    struct calculator_response response;

    memset(&response, 0, sizeof(response));
    calculator_reset(&response.state);
    for (;;) {
        if (read_all(0, &request, sizeof(request)) < 0) return 0;
        if (request.command == CALCULATOR_COMMAND_QUIT) return 0;
        if (request.command == CALCULATOR_COMMAND_RESET) {
            calculator_reset(&response.state);
            response.changed = 1;
        } else if (request.command == CALCULATOR_COMMAND_CLICK) {
            response.changed = calculator_click(
                &response.state, 0, 0, request.width, request.height,
                request.pointer_x, request.pointer_y);
        } else if (request.command == CALCULATOR_COMMAND_KEY) {
            response.changed = calculator_key(&response.state, request.key);
        } else response.changed = 0;
        if (write_all(1, &response, sizeof(response)) < 0) return 0;
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--embedded") == 0)
        return embedded_main();
    puts("Calculator is a desktop application. Run 'desktop' first.");
    return 0;
}
