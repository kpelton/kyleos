#include "extras/desktop/shell.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "extras/kui/kui.h"
#include "extras/kui/kui_protocol.h"
#include "extras/kuid/shell_protocol.h"

#define MAX_APPS 24
#define MAX_WINDOWS 10
#define MANIFEST_PATH_MAX 160
#define AQUA_DOCK_CONFIG "/etc/aqua-dock.conf"
#define TASKBAR_HEIGHT (kui_style_current()->panel_bottom)
#define WINDOW_BORDER (kui_style_current()->window_border)
#define WINDOW_TITLE_HEIGHT (kui_style_current()->title_height)
#define WINDOW_BOTTOM (kui_style_current()->window_bottom)
#define MIN_OUTER_WIDTH 268
#define MIN_OUTER_HEIGHT 178

struct kyleos_dirent {
    uint64_t inode;
    char name[257];
    int type;
    uint64_t size;
};

struct shell_app {
    char id[32];
    char name[64];
    char executable[128];
    char icon_path[128];
    char argument[KUI_PROTOCOL_TEXT_MAX];
    char accepts[32];
    int desktop;
    int start_menu;
    int pinned;
    int autostart;
    int exclusive;
    int width;
    int height;
    int position_x;
    int position_y;
    struct kui_image icon;
};

struct shell_window {
    int visible;
    int minimized;
    int taskbar_order;
    int x;
    int y;
    int width;
    int height;
    int restore_x;
    int restore_y;
    int restore_width;
    int restore_height;
    int snapped;
    int minimum_width;
    int minimum_height;
    char title[KUI_PROTOCOL_TITLE_MAX];
    char application[32];
    char icon_path[128];
    int pid;
    int input_fd;
    int output_fd;
    uint32_t serial;
    uint32_t timer_ms;
    uint32_t next_timer;
    uint32_t *content_pixels;
    unsigned char *display_commands;
    uint32_t display_capacity;
    int content_width;
    int content_height;
    struct kui_surface content;
};

struct shell_action {
    uint32_t type;
    char argument[KUI_PROTOCOL_CLIPBOARD_MAX];
};

static struct shell_app apps[MAX_APPS];
static int app_count;
static struct shell_window windows[MAX_WINDOWS];
static int retired_window_pids[MAX_WINDOWS];
static int next_taskbar_order;
static int next_window_position;
static int screen_width;
static int screen_height;
static uint32_t *frame_pixels;
static uint32_t frame_bytes;
static struct kui_surface frame;
static struct kui_image wallpaper;
static int cursor_x;
static int cursor_y;
static uint8_t mouse_buttons;
static int running = 1;
static int start_menu_open;
static int selected_icon = -1;
static int drag_window = -1;
static int drag_dx;
static int drag_dy;
static int resize_window = -1;
static int resize_right_gap;
static int resize_bottom_gap;
static int pointer_window = -1;
static char clipboard_data[KUI_PROTOCOL_CLIPBOARD_MAX];
static uint32_t clipboard_kind;
static int shell_pid = -1;
static int shell_input = -1;
static int shell_output = -1;
static char shell_name[32];
static uint32_t shell_serial;
/* Shell frames are bounded by the protocol. Reuse storage while dragging so
 * redraws never churn or extend the userspace heap. */
static unsigned char shell_display[KUI_PROTOCOL_MAX_DISPLAY];
static uint32_t shell_display_bytes;
static struct kui_shell_request shell_cached_request;
static int shell_cache_valid;
static void copy_text(char *destination, size_t capacity, const char *source);
static int shell_log_fd = -1;

/* stdout is the compositor protocol, so diagnostics go to a file. */
static void shell_log(const char *format, ...)
{
    char buffer[512];
    va_list arguments;
    int length;

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if (length <= 0) return;
    if (length >= (int)sizeof(buffer)) length = sizeof(buffer) - 1;
    if (shell_log_fd < 0)
        shell_log_fd = open("/SHELL.LOG", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (shell_log_fd >= 0)
        write(shell_log_fd, buffer, (size_t)length);
}

static int start_shell(const char *path)
{
    struct kui_shell_profile profile;
    int to_shell[2];
    int from_shell[2];
    int child;

    if (path == NULL || pipe(to_shell) < 0)
        return -1;
    if (pipe(from_shell) < 0) {
        close(to_shell[0]);
        close(to_shell[1]);
        return -1;
    }
    child = fork();
    if (child == 0) {
        char *argv[] = { (char *)path, NULL };

        close(to_shell[1]);
        close(from_shell[0]);
        dup2(to_shell[0], 0);
        dup2(from_shell[1], 1);
        close(to_shell[0]);
        close(from_shell[1]);
        execve(path, argv, NULL);
        _exit(127);
    }
    close(to_shell[0]);
    close(from_shell[1]);
    if (child < 0) {
        close(to_shell[1]);
        close(from_shell[0]);
        return -1;
    }
    if (kui_read_all_timeout(from_shell[0], &profile, sizeof(profile),
                             1000) < 0 ||
        profile.magic != KUI_SHELL_MAGIC ||
        profile.version != KUI_SHELL_VERSION ||
        profile.style.window_border < 1 || profile.style.window_border > 16 ||
        profile.style.title_height < 18 || profile.style.title_height > 64 ||
        profile.style.panel_top < 0 || profile.style.panel_top > 128 ||
        profile.style.panel_bottom < 0 || profile.style.panel_bottom > 128) {
        close(to_shell[1]);
        close(from_shell[0]);
        waitpid(child, NULL, 0);
        return -1;
    }
    profile.name[sizeof(profile.name) - 1] = '\0';
    shell_pid = child;
    shell_input = to_shell[1];
    shell_output = from_shell[0];
    copy_text(shell_name, sizeof(shell_name), profile.name);
    kui_style_apply(&profile.style);
    shell_cache_valid = 0;
    return 0;
}

static void stop_shell(void)
{
    int status;

    if (shell_input >= 0) close(shell_input);
    if (shell_output >= 0) close(shell_output);
    if (shell_pid > 0) waitpid(shell_pid, &status, 0);
    shell_input = -1;
    shell_output = -1;
    shell_pid = -1;
    shell_cache_valid = 0;
}

static int aqua_shell(void)
{
    return (kui_style_current()->flags & KUI_STYLE_AQUA) != 0;
}

static int work_top(void)
{
    return kui_style_current()->panel_top;
}

static int work_bottom(void)
{
    return screen_height - kui_style_current()->panel_bottom;
}

static int work_height(void)
{
    return work_bottom() - work_top();
}

static int get_directory_entries(int fd, struct kyleos_dirent *entries,
                                 int count)
{
    register long rax asm("rax") = 16;
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"((long)fd), "S"(entries), "d"((long)count)
                 : "memory");
    return (int)rax;
}

static char *trim(char *text)
{
    char *end;

    while (*text == ' ' || *text == '\t' || *text == '\r') text++;
    end = text + strlen(text);
    while (end > text &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
        *--end = '\0';
    return text;
}

static void copy_text(char *destination, size_t capacity, const char *source)
{
    size_t length;
    if (capacity == 0) return;
    length = strlen(source);
    if (length >= capacity) length = capacity - 1;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

static int text_boolean(const char *value)
{
    return strcmp(value, "true") == 0 || strcmp(value, "yes") == 0 ||
           strcmp(value, "1") == 0;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static int load_manifest(const char *path, struct shell_app *app)
{
    char data[2048];
    char *line;
    int fd;
    int count;

    memset(app, 0, sizeof(*app));
    app->width = 700;
    app->height = 481;
    app->position_x = -1;
    app->position_y = -1;
    fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    count = read(fd, data, sizeof(data) - 1);
    close(fd);
    if (count <= 0) return -1;
    data[count] = '\0';
    line = strtok(data, "\n");
    while (line != NULL) {
        char *equals;
        char *key = trim(line);
        char *value;

        if (*key == '\0' || *key == '#') { line = strtok(NULL, "\n"); continue; }
        equals = strchr(key, '=');
        if (equals == NULL) { line = strtok(NULL, "\n"); continue; }
        *equals = '\0';
        value = trim(equals + 1);
        key = trim(key);
        if (strcmp(key, "id") == 0)
            copy_text(app->id, sizeof(app->id), value);
        else if (strcmp(key, "name") == 0)
            copy_text(app->name, sizeof(app->name), value);
        else if (strcmp(key, "exec") == 0)
            copy_text(app->executable, sizeof(app->executable), value);
        else if (strcmp(key, "icon") == 0)
            copy_text(app->icon_path, sizeof(app->icon_path), value);
        else if (strcmp(key, "argument") == 0)
            copy_text(app->argument, sizeof(app->argument), value);
        else if (strcmp(key, "accepts") == 0)
            copy_text(app->accepts, sizeof(app->accepts), value);
        else if (strcmp(key, "desktop") == 0)
            app->desktop = text_boolean(value);
        else if (strcmp(key, "start_menu") == 0)
            app->start_menu = text_boolean(value);
        else if (strcmp(key, "autostart") == 0)
            app->autostart = text_boolean(value);
        else if (strcmp(key, "exclusive") == 0)
            app->exclusive = text_boolean(value);
        else if (strcmp(key, "width") == 0)
            app->width = atoi(value);
        else if (strcmp(key, "height") == 0)
            app->height = atoi(value);
        else if (strcmp(key, "x") == 0)
            app->position_x = atoi(value);
        else if (strcmp(key, "y") == 0)
            app->position_y = atoi(value);
        line = strtok(NULL, "\n");
    }
    if (app->executable[0] == '\0') return -1;
    if (app->name[0] == '\0')
        copy_text(app->name, sizeof(app->name), base_name(app->executable));
    if (app->id[0] == '\0')
        copy_text(app->id, sizeof(app->id), app->name);
    if (app->width < 240) app->width = 240;
    if (app->height < 140) app->height = 140;
    if (app->icon_path[0] != '\0')
        kui_image_load(app->icon_path, &app->icon);
    return 0;
}

static int has_suffix(const char *text, const char *suffix)
{
    size_t text_length = strlen(text);
    size_t suffix_length = strlen(suffix);
    return text_length >= suffix_length &&
           strcmp(text + text_length - suffix_length, suffix) == 0;
}

static void load_applications(void)
{
    struct kyleos_dirent entries[64];
    int fd = open("/usr/share/applications", O_RDONLY);
    int count;

    if (fd < 0) return;
    count = get_directory_entries(fd, entries,
                                  (int)(sizeof(entries) / sizeof(entries[0])));
    close(fd);
    if (count < 0) return;
    for (int i = 0; i < count && app_count < MAX_APPS; i++) {
        static const char prefix[] = "/usr/share/applications/";
        char path[MANIFEST_PATH_MAX];
        size_t name_length;

        if (!has_suffix(entries[i].name, ".app")) continue;
        name_length = strlen(entries[i].name);
        if (sizeof(prefix) - 1 + name_length >= sizeof(path)) continue;
        memcpy(path, prefix, sizeof(prefix) - 1);
        memcpy(path + sizeof(prefix) - 1, entries[i].name, name_length + 1);
        if (load_manifest(path, &apps[app_count]) == 0) app_count++;
    }
}

static int dock_app_by_id(const char *id)
{
    for (int i = 0; i < app_count; i++)
        if (strcmp(apps[i].id, id) == 0) return i;
    return -1;
}

static void load_dock_config(void)
{
    FILE *file = fopen(AQUA_DOCK_CONFIG, "r");
    char line[96];
    for (int i = 0; i < app_count; i++) apps[i].pinned = apps[i].start_menu;
    if (file == NULL) return;
    for (int i = 0; i < app_count; i++) apps[i].pinned = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        char id[64];
        if (sscanf(line, "pin=%63s", id) == 1) {
            int app = dock_app_by_id(id);
            if (app >= 0) apps[app].pinned = 1;
        }
    }
    fclose(file);
}

static void save_dock_config(void)
{
    FILE *file = fopen(AQUA_DOCK_CONFIG, "w");
    if (file == NULL) return;
    for (int i = 0; i < app_count; i++)
        if (apps[i].pinned) fprintf(file, "pin=%s\n", apps[i].id);
    fclose(file);
}

static int content_width(const struct shell_window *window)
{
    return window->width - WINDOW_BORDER * 2;
}

static int content_height(const struct shell_window *window)
{
    return window->height - WINDOW_TITLE_HEIGHT - WINDOW_BOTTOM;
}

static int content_x(const struct shell_window *window)
{
    return window->x + WINDOW_BORDER;
}

static int content_y(const struct shell_window *window)
{
    return window->y + WINDOW_TITLE_HEIGHT;
}

static int ensure_content(struct shell_window *window, int width, int height)
{
    uint32_t *pixels;
    size_t count;

    if (width <= 0 || height <= 0 || width > 4096 || height > 2160)
        return -1;
    if (window->content_pixels != NULL && window->content_width == width &&
        window->content_height == height)
        return 0;
    count = (size_t)width * height;
    if (count > (size_t)4096 * 2160) return -1;
    pixels = realloc(window->content_pixels, count * sizeof(uint32_t));
    if (pixels == NULL) return -1;
    window->content_pixels = pixels;
    window->content_width = width;
    window->content_height = height;
    kui_surface_init(&window->content, pixels, width, height);
    return 0;
}

static int start_process(struct shell_window *window, const char *path)
{
    int to_child[2];
    int from_child[2];
    int child;

    shell_log("START request path=%s title=%s\n", path, window->title);
    if (pipe(to_child) < 0) return -1;
    if (pipe(from_child) < 0) {
        close(to_child[0]); close(to_child[1]); return -1;
    }
    child = fork();
    if (child == 0) {
        char *argv[] = { (char *)base_name(path), "--kui-client", NULL };

        close(to_child[1]);
        close(from_child[0]);
        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        close(to_child[0]);
        close(from_child[1]);
        execve(path, argv, NULL);
        _exit(127);
    }
    close(to_child[0]);
    close(from_child[1]);
    if (child < 0) {
        close(to_child[1]); close(from_child[0]); return -1;
    }
    window->pid = child;
    window->input_fd = to_child[1];
    window->output_fd = from_child[0];
    shell_log("START pid=%d path=%s\n", child, path);
    return 0;
}

static int send_window_event(int index, uint32_t type, int x, int y,
                             int button, int buttons, int wheel,
                             int key_code, int key_extended, int key_pressed,
                             uint32_t modifiers, char character,
                             const char *argument,
                             struct shell_action *action)
{
    struct shell_window *window = &windows[index];
    struct kui_event event;
    struct kui_response response;
    unsigned char *display = NULL;
    int width = content_width(window);
    int height = content_height(window);

    if (!window->visible || window->pid <= 0) return -1;
    memset(&event, 0, sizeof(event));
    event.magic = KUI_PROTOCOL_MAGIC;
    event.version = KUI_PROTOCOL_VERSION;
    event.type = type;
    event.serial = ++window->serial;
    event.width = width;
    event.height = height;
    event.x = x;
    event.y = y;
    event.button = button;
    event.buttons = buttons;
    event.wheel = wheel;
    event.key_code = key_code;
    event.key_extended = key_extended;
    event.key_pressed = key_pressed;
    event.modifiers = modifiers;
    event.character = character;
    event.seed = kui_ticks_ms() ^ ((uint32_t)x << 16) ^ (uint32_t)y;
    event.style = *kui_style_current();
    if (argument != NULL)
        copy_text(event.argument, sizeof(event.argument), argument);
    event.clipboard_kind = clipboard_kind;
    copy_text(event.clipboard, sizeof(event.clipboard), clipboard_data);
    if (kui_write_all(window->input_fd, &event, sizeof(event)) < 0) {
        shell_log("PROTO write-failed pid=%d event=%u serial=%u\n",
                  window->pid, type, event.serial);
        return -1;
    }
    if (type == KUI_EVENT_CLOSE) return 0;
    if (kui_read_all_timeout(window->output_fd, &response,
                             sizeof(response), 500) < 0 ||
        response.magic != KUI_PROTOCOL_MAGIC ||
        response.version != KUI_PROTOCOL_VERSION ||
        response.serial != event.serial ||
        response.display_bytes > KUI_PROTOCOL_MAX_DISPLAY) {
        shell_log("PROTO read-failed pid=%d event=%u serial=%u\n",
                  window->pid, type, event.serial);
        return -1;
    }
    if (response.display_bytes != 0) {
        if (response.display_bytes > window->display_capacity) {
            display = realloc(window->display_commands,
                              response.display_bytes);
            if (display == NULL) return -1;
            window->display_commands = display;
            window->display_capacity = response.display_bytes;
        }
        display = window->display_commands;
        if (kui_read_all_timeout(window->output_fd, display,
                                 response.display_bytes, 500) < 0) {
            return -1;
        }
    }
    if (response.flags & KUI_RESPONSE_TITLE) {
        response.title[sizeof(response.title) - 1] = '\0';
        copy_text(window->title, sizeof(window->title), response.title);
    }
    if (response.flags & KUI_RESPONSE_CONSTRAINTS) {
        window->minimum_width = response.min_width + WINDOW_BORDER * 2;
        window->minimum_height = response.min_height +
                                 WINDOW_TITLE_HEIGHT + WINDOW_BOTTOM;
        if (window->minimum_width < MIN_OUTER_WIDTH)
            window->minimum_width = MIN_OUTER_WIDTH;
        if (window->minimum_height < MIN_OUTER_HEIGHT)
            window->minimum_height = MIN_OUTER_HEIGHT;
    }
    if (response.flags & KUI_RESPONSE_TIMER) {
        window->timer_ms = response.timer_ms;
        window->next_timer = kui_ticks_ms() + response.timer_ms;
    }
    if (response.flags & KUI_RESPONSE_DISPLAY) {
        int partial = (response.flags & KUI_RESPONSE_PARTIAL) != 0;
        int content_ready = window->content_pixels != NULL &&
                            window->content_width == width &&
                            window->content_height == height;

        if (partial && (!content_ready || response.damage_x < 0 ||
            response.damage_y < 0 || response.damage_width <= 0 ||
            response.damage_height <= 0 ||
            response.damage_x > width - response.damage_width ||
            response.damage_y > height - response.damage_height)) {
            return -1;
        }
        if (ensure_content(window, width, height) < 0) {
            return -1;
        }
        if (partial)
            kui_fill_rect(&window->content, response.damage_x,
                          response.damage_y, response.damage_width,
                          response.damage_height, KUI_WHITE);
        else
            kui_fill_rect(&window->content, 0, 0, width, height, KUI_WHITE);
        if (kui_display_replay(&window->content, display,
                               response.display_bytes) < 0) {
            return -1;
        }
    }
    if (action != NULL && response.action != KUI_ACTION_NONE) {
        action->type = response.action;
        response.action_argument[sizeof(response.action_argument) - 1] = '\0';
        copy_text(action->argument, sizeof(action->argument),
                  response.action_argument);
    }
    return response.status < 0 ? -1 : 0;
}

static int top_window(void)
{
    for (int i = MAX_WINDOWS - 1; i >= 0; i--)
        if (windows[i].visible && !windows[i].minimized) return i;
    return -1;
}

static int raise_window(int index)
{
    struct shell_window selected = windows[index];
    for (int i = index; i < MAX_WINDOWS - 1; i++) windows[i] = windows[i + 1];
    windows[MAX_WINDOWS - 1] = selected;
    return MAX_WINDOWS - 1;
}

static void reap_retired_windows(void)
{
    for (int i = 0; i < MAX_WINDOWS; i++) {
        int status;

        if (retired_window_pids[i] > 0 &&
            waitpid(retired_window_pids[i], &status, WNOHANG) != 0) {
            shell_log("EXIT pid=%d status=%d\n", retired_window_pids[i],
                      status);
            retired_window_pids[i] = 0;
        }
    }
}

static void retire_window_process(int pid)
{
    int status;

    if (pid <= 0 || waitpid(pid, &status, WNOHANG) != 0) return;
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (retired_window_pids[i] == 0) {
            retired_window_pids[i] = pid;
            return;
        }
}

static void position_window(struct shell_window *window)
{
    int max_x = screen_width - window->width - 10;
    int max_y = work_bottom() - window->height - 10;
    int position = next_window_position++;

    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    window->x = 30 + (position % 12) * 26;
    window->y = work_top() + 24 + (position % 10) * 22;
    if (window->x > max_x) window->x = max_x;
    if (window->y > max_y) window->y = max_y;
}

static void close_window_index(int index)
{
    struct shell_window *window;

    if (index < 0 || index >= MAX_WINDOWS || !windows[index].visible) return;
    if (pointer_window == index) pointer_window = -1;
    window = &windows[index];
    shell_log("CLOSE request pid=%d app=%s title=%s\n", window->pid,
              window->application, window->title);
    send_window_event(index, KUI_EVENT_CLOSE, 0, 0, 0, 0, 0,
                      0, 0, 0, 0, 0, NULL, NULL);
    close(window->input_fd);
    close(window->output_fd);
    retire_window_process(window->pid);
    free(window->content_pixels);
    free(window->display_commands);
    memset(window, 0, sizeof(*window));
}

static int run_exclusive(const char *path)
{
    char *argv[] = { (char *)path, NULL };
    int child;
    int status;
    int pressed;
    int dx;
    int dy;
    int wheel;
    uint8_t code;
    uint8_t extended;
    uint8_t buttons;

    kui_tty_raw(0);
    memset(frame_pixels, 0, frame_bytes);
    kui_present(&frame);
    child = fork();
    if (child == 0) { execve(path, argv, NULL); _exit(127); }
    if (child > 0) waitpid(child, &status, 0);
    kui_tty_raw(1);
    while (kui_key_poll(&pressed, &code, &extended) > 0) { }
    while (kui_mouse_poll_wheel(&dx, &dy, &buttons, &wheel) > 0) { }
    mouse_buttons = 0;
    return child < 0 ? -1 : 0;
}

static const char *application_name(const struct shell_app *app)
{
    if (strcmp(app->executable, "/bin/finder") == 0) return "Finder";
    if (strcmp(app->executable, "/bin/editor") == 0) return "TextEdit";
    if (strcmp(app->executable, "/bin/cide") == 0) return "C IDE";
    if (strcmp(app->executable, "/bin/kterm") == 0) return "Terminal";
    return app->name;
}

static int open_application(const struct shell_app *app, const char *argument)
{
    int slot = -1;
    struct shell_action action;

    if (app->exclusive) return run_exclusive(app->executable);
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (!windows[i].visible) { slot = i; break; }
    if (slot < 0) return -1;
    memset(&windows[slot], 0, sizeof(windows[slot]));
    windows[slot].visible = 1;
    windows[slot].taskbar_order = next_taskbar_order++;
    windows[slot].pid = -1;
    windows[slot].input_fd = -1;
    windows[slot].output_fd = -1;
    windows[slot].width = app->width + WINDOW_BORDER * 2;
    windows[slot].height = app->height + WINDOW_TITLE_HEIGHT + WINDOW_BOTTOM;
    if (windows[slot].width > screen_width)
        windows[slot].width = screen_width;
    if (windows[slot].height > work_height())
        windows[slot].height = work_height();
    windows[slot].minimum_width = MIN_OUTER_WIDTH;
    windows[slot].minimum_height = MIN_OUTER_HEIGHT;
    copy_text(windows[slot].title, sizeof(windows[slot].title), app->name);
    copy_text(windows[slot].application,
              sizeof(windows[slot].application), application_name(app));
    copy_text(windows[slot].icon_path, sizeof(windows[slot].icon_path),
              app->icon_path);
    position_window(&windows[slot]);
    if (app->position_x >= 0) windows[slot].x = app->position_x;
    if (app->position_y >= 0) windows[slot].y = app->position_y;
    if (windows[slot].x + windows[slot].width > screen_width)
        windows[slot].x = screen_width - windows[slot].width;
    if (windows[slot].y < work_top()) windows[slot].y = work_top();
    if (windows[slot].y + windows[slot].height > work_bottom())
        windows[slot].y = work_bottom() - windows[slot].height;
    if (windows[slot].x < 0) windows[slot].x = 0;
    if (start_process(&windows[slot], app->executable) < 0) {
        shell_log("OPEN failed path=%s title=%s\n", app->executable,
                  app->name);
        memset(&windows[slot], 0, sizeof(windows[slot]));
        return -1;
    }
    memset(&action, 0, sizeof(action));
    if (send_window_event(slot, KUI_EVENT_INIT, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 0,
                          argument != NULL ? argument : app->argument,
                          &action) < 0) {
        close_window_index(slot);
        shell_log("OPEN init-failed path=%s title=%s\n", app->executable,
                  app->name);
        return -1;
    }
    shell_log("OPEN success pid=%d path=%s title=%s\n",
              windows[slot].pid, app->executable, app->name);
    return raise_window(slot);
}

static const struct shell_app *find_app_by_executable(const char *path)
{
    const struct shell_app *best = NULL;
    int best_score = -1;
    const char *name = base_name(path);

    for (int i = 0; i < app_count; i++) {
        int score;
        if (strcmp(apps[i].executable, path) != 0) continue;
        score = strcmp(apps[i].id, name) == 0 ? 8 :
                apps[i].accepts[0] != '\0' ? 4 :
                apps[i].start_menu ? 2 : apps[i].desktop ? 1 : 0;
        if (score > best_score) { best = &apps[i]; best_score = score; }
    }
    return best;
}

static int is_executable(const char *path)
{
    unsigned char magic[4];
    int fd = open(path, O_RDONLY);
    int count;
    if (fd < 0) return 0;
    count = read(fd, magic, sizeof(magic));
    close(fd);
    return count == 4 && magic[0] == 0x7f && magic[1] == 'E' &&
           magic[2] == 'L' && magic[3] == 'F';
}

static int is_text_file(const char *path)
{
    unsigned char sample[512];
    int fd = open(path, O_RDONLY);
    int count;
    if (fd < 0) return 0;
    count = read(fd, sample, sizeof(sample));
    close(fd);
    if (count < 0) return 0;
    for (int i = 0; i < count; i++)
        if (sample[i] == 0 ||
            (sample[i] < 32 && sample[i] != '\n' && sample[i] != '\r' &&
             sample[i] != '\t' && sample[i] != '\f'))
            return 0;
    return 1;
}

static int is_kimg_file(const char *path)
{
    unsigned char header[8];
    int fd = open(path, O_RDONLY);
    int count;

    if (fd < 0) return 0;
    count = read(fd, header, sizeof(header));
    close(fd);
    return count == (int)sizeof(header) &&
           memcmp(header, "KIMG", 4) == 0 && header[4] == 1 &&
           header[5] == 0 && header[6] == 0 && header[7] == 0;
}

static int is_c_source_file(const char *path)
{
    size_t length = strlen(path);
    char extension;

    if (length < 2 || path[length - 2] != '.') return 0;
    extension = path[length - 1];
    return extension == 'c' || extension == 'C' ||
           extension == 'h' || extension == 'H';
}

static int is_csv_file(const char *path)
{
    size_t length = strlen(path);

    if (length < 4 || path[length - 4] != '.') return 0;
    return (path[length - 3] == 'c' || path[length - 3] == 'C') &&
           (path[length - 2] == 's' || path[length - 2] == 'S') &&
           (path[length - 1] == 'v' || path[length - 1] == 'V');
}

static void open_path(const char *path)
{
    const struct shell_app *app;

    if (is_executable(path)) {
        app = find_app_by_executable(path);
        if (app != NULL) open_application(app, NULL);
        else run_exclusive(path);
        return;
    }
    if (is_kimg_file(path)) {
        for (int i = 0; i < app_count; i++)
            if (strcmp(apps[i].accepts, "image") == 0) {
                open_application(&apps[i], path);
                return;
            }
    }
    if (is_text_file(path)) {
        if (is_csv_file(path)) {
            for (int i = 0; i < app_count; i++)
                if (strcmp(apps[i].accepts, "csv") == 0) {
                    open_application(&apps[i], path);
                    return;
                }
        }
        if (is_c_source_file(path)) {
            for (int i = 0; i < app_count; i++)
                if (strcmp(apps[i].accepts, "c") == 0) {
                    open_application(&apps[i], path);
                    return;
                }
        }
        for (int i = 0; i < app_count; i++)
            if (strcmp(apps[i].accepts, "text") == 0) {
                open_application(&apps[i], path);
                return;
            }
    }
}

static void broadcast_refresh(void)
{
    for (int i = 0; i < MAX_WINDOWS; i++) {
        struct shell_action ignored;
        if (!windows[i].visible) continue;
        memset(&ignored, 0, sizeof(ignored));
        if (send_window_event(i, KUI_EVENT_REFRESH, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, NULL, &ignored) < 0)
            close_window_index(i);
    }
}

static void apply_action(const struct shell_action *action)
{
    const struct shell_app *app;
    if (action == NULL || action->type == KUI_ACTION_NONE) return;
    if (action->type == KUI_ACTION_OPEN_PATH) open_path(action->argument);
    else if (action->type == KUI_ACTION_LAUNCH) {
        app = find_app_by_executable(action->argument);
        if (app != NULL) open_application(app, NULL);
        else run_exclusive(action->argument);
    } else if (action->type == KUI_ACTION_FILE_SAVED) {
        broadcast_refresh();
    } else if (action->type == KUI_ACTION_COPY_PATH) {
        copy_text(clipboard_data, sizeof(clipboard_data), action->argument);
        clipboard_kind = KUI_CLIPBOARD_PATH;
    } else if (action->type == KUI_ACTION_COPY_TEXT) {
        copy_text(clipboard_data, sizeof(clipboard_data), action->argument);
        clipboard_kind = KUI_CLIPBOARD_TEXT;
    }
}

static int window_at(int x, int y)
{
    for (int i = MAX_WINDOWS - 1; i >= 0; i--)
        if (windows[i].visible && !windows[i].minimized &&
            kui_in_rect(x, y, windows[i].x, windows[i].y,
                        windows[i].width, windows[i].height))
            return i;
    return -1;
}

static int desktop_app_at(int x, int y)
{
    int icon = 0;
    int rows = (work_height() - 12) / 82;
    if (rows < 1) rows = 1;
    for (int i = 0; i < app_count; i++) {
        int ix;
        int iy;
        if (!apps[i].desktop ||
            (aqua_shell() && strcmp(apps[i].id, "system-disk") != 0))
            continue;
        ix = aqua_shell() ? screen_width - 94 :
                            12 + (icon / rows) * 94;
        iy = work_top() + 8 + (icon % rows) * 82;
        if (kui_in_rect(x, y, ix, iy, 82, 77)) return i;
        icon++;
    }
    return -1;
}

static void blit_content(const struct shell_window *window)
{
    int x = content_x(window);
    int y = content_y(window);
    int width = window->content_width;
    int height = window->content_height;

    if (window->content_pixels == NULL) return;
    if (width > screen_width - x) width = screen_width - x;
    if (height > screen_height - y) height = screen_height - y;
    if (width <= 0 || height <= 0) return;
    for (int row = 0; row < height; row++)
        memcpy(frame.pixels + (y + row) * frame.stride + x,
               window->content_pixels + row * window->content.stride,
               width * sizeof(uint32_t));
}

static void draw_window(const struct shell_window *window, int active)
{
    int x = window->x;
    int y = window->y;
    int width = window->width;
    int height = window->height;

    kui_draw_window(&frame, x, y, width, height, window->title, active);
    if (aqua_shell()) {
        static const uint32_t colors[] = {
            0x00ef5b54U, 0x00f2c14eU, 0x0058c75eU
        };
        static const uint32_t edges[] = {
            0x0099332fU, 0x009b7a24U, 0x002c8730U
        };

        for (int control = 0; control < 3; control++) {
            int cx = x + 8 + control * 19;
            uint32_t face = active ? colors[control] : 0x00c2c5c8U;
            uint32_t edge = active ? edges[control] : 0x00878b8fU;

            kui_fill_rect(&frame, cx + 4, y + 6, 6, 1, edge);
            kui_fill_rect(&frame, cx + 2, y + 7, 10, 1, edge);
            kui_fill_rect(&frame, cx + 1, y + 8, 12, 8, edge);
            kui_fill_rect(&frame, cx + 2, y + 16, 10, 1, edge);
            kui_fill_rect(&frame, cx + 4, y + 17, 6, 1, edge);
            kui_fill_rect(&frame, cx + 4, y + 7, 6, 1, face);
            kui_fill_rect(&frame, cx + 3, y + 8, 8, 8, face);
            kui_fill_rect(&frame, cx + 2, y + 10, 10, 4, face);
            kui_fill_rect_alpha(&frame, cx + 4, y + 8, 5, 3,
                                0x80ffffffU);
        }
    } else {
        kui_draw_button(&frame, x + width - 47, y + 4, 19, 17, "_", 0);
        kui_draw_button(&frame, x + width - 24, y + 4, 19, 17, "X", 0);
    }
    kui_fill_rect(&frame, content_x(window), content_y(window),
                  content_width(window), content_height(window), KUI_WHITE);
    blit_content(window);
    kui_outline_rect(&frame, content_x(window) - 1, content_y(window) - 1,
                     content_width(window) + 2, content_height(window) + 2,
                     KUI_SHADOW);
    for (int offset = 2; offset < 12; offset += 3)
        kui_line(&frame, x + width - 15 + offset, y + height - 2,
                 x + width - 2, y + height - 15 + offset, KUI_BLACK);
}

static int taskbar_window_count(void)
{
    int count = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) if (windows[i].visible) count++;
    return count;
}

static int taskbar_window_by_rank(int rank)
{
    int previous = -1;
    for (int current = 0; current <= rank; current++) {
        int selected = -1;
        int order = 0x7fffffff;
        for (int i = 0; i < MAX_WINDOWS; i++)
            if (windows[i].visible && windows[i].taskbar_order > previous &&
                windows[i].taskbar_order < order) {
                selected = i; order = windows[i].taskbar_order;
            }
        if (selected < 0) return -1;
        if (current == rank) return selected;
        previous = order;
    }
    return -1;
}

static int taskbar_button_width(void)
{
    int count = taskbar_window_count();
    int width = count != 0 ? (screen_width - 220) / count : 0;
    if (width > 180) width = 180;
    return width;
}

static int start_entry_count(void);
static int start_app_by_rank(int rank);
static int dock_entry_count(void);
static int dock_app_by_rank(int rank);

static void __attribute__((unused)) draw_taskbar(void)
{
    if (aqua_shell()) {
        int app_icons = dock_entry_count();
        int window_icons = taskbar_window_count();
        int dock_count;
        int dock_width;
        int dock_x;
        int dock_y = screen_height - 58;
        char clock[16];
        uint32_t minutes = kui_ticks_ms() / 60000U;
        int active = top_window();

        if (app_icons > 12) app_icons = 12;
        dock_count = app_icons + window_icons;
        if (dock_count > (screen_width - 20) / 48)
            dock_count = (screen_width - 20) / 48;
        dock_width = 20 + dock_count * 48;
        dock_x = (screen_width - dock_width) / 2;
        kui_fill_rect(&frame, 0, 0, screen_width, 24,
                      kui_style_current()->panel);
        kui_fill_rect(&frame, 0, 23, screen_width, 1, 0x00787878U);
        kui_draw_text(&frame, 12, 4, "K", KUI_BLACK,
                      kui_style_current()->panel, 0);
        kui_draw_text(&frame, 38, 4,
                      active >= 0 ? windows[active].title : shell_name,
                      KUI_BLACK, kui_style_current()->panel, 0);
        snprintf(clock, sizeof(clock), "%02u:%02u",
                 (unsigned)((minutes / 60U) % 24U),
                 (unsigned)(minutes % 60U));
        kui_draw_text(&frame, screen_width - 68, 4, clock, KUI_BLACK,
                      kui_style_current()->panel, 0);
        kui_fill_rect(&frame, dock_x + 5, dock_y + 5,
                      dock_width, 52, 0x005d7187U);
        kui_fill_rect(&frame, dock_x, dock_y, dock_width, 52, 0x00cbd5dfU);
        kui_outline_rect(&frame, dock_x, dock_y, dock_width, 52,
                         0x006b7f93U);
        for (int rank = 0; rank < app_icons && rank < dock_count; rank++) {
            int app = dock_app_by_rank(rank);
            int x = dock_x + 10 + rank * 48;

            if (app < 0) continue;
            if (apps[app].icon.pixels != NULL)
                kui_blit_image_scaled(&frame, &apps[app].icon,
                                      x + 4, dock_y + 5, 36, 36);
            else {
                kui_fill_rect(&frame, x + 4, dock_y + 5, 36, 36,
                              0x00f4f7fbU);
                kui_outline_rect(&frame, x + 4, dock_y + 5, 36, 36,
                                 0x006b7f93U);
                kui_draw_text(&frame, x + 18, dock_y + 15,
                              apps[app].name, KUI_BLACK, 0x00f4f7fbU, 0);
            }
        }
        for (int rank = 0; rank < window_icons && app_icons + rank < dock_count;
             rank++) {
            int index = taskbar_window_by_rank(rank);
            int x = dock_x + 10 + (app_icons + rank) * 48;
            char label[2] = { '?', '\0' };

            if (index < 0) continue;
            if (windows[index].title[0] != '\0')
                label[0] = windows[index].title[0];
            kui_fill_rect(&frame, x + 4, dock_y + 5, 36, 36,
                          index == active ? 0x008ab7e8U : 0x00eef2f6U);
            kui_outline_rect(&frame, x + 4, dock_y + 5, 36, 36,
                             0x006b7f93U);
            kui_draw_text(&frame, x + 18, dock_y + 15, label, KUI_BLACK,
                          index == active ? 0x008ab7e8U : 0x00eef2f6U, 0);
            if (windows[index].minimized)
                kui_fill_rect(&frame, x + 12, dock_y + 45, 20, 2,
                              0x004f5963U);
        }
        return;
    }
    int y = screen_height - TASKBAR_HEIGHT;
    int count = taskbar_window_count();
    int button_width = taskbar_button_width();
    int x = 102;
    char clock[16];
    uint32_t minutes = kui_ticks_ms() / 60000U;

    kui_fill_rect(&frame, 0, y, screen_width, TASKBAR_HEIGHT, KUI_TITLE_BLUE);
    kui_fill_rect(&frame, 0, y, screen_width, 2, 0x0085bcffU);
    kui_draw_button(&frame, 3, y + 4, 94, 25, "start", start_menu_open);
    for (int rank = 0; rank < count; rank++) {
        int index = taskbar_window_by_rank(rank);
        uint32_t color = index == top_window() ? 0x0015449bU : 0x00236bc1U;
        char title[24];
        int maximum = button_width / 8 - 2;
        if (index < 0) continue;
        if (maximum < 0) maximum = 0;
        if (maximum > (int)sizeof(title) - 1) maximum = sizeof(title) - 1;
        strncpy(title, windows[index].title, maximum);
        title[maximum] = '\0';
        kui_fill_rect(&frame, x, y + 4, button_width - 2, 25, color);
        kui_outline_rect(&frame, x, y + 4, button_width - 2, 25,
                         0x00002271U);
        kui_draw_text(&frame, x + 7, y + 8, title, KUI_WHITE, color, 0);
        x += button_width;
    }
    snprintf(clock, sizeof(clock), "%02u:%02u",
             (unsigned)((minutes / 60U) % 24U),
             (unsigned)(minutes % 60U));
    kui_draw_text(&frame, screen_width - 68, y + 8,
                  clock, KUI_WHITE, KUI_TITLE_BLUE, 0);
}

static int start_entry_count(void)
{
    int count = 0;
    for (int i = 0; i < app_count; i++)
        if (aqua_shell() || apps[i].start_menu) count++;
    return count;
}

static int start_app_by_rank(int rank)
{
    for (int i = 0; i < app_count; i++)
        if ((aqua_shell() || apps[i].start_menu) && rank-- == 0) return i;
    return -1;
}

static int dock_entry_count(void)
{
    int count = 0;
    for (int i = 0; i < app_count; i++) if (apps[i].pinned) count++;
    return count;
}

static int dock_app_by_rank(int rank)
{
    for (int i = 0; i < app_count; i++)
        if (apps[i].pinned && rank-- == 0) return i;
    return -1;
}

static int app_rank_for_window(int window)
{
    int fallback = -1;

    for (int rank = 0; rank < dock_entry_count(); rank++) {
        int app = dock_app_by_rank(rank);

        if (app < 0 || strcmp(application_name(&apps[app]),
                              windows[window].application) != 0)
            continue;
        if (strcmp(apps[app].icon_path, windows[window].icon_path) == 0)
            return rank;
        if (fallback < 0) fallback = rank;
    }
    return fallback;
}

static int shell_exchange(uint32_t type, int x, int y, int button,
                          struct kui_shell_response *result)
{
    struct kui_shell_request request;
    struct kui_shell_response response;
    int timeout_ms = type == KUI_SHELL_RENDER ? 1500 : 250;

    memset(&request, 0, sizeof(request));
    request.magic = KUI_SHELL_MAGIC;
    request.version = KUI_SHELL_VERSION;
    request.serial = ++shell_serial;
    request.type = type;
    request.screen_width = screen_width;
    request.screen_height = screen_height;
    request.ticks_ms = kui_ticks_ms();
    if (type == KUI_SHELL_RENDER)
        request.ticks_ms = request.ticks_ms / 60000U * 60000U;
    request.x = x;
    request.y = y;
    if (type == KUI_SHELL_RENDER &&
        (!aqua_shell() || y < work_bottom() - 24)) {
        /* Pointer position only changes Aqua's Dock while it is close enough
         * to magnify an icon. Keep every other shell frame cacheable. */
        request.x = -1;
        request.y = -1;
    }
    request.button = button;
    request.app_count = dock_entry_count();
    if (request.app_count > KUI_SHELL_MAX_APPS)
        request.app_count = KUI_SHELL_MAX_APPS;
    for (int rank = 0; rank < request.app_count; rank++) {
        int app = dock_app_by_rank(rank);

        if (app >= 0)
            copy_text(request.apps[rank].title,
                      sizeof(request.apps[rank].title), apps[app].name);
        if (app >= 0)
            copy_text(request.apps[rank].icon_path,
                      sizeof(request.apps[rank].icon_path),
                      apps[app].icon_path);
    }
    request.menu_app_count = app_count > KUI_SHELL_MAX_APPS ?
                             KUI_SHELL_MAX_APPS : app_count;
    for (int rank = 0; rank < request.menu_app_count; rank++) {
        copy_text(request.menu_apps[rank].title,
                  sizeof(request.menu_apps[rank].title), apps[rank].name);
        copy_text(request.menu_apps[rank].icon_path,
                  sizeof(request.menu_apps[rank].icon_path),
                  apps[rank].icon_path);
    }
    request.window_count = taskbar_window_count();
    {
        int active = top_window();

        copy_text(request.active_application,
                  sizeof(request.active_application),
                  active >= 0 ? windows[active].application : "Finder");
    }
    if (request.window_count > KUI_SHELL_MAX_WINDOWS)
        request.window_count = KUI_SHELL_MAX_WINDOWS;
    for (int rank = 0; rank < request.window_count; rank++) {
        int index = taskbar_window_by_rank(rank);

        if (index < 0) continue;
        /* Aqua identifies Dock entries with app_index and never renders the
         * document title. Keep its shell request stable across dirty-marker
         * and status-title changes so those changes do not regenerate the
         * large translucent Dock while an application starts heavy work. */
        copy_text(request.windows[rank].title,
                  sizeof(request.windows[rank].title),
                  aqua_shell() ? windows[index].application :
                                 windows[index].title);
        copy_text(request.windows[rank].icon_path,
                  sizeof(request.windows[rank].icon_path),
                  windows[index].icon_path);
        request.windows[rank].minimized = windows[index].minimized;
        request.windows[rank].active = index == top_window();
        request.windows[rank].app_index = app_rank_for_window(index);
    }
    if (type == KUI_SHELL_RENDER && shell_cache_valid) {
        uint32_t serial = request.serial;

        request.serial = 0;
        if (memcmp(&request, &shell_cached_request, sizeof(request)) == 0) {
            request.serial = serial;
            return shell_display_bytes == 0 ? 0 :
                   kui_display_replay(&frame, shell_display,
                                      shell_display_bytes);
        }
        request.serial = serial;
    }
    if (kui_write_all(shell_input, &request, sizeof(request)) < 0 ||
        kui_read_all_timeout(shell_output, &response, sizeof(response),
                             timeout_ms) < 0 ||
        response.magic != KUI_SHELL_MAGIC ||
        response.version != KUI_SHELL_VERSION ||
        response.serial != request.serial ||
        response.display_bytes > KUI_PROTOCOL_MAX_DISPLAY)
        return -1;
    if (response.display_bytes != 0) {
        if (kui_read_all_timeout(shell_output, shell_display,
                                 response.display_bytes, timeout_ms) < 0 ||
            kui_display_replay(&frame, shell_display,
                               response.display_bytes) < 0) {
            return -1;
        }
    }
    if (type == KUI_SHELL_RENDER) {
        shell_cached_request = request;
        shell_cached_request.serial = 0;
        shell_display_bytes = response.display_bytes;
        shell_cache_valid = 1;
    } else if (response.consumed ||
               response.action != KUI_SHELL_ACTION_NONE) {
        /* Clicks inside an application do not change shell-owned UI. Keep
         * Aqua's large Dock frame cached so launching CPU-heavy work from a
         * button cannot force a full shell redraw at the same time. */
        shell_cache_valid = 0;
    }
    if (result != NULL)
        *result = response;
    return 0;
}

static int dispatch_shell_pointer(int x, int y, int button)
{
    struct kui_shell_response response;

    memset(&response, 0, sizeof(response));
    if (shell_exchange(KUI_SHELL_POINTER, x, y, button, &response) < 0) {
        fprintf(stderr, "kuid: shell pointer exchange failed at %d,%d\n",
                x, y);
        running = 0;
        return 1;
    }
    if (response.action == KUI_SHELL_ACTION_OPEN_APP) {
        int app = dock_app_by_rank(response.index);
        if (app >= 0) open_application(&apps[app], NULL);
    } else if (response.action == KUI_SHELL_ACTION_OPEN_MENU_APP) {
        int app = start_app_by_rank(response.index);
        if (app >= 0) open_application(&apps[app], NULL);
    } else if (response.action == KUI_SHELL_ACTION_TOGGLE_PIN) {
        int app = -1;
        if (response.index >= 100 && response.index < 100 + MAX_WINDOWS) {
            int window = taskbar_window_by_rank(response.index - 100);
            if (window < 0) return response.consumed != 0;
            for (int i = 0; i < app_count; i++)
                if (strcmp(application_name(&apps[i]),
                           windows[window].application) == 0 ||
                    strcmp(apps[i].name, windows[window].application) == 0 ||
                    strcmp(apps[i].icon_path, windows[window].icon_path) == 0)
                    app = i;
        } else {
            app = dock_app_by_rank(response.index);
        }
        shell_log("DOCK toggle index=%d app=%d window=%d\n", response.index,
                  app, response.index >= 100 ? response.index - 100 : -1);
        if (app >= 0) {
            apps[app].pinned = !apps[app].pinned;
            save_dock_config();
            shell_log("DOCK pin id=%s value=%d\n", apps[app].id,
                      apps[app].pinned);
        }
    } else if (response.action == KUI_SHELL_ACTION_ACTIVATE_WINDOW) {
        int index = taskbar_window_by_rank(response.index);
        if (index >= 0) {
            windows[index].minimized = 0;
            raise_window(index);
        }
    } else if (response.action == KUI_SHELL_ACTION_CLOSE_ACTIVE) {
        close_window_index(top_window());
    } else if (response.action == KUI_SHELL_ACTION_QUIT) {
        fprintf(stderr, "kuid: shell requested desktop exit\n");
        running = 0;
    }
    return response.consumed != 0;
}

static void __attribute__((unused)) draw_start_menu(void)
{
    int count;
    int height;
    int x = 3;
    int y;

    if (!start_menu_open) return;
    count = start_entry_count();
    height = 12 + (count + 2) * 26;
    y = aqua_shell() ? 24 : screen_height - TASKBAR_HEIGHT - height;
    if (aqua_shell()) {
        x = 4;
        height = 12 + (count + 1) * 26;
    }
    kui_fill_rect(&frame, x, y, 250, height, 0x00f5f5f5U);
    kui_outline_rect(&frame, x, y, 250, height, KUI_TITLE_BLUE);
    for (int rank = 0; rank < count; rank++) {
        int app = start_app_by_rank(rank);
        kui_draw_text(&frame, x + 14, y + 8 + rank * 26,
                      apps[app].name, KUI_BLACK, 0x00f5f5f5U, 0);
    }
    if (aqua_shell())
        kui_draw_text(&frame, x + 14, y + 8 + count * 26,
                      "Quit Aqua", KUI_BLACK, 0x00f5f5f5U, 0);
    else {
        kui_draw_text(&frame, x + 14, y + 8 + count * 26,
                      "Close Window", KUI_BLACK, 0x00f5f5f5U, 0);
        kui_draw_text(&frame, x + 14, y + 8 + (count + 1) * 26,
                      "Quit to Shell", KUI_BLACK, 0x00f5f5f5U, 0);
    }
}

static void draw_desktop_icons(void)
{
    int icon = 0;
    int rows = (work_height() - 12) / 82;
    if (rows < 1) rows = 1;
    for (int i = 0; i < app_count; i++) {
        int x;
        int y;
        int width;
        if (!apps[i].desktop ||
            (aqua_shell() && strcmp(apps[i].id, "system-disk") != 0))
            continue;
        x = aqua_shell() ? screen_width - 94 :
                           12 + (icon / rows) * 94;
        y = work_top() + 8 + (icon % rows) * 82;
        if (apps[i].icon.pixels != NULL)
            kui_blit_image(&frame, &apps[i].icon, x + 9, y);
        else {
            kui_fill_rect(&frame, x + 18, y + 8, 42, 42, KUI_LIGHT);
            kui_outline_rect(&frame, x + 18, y + 8, 42, 42, KUI_SHADOW);
        }
        width = kui_text_width(apps[i].name);
        if (selected_icon == i)
            kui_fill_rect(&frame, x + 39 - width / 2, y + 60,
                          width + 4, 18, 0x00316ac5U);
        kui_draw_text(&frame, x + 41 - width / 2, y + 61,
                      apps[i].name, KUI_WHITE,
                      selected_icon == i ? 0x00316ac5U : 0, 0);
        icon++;
    }
}

static void draw_cursor(void)
{
    static const uint16_t shape[16] = {
        0x8000,0xc000,0xe000,0xf000,0xf800,0xfc00,0xfe00,0xff00,
        0xff80,0xfc00,0xdc00,0x8e00,0x0600,0x0700,0x0300,0x0000
    };
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 10; x++)
            if (shape[y] & (0x8000 >> x))
                kui_pixel(&frame, cursor_x + x, cursor_y + y,
                          x == 0 || y == 0 ? KUI_WHITE : KUI_BLACK);
}

static void render(void)
{
    int active = top_window();
    if (wallpaper.pixels != NULL)
        kui_blit_image_scaled(&frame, &wallpaper, 0, 0,
                              screen_width, screen_height);
    else kui_draw_desktop(&frame, work_top());
    draw_desktop_icons();
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].visible && !windows[i].minimized)
            draw_window(&windows[i], i == active);
    if (shell_exchange(KUI_SHELL_RENDER, cursor_x, cursor_y, 0, NULL) < 0) {
        fprintf(stderr, "kuid: shell frame render failed\n");
        running = 0;
    }
    draw_cursor();
    kui_present(&frame);
}

static void dispatch_pointer(int index, int button, int buttons, int wheel)
{
    struct shell_action action;
    struct shell_window *window = &windows[index];
    memset(&action, 0, sizeof(action));
    if (send_window_event(index, KUI_EVENT_POINTER,
                          cursor_x - content_x(window),
                          cursor_y - content_y(window),
                          button, buttons, wheel, 0, 0, 0, 0, 0,
                          NULL, &action) < 0)
        close_window_index(index);
    else apply_action(&action);
}

static void notify_resize(int index)
{
    struct shell_action action;
    memset(&action, 0, sizeof(action));
    if (send_window_event(index, KUI_EVENT_RESIZE, 0, 0, 0,
                          mouse_buttons, 0, 0, 0, 0, 0, 0,
                          NULL, &action) < 0)
        close_window_index(index);
    else apply_action(&action);
}

static void snap_window(int index, int side)
{
    struct shell_window *window = &windows[index];
    int half = screen_width / 2;

    if (!window->snapped) {
        window->restore_x = window->x;
        window->restore_y = window->y;
        window->restore_width = window->width;
        window->restore_height = window->height;
    }
    window->snapped = side;
    window->x = side < 0 ? 0 : half;
    window->y = work_top();
    window->width = side < 0 ? half : screen_width - half;
    window->height = work_height();
    notify_resize(index);
}

static void maximize_window(int index)
{
    struct shell_window *window = &windows[index];

    if (!window->snapped) {
        window->restore_x = window->x;
        window->restore_y = window->y;
        window->restore_width = window->width;
        window->restore_height = window->height;
        window->x = 0;
        window->y = work_top();
        window->width = screen_width;
        window->height = work_height();
        window->snapped = 2;
    } else {
        window->x = window->restore_x;
        window->y = window->restore_y;
        window->width = window->restore_width;
        window->height = window->restore_height;
        window->snapped = 0;
    }
    notify_resize(index);
}

static void activate_taskbar(int x)
{
    int width = taskbar_button_width();
    int rank;
    int index;
    if (width <= 0 || x < 102) return;
    rank = (x - 102) / width;
    index = taskbar_window_by_rank(rank);
    if (index < 0) return;
    if (windows[index].minimized) windows[index].minimized = 0;
    else if (index == top_window()) windows[index].minimized = 1;
    else raise_window(index);
}

static void activate_start_item(int x, int y)
{
    int count = start_entry_count();
    int height = 12 + (count + (aqua_shell() ? 1 : 2)) * 26;
    int top = aqua_shell() ? 24 : screen_height - TASKBAR_HEIGHT - height;
    int rank;
    if (!kui_in_rect(x, y, 3, top, 250, height)) {
        start_menu_open = 0; return;
    }
    rank = (y - top - 5) / 26;
    start_menu_open = 0;
    if (rank < count) {
        int app = start_app_by_rank(rank);
        if (app >= 0) open_application(&apps[app], NULL);
    } else if (aqua_shell()) running = 0;
    else if (rank == count) close_window_index(top_window());
    else running = 0;
}

static void activate_dock(int x, int y)
{
    int app_icons = dock_entry_count();
    int window_icons = taskbar_window_count();
    int count;
    int width;
    int left;
    int rank;

    if (app_icons > 12) app_icons = 12;
    count = app_icons + window_icons;
    if (count > (screen_width - 20) / 48)
        count = (screen_width - 20) / 48;
    width = 20 + count * 48;
    left = (screen_width - width) / 2;
    if (!kui_in_rect(x, y, left, screen_height - 58, width, 52))
        return;
    rank = (x - left - 10) / 48;
    if (rank >= 0 && rank < app_icons && rank < count) {
        int app = dock_app_by_rank(rank);
        if (app >= 0) open_application(&apps[app], NULL);
    } else if (rank >= app_icons && rank < count) {
        int index = taskbar_window_by_rank(rank - app_icons);
        if (index >= 0) {
            windows[index].minimized = 0;
            raise_window(index);
        }
    }
}

static void handle_mouse_down(int button)
{
    int index;
    pointer_window = -1;
    if (aqua_shell() && (button == 1 || button == 2) &&
        dispatch_shell_pointer(cursor_x, cursor_y, button))
        return;
    if (aqua_shell() && cursor_y < work_top()) {
        if (button == 1 && cursor_x < 30)
            start_menu_open = !start_menu_open;
        return;
    }
    if (aqua_shell() && cursor_y >= work_bottom()) {
        if (button == 1) activate_dock(cursor_x, cursor_y);
        return;
    }
    if (cursor_y >= screen_height - TASKBAR_HEIGHT) {
        if (button == 1 && cursor_x < 100)
            start_menu_open = !start_menu_open;
        else if (button == 1) {
            start_menu_open = 0; activate_taskbar(cursor_x);
        }
        return;
    }
    if (start_menu_open) { if (button == 1) activate_start_item(cursor_x, cursor_y); return; }
    index = window_at(cursor_x, cursor_y);
    if (index >= 0) {
        index = raise_window(index);
        if (aqua_shell() && button == 1 &&
            kui_in_rect(cursor_x, cursor_y,
                        windows[index].x + 8, windows[index].y + 7,
                        12, 12)) {
            close_window_index(index);
        } else if (aqua_shell() && button == 1 &&
                   kui_in_rect(cursor_x, cursor_y,
                               windows[index].x + 27,
                               windows[index].y + 7, 12, 12)) {
            windows[index].minimized = 1;
        } else if (aqua_shell() && button == 1 &&
                   kui_in_rect(cursor_x, cursor_y,
                               windows[index].x + 46,
                               windows[index].y + 7, 12, 12)) {
            maximize_window(index);
        } else if (!aqua_shell() && button == 1 &&
            kui_in_rect(cursor_x, cursor_y,
                        windows[index].x + windows[index].width - 24,
                        windows[index].y + 4, 19, 17)) {
            close_window_index(index);
        } else if (!aqua_shell() && button == 1 &&
                   kui_in_rect(cursor_x, cursor_y,
                               windows[index].x + windows[index].width - 47,
                               windows[index].y + 4, 19, 17)) {
            windows[index].minimized = 1;
        } else if (button == 1 &&
                   kui_in_rect(cursor_x, cursor_y,
                               windows[index].x + windows[index].width - 17,
                               windows[index].y + windows[index].height - 17,
                               17, 17)) {
            windows[index].snapped = 0;
            resize_window = index;
            resize_right_gap = windows[index].x + windows[index].width - cursor_x;
            resize_bottom_gap = windows[index].y + windows[index].height - cursor_y;
        } else if (button == 1 &&
                   kui_in_rect(cursor_x, cursor_y, windows[index].x,
                               windows[index].y, windows[index].width,
                               WINDOW_TITLE_HEIGHT)) {
            if (windows[index].snapped) {
                int restored_width = windows[index].restore_width;
                int restored_height = windows[index].restore_height;
                windows[index].width = restored_width;
                windows[index].height = restored_height;
                windows[index].x = cursor_x - restored_width / 2;
                windows[index].y = cursor_y - 10;
                windows[index].snapped = 0;
                notify_resize(index);
            }
            drag_window = index;
            drag_dx = cursor_x - windows[index].x;
            drag_dy = cursor_y - windows[index].y;
        } else if (kui_in_rect(cursor_x, cursor_y, content_x(&windows[index]),
                               content_y(&windows[index]),
                               content_width(&windows[index]),
                               content_height(&windows[index]))) {
            if (button == 1) pointer_window = index;
            dispatch_pointer(index, button, mouse_buttons | (uint8_t)button, 0);
        }
        return;
    }
    if (button == 1) {
        index = desktop_app_at(cursor_x, cursor_y);
        selected_icon = index;
        if (index >= 0) open_application(&apps[index], NULL);
    }
}

static int handle_mouse(void)
{
    int changed = 0;
    int events_handled = 0;
    int pointer_moved = 0;
    int dx;
    int dy;
    int wheel;
    uint8_t buttons;
    while (events_handled < 32 &&
           kui_mouse_poll_wheel(&dx, &dy, &buttons, &wheel) > 0) {
        int pressed_left = (buttons & 1) && !(mouse_buttons & 1);
        int released_left = !(buttons & 1) && (mouse_buttons & 1);

        events_handled++;
        cursor_x += kui_pointer_delta(dx);
        cursor_y -= kui_pointer_delta(dy);
        if (cursor_x < 0) cursor_x = 0;
        if (cursor_y < 0) cursor_y = 0;
        if (cursor_x > screen_width - 11) cursor_x = screen_width - 11;
        if (cursor_y > screen_height - 17) cursor_y = screen_height - 17;
        if (wheel != 0) {
            int index = window_at(cursor_x, cursor_y);
            if (index >= 0) dispatch_pointer(index, 0, buttons, wheel);
        }
        if (pressed_left) handle_mouse_down(1);
        if ((buttons & 2) && !(mouse_buttons & 2)) handle_mouse_down(2);
        mouse_buttons = buttons;
        if (!pressed_left && (buttons & 1) && (dx != 0 || dy != 0) &&
            pointer_window >= 0 && windows[pointer_window].visible &&
            drag_window < 0 && resize_window < 0)
            pointer_moved = 1;
        if (drag_window >= 0 && windows[drag_window].visible) {
            struct shell_window *window = &windows[drag_window];
            window->x = cursor_x - drag_dx;
            window->y = cursor_y - drag_dy;
            if (window->x < 0) window->x = 0;
            if (window->y < work_top()) window->y = work_top();
            if (window->x + window->width > screen_width)
                window->x = screen_width - window->width;
            if (window->y + window->height > work_bottom())
                window->y = work_bottom() - window->height;
        }
        if (resize_window >= 0 && windows[resize_window].visible) {
            struct shell_window *window = &windows[resize_window];
            int width = cursor_x - window->x + resize_right_gap;
            int height = cursor_y - window->y + resize_bottom_gap;
            if (width < window->minimum_width) width = window->minimum_width;
            if (height < window->minimum_height) height = window->minimum_height;
            if (window->x + width > screen_width) width = screen_width - window->x;
            if (window->y + height > work_bottom())
                height = work_bottom() - window->y;
            if (width != window->width || height != window->height) {
                window->width = width;
                window->height = height;
                notify_resize(resize_window);
            }
        }
        if (released_left) {
            if (pointer_moved && pointer_window >= 0 &&
                windows[pointer_window].visible) {
                dispatch_pointer(pointer_window, 0, 1, 0);
                pointer_moved = 0;
            }
            if (pointer_window >= 0 && windows[pointer_window].visible)
                dispatch_pointer(pointer_window, 0, buttons, 0);
            pointer_window = -1;
            if (drag_window >= 0 && windows[drag_window].visible) {
                if (cursor_x <= 5) snap_window(drag_window, -1);
                else if (cursor_x >= screen_width - 16)
                    snap_window(drag_window, 1);
            }
            drag_window = -1;
            resize_window = -1;
        }
        changed = 1;
    }
    if (pointer_moved && (mouse_buttons & 1) && pointer_window >= 0 &&
        windows[pointer_window].visible)
        dispatch_pointer(pointer_window, 0, mouse_buttons, 0);
    return changed;
}

static int handle_keyboard(void)
{
    static int alt_down;
    static int ctrl_down;
    static int shift_down;
    int changed = 0;
    int pressed;
    uint8_t code;
    uint8_t extended;

    while (kui_key_poll(&pressed, &code, &extended) > 0) {
        uint32_t modifiers;
        char character = 0;
        int top;
        struct shell_action action;
        if (!extended && code == 56) { alt_down = pressed; continue; }
        if (!extended && code == 29) { ctrl_down = pressed; continue; }
        if (!extended && (code == 42 || code == 54)) {
            shift_down = pressed; continue;
        }
        if (pressed && alt_down && !extended && code == 16) {
            running = 0; changed = 1; continue;
        }
        if (pressed && !extended && code == 1) {
            if (start_menu_open) start_menu_open = 0;
            else close_window_index(top_window());
            changed = 1; continue;
        }
        top = top_window();
        if (top < 0) continue;
        if (!extended) {
            if (code == 28) character = '\n';
            else if (code == 15) character = '\t';
            else if (code == 14) character = '\b';
            else character = kui_scan_to_char(code, shift_down);
        }
        modifiers = (shift_down ? KUI_MOD_SHIFT : 0) |
                    (ctrl_down ? KUI_MOD_CTRL : 0) |
                    (alt_down ? KUI_MOD_ALT : 0);
        memset(&action, 0, sizeof(action));
        if (send_window_event(top, KUI_EVENT_KEY, 0, 0, 0, 0, 0,
                              code, extended, pressed, modifiers, character,
                              NULL, &action) < 0)
            close_window_index(top);
        else apply_action(&action);
        changed = 1;
    }
    return changed;
}

static int handle_timers(void)
{
    uint32_t now = kui_ticks_ms();
    int changed = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        struct shell_action action;
        if (!windows[i].visible || windows[i].timer_ms == 0 ||
            (int32_t)(now - windows[i].next_timer) < 0)
            continue;
        windows[i].next_timer = now + windows[i].timer_ms;
        memset(&action, 0, sizeof(action));
        if (send_window_event(i, KUI_EVENT_TIMER, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, NULL, &action) < 0)
            close_window_index(i);
        else apply_action(&action);
        changed = 1;
    }
    return changed;
}

static void restore_console(void)
{
    kui_tty_raw(0);
    if (frame_pixels != NULL) memset(frame_pixels, 0, frame_bytes);
    if (frame_pixels != NULL) kui_present(&frame);
    printf("\033[2J\033[H");
    fflush(stdout);
}

int desktop_shell_run(const char *shell_path)
{
    uint32_t last_clock_minute;
    int shell_restarts = 0;

    if (start_shell(shell_path) < 0) {
        fprintf(stderr, "kuid: unable to start shell %s\n", shell_path);
        return 1;
    }
    if (kui_screen_size(&screen_width, &screen_height) < 0 ||
        screen_width < 320 || screen_height < 200 ||
        screen_width > 4096 || screen_height > 2160) {
        screen_width = KUI_SCREEN_WIDTH;
        screen_height = KUI_SCREEN_HEIGHT;
    }
    frame_bytes = (uint32_t)screen_width * screen_height * sizeof(uint32_t);
    frame_pixels = calloc((size_t)screen_width * screen_height,
                          sizeof(uint32_t));
    if (frame_pixels == NULL) {
        stop_shell();
        return 1;
    }
    kui_surface_init(&frame, frame_pixels, screen_width, screen_height);
    kui_image_load(aqua_shell() ? "/usr/share/gui/aqua-wallpaper.kimg" :
                                 "/usr/share/gui/wallpaper.kimg",
                   &wallpaper);
    load_applications();
    load_dock_config();
    if (aqua_shell()) save_dock_config();
    cursor_x = screen_width / 2;
    cursor_y = screen_height / 2;
    kui_tty_raw(1);
    atexit(restore_console);
    for (int i = 0; i < app_count; i++)
        if (apps[i].autostart) open_application(&apps[i], NULL);
    render();
    last_clock_minute = kui_ticks_ms() / 60000U;
    while (running) {
        struct kyleos_fd_status shell_status;
        int changed = handle_mouse();

        reap_retired_windows();
        if (kui_fd_status(shell_output, &shell_status) < 0 ||
            shell_status.hung_up) {
            fprintf(stderr, "kuid: shell %s disconnected\n", shell_name);
            stop_shell();
            if (shell_restarts++ == 0 && start_shell(shell_path) == 0) {
                changed = 1;
            } else {
                running = 0;
                break;
            }
        }
        changed |= handle_keyboard();
        changed |= handle_timers();
        if (last_clock_minute != kui_ticks_ms() / 60000U) {
            last_clock_minute = kui_ticks_ms() / 60000U;
            changed = 1;
        }
        if (changed) render();
        else kui_sleep_ms(10);
    }
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].visible) close_window_index(i);
    for (int i = 0; i < app_count; i++) kui_image_free(&apps[i].icon);
    kui_image_free(&wallpaper);
    free(frame_pixels);
    frame_pixels = NULL;
    stop_shell();
    return 0;
}
