#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "extras/doom-aqua/doom_aqua_ui.h"
#include "extras/doom/doomgeneric/doomkeys.h"

int main(void)
{
    struct kui_command_buffer commands;
    struct kui_surface recording;
    struct kui_surface output;
    uint32_t source[320 * 200];
    uint32_t *destination;
    uint32_t frame_bytes;
    int failures = 0;

    failures += doom_aqua_convert_key(1, 0) != KEY_ESCAPE;
    failures += doom_aqua_convert_key(17, 0) != 'w';
    failures += doom_aqua_convert_key(29, 0) != KEY_FIRE;
    failures += doom_aqua_convert_key(56, 0) != KEY_LALT;
    failures += doom_aqua_convert_key(72, 1) != KEY_UPARROW;
    failures += doom_aqua_convert_key(77, 1) != KEY_RIGHTARROW;
    failures += doom_aqua_convert_key(72, 0) != 0;

    for (unsigned int i = 0; i < sizeof(source) / sizeof(source[0]); i++)
        source[i] = 0x00123456U;
    destination = malloc(853U * 511U * sizeof(*destination));
    if (destination == NULL) return 2;
    for (int i = 0; i < 853 * 511; i++) destination[i] = 0x00ffffffU;

    kui_command_buffer_init(&commands);
    kui_surface_init_commands(&recording, &commands, 853, 511);
    doom_aqua_draw_frame(&recording, source, 320, 200);
    failures += commands.failed;
    failures += commands.length == 0 ||
                commands.length > KUI_PROTOCOL_MAX_DISPLAY;
    frame_bytes = commands.length;

    kui_surface_init(&output, destination, 853, 511);
    failures += kui_display_replay(&output, commands.data, commands.length) < 0;
    failures += destination[0] != KUI_BLACK;
    failures += destination[255 * 853 + 426] != 0x00123456U;

    kui_command_buffer_free(&commands);
    free(destination);
    if (failures != 0) {
        fprintf(stderr, "Doom Aqua UI failed: %d check(s)\n", failures);
        return 1;
    }
    printf("Doom Aqua UI passed (%u-byte frame)\n", frame_bytes);
    return 0;
}
