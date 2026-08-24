/* Aqua KUI video backend for Quake's 8-bit software renderer. */
#include "quakedef.h"
#include "d_local.h"

#include "extras/kui/kui.h"
#include "quake_aqua.h"

#define BASEWIDTH 320
#define BASEHEIGHT 200

viddef_t vid;

static byte vid_buffer[BASEWIDTH * BASEHEIGHT];
static short zbuffer[BASEWIDTH * BASEHEIGHT];
static byte surfcache[SURFCACHE_SIZE_AT_320X200];
static uint32_t palette32[256];
static int frame_ready;

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
    frame_ready = 1;
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
    (void)rects;
    frame_ready = 1;
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
    frame_ready = 1;
}

void D_EndDirectRect(int x, int y, int width, int height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    frame_ready = 1;
}

void QuakeAqua_DrawFrame(struct kui_surface *surface)
{
    int draw_width;
    int draw_height;
    int x;
    int y;

    if (surface == NULL || surface->width <= 0 || surface->height <= 0)
        return;
    draw_width = surface->width;
    draw_height = draw_width * BASEHEIGHT / BASEWIDTH;
    if (draw_height > surface->height) {
        draw_height = surface->height;
        draw_width = draw_height * BASEWIDTH / BASEHEIGHT;
    }
    x = (surface->width - draw_width) / 2;
    y = (surface->height - draw_height) / 2;
    kui_fill_rect(surface, 0, 0, surface->width, surface->height, KUI_BLACK);
    kui_draw_indexed_bitmap_scaled(surface, vid_buffer, palette32,
                                   BASEWIDTH, BASEHEIGHT,
                                   x, y, draw_width, draw_height);
}

int QuakeAqua_TakeFrameReady(void)
{
    int ready = frame_ready;

    frame_ready = 0;
    return ready;
}
