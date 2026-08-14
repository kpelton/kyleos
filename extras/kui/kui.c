#include "kui.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "include/uapi/mouse.h"
#include "output/font.h"

void kui_surface_init(struct kui_surface *surface, uint32_t *pixels,
                      int width, int height)
{
    surface->pixels = pixels;
    surface->width = width;
    surface->height = height;
    surface->stride = width;
}

int kui_present(const struct kui_surface *surface)
{
    register long rax asm("rax") = 23;
    uint32_t bytes = surface->stride * surface->height * sizeof(uint32_t);
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"(surface->pixels), "S"((long)bytes) : "memory");
    return (int)rax;
}

int kui_screen_size(int *width, int *height)
{
    register long rax asm("rax") = 31;
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"(width), "S"(height) : "memory");
    return (int)rax;
}

int kui_key_poll(int *pressed, uint8_t *scancode, uint8_t *extended)
{
    register long rax asm("rax") = 24;
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"(pressed), "S"(scancode), "d"(extended) : "memory");
    return (int)rax;
}

void kui_tty_raw(int enabled)
{
    register long rax asm("rax") = 27;
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"((long)enabled) : "memory");
}

int kui_mouse_poll(int *dx, int *dy, uint8_t *buttons)
{
    static int mouse_fd = -2;
    struct kyleos_mouse_event event;

    if (mouse_fd == -2)
        mouse_fd = open(KYLEOS_MOUSE_DEVICE, O_RDONLY);
    if (mouse_fd >= 0) {
        int result = read(mouse_fd, (char *)&event, sizeof(event));

        if (result == sizeof(event)) {
            *dx = event.dx;
            *dy = event.dy;
            *buttons = event.buttons;
            return 1;
        }
        if (result == 0)
            return 0;
        close(mouse_fd);
        mouse_fd = -1;
    }

    /* Compatibility path for images built before /dev/mouse existed. */
    register long rax asm("rax") = 30;
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"(dx), "S"(dy), "d"(buttons) : "memory");
    return (int)rax;
}

uint32_t kui_ticks_ms(void)
{
    register long rax asm("rax") = 25;
    asm volatile("int $0x80" : "+a"(rax) : : "memory");
    return (uint32_t)rax;
}

void kui_sleep_ms(uint32_t milliseconds)
{
    register long rax asm("rax") = 0;
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"((long)milliseconds) : "memory");
}

int kui_pointer_delta(int delta)
{
    /* Fixed scaling avoids visible jumps at the old 1x/2x/3x thresholds. */
    return delta * 2;
}

char kui_scan_to_char(uint8_t scan, int shift)
{
    static const char normal[] = {
        0, 0, '1','2','3','4','5','6','7','8','9','0','-','=',0,0,
        'q','w','e','r','t','y','u','i','o','p','[',']',0,0,
        'a','s','d','f','g','h','j','k','l',';',39,'`',0,'\\',
        'z','x','c','v','b','n','m',',','.','/',0,0,0,' '
    };
    static const char shifted[] = {
        0, 0, '!','@','#','$','%','^','&','*','(',')','_','+',0,0,
        'Q','W','E','R','T','Y','U','I','O','P','{','}',0,0,
        'A','S','D','F','G','H','J','K','L',':',34,'~',0,'|',
        'Z','X','C','V','B','N','M','<','>','?',0,0,0,' '
    };

    if (scan >= sizeof(normal)) return 0;
    return shift ? shifted[scan] : normal[scan];
}

int kui_in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && py >= y && px < x + w && py < y + h;
}

void kui_pixel(struct kui_surface *surface, int x, int y, uint32_t color)
{
    if ((unsigned)x < (unsigned)surface->width &&
        (unsigned)y < (unsigned)surface->height)
        surface->pixels[y * surface->stride + x] = color;
}

void kui_fill_rect(struct kui_surface *surface, int x, int y, int w, int h,
                   uint32_t color)
{
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > surface->width ? surface->width : x + w;
    int y1 = y + h > surface->height ? surface->height : y + h;

    for (int row = y0; row < y1; row++)
        for (int col = x0; col < x1; col++)
            surface->pixels[row * surface->stride + col] = color;
}

void kui_outline_rect(struct kui_surface *surface, int x, int y, int w, int h,
                      uint32_t color)
{
    kui_fill_rect(surface, x, y, w, 1, color);
    kui_fill_rect(surface, x, y + h - 1, w, 1, color);
    kui_fill_rect(surface, x, y, 1, h, color);
    kui_fill_rect(surface, x + w - 1, y, 1, h, color);
}

void kui_line(struct kui_surface *surface, int x0, int y0, int x1, int y1,
              uint32_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        kui_pixel(surface, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int twice = error * 2;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}

void kui_draw_char(struct kui_surface *surface, int x, int y,
                   unsigned char ch, uint32_t foreground,
                   uint32_t background, int opaque)
{
    for (int row = 0; row < 16; row++) {
        uint8_t bits = terminal_font[(unsigned)ch * 16 + row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col))
                kui_pixel(surface, x + col, y + row, foreground);
            else if (opaque)
                kui_pixel(surface, x + col, y + row, background);
        }
    }
}

void kui_draw_text(struct kui_surface *surface, int x, int y,
                   const char *text, uint32_t foreground,
                   uint32_t background, int opaque)
{
    while (*text != '\0') {
        kui_draw_char(surface, x, y, (unsigned char)*text++, foreground,
                      background, opaque);
        x += 8;
    }
}

int kui_text_width(const char *text)
{
    return (int)strlen(text) * 8;
}

void kui_draw_desktop(struct kui_surface *surface, int menu_height)
{
    for (int y = menu_height; y < surface->height; y++)
        for (int x = 0; x < surface->width; x++)
            surface->pixels[y * surface->stride + x] =
                ((x + y * 3) & 15) == 0 ? KUI_DESKTOP_DARK : KUI_DESKTOP_TEAL;
}

void kui_draw_bevel(struct kui_surface *surface, int x, int y, int w, int h,
                    uint32_t face, int pressed)
{
    uint32_t top = pressed ? KUI_SHADOW : KUI_WHITE;
    uint32_t bottom = pressed ? KUI_WHITE : KUI_SHADOW;

    kui_fill_rect(surface, x, y, w, h, face);
    kui_fill_rect(surface, x, y, w, 1, top);
    kui_fill_rect(surface, x, y, 1, h, top);
    kui_fill_rect(surface, x, y + h - 1, w, 1, bottom);
    kui_fill_rect(surface, x + w - 1, y, 1, h, bottom);
    kui_outline_rect(surface, x - 1, y - 1, w + 2, h + 2, KUI_BLACK);
}

void kui_draw_button(struct kui_surface *surface, int x, int y, int w, int h,
                     const char *label, int pressed)
{
    int offset = pressed ? 1 : 0;
    kui_draw_bevel(surface, x, y, w, h, KUI_FACE, pressed);
    kui_draw_text(surface, x + (w - kui_text_width(label)) / 2 + offset,
                  y + (h - 16) / 2 + offset, label,
                  KUI_BLACK, KUI_FACE, 0);
}

void kui_draw_window(struct kui_surface *surface, int x, int y, int w, int h,
                     const char *title, int active)
{
    kui_fill_rect(surface, x + 5, y + 5, w, h, KUI_BLACK);
    kui_draw_bevel(surface, x, y, w, h, KUI_FACE, 0);
    kui_fill_rect(surface, x + 3, y + 3, w - 6, 16,
                  active ? KUI_TITLE_BLUE : KUI_SHADOW);
    kui_draw_text(surface, x + (w - kui_text_width(title)) / 2, y + 3,
                  title, KUI_WHITE,
                  active ? KUI_TITLE_BLUE : KUI_SHADOW, 0);
}
