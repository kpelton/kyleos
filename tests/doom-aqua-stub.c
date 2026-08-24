#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomgeneric.h"
#include "doomkeys.h"

pixel_t *DG_ScreenBuffer;

static void fill(uint32_t color)
{
    for (int i = 0; i < DOOMGENERIC_RESX * DOOMGENERIC_RESY; i++)
        DG_ScreenBuffer[i] = color;
}

void doomgeneric_Create(int argc, char **argv)
{
    if (argc != 3 || strcmp(argv[1], "-iwad") != 0 ||
        strcmp(argv[2], "/usr/share/doom/doom.wad") != 0)
        exit(3);
    DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY *
                             sizeof(*DG_ScreenBuffer));
    if (DG_ScreenBuffer == NULL) exit(2);
    puts("Doom Aqua protocol diagnostic");
    fill(0x00123456U);
    DG_SetWindowTitle("DOOM Shareware");
    DG_DrawFrame();
}

void doomgeneric_Tick(void)
{
    int pressed;
    unsigned char key;

    while (DG_GetKey(&pressed, &key)) {
        if (pressed && key == KEY_FIRE) fill(0x00aa0000U);
    }
    DG_DrawFrame();
}
