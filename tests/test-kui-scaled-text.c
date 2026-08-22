#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extras/kui/kui.h"

static int check(int condition, const char *message)
{
    if (!condition) fprintf(stderr, "FAIL scaled text: %s\n", message);
    return !condition;
}

static int colored_pixels(const uint32_t *pixels, int count)
{
    int colored = 0;

    for (int index = 0; index < count; index++)
        if (pixels[index] != KUI_WHITE) colored++;
    return colored;
}

int main(void)
{
    struct kui_command_buffer commands;
    struct kui_surface recorded;
    struct kui_surface normal_surface;
    struct kui_surface bold_surface;
    struct kui_surface replayed;
    const struct kui_display_command *command;
    uint32_t normal_pixels[64 * 64];
    uint32_t bold_pixels[64 * 64];
    uint32_t replay_pixels[64 * 64];
    int failures = 0;

    failures += check(kui_mono_text_width_scaled("Test", 3) ==
                      kui_mono_text_width("Test") * 3,
                      "monospace scaled width");
    failures += check(kui_text_width_scaled("Test", 2) ==
                      kui_text_width("Test") * 2,
                      "proportional scaled width");

    kui_command_buffer_init(&commands);
    kui_surface_init_commands(&recorded, &commands, 64, 64);
    kui_draw_mono_text_styled(&recorded, 2, 3, "A", KUI_BLACK, KUI_WHITE,
                              0, 2, 1);
    command = (const struct kui_display_command *)commands.data;
    failures += check(commands.length > sizeof(*command) &&
                      command->type == KUI_DISPLAY_TEXT,
                      "styled text command recorded");
    failures += check((command->flags & KUI_TEXT_MONOSPACE) != 0 &&
                      (command->flags & KUI_TEXT_BOLD) != 0 &&
                      ((command->flags & KUI_TEXT_SCALE_MASK) >>
                       KUI_TEXT_SCALE_SHIFT) == 1,
                      "style flags encoded");

    for (int index = 0; index < 64 * 64; index++) {
        normal_pixels[index] = KUI_WHITE;
        bold_pixels[index] = KUI_WHITE;
        replay_pixels[index] = KUI_WHITE;
    }
    kui_surface_init(&normal_surface, normal_pixels, 64, 64);
    kui_surface_init(&bold_surface, bold_pixels, 64, 64);
    kui_surface_init(&replayed, replay_pixels, 64, 64);
    kui_draw_mono_text_styled(&normal_surface, 2, 3, "A", KUI_BLACK,
                              KUI_WHITE, 0, 2, 0);
    kui_draw_mono_text_styled(&bold_surface, 2, 3, "A", KUI_BLACK,
                              KUI_WHITE, 0, 2, 1);
    failures += check(colored_pixels(bold_pixels, 64 * 64) >
                      colored_pixels(normal_pixels, 64 * 64),
                      "bold adds visible stroke weight");
    failures += check(kui_display_replay(&replayed, commands.data,
                                         commands.length) == 0,
                      "styled command replays");
    failures += check(colored_pixels(replay_pixels, 64 * 64) > 0,
                      "replay draws pixels");
    kui_command_buffer_free(&commands);
    if (failures == 0) puts("KUI scaled text tests passed");
    return failures != 0;
}
