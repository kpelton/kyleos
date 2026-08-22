#include <stdio.h>
#include <string.h>

#include "extras/desktop/desktop_config.h"
#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"

struct control_center_state {
    struct desktop_config config;
    int aqua;
    int apply_requested;
    char status[64];
};

static void copy_text(char *destination, size_t capacity, const char *source)
{
    size_t length;

    if (capacity == 0) return;
    length = strlen(source);
    if (length >= capacity) length = capacity - 1;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

static char *current_wallpaper(struct control_center_state *state)
{
    return state->aqua ? state->config.aqua_wallpaper :
                         state->config.win95_wallpaper;
}

static void choose_wallpaper(struct control_center_state *state,
                             const char *path)
{
    copy_text(current_wallpaper(state), DESKTOP_CONFIG_PATH_MAX, path);
    copy_text(state->status, sizeof(state->status),
              "Press Apply to save changes");
}

static const char *speed_name(int speed)
{
    static const char *names[] = { "", "Slow", "Normal", "Fast", "Fastest" };

    return speed >= 1 && speed <= 4 ? names[speed] : "Normal";
}

static void draw_choice(struct kui_surface *surface, int x, int y,
                        int width, const char *label, int selected)
{
    kui_draw_button(surface, x, y, width, 42, label, selected);
    if (selected)
        kui_fill_rect(surface, x + 7, y + 31, width - 14, 3,
                      KUI_TITLE_BLUE);
}

static void draw(struct kui_surface *surface,
                 const struct control_center_state *state,
                 int width, int height)
{
    int content_width = width - 36;
    int wallpaper_width = (content_width - 46) / 2;
    int bottom = height - 42;
    const char *wallpaper = state->aqua ? state->config.aqua_wallpaper :
                                         state->config.win95_wallpaper;
    char icon_label[40];
    char speed[48];

    kui_fill_rect(surface, 0, 0, width, height, KUI_FACE);

    kui_draw_bevel(surface, 18, 16, content_width, 178, KUI_FACE, 1);
    kui_draw_text(surface, 32, 27, "Appearance", KUI_BLACK, KUI_FACE, 0);
    kui_draw_text(surface, 32, 53,
                  state->aqua ? "Aqua wallpaper" : "KyleOS 95 wallpaper",
                  KUI_SHADOW, KUI_FACE, 0);
    draw_choice(surface, 32, 76, wallpaper_width, "Aqua Blue",
                strcmp(wallpaper, DESKTOP_WALLPAPER_AQUA) == 0);
    draw_choice(surface, 38 + wallpaper_width, 76, wallpaper_width,
                "KyleOS Hills",
                strcmp(wallpaper, DESKTOP_WALLPAPER_WIN95) == 0);
    snprintf(icon_label, sizeof(icon_label), "%s Show desktop icons",
             state->config.show_desktop_icons ? "[x]" : "[ ]");
    kui_draw_button(surface, 32, 137, 210, 32, icon_label, 0);

    kui_draw_bevel(surface, 18, 206, content_width, 108, KUI_FACE, 1);
    kui_draw_text(surface, 32, 218, "Mouse", KUI_BLACK, KUI_FACE, 0);
    kui_draw_text(surface, 32, 252, "Pointer speed", KUI_SHADOW,
                  KUI_FACE, 0);
    kui_draw_button(surface, 150, 242, 44, 32, "-", 0);
    snprintf(speed, sizeof(speed), "%s (%d/4)",
             speed_name(state->config.pointer_speed),
             state->config.pointer_speed);
    kui_draw_button(surface, 200, 242, 150, 32, speed, 0);
    kui_draw_button(surface, 356, 242, 44, 32, "+", 0);

    kui_draw_text(surface, 20, bottom + 9, state->status,
                  KUI_SHADOW, KUI_FACE, 0);
    kui_draw_button(surface, width - 196, bottom, 82, 30, "Revert", 0);
    kui_draw_button(surface, width - 104, bottom, 86, 30, "Apply", 0);
}

static void handle_click(struct control_center_state *state,
                         int width, int height, int x, int y)
{
    int content_width = width - 36;
    int wallpaper_width = (content_width - 46) / 2;
    int bottom = height - 42;

    if (kui_in_rect(x, y, 32, 76, wallpaper_width, 42)) {
        choose_wallpaper(state, DESKTOP_WALLPAPER_AQUA);
    } else if (kui_in_rect(x, y, 38 + wallpaper_width, 76,
                           wallpaper_width, 42)) {
        choose_wallpaper(state, DESKTOP_WALLPAPER_WIN95);
    } else if (kui_in_rect(x, y, 32, 137, 210, 32)) {
        state->config.show_desktop_icons =
            !state->config.show_desktop_icons;
        copy_text(state->status, sizeof(state->status),
                  "Press Apply to save changes");
    } else if (kui_in_rect(x, y, 150, 242, 44, 32)) {
        if (state->config.pointer_speed > 1)
            state->config.pointer_speed--;
        copy_text(state->status, sizeof(state->status),
                  "Press Apply to save changes");
    } else if (kui_in_rect(x, y, 356, 242, 44, 32)) {
        if (state->config.pointer_speed < 4)
            state->config.pointer_speed++;
        copy_text(state->status, sizeof(state->status),
                  "Press Apply to save changes");
    } else if (kui_in_rect(x, y, width - 196, bottom, 82, 30)) {
        desktop_config_load(&state->config, DESKTOP_CONFIG_PATH);
        copy_text(state->status, sizeof(state->status), "Changes reverted");
    } else if (kui_in_rect(x, y, width - 104, bottom, 86, 30)) {
        if (desktop_config_save(&state->config, DESKTOP_CONFIG_PATH) == 0) {
            state->apply_requested = 1;
            copy_text(state->status, sizeof(state->status), "Settings applied");
        } else {
            copy_text(state->status, sizeof(state->status),
                      "Could not save /etc/desktop.cfg");
        }
    }
}

static int embedded_main(void)
{
    struct control_center_state state;
    struct kui_client client;
    struct kui_event event;

    memset(&state, 0, sizeof(state));
    desktop_config_defaults(&state.config);
    kui_client_init(&client);
    for (;;) {
        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        state.aqua = (event.style.flags & KUI_STYLE_AQUA) != 0;
        if (event.type == KUI_EVENT_INIT) {
            if (desktop_config_load(&state.config, DESKTOP_CONFIG_PATH) == 0)
                copy_text(state.status, sizeof(state.status), "Ready");
            else
                copy_text(state.status, sizeof(state.status),
                          "Using built-in defaults");
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1) {
            handle_click(&state, event.width, event.height, event.x, event.y);
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed) {
            if (event.character == '+' && state.config.pointer_speed < 4)
                state.config.pointer_speed++;
            else if (event.character == '-' && state.config.pointer_speed > 1)
                state.config.pointer_speed--;
            else if (event.character == '\n' || event.character == '\r') {
                if (desktop_config_save(&state.config,
                                        DESKTOP_CONFIG_PATH) == 0) {
                    state.apply_requested = 1;
                    copy_text(state.status, sizeof(state.status),
                              "Settings applied");
                }
            }
        }
        kui_client_begin(&client, &event, "Control Center");
        kui_client_set_constraints(&client, 520, 380, 620, 430);
        draw(&client.surface, &state, event.width, event.height);
        if (state.apply_requested) {
            kui_client_set_action(&client, KUI_ACTION_SETTINGS_CHANGED,
                                  DESKTOP_CONFIG_PATH);
            state.apply_requested = 0;
        }
        if (kui_client_send(&client) < 0) break;
    }
    kui_client_free(&client);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && (strcmp(argv[1], "--embedded") == 0 ||
                     strcmp(argv[1], "--kui-client") == 0))
        return embedded_main();
    puts("Control Center is a desktop application. Run 'desktop' first.");
    return 0;
}
