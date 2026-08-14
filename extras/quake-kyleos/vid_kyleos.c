/* KyleOS video backend for the original Quake software renderer. */
#include "quakedef.h"
#include "d_local.h"
#include "kyleos_syscalls.h"

#define BASEWIDTH 320
#define BASEHEIGHT 200
#define PRESENT_WIDTH 640
#define PRESENT_HEIGHT 400

viddef_t vid;

static byte vid_buffer[BASEWIDTH * BASEHEIGHT];
static short zbuffer[BASEWIDTH * BASEHEIGHT];
static byte surfcache[SURFCACHE_SIZE_AT_320X200];
static unsigned palette32[256];
static unsigned present_buffer[PRESENT_WIDTH * PRESENT_HEIGHT];

unsigned short d_8to16table[256];
unsigned d_8to24table[256];

void VID_SetPalette(unsigned char *palette)
{
    int i;

    for (i = 0; i < 256; i++) {
        unsigned r = palette[i * 3];
        unsigned g = palette[i * 3 + 1];
        unsigned b = palette[i * 3 + 2];
        unsigned color = (r << 16) | (g << 8) | b;

        palette32[i] = color;
        d_8to24table[i] = color;
        d_8to16table[i] = (unsigned short)(((r >> 3) << 11) |
                                           ((g >> 2) << 5) |
                                           (b >> 3));
    }
}

void VID_ShiftPalette(unsigned char *palette)
{
    VID_SetPalette(palette);
}

void VID_Init(unsigned char *palette)
{
    VID_SetPalette(palette);

    vid.maxwarpwidth = vid.width = vid.conwidth = BASEWIDTH;
    vid.maxwarpheight = vid.height = vid.conheight = BASEHEIGHT;
    vid.aspect = 1.0;
    vid.numpages = 1;
    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));
    vid.buffer = vid.conbuffer = vid_buffer;
    vid.rowbytes = vid.conrowbytes = BASEWIDTH;

    d_pzbuffer = zbuffer;
    D_InitCaches(surfcache, sizeof(surfcache));
}

void VID_Shutdown(void)
{
}

void VID_Update(vrect_t *rects)
{
    int y;
    (void)rects;

    for (y = 0; y < BASEHEIGHT; y++) {
        int x;
        unsigned *row0 = present_buffer + (y * 2) * PRESENT_WIDTH;
        unsigned *row1 = row0 + PRESENT_WIDTH;
        /* Keep palette lookup scalar.  GCC's vectorized gather sequence was
         * unstable on this target; a scalar byte load is cheap at 320x200. */
        volatile byte *source = vid_buffer + y * BASEWIDTH;

        for (x = 0; x < BASEWIDTH; x++) {
            unsigned color = palette32[source[x]];
            row0[x * 2] = color;
            row0[x * 2 + 1] = color;
            row1[x * 2] = color;
            row1[x * 2 + 1] = color;
        }
    }

    if (kyleos_framebuffer_present(present_buffer,
                                   sizeof(present_buffer)) < 0)
        Sys_Error("KyleOS framebuffer presentation failed");
}

int VID_SetMode(int modenum, unsigned char *palette)
{
    (void)modenum;
    VID_SetPalette(palette);
    return 1;
}

void VID_HandlePause(qboolean pause)
{
    (void)pause;
}

void D_BeginDirectRect(int x, int y, byte *bitmap, int width, int height)
{
    int row;

    if (x < 0 || y < 0 || x + width > BASEWIDTH || y + height > BASEHEIGHT)
        return;
    for (row = 0; row < height; row++)
        memcpy(vid_buffer + (y + row) * BASEWIDTH + x,
               bitmap + row * width, width);
}

void D_EndDirectRect(int x, int y, int width, int height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}
