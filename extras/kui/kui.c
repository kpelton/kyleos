#include "kui.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/uapi/framebuffer.h"
#include "include/uapi/mouse.h"
#include "output/font.h"

void kui_surface_init(struct kui_surface *surface, uint32_t *pixels,
                      int width, int height)
{
    surface->pixels = pixels;
    surface->width = width;
    surface->height = height;
    surface->stride = width;
    surface->commands = NULL;
}

int kui_read_all(int fd, void *buffer, uint32_t bytes)
{
    uint32_t total = 0;

    while (total < bytes) {
        int count = read(fd, (char *)buffer + total, bytes - total);
        if (count <= 0) return -1;
        total += (uint32_t)count;
    }
    return 0;
}

int kui_write_all(int fd, const void *buffer, uint32_t bytes)
{
    uint32_t total = 0;

    while (total < bytes) {
        int count = write(fd, (const char *)buffer + total, bytes - total);
        if (count <= 0) return -1;
        total += (uint32_t)count;
    }
    return 0;
}

void kui_command_buffer_init(struct kui_command_buffer *buffer)
{
    memset(buffer, 0, sizeof(*buffer));
}

void kui_command_buffer_reset(struct kui_command_buffer *buffer)
{
    buffer->length = 0;
    buffer->failed = 0;
}

void kui_command_buffer_free(struct kui_command_buffer *buffer)
{
    if (buffer == NULL) return;
    free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}

static void *kui_command_append(struct kui_command_buffer *buffer,
                                uint32_t bytes)
{
    unsigned char *data;
    uint32_t capacity;
    void *result;

    if (buffer == NULL || buffer->failed || bytes == 0 ||
        bytes > KUI_PROTOCOL_MAX_DISPLAY - buffer->length) {
        if (buffer != NULL) buffer->failed = 1;
        return NULL;
    }
    if (buffer->length + bytes > buffer->capacity) {
        capacity = buffer->capacity != 0 ? buffer->capacity : 4096;
        while (capacity < buffer->length + bytes) {
            if (capacity >= KUI_PROTOCOL_MAX_DISPLAY / 2) {
                capacity = KUI_PROTOCOL_MAX_DISPLAY;
                break;
            }
            capacity *= 2;
        }
        data = realloc(buffer->data, capacity);
        if (data == NULL) {
            buffer->failed = 1;
            return NULL;
        }
        buffer->data = data;
        buffer->capacity = capacity;
    }
    result = buffer->data + buffer->length;
    buffer->length += bytes;
    return result;
}

void kui_surface_init_commands(struct kui_surface *surface,
                               struct kui_command_buffer *buffer,
                               int width, int height)
{
    surface->pixels = NULL;
    surface->width = width;
    surface->height = height;
    surface->stride = width;
    surface->commands = buffer;
}

static void kui_record_command(struct kui_surface *surface, uint16_t type,
                               int x0, int y0, int x1, int y1,
                               uint32_t foreground, uint32_t background,
                               uint32_t flags, const char *text)
{
    struct kui_display_command *command;
    uint32_t text_bytes = text != NULL ? (uint32_t)strlen(text) + 1 : 0;
    uint32_t bytes = sizeof(*command) + text_bytes;

    if (bytes > 0xffffU) {
        surface->commands->failed = 1;
        return;
    }
    command = kui_command_append(surface->commands, bytes);
    if (command == NULL) return;
    command->type = type;
    command->size = (uint16_t)bytes;
    command->x0 = x0;
    command->y0 = y0;
    command->x1 = x1;
    command->y1 = y1;
    command->foreground = foreground;
    command->background = background;
    command->flags = flags;
    if (text_bytes != 0)
        memcpy(command + 1, text, text_bytes);
}

static void kui_record_bitmap_row(struct kui_surface *surface,
                                  const uint32_t *pixels,
                                  int source_width, int source_height,
                                  int source_y, int x, int y,
                                  int width, int height)
{
    struct kui_display_command *command;
    uint32_t pixel_bytes;
    uint32_t bytes;

    if (surface == NULL || surface->commands == NULL || pixels == NULL ||
        source_width <= 0 || source_height <= 0 || source_y < 0 ||
        source_y >= source_height) {
        if (surface != NULL && surface->commands != NULL)
            surface->commands->failed = 1;
        return;
    }
    pixel_bytes = (uint32_t)source_width * sizeof(uint32_t);
    bytes = sizeof(*command) + pixel_bytes;
    if (source_width > 4096 || source_height > 2160 || bytes > 0xffffU) {
        surface->commands->failed = 1;
        return;
    }
    command = kui_command_append(surface->commands, bytes);
    if (command == NULL) return;
    command->type = KUI_DISPLAY_BITMAP_ROW;
    command->size = (uint16_t)bytes;
    command->x0 = x;
    command->y0 = y;
    command->x1 = width;
    command->y1 = height;
    command->foreground = (uint32_t)source_width;
    command->background = (uint32_t)source_height;
    command->flags = (uint32_t)source_y;
    memcpy(command + 1, pixels, pixel_bytes);
}

static void kui_record_bitmap_region_row(struct kui_surface *surface,
                                         const uint32_t *pixels,
                                         int source_width, int source_height,
                                         int source_x, int source_y,
                                         int region_width,
                                         int x, int y, int width, int height)
{
    struct kui_display_command *command;
    uint32_t pixel_bytes;
    uint32_t bytes;

    if (surface == NULL || surface->commands == NULL || pixels == NULL ||
        source_width <= 0 || source_width > 4096 || source_height <= 0 ||
        source_height > 2160 || source_x < 0 || source_y < 0 ||
        region_width <= 0 || source_x > source_width - region_width ||
        source_y >= source_height) {
        if (surface != NULL && surface->commands != NULL)
            surface->commands->failed = 1;
        return;
    }
    pixel_bytes = (uint32_t)region_width * sizeof(uint32_t);
    bytes = sizeof(*command) + pixel_bytes;
    if (bytes > 0xffffU) {
        surface->commands->failed = 1;
        return;
    }
    command = kui_command_append(surface->commands, bytes);
    if (command == NULL) return;
    command->type = KUI_DISPLAY_BITMAP_REGION_ROW;
    command->size = (uint16_t)bytes;
    command->x0 = x;
    command->y0 = y;
    command->x1 = width;
    command->y1 = height;
    command->foreground = (uint32_t)source_width;
    command->background = (uint32_t)source_height;
    command->flags = ((uint32_t)source_y << 16) | (uint32_t)source_x;
    memcpy(command + 1, pixels, pixel_bytes);
}

int kui_display_replay(struct kui_surface *surface,
                       const void *commands, uint32_t bytes)
{
    const unsigned char *cursor = commands;
    uint32_t remaining = bytes;

    if (surface == NULL || surface->pixels == NULL ||
        (commands == NULL && bytes != 0)) return -1;
    while (remaining != 0) {
        const struct kui_display_command *command;

        if (remaining < sizeof(*command)) return -1;
        command = (const struct kui_display_command *)cursor;
        if (command->size < sizeof(*command) || command->size > remaining)
            return -1;
        if (command->type == KUI_DISPLAY_FILL_RECT) {
            if (command->x0 < -8192 || command->x0 > 8192 ||
                command->y0 < -8192 || command->y0 > 8192 ||
                command->x1 < 0 || command->x1 > 8192 ||
                command->y1 < 0 || command->y1 > 8192)
                return -1;
            kui_fill_rect(surface, command->x0, command->y0,
                          command->x1, command->y1, command->foreground);
        } else if (command->type == KUI_DISPLAY_LINE) {
            if (command->x0 < -8192 || command->x0 > 8192 ||
                command->y0 < -8192 || command->y0 > 8192 ||
                command->x1 < -8192 || command->x1 > 8192 ||
                command->y1 < -8192 || command->y1 > 8192)
                return -1;
            kui_line(surface, command->x0, command->y0,
                     command->x1, command->y1, command->foreground);
        } else if (command->type == KUI_DISPLAY_TEXT) {
            uint32_t text_bytes = command->size - sizeof(*command);
            const char *text = (const char *)(command + 1);

            if (command->x0 < -8192 || command->x0 > 8192 ||
                command->y0 < -8192 || command->y0 > 8192 ||
                text_bytes == 0 || text_bytes > 4096 ||
                text[text_bytes - 1] != '\0')
                return -1;
            kui_draw_text(surface, command->x0, command->y0, text,
                          command->foreground, command->background,
                          (int)command->flags);
        } else if (command->type == KUI_DISPLAY_BITMAP_ROW) {
            uint32_t source_width = command->foreground;
            uint32_t source_height = command->background;
            uint32_t source_y = command->flags;
            uint32_t expected_size;
            const uint32_t *source = (const uint32_t *)(command + 1);
            int row_top;
            int row_bottom;
            int left;
            int right;

            if (source_width == 0 || source_width > 4096 ||
                source_height == 0 || source_height > 2160 ||
                source_y >= source_height || command->x0 < -8192 ||
                command->x0 > 8192 || command->y0 < -8192 ||
                command->y0 > 8192 || command->x1 <= 0 ||
                command->x1 > 8192 || command->y1 <= 0 ||
                command->y1 > 8192)
                return -1;
            expected_size = sizeof(*command) +
                            source_width * sizeof(uint32_t);
            if (command->size != expected_size) return -1;
            row_top = command->y0 +
                (int)((source_y * (uint32_t)command->y1 +
                       source_height - 1) / source_height);
            row_bottom = command->y0 +
                (int)(((source_y + 1) * (uint32_t)command->y1 +
                       source_height - 1) / source_height);
            left = command->x0 < 0 ? 0 : command->x0;
            right = command->x0 + command->x1;
            if (right > surface->width) right = surface->width;
            if (row_top < 0) row_top = 0;
            if (row_bottom > surface->height) row_bottom = surface->height;
            for (int destination_y = row_top;
                 destination_y < row_bottom; destination_y++) {
                uint32_t *destination = surface->pixels +
                    destination_y * surface->stride;

                for (int destination_x = left;
                     destination_x < right; destination_x++) {
                    int source_x = (destination_x - command->x0) *
                                   (int)source_width / command->x1;
                    destination[destination_x] =
                        source[source_x] & 0x00ffffffU;
                }
            }
        } else if (command->type == KUI_DISPLAY_BITMAP_REGION_ROW) {
            uint32_t source_width = command->foreground;
            uint32_t source_height = command->background;
            uint32_t source_x = command->flags & 0xffffU;
            uint32_t source_y = command->flags >> 16;
            uint32_t payload_bytes = command->size - sizeof(*command);
            uint32_t region_width;
            const uint32_t *source = (const uint32_t *)(command + 1);
            int row_top;
            int row_bottom;
            int left;
            int right;

            if (command->size < sizeof(*command) ||
                payload_bytes == 0 || payload_bytes % sizeof(uint32_t) != 0)
                return -1;
            region_width = payload_bytes / sizeof(uint32_t);
            if (source_width == 0 || source_width > 4096 ||
                source_height == 0 || source_height > 2160 ||
                source_x >= source_width || source_y >= source_height ||
                region_width > source_width - source_x ||
                command->x0 < -8192 || command->x0 > 8192 ||
                command->y0 < -8192 || command->y0 > 8192 ||
                command->x1 <= 0 || command->x1 > 8192 ||
                command->y1 <= 0 || command->y1 > 8192)
                return -1;
            row_top = command->y0 +
                (int)((source_y * (uint32_t)command->y1 +
                       source_height - 1) / source_height);
            row_bottom = command->y0 +
                (int)(((source_y + 1) * (uint32_t)command->y1 +
                       source_height - 1) / source_height);
            left = command->x0 +
                (int)((source_x * (uint32_t)command->x1 +
                       source_width - 1) / source_width);
            right = command->x0 +
                (int)(((source_x + region_width) *
                       (uint32_t)command->x1 + source_width - 1) /
                      source_width);
            if (left < 0) left = 0;
            if (right > surface->width) right = surface->width;
            if (row_top < 0) row_top = 0;
            if (row_bottom > surface->height) row_bottom = surface->height;
            for (int destination_y = row_top;
                 destination_y < row_bottom; destination_y++) {
                uint32_t *destination = surface->pixels +
                    destination_y * surface->stride;

                for (int destination_x = left;
                     destination_x < right; destination_x++) {
                    int pixel_x = (destination_x - command->x0) *
                                  (int)source_width / command->x1;

                    if (pixel_x < (int)source_x)
                        pixel_x = (int)source_x;
                    if (pixel_x >= (int)(source_x + region_width))
                        pixel_x = (int)(source_x + region_width - 1);
                    destination[destination_x] =
                        source[pixel_x - (int)source_x] & 0x00ffffffU;
                }
            }
        } else return -1;
        cursor += command->size;
        remaining -= command->size;
    }
    return 0;
}

int kui_present(const struct kui_surface *surface)
{
    register long rax asm("rax") = 23;
    uint32_t bytes = surface->stride * surface->height * sizeof(uint32_t);
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"(surface->pixels), "S"((long)bytes) : "memory");
    return (int)rax;
}

int kui_present_rect(const struct kui_surface *surface,
                     int x, int y, int width, int height)
{
    struct kyleos_framebuffer_rect rect;
    register long rax asm("rax") = 32;

    if (surface == NULL || surface->pixels == NULL ||
        width <= 0 || height <= 0)
        return -1;
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x >= surface->width || y >= surface->height ||
        width <= 0 || height <= 0)
        return -1;
    if (width > surface->width - x) width = surface->width - x;
    if (height > surface->height - y) height = surface->height - y;
    rect.surface_width = surface->width;
    rect.surface_height = surface->height;
    rect.surface_stride = surface->stride;
    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"(surface->pixels), "S"(&rect) : "memory");
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

int kui_mouse_poll_wheel(int *dx, int *dy, uint8_t *buttons, int *wheel)
{
    static int mouse_fd = -2;
    static struct kyleos_mouse_event events[32];
    static int event_count;
    static int event_index;
    struct kyleos_mouse_event *event;

    if (event_index < event_count) {
        event = &events[event_index++];
        *dx = event->dx;
        *dy = event->dy;
        *buttons = event->buttons;
        *wheel = event->wheel;
        return 1;
    }

    if (mouse_fd == -2)
        mouse_fd = open(KYLEOS_MOUSE_DEVICE, O_RDONLY);
    if (mouse_fd >= 0) {
        int result = read(mouse_fd, (char *)events, sizeof(events));

        if (result >= (int)sizeof(events[0])) {
            event_count = result / (int)sizeof(events[0]);
            event_index = 1;
            event = &events[0];
            *dx = event->dx;
            *dy = event->dy;
            *buttons = event->buttons;
            *wheel = event->wheel;
            return 1;
        }
        if (result == 0)
            return 0;
        close(mouse_fd);
        mouse_fd = -1;
        event_count = 0;
        event_index = 0;
    }

    /* Compatibility path for images built before /dev/mouse existed. */
    register long rax asm("rax") = 30;
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"(dx), "S"(dy), "d"(buttons) : "memory");
    *wheel = 0;
    return (int)rax;
}

int kui_mouse_poll(int *dx, int *dy, uint8_t *buttons)
{
    int wheel;

    return kui_mouse_poll_wheel(dx, dy, buttons, &wheel);
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
    if (surface->commands != NULL) {
        kui_record_command(surface, KUI_DISPLAY_FILL_RECT,
                           x, y, 1, 1, color, 0, 0, NULL);
        return;
    }
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

    if (surface->commands != NULL) {
        kui_record_command(surface, KUI_DISPLAY_FILL_RECT,
                           x, y, w, h, color, 0, 0, NULL);
        return;
    }
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

    if (surface->commands != NULL) {
        kui_record_command(surface, KUI_DISPLAY_LINE,
                           x0, y0, x1, y1, color, 0, 0, NULL);
        return;
    }
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
    if (surface->commands != NULL) {
        char text[2] = { (char)ch, '\0' };
        kui_record_command(surface, KUI_DISPLAY_TEXT, x, y, 0, 0,
                           foreground, background, (uint32_t)opaque, text);
        return;
    }
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
    if (surface->commands != NULL) {
        kui_record_command(surface, KUI_DISPLAY_TEXT, x, y, 0, 0,
                           foreground, background, (uint32_t)opaque, text);
        return;
    }
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

struct kui_kimg_header {
    unsigned char magic[4];
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t flags;
};

static int kui_read_full(int fd, void *buffer, uint32_t bytes)
{
    unsigned char *output = buffer;
    uint32_t total = 0;

    while (total < bytes) {
        int count = read(fd, (char *)output + total, bytes - total);
        if (count <= 0)
            return -1;
        total += (uint32_t)count;
    }
    return 0;
}

int kui_image_load(const char *path, struct kui_image *image)
{
    struct kui_kimg_header header;
    uint32_t pixels;
    uint32_t bytes;
    int fd;

    if (path == NULL || image == NULL)
        return -1;
    memset(image, 0, sizeof(*image));
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    if (kui_read_full(fd, &header, sizeof(header)) < 0 ||
        memcmp(header.magic, "KIMG", 4) != 0 || header.version != 1 ||
        header.width == 0 || header.height == 0 ||
        header.width > 4096 || header.height > 2160 ||
        header.width > 0xffffffffU / header.height) {
        close(fd);
        return -1;
    }
    pixels = header.width * header.height;
    if (pixels > 0xffffffffU / sizeof(uint32_t)) {
        close(fd);
        return -1;
    }
    bytes = pixels * sizeof(uint32_t);
    image->pixels = malloc(bytes);
    if (image->pixels == NULL ||
        kui_read_full(fd, image->pixels, bytes) < 0) {
        free(image->pixels);
        memset(image, 0, sizeof(*image));
        close(fd);
        return -1;
    }
    close(fd);
    image->width = (int)header.width;
    image->height = (int)header.height;
    image->opaque = (header.flags & KUI_KIMG_FLAG_OPAQUE) != 0;
    return 0;
}

void kui_image_free(struct kui_image *image)
{
    if (image == NULL)
        return;
    free(image->pixels);
    memset(image, 0, sizeof(*image));
}

static uint32_t kui_alpha_pixel(uint32_t destination, uint32_t source)
{
    uint32_t alpha = source >> 24;
    uint32_t inverse;
    uint32_t red;
    uint32_t green;
    uint32_t blue;

    if (alpha == 0)
        return destination;
    if (alpha == 255)
        return source & 0x00ffffffU;
    inverse = 255 - alpha;
    red = (((source >> 16) & 0xff) * alpha +
           ((destination >> 16) & 0xff) * inverse + 127) / 255;
    green = (((source >> 8) & 0xff) * alpha +
             ((destination >> 8) & 0xff) * inverse + 127) / 255;
    blue = ((source & 0xff) * alpha + (destination & 0xff) * inverse + 127) /
           255;
    return (red << 16) | (green << 8) | blue;
}

void kui_blit_image(struct kui_surface *surface,
                    const struct kui_image *image, int x, int y)
{
    int source_x = x < 0 ? -x : 0;
    int source_y = y < 0 ? -y : 0;
    int width;
    int height;

    if (surface == NULL || image == NULL || image->pixels == NULL)
        return;
    width = image->width - source_x;
    height = image->height - source_y;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (width > surface->width - x) width = surface->width - x;
    if (height > surface->height - y) height = surface->height - y;
    if (width <= 0 || height <= 0)
        return;
    for (int row = 0; row < height; row++) {
        uint32_t *destination = surface->pixels +
                                (y + row) * surface->stride + x;
        const uint32_t *source = image->pixels +
                                 (source_y + row) * image->width + source_x;
        if (image->opaque)
            memcpy(destination, source, width * sizeof(uint32_t));
        else
            for (int column = 0; column < width; column++)
                destination[column] =
                    kui_alpha_pixel(destination[column], source[column]);
    }
}

void kui_blit_image_scaled(struct kui_surface *surface,
                           const struct kui_image *image,
                           int x, int y, int width, int height)
{
    int left = x < 0 ? 0 : x;
    int top = y < 0 ? 0 : y;
    int right;
    int bottom;

    if (surface == NULL || image == NULL || image->pixels == NULL ||
        width <= 0 || height <= 0)
        return;
    if (width == image->width && height == image->height) {
        kui_blit_image(surface, image, x, y);
        return;
    }
    right = x + width > surface->width ? surface->width : x + width;
    bottom = y + height > surface->height ? surface->height : y + height;
    for (int destination_y = top; destination_y < bottom; destination_y++) {
        int source_y = (destination_y - y) * image->height / height;
        for (int destination_x = left; destination_x < right;
             destination_x++) {
            int source_x = (destination_x - x) * image->width / width;
            uint32_t source = image->pixels[source_y * image->width + source_x];
            uint32_t *destination = surface->pixels +
                destination_y * surface->stride + destination_x;
            *destination = image->opaque ? source & 0x00ffffffU :
                kui_alpha_pixel(*destination, source);
        }
    }
}

void kui_draw_bitmap_scaled(struct kui_surface *surface,
                            const uint32_t *pixels,
                            int source_width, int source_height,
                            int x, int y, int width, int height)
{
    struct kui_image image;

    if (surface == NULL || pixels == NULL || source_width <= 0 ||
        source_height <= 0 || width <= 0 || height <= 0)
        return;
    if (surface->commands == NULL) {
        image.pixels = (uint32_t *)pixels;
        image.width = source_width;
        image.height = source_height;
        image.opaque = 1;
        kui_blit_image_scaled(surface, &image, x, y, width, height);
        return;
    }
    for (int row = 0; row < source_height; row++)
        kui_record_bitmap_row(surface, pixels + row * source_width,
                              source_width, source_height, row,
                              x, y, width, height);
}

void kui_draw_bitmap_region_scaled(struct kui_surface *surface,
                                   const uint32_t *pixels,
                                   int source_width, int source_height,
                                   int source_x, int source_y,
                                   int region_width, int region_height,
                                   int x, int y, int width, int height)
{
    int source_right;
    int source_bottom;

    if (surface == NULL || pixels == NULL || source_width <= 0 ||
        source_height <= 0 || source_x < 0 || source_y < 0 ||
        region_width <= 0 || region_height <= 0 || width <= 0 || height <= 0 ||
        source_x >= source_width || source_y >= source_height)
        return;
    source_right = source_x + region_width;
    source_bottom = source_y + region_height;
    if (source_right > source_width) source_right = source_width;
    if (source_bottom > source_height) source_bottom = source_height;
    if (surface->commands != NULL) {
        for (int row = source_y; row < source_bottom; row++)
            kui_record_bitmap_region_row(
                surface, pixels + row * source_width + source_x,
                source_width, source_height, source_x, row,
                source_right - source_x, x, y, width, height);
        return;
    }
    {
        int top = y + (source_y * height + source_height - 1) /
                      source_height;
        int bottom = y + (source_bottom * height + source_height - 1) /
                         source_height;
        int left = x + (source_x * width + source_width - 1) / source_width;
        int right = x + (source_right * width + source_width - 1) /
                        source_width;

        if (top < 0) top = 0;
        if (bottom > surface->height) bottom = surface->height;
        if (left < 0) left = 0;
        if (right > surface->width) right = surface->width;
        for (int destination_y = top; destination_y < bottom;
             destination_y++) {
            int pixel_y = (destination_y - y) * source_height / height;

            for (int destination_x = left; destination_x < right;
                 destination_x++) {
                int pixel_x = (destination_x - x) * source_width / width;

                surface->pixels[destination_y * surface->stride +
                                destination_x] =
                    pixels[pixel_y * source_width + pixel_x] & 0x00ffffffU;
            }
        }
    }
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
    uint32_t face = pressed ? 0x00c9d9efU : 0x00f4f7fbU;

    kui_fill_rect(surface, x, y, w, h, face);
    kui_outline_rect(surface, x, y, w, h, 0x00003599U);
    kui_fill_rect(surface, x + 2, y + 2, w - 4, 1, KUI_WHITE);
    kui_fill_rect(surface, x + 2, y + h - 3, w - 4, 1, 0x0099a8bdU);
    kui_draw_text(surface, x + (w - kui_text_width(label)) / 2 + offset,
                  y + (h - 16) / 2 + offset, label,
                  KUI_BLACK, face, 0);
}

void kui_draw_window(struct kui_surface *surface, int x, int y, int w, int h,
                     const char *title, int active)
{
    uint32_t top = active ? 0x003d95ffU : 0x0090a6c5U;
    uint32_t bottom = active ? 0x000057c9U : 0x00657791U;

    kui_fill_rect(surface, x + 5, y + 5, w, h, 0x003c4966U);
    kui_fill_rect(surface, x, y, w, h, KUI_FACE);
    kui_outline_rect(surface, x, y, w, h,
                     active ? 0x00003599U : 0x005b6f8eU);
    for (int row = 0; row < 20; row++) {
        uint32_t red = (((top >> 16) & 0xff) * (19 - row) +
                        ((bottom >> 16) & 0xff) * row) / 19;
        uint32_t green = (((top >> 8) & 0xff) * (19 - row) +
                          ((bottom >> 8) & 0xff) * row) / 19;
        uint32_t blue = ((top & 0xff) * (19 - row) +
                         (bottom & 0xff) * row) / 19;
        kui_fill_rect(surface, x + 3, y + 3 + row, w - 6, 1,
                      (red << 16) | (green << 8) | blue);
    }
    kui_fill_rect(surface, x + 4, y + 4, w - 8, 1, 0x007db9ffU);
    kui_draw_text(surface, x + 8, y + 5, title, KUI_WHITE, top, 0);
}
