#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"
#include "extras/kui/kui_dialog.h"
#include "extras/kui/kui_file_dialog.h"
#include "extras/kui/kui_menu.h"
#include "extras/paint/paint_brush.h"

#define PAINT_CANVAS_WIDTH 256
#define PAINT_CANVAS_HEIGHT 192
#define PAINT_PATH_MAX KUI_FILE_DIALOG_PATH_MAX
#define PAINT_TOOLBAR_TOP KUI_MENU_BAR_HEIGHT
#define PAINT_TOOLBAR_HEIGHT 67
#define PAINT_STATUS_HEIGHT 23
#define PAINT_SIZE_BUTTON_X 62
#define PAINT_SIZE_BUTTON_Y (PAINT_TOOLBAR_TOP + 46)
#define PAINT_SIZE_BUTTON_WIDTH 20
#define PAINT_SIZE_BUTTON_STEP 22

enum paint_tool {
    PAINT_TOOL_PENCIL,
    PAINT_TOOL_ERASER
};

enum paint_menu_action {
    PAINT_MENU_OPEN = 1,
    PAINT_MENU_SAVE,
    PAINT_MENU_SAVE_AS,
    PAINT_MENU_CLEAR
};

enum paint_confirmation {
    PAINT_CONFIRM_NONE,
    PAINT_CONFIRM_OPEN,
    PAINT_CONFIRM_CLEAR
};

struct paint_state {
    uint32_t pixels[PAINT_CANVAS_WIDTH * PAINT_CANVAS_HEIGHT];
    char path[PAINT_PATH_MAX];
    char pending_path[PAINT_PATH_MAX];
    char status[64];
    int tool;
    int color;
    int brush_size;
    int dirty;
    int drawing;
    int last_x;
    int last_y;
    int confirmation;
};

struct paint_layout {
    int canvas_x;
    int canvas_y;
    int canvas_width;
    int canvas_height;
};

struct paint_damage {
    int x;
    int y;
    int width;
    int height;
    int valid;
};

struct paint_kimg_header {
    unsigned char magic[4];
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t flags;
};

static const uint32_t paint_colors[] = {
    0x00000000U, 0x00808080U, 0x00c0c0c0U, 0x00ffffffU,
    0x00800000U, 0x00ff0000U, 0x00808000U, 0x00ffff00U,
    0x00008000U, 0x0000ff00U, 0x00008080U, 0x0000ffffU,
    0x00000080U, 0x000000ffU, 0x00800080U, 0x00ff00ffU
};

static const int paint_brush_sizes[] = { 1, 3, 5, 9, 15 };

static const struct kui_menu_item paint_file_items[] = {
    { "Open...", "Ctrl+O", PAINT_MENU_OPEN, 1 },
    { "Save", "Ctrl+S", PAINT_MENU_SAVE, 1 },
    { "Save As...", "Ctrl+Shift+S", PAINT_MENU_SAVE_AS, 1 }
};

static const struct kui_menu_item paint_edit_items[] = {
    { "Clear Image", NULL, PAINT_MENU_CLEAR, 1 }
};

static const struct kui_menu paint_menus[] = {
    { "File", paint_file_items, 3 },
    { "Edit", paint_edit_items, 1 }
};

static const char *paint_base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static void paint_clear(struct paint_state *state, int dirty)
{
    for (int i = 0; i < PAINT_CANVAS_WIDTH * PAINT_CANVAS_HEIGHT; i++)
        state->pixels[i] = KUI_WHITE;
    state->dirty = dirty;
    state->drawing = 0;
    state->status[0] = '\0';
}

static void paint_layout(const struct kui_event *event,
                         struct paint_layout *layout)
{
    int available_width = event->width - 24;
    int available_height = event->height - PAINT_TOOLBAR_TOP -
                           PAINT_TOOLBAR_HEIGHT - PAINT_STATUS_HEIGHT - 12;

    if (available_width < 1) available_width = 1;
    if (available_height < 1) available_height = 1;
    layout->canvas_width = available_width;
    layout->canvas_height = available_width * PAINT_CANVAS_HEIGHT /
                            PAINT_CANVAS_WIDTH;
    if (layout->canvas_height > available_height) {
        layout->canvas_height = available_height;
        layout->canvas_width = available_height * PAINT_CANVAS_WIDTH /
                               PAINT_CANVAS_HEIGHT;
    }
    if (layout->canvas_width < 1) layout->canvas_width = 1;
    if (layout->canvas_height < 1) layout->canvas_height = 1;
    layout->canvas_x = (event->width - layout->canvas_width) / 2;
    layout->canvas_y = PAINT_TOOLBAR_TOP + PAINT_TOOLBAR_HEIGHT +
                       (available_height - layout->canvas_height) / 2;
}

static int paint_canvas_point(const struct paint_layout *layout,
                              int x, int y, int *canvas_x, int *canvas_y)
{
    if (!kui_in_rect(x, y, layout->canvas_x, layout->canvas_y,
                     layout->canvas_width, layout->canvas_height))
        return 0;
    *canvas_x = (x - layout->canvas_x) * PAINT_CANVAS_WIDTH /
                layout->canvas_width;
    *canvas_y = (y - layout->canvas_y) * PAINT_CANVAS_HEIGHT /
                layout->canvas_height;
    if (*canvas_x >= PAINT_CANVAS_WIDTH) *canvas_x = PAINT_CANVAS_WIDTH - 1;
    if (*canvas_y >= PAINT_CANVAS_HEIGHT) *canvas_y = PAINT_CANVAS_HEIGHT - 1;
    return 1;
}

static void paint_set_damage(struct paint_damage *damage,
                             int x0, int y0, int x1, int y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= PAINT_CANVAS_WIDTH) x1 = PAINT_CANVAS_WIDTH - 1;
    if (y1 >= PAINT_CANVAS_HEIGHT) y1 = PAINT_CANVAS_HEIGHT - 1;
    if (x0 > x1 || y0 > y1) return;
    damage->x = x0;
    damage->y = y0;
    damage->width = x1 - x0 + 1;
    damage->height = y1 - y0 + 1;
    damage->valid = 1;
}

static int paint_scale_edge(int value, int destination, int source)
{
    return (value * destination + source - 1) / source;
}

static void paint_damage_destination(const struct paint_layout *layout,
                                     const struct paint_damage *damage,
                                     int *x, int *y, int *width, int *height)
{
    int right;
    int bottom;

    *x = layout->canvas_x +
         paint_scale_edge(damage->x, layout->canvas_width,
                          PAINT_CANVAS_WIDTH);
    *y = layout->canvas_y +
         paint_scale_edge(damage->y, layout->canvas_height,
                          PAINT_CANVAS_HEIGHT);
    right = layout->canvas_x +
            paint_scale_edge(damage->x + damage->width,
                             layout->canvas_width, PAINT_CANVAS_WIDTH);
    bottom = layout->canvas_y +
             paint_scale_edge(damage->y + damage->height,
                              layout->canvas_height, PAINT_CANVAS_HEIGHT);
    *width = right - *x;
    *height = bottom - *y;
}

static void paint_brush(struct paint_state *state, int x, int y)
{
    uint32_t color = state->tool == PAINT_TOOL_ERASER ?
                     KUI_WHITE : paint_colors[state->color];

    paint_brush_stamp(state->pixels, PAINT_CANVAS_WIDTH,
                      PAINT_CANVAS_HEIGHT, x, y, state->brush_size, color);
    state->dirty = 1;
    state->status[0] = '\0';
}

static void paint_line(struct paint_state *state,
                       int x0, int y0, int x1, int y1)
{
    uint32_t color = state->tool == PAINT_TOOL_ERASER ?
                     KUI_WHITE : paint_colors[state->color];

    paint_brush_stroke(state->pixels, PAINT_CANVAS_WIDTH,
                       PAINT_CANVAS_HEIGHT, x0, y0, x1, y1,
                       state->brush_size, color);
    state->dirty = 1;
    state->status[0] = '\0';
}

static uint32_t paint_pixel_on_white(uint32_t source, int opaque)
{
    uint32_t alpha;
    uint32_t inverse;
    uint32_t red;
    uint32_t green;
    uint32_t blue;

    if (opaque) return source & 0x00ffffffU;
    alpha = source >> 24;
    inverse = 255 - alpha;
    red = (((source >> 16) & 0xff) * alpha + 255 * inverse + 127) / 255;
    green = (((source >> 8) & 0xff) * alpha + 255 * inverse + 127) / 255;
    blue = ((source & 0xff) * alpha + 255 * inverse + 127) / 255;
    return (red << 16) | (green << 8) | blue;
}

static int paint_load(struct paint_state *state, const char *path)
{
    struct kui_image image;

    if (kui_image_load(path, &image) < 0) return -1;
    for (int y = 0; y < PAINT_CANVAS_HEIGHT; y++) {
        int source_y = y * image.height / PAINT_CANVAS_HEIGHT;

        for (int x = 0; x < PAINT_CANVAS_WIDTH; x++) {
            int source_x = x * image.width / PAINT_CANVAS_WIDTH;
            state->pixels[y * PAINT_CANVAS_WIDTH + x] =
                paint_pixel_on_white(
                    image.pixels[source_y * image.width + source_x],
                    image.opaque);
        }
    }
    kui_image_free(&image);
    strncpy(state->path, path, sizeof(state->path) - 1);
    state->path[sizeof(state->path) - 1] = '\0';
    state->dirty = 0;
    state->drawing = 0;
    state->status[0] = '\0';
    return 0;
}

static int paint_save_to(struct paint_state *state, const char *path)
{
    struct paint_kimg_header header = {
        { 'K', 'I', 'M', 'G' }, 1, PAINT_CANVAS_WIDTH,
        PAINT_CANVAS_HEIGHT, KUI_KIMG_FLAG_OPAQUE
    };
    uint32_t bytes = sizeof(state->pixels);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int result = -1;

    if (fd >= 0 && kui_write_all(fd, &header, sizeof(header)) == 0 &&
        kui_write_all(fd, state->pixels, bytes) == 0)
        result = 0;
    if (fd >= 0) close(fd);
    if (result == 0) {
        size_t length = strlen(path);

        if (length >= sizeof(state->path)) length = sizeof(state->path) - 1;
        memmove(state->path, path, length);
        state->path[length] = '\0';
        state->dirty = 0;
        strcpy(state->status, "Saved");
    } else {
        strcpy(state->status, "Save failed");
    }
    return result;
}

static void paint_open_file_dialog(struct kui_file_dialog *dialog,
                                   const struct paint_state *state, int mode)
{
    const char *path = state->path[0] != '\0' ? state->path :
                       "/PICTURE.KIMG";

    kui_file_dialog_open(dialog, mode, path);
}

static void paint_request_open(struct paint_state *state,
                               struct kui_file_dialog *file_dialog,
                               struct kui_confirm_dialog *confirm_dialog,
                               const char *path)
{
    if (state->dirty) {
        strncpy(state->pending_path, path, sizeof(state->pending_path) - 1);
        state->pending_path[sizeof(state->pending_path) - 1] = '\0';
        state->confirmation = PAINT_CONFIRM_OPEN;
        kui_file_dialog_close(file_dialog);
        kui_confirm_dialog_open(confirm_dialog, "Unsaved Image",
                                "Discard changes and open another image?",
                                "Open", "Cancel");
    } else if (paint_load(state, path) == 0) {
        kui_file_dialog_close(file_dialog);
    } else {
        kui_file_dialog_set_error(file_dialog, "Not a valid KIMG image");
    }
}

static void paint_render(struct kui_client *client,
                         const struct kui_event *event,
                         const struct paint_state *state)
{
    struct paint_layout layout;
    char default_status[64];
    int palette_x = 176;
    int palette_y = PAINT_TOOLBAR_TOP + 8;

    paint_layout(event, &layout);
    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  KUI_FACE);
    kui_fill_rect(&client->surface, 0, PAINT_TOOLBAR_TOP,
                  event->width, PAINT_TOOLBAR_HEIGHT, 0x00ddd9c8U);
    kui_fill_rect(&client->surface, 0,
                  PAINT_TOOLBAR_TOP + PAINT_TOOLBAR_HEIGHT - 1,
                  event->width, 1, KUI_SHADOW);
    kui_draw_button(&client->surface, 8, PAINT_TOOLBAR_TOP + 10,
                    72, 31, "Pencil", state->tool == PAINT_TOOL_PENCIL);
    kui_draw_button(&client->surface, 88, PAINT_TOOLBAR_TOP + 10,
                    72, 31, "Eraser", state->tool == PAINT_TOOL_ERASER);
    kui_fill_rect(&client->surface, 8, PAINT_TOOLBAR_TOP + 48,
                  22, 14, paint_colors[state->color]);
    kui_outline_rect(&client->surface, 8, PAINT_TOOLBAR_TOP + 48,
                     22, 14, KUI_BLACK);
    kui_draw_text(&client->surface, 35, PAINT_TOOLBAR_TOP + 48,
                  "Size", KUI_BLACK, 0x00ddd9c8U, 0);
    for (unsigned int i = 0;
         i < sizeof(paint_brush_sizes) / sizeof(paint_brush_sizes[0]); i++) {
        char label[4];

        snprintf(label, sizeof(label), "%d", paint_brush_sizes[i]);
        kui_draw_button(&client->surface,
                        PAINT_SIZE_BUTTON_X + (int)i * PAINT_SIZE_BUTTON_STEP,
                        PAINT_SIZE_BUTTON_Y, PAINT_SIZE_BUTTON_WIDTH, 18,
                        label, state->brush_size == paint_brush_sizes[i]);
    }
    for (int i = 0; i < 16; i++) {
        int x = palette_x + (i % 8) * 24;
        int y = palette_y + (i / 8) * 25;

        kui_fill_rect(&client->surface, x, y, 20, 20, paint_colors[i]);
        kui_outline_rect(&client->surface, x, y, 20, 20,
                         i == state->color ? KUI_TITLE_BLUE : KUI_BLACK);
        if (i == state->color)
            kui_outline_rect(&client->surface, x - 2, y - 2, 24, 24,
                             KUI_TITLE_BLUE);
    }
    kui_fill_rect(&client->surface, layout.canvas_x - 3,
                  layout.canvas_y - 3, layout.canvas_width + 6,
                  layout.canvas_height + 6, KUI_SHADOW);
    kui_outline_rect(&client->surface, layout.canvas_x - 4,
                     layout.canvas_y - 4, layout.canvas_width + 8,
                     layout.canvas_height + 8, KUI_BLACK);
    kui_draw_bitmap_scaled(&client->surface, state->pixels,
                           PAINT_CANVAS_WIDTH, PAINT_CANVAS_HEIGHT,
                           layout.canvas_x, layout.canvas_y,
                           layout.canvas_width, layout.canvas_height);
    kui_fill_rect(&client->surface, 0, event->height - PAINT_STATUS_HEIGHT,
                  event->width, PAINT_STATUS_HEIGHT, KUI_FACE);
    kui_fill_rect(&client->surface, 0, event->height - PAINT_STATUS_HEIGHT,
                  event->width, 1, KUI_WHITE);
    snprintf(default_status, sizeof(default_status),
             "256 x 192 KIMG - %d px %s", state->brush_size,
             state->tool == PAINT_TOOL_ERASER ? "eraser" : "brush");
    kui_draw_text(&client->surface, 7,
                  event->height - PAINT_STATUS_HEIGHT + 5,
                  state->status[0] != '\0' ? state->status : default_status,
                  KUI_BLACK, KUI_FACE, 0);
}

static void paint_render_damage(struct kui_client *client,
                                const struct kui_event *event,
                                const struct paint_state *state,
                                const struct paint_damage *damage)
{
    struct paint_layout layout;
    int x;
    int y;
    int width;
    int height;

    paint_layout(event, &layout);
    paint_damage_destination(&layout, damage, &x, &y, &width, &height);
    if (width <= 0 || height <= 0) {
        kui_client_set_no_display(client);
        return;
    }
    kui_client_set_damage(client, x, y, width, height);
    kui_draw_bitmap_region_scaled(
        &client->surface, state->pixels,
        PAINT_CANVAS_WIDTH, PAINT_CANVAS_HEIGHT,
        damage->x, damage->y, damage->width, damage->height,
        layout.canvas_x, layout.canvas_y,
        layout.canvas_width, layout.canvas_height);
}

static int paint_client_main(void)
{
    static struct paint_state state;
    struct kui_file_dialog file_dialog;
    struct kui_confirm_dialog confirm_dialog;
    struct kui_menu_state menu_state;
    struct kui_client client;
    struct kui_event event;

    memset(&state, 0, sizeof(state));
    memset(&file_dialog, 0, sizeof(file_dialog));
    memset(&confirm_dialog, 0, sizeof(confirm_dialog));
    paint_clear(&state, 0);
    kui_menu_init(&menu_state);
    kui_client_init(&client);
    for (;;) {
        struct paint_layout layout;
        struct paint_damage damage;
        int menu_action = 0;
        int menu_consumed = 0;
        int saved = 0;
        int partial_display = 0;
        int no_display = 0;
        char title[KUI_PROTOCOL_TITLE_MAX];

        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        memset(&damage, 0, sizeof(damage));
        paint_layout(&event, &layout);
        if (event.type == KUI_EVENT_INIT) {
            paint_clear(&state, 0);
            state.path[0] = '\0';
            state.pending_path[0] = '\0';
            state.tool = PAINT_TOOL_PENCIL;
            state.color = 0;
            state.brush_size = 3;
            state.confirmation = PAINT_CONFIRM_NONE;
            kui_menu_close(&menu_state);
            kui_file_dialog_close(&file_dialog);
            kui_confirm_dialog_close(&confirm_dialog);
            if (event.argument[0] != '\0' &&
                paint_load(&state, event.argument) < 0)
                strcpy(state.status, "Could not open image");
        } else if (confirm_dialog.active) {
            int result = kui_confirm_dialog_handle(&confirm_dialog, &event);

            if (result == KUI_CONFIRM_DIALOG_ACCEPT) {
                kui_confirm_dialog_close(&confirm_dialog);
                if (state.confirmation == PAINT_CONFIRM_OPEN) {
                    if (paint_load(&state, state.pending_path) < 0) {
                        kui_file_dialog_open(&file_dialog,
                                             KUI_FILE_DIALOG_OPEN,
                                             state.pending_path);
                        kui_file_dialog_set_error(&file_dialog,
                                                  "Not a valid KIMG image");
                    }
                    state.pending_path[0] = '\0';
                } else if (state.confirmation == PAINT_CONFIRM_CLEAR) {
                    paint_clear(&state, 1);
                }
                state.confirmation = PAINT_CONFIRM_NONE;
            } else if (result == KUI_CONFIRM_DIALOG_CANCEL) {
                state.pending_path[0] = '\0';
                state.confirmation = PAINT_CONFIRM_NONE;
            }
        } else if (file_dialog.active) {
            char selected_path[PAINT_PATH_MAX];
            int result;

            selected_path[0] = '\0';
            result = kui_file_dialog_handle(&file_dialog, &event,
                                            selected_path,
                                            sizeof(selected_path));
            if (result == KUI_FILE_DIALOG_ACCEPT) {
                if (file_dialog.mode == KUI_FILE_DIALOG_OPEN) {
                    paint_request_open(&state, &file_dialog,
                                       &confirm_dialog, selected_path);
                } else if (paint_save_to(&state, selected_path) == 0) {
                    kui_file_dialog_close(&file_dialog);
                    saved = 1;
                } else {
                    kui_file_dialog_set_error(&file_dialog,
                                              "Could not save this image");
                }
            }
        } else if ((menu_action = kui_menu_handle(
                        &menu_state, &event, paint_menus, 2,
                        &menu_consumed)) != 0) {
            if (menu_action == PAINT_MENU_OPEN) {
                paint_open_file_dialog(&file_dialog, &state,
                                       KUI_FILE_DIALOG_OPEN);
            } else if (menu_action == PAINT_MENU_SAVE) {
                if (state.path[0] == '\0')
                    paint_open_file_dialog(&file_dialog, &state,
                                           KUI_FILE_DIALOG_SAVE);
                else if (paint_save_to(&state, state.path) == 0)
                    saved = 1;
            } else if (menu_action == PAINT_MENU_SAVE_AS) {
                paint_open_file_dialog(&file_dialog, &state,
                                       KUI_FILE_DIALOG_SAVE);
            } else if (menu_action == PAINT_MENU_CLEAR) {
                state.confirmation = PAINT_CONFIRM_CLEAR;
                kui_confirm_dialog_open(&confirm_dialog, "Clear Image",
                                        "Erase everything on the canvas?",
                                        "Clear", "Cancel");
            }
        } else if (menu_consumed) {
            state.drawing = 0;
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   (event.modifiers & KUI_MOD_CTRL)) {
            if ((event.modifiers & KUI_MOD_SHIFT) &&
                !event.key_extended && event.key_code == 31) {
                kui_menu_close(&menu_state);
                paint_open_file_dialog(&file_dialog, &state,
                                       KUI_FILE_DIALOG_SAVE);
            } else if (!event.key_extended && event.key_code == 31) {
                if (state.path[0] == '\0')
                    paint_open_file_dialog(&file_dialog, &state,
                                           KUI_FILE_DIALOG_SAVE);
                else if (paint_save_to(&state, state.path) == 0)
                    saved = 1;
            } else if (!event.key_extended && event.key_code == 24) {
                kui_menu_close(&menu_state);
                paint_open_file_dialog(&file_dialog, &state,
                                       KUI_FILE_DIALOG_OPEN);
            }
        } else if (event.type == KUI_EVENT_POINTER) {
            int canvas_x;
            int canvas_y;

            if (event.button == 1 &&
                kui_in_rect(event.x, event.y, 8,
                            PAINT_TOOLBAR_TOP + 10, 72, 31)) {
                state.tool = PAINT_TOOL_PENCIL;
                state.drawing = 0;
            } else if (event.button == 1 &&
                       kui_in_rect(event.x, event.y, 88,
                                   PAINT_TOOLBAR_TOP + 10, 72, 31)) {
                state.tool = PAINT_TOOL_ERASER;
                state.drawing = 0;
            } else if (event.button == 1 && event.x >= 176 &&
                       event.x < 176 + 8 * 24 &&
                       event.y >= PAINT_TOOLBAR_TOP + 8 &&
                       event.y < PAINT_TOOLBAR_TOP + 8 + 2 * 25) {
                int column = (event.x - 176) / 24;
                int row = (event.y - PAINT_TOOLBAR_TOP - 8) / 25;
                int color = row * 8 + column;

                if (color >= 0 && color < 16) {
                    state.color = color;
                    state.tool = PAINT_TOOL_PENCIL;
                }
                state.drawing = 0;
            } else if (event.button == 1 &&
                       event.x >= PAINT_SIZE_BUTTON_X &&
                       event.x < PAINT_SIZE_BUTTON_X +
                           (int)(sizeof(paint_brush_sizes) /
                                 sizeof(paint_brush_sizes[0])) *
                           PAINT_SIZE_BUTTON_STEP &&
                       event.y >= PAINT_SIZE_BUTTON_Y &&
                       event.y < PAINT_SIZE_BUTTON_Y + 18) {
                int index = (event.x - PAINT_SIZE_BUTTON_X) /
                            PAINT_SIZE_BUTTON_STEP;

                if ((event.x - PAINT_SIZE_BUTTON_X) %
                        PAINT_SIZE_BUTTON_STEP < PAINT_SIZE_BUTTON_WIDTH &&
                    index >= 0 && index <
                        (int)(sizeof(paint_brush_sizes) /
                              sizeof(paint_brush_sizes[0])))
                    state.brush_size = paint_brush_sizes[index];
                state.drawing = 0;
            } else if ((event.buttons & 1) &&
                       paint_canvas_point(&layout, event.x, event.y,
                                          &canvas_x, &canvas_y)) {
                int radius = paint_brush_radius(state.brush_size);

                if (event.button == 1 || !state.drawing) {
                    paint_set_damage(&damage,
                                     canvas_x - radius, canvas_y - radius,
                                     canvas_x + radius, canvas_y + radius);
                    paint_brush(&state, canvas_x, canvas_y);
                } else {
                    int x0 = state.last_x < canvas_x ?
                             state.last_x : canvas_x;
                    int y0 = state.last_y < canvas_y ?
                             state.last_y : canvas_y;
                    int x1 = state.last_x > canvas_x ?
                             state.last_x : canvas_x;
                    int y1 = state.last_y > canvas_y ?
                             state.last_y : canvas_y;

                    paint_set_damage(&damage, x0 - radius, y0 - radius,
                                     x1 + radius, y1 + radius);
                    paint_line(&state, state.last_x, state.last_y,
                               canvas_x, canvas_y);
                }
                state.last_x = canvas_x;
                state.last_y = canvas_y;
                state.drawing = 1;
                partial_display = damage.valid;
            } else {
                state.drawing = 0;
                no_display = 1;
            }
        }
        snprintf(title, sizeof(title), "%s%s - Paint",
                 state.path[0] != '\0' ? paint_base_name(state.path) :
                 "Untitled", state.dirty ? " *" : "");
        kui_client_begin(&client, &event, title);
        kui_client_set_constraints(&client, 440, 340, 800, 600);
        if (partial_display) {
            paint_render_damage(&client, &event, &state, &damage);
        } else if (no_display) {
            kui_client_set_no_display(&client);
        } else {
            paint_render(&client, &event, &state);
            kui_menu_render(&client.surface, &menu_state, paint_menus, 2);
            kui_file_dialog_render(&client.surface, &event, &file_dialog);
            kui_confirm_dialog_render(&client.surface, &event,
                                      &confirm_dialog);
        }
        kui_client_set_timer(&client, kui_file_dialog_timer_ms(&file_dialog));
        if (saved)
            kui_client_set_action(&client, KUI_ACTION_FILE_SAVED, state.path);
        if (kui_client_send(&client) < 0) break;
    }
    kui_client_free(&client);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && (strcmp(argv[1], "--embedded") == 0 ||
                     strcmp(argv[1], "--kui-client") == 0))
        return paint_client_main();
    puts("Paint is a desktop application. Run 'desktop' first.");
    return 0;
}
