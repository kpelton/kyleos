#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "extras/kui/kui.h"

int main(void)
{
    struct kui_command_buffer commands;
    struct kui_surface recording;
    struct kui_surface output;
    uint8_t source[320 * 200];
    uint32_t palette[256];
    uint32_t *destination;
    int failures = 0;

    for (int index = 0; index < 256; index++)
        palette[index] = (uint32_t)index << 16 |
                         (uint32_t)(255 - index) << 8 |
                         (uint32_t)index;
    for (unsigned int index = 0;
         index < sizeof(source) / sizeof(source[0]); index++)
        source[index] = (uint8_t)(index & 0xffU);
    destination = malloc(640U * 400U * sizeof(*destination));
    if (destination == NULL) return 2;

    kui_command_buffer_init(&commands);
    kui_surface_init_commands(&recording, &commands, 640, 400);
    kui_draw_indexed_bitmap_scaled(&recording, source, palette,
                                   320, 200, 0, 0, 640, 400);
    failures += commands.failed;
    failures += commands.length != 65056U;

    kui_surface_init(&output, destination, 640, 400);
    failures += kui_display_replay(&output, commands.data,
                                   commands.length) < 0;
    failures += destination[0] != palette[source[0]];
    failures += destination[399 * 640 + 639] !=
                palette[source[199 * 320 + 319]];
    failures += kui_display_replay(&output, commands.data,
                                   commands.length - 1U) == 0;

    kui_command_buffer_free(&commands);
    free(destination);
    if (failures != 0) {
        fprintf(stderr, "Quake Aqua indexed frame failed: %d check(s)\n",
                failures);
        return 1;
    }
    puts("Quake Aqua indexed frame passed (65056-byte command)");
    return 0;
}
