/* KyleOS display, input, and timer backend for Chocolate Duke3D. */
#include "build.h"
#include "display.h"
#include "draw.h"
#include "engine.h"
#include "kyleos_syscalls.h"
#include "SDL.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DUKE_WIDTH 320
#define DUKE_HEIGHT 200
#define PRESENT_WIDTH 640
#define PRESENT_HEIGHT 400

int32_t xres, yres, bytesperline, imageSize, maxpages;
uint8_t *frameplace;
uint8_t *frameoffset;
uint8_t *screen;
uint8_t vesachecked;
int32_t buffermode, origbuffermode, linearmode;
uint8_t permanentupdate, vgacompatible;

/* The menu declares this SDL-era symbol even though KyleOS never uses it. */
SDL_Surface *surface;

uint8_t lastPalette[768];

static uint8_t video_buffer[DUKE_WIDTH * DUKE_HEIGHT];
static uint32_t present_buffer[PRESENT_WIDTH * PRESENT_HEIGHT];
static uint32_t palette32[256];
static uint8_t palette_vbe[256 * 4];
static unsigned int lastkey;
static uint8_t drawpixel_color;
static int raw_mode_enabled;

static int64_t timerfreq;
static int32_t timerlastsample;
static int timerticspersec;
static void (*usertimercallback)(void);

static void restore_console(void)
{
    if (raw_mode_enabled) {
        kyleos_tty_raw(0);
        raw_mode_enabled = 0;
    }
}

static void present_frame(void)
{
    int y;

    for (y = 0; y < DUKE_HEIGHT; y++) {
        int x;
        const uint8_t *src = video_buffer + y * DUKE_WIDTH;
        uint32_t *row0 = present_buffer + (y * 2) * PRESENT_WIDTH;
        uint32_t *row1 = row0 + PRESENT_WIDTH;

        for (x = 0; x < DUKE_WIDTH; x++) {
            uint32_t color = palette32[src[x]];
            row0[x * 2] = color;
            row0[x * 2 + 1] = color;
            row1[x * 2] = color;
            row1[x * 2 + 1] = color;
        }
    }

    if (kyleos_framebuffer_present(present_buffer,
                                   sizeof(present_buffer)) < 0)
        Error(EXIT_FAILURE, "KyleOS framebuffer presentation failed\n");
}

void _platform_init(int argc, char **argv, const char *title,
                    const char *icon_name)
{
    (void)title;
    (void)icon_name;
    _argc = argc;
    _argv = argv;

    if (!raw_mode_enabled) {
        atexit(restore_console);
        kyleos_tty_raw(1);
        raw_mode_enabled = 1;
    }
}

void _handle_events(void)
{
    int pressed;
    unsigned char code;
    unsigned char extended;

    while (kyleos_key_poll(&pressed, &code, &extended) > 0) {
        if (extended) {
            lastkey = 0xe0;
            keyhandler();
        }
        lastkey = code | (pressed ? 0 : 0x80);
        keyhandler();
    }
}

uint8_t _readlastkeyhit(void)
{
    return (uint8_t)lastkey;
}

void _idle(void)
{
    _handle_events();
    kyleos_sleep_ms(1);
}

void initkeys(void) {}
void uninitkeys(void) {}

void _joystick_init(void) {}
void _joystick_deinit(void) {}
int _joystick_update(void) { return 0; }
int _joystick_axis(int axis) { (void)axis; return 0; }
int _joystick_hat(int hat) { (void)hat; return -1; }
int _joystick_button(int button) { (void)button; return 0; }

int setupmouse(void)
{
    moustat = 0;
    return 0;
}

void readmousexy(short *x, short *y)
{
    if (x != NULL) *x = 0;
    if (y != NULL) *y = 0;
}

void readmousebstatus(short *status)
{
    if (status != NULL) *status = 0;
}

void getvalidvesamodes(void)
{
    if (validmodecnt != 0)
        return;
    validmodecnt = 1;
    validmode[0] = 0;
    validmodexdim[0] = DUKE_WIDTH;
    validmodeydim[0] = DUKE_HEIGHT;
}

int32_t _setgamemode(uint8_t davidoption, int32_t requested_width,
                     int32_t requested_height)
{
    int i;
    int lookup_bytes;

    (void)davidoption;
    (void)requested_width;
    (void)requested_height;

    xdim = xres = DUKE_WIDTH;
    ydim = yres = DUKE_HEIGHT;
    bytesperline = DUKE_WIDTH;
    imageSize = sizeof(video_buffer);
    maxpages = numpages = 1;
    vesachecked = vgacompatible = linearmode = 1;
    buffermode = origbuffermode = 0;
    qsetmode = DUKE_HEIGHT;
    activepage = visualpage = 0;
    vidoption = 1;

    frameplace = frameoffset = screen = video_buffer;
    memset(video_buffer, 0, sizeof(video_buffer));

    lookup_bytes = ydim * 4 * (int)sizeof(int32_t);
    free(horizlookup);
    free(horizlookup2);
    horizlookup = malloc(lookup_bytes);
    horizlookup2 = malloc(lookup_bytes);
    if (horizlookup == NULL || horizlookup2 == NULL)
        Error(EXIT_FAILURE, "Unable to allocate Duke3D video lookups\n");

    for (i = 0; i <= ydim; i++)
        ylookup[i] = i * bytesperline;

    horizycent = (ydim * 4) >> 1;
    oxyaspect = oxdimen = oviewingrange = -1;
    setBytesPerLine(bytesperline);
    setview(0, 0, xdim - 1, ydim - 1);
    setbrightness(curbrightness, palette);
    return 0;
}

void setvmode(int mode) { (void)mode; }

void _uninitengine(void)
{
    restore_console();
}

void *_getVideoBase(void)
{
    return video_buffer;
}

int VBE_setPalette(uint8_t *palettebuffer)
{
    int i;

    memcpy(palette_vbe, palettebuffer, sizeof(palette_vbe));
    for (i = 0; i < 256; i++) {
        unsigned b = palettebuffer[i * 4] * 255U / 63U;
        unsigned g = palettebuffer[i * 4 + 1] * 255U / 63U;
        unsigned r = palettebuffer[i * 4 + 2] * 255U / 63U;
        palette32[i] = (r << 16) | (g << 8) | b;
        lastPalette[i * 3] = palettebuffer[i * 4 + 2];
        lastPalette[i * 3 + 1] = palettebuffer[i * 4 + 1];
        lastPalette[i * 3 + 2] = palettebuffer[i * 4];
    }
    return 1;
}

int VBE_getPalette(int32_t start, int32_t num, uint8_t *palettebuffer)
{
    if (start < 0 || num < 0 || start + num > 256)
        return 0;
    memcpy(palettebuffer + start * 4, palette_vbe + start * 4, num * 4);
    return 1;
}

void _nextpage(void)
{
    _handle_events();
    present_frame();
}

void _updateScreenRect(int32_t x, int32_t y, int32_t w, int32_t h)
{
    (void)x; (void)y; (void)w; (void)h;
    present_frame();
}

uint8_t readpixel(uint8_t *location) { return *location; }
void drawpixel(uint8_t *location, uint8_t pixel) { *location = pixel; }
void setcolor16(uint8_t color) { drawpixel_color = color; }

void drawpixel16(int32_t offset)
{
    if (offset >= 0 && offset < imageSize)
        video_buffer[offset] = drawpixel_color;
}

void fillscreen16(int32_t offset, int32_t color, int32_t blocksize)
{
    if (offset < 0) {
        blocksize += offset;
        offset = 0;
    }
    if (blocksize <= 0 || offset >= imageSize)
        return;
    if (blocksize > imageSize - offset)
        blocksize = imageSize - offset;
    memset(video_buffer + offset, color, blocksize);
}

void drawline16(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                uint8_t color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        if (x0 >= 0 && x0 < xdim && y0 >= 0 && y0 < ydim)
            video_buffer[y0 * bytesperline + x0] = color;
        if (x0 == x1 && y0 == y1)
            break;
        if (2 * error >= dy) { error += dy; x0 += sx; }
        if (2 * error <= dx) { error += dx; y0 += sy; }
    }
}

void clear2dscreen(void)
{
    memset(video_buffer, 0, sizeof(video_buffer));
}

int screencapture(char *filename, uint8_t inverse)
{
    (void)filename;
    (void)inverse;
    return -1;
}

void fullscreen_toggle_and_change_driver(void) {}

void TIMER_GetPlatformTicks(int64_t *ticks)
{
    *ticks = kyleos_ticks_ms();
}

int TIMER_GetPlatformTicksInOneSecond(int64_t *ticks)
{
    *ticks = 1000;
    return 1;
}

void (*installusertimercallback(void (*callback)(void)))(void)
{
    void (*old_callback)(void) = usertimercallback;
    usertimercallback = callback;
    return old_callback;
}

int inittimer(int tickspersecond)
{
    int64_t now;

    if (timerfreq != 0)
        return 0;
    timerfreq = 1000;
    timerticspersec = tickspersecond;
    TIMER_GetPlatformTicks(&now);
    timerlastsample = (int32_t)(now * timerticspersec / timerfreq);
    usertimercallback = NULL;
    return 0;
}

void uninittimer(void)
{
    timerfreq = 0;
    timerticspersec = 0;
}

void sampletimer(void)
{
    int64_t now;
    int32_t elapsed;

    if (timerfreq == 0)
        return;
    TIMER_GetPlatformTicks(&now);
    elapsed = (int32_t)(now * timerticspersec / timerfreq) -
              timerlastsample;
    if (elapsed <= 0)
        return;
    totalclock += elapsed;
    timerlastsample += elapsed;
    while (usertimercallback != NULL && elapsed-- > 0)
        usertimercallback();
}

uint32_t getticks(void)
{
    return kyleos_ticks_ms();
}

int gettimerfreq(void)
{
    return timerticspersec;
}
