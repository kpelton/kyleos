#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "extras/finder/finder_ipc.h"
#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"
#include "extras/kui/kui_dialog.h"

#define FINDER_HISTORY_MAX 16
#define CONTEXT_MENU_WIDTH 176
#define CONTEXT_MENU_ROW_HEIGHT 24
#define CONTEXT_MENU_HEIGHT (CONTEXT_MENU_ROW_HEIGHT * 3 + 4)

enum finder_context_kind {
    FINDER_CONTEXT_NONE,
    FINDER_CONTEXT_ITEM,
    FINDER_CONTEXT_BACKGROUND
};

enum finder_create_kind {
    FINDER_CREATE_NONE,
    FINDER_CREATE_FOLDER,
    FINDER_CREATE_TEXT_FILE
};

struct kyleos_dirent {
    uint64_t inode;
    char name[257];
    int type;
    uint64_t size;
};

static int get_directory_entries(int fd, struct kyleos_dirent *entries,
                                 int count)
{
    register long rax asm("rax") = 16;
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"((long)fd), "S"(entries), "d"((long)count)
                 : "memory");
    return (int)rax;
}

static const char *path_name(const char *path)
{
    const char *name = strrchr(path, '/');

    if (path[0] == '/' && path[1] == '\0') return "System Disk";
    return name != NULL ? name + 1 : path;
}

static void copy_path(char destination[FINDER_PATH_MAX], const char *source)
{
    size_t length = strlen(source);
    if (length >= FINDER_PATH_MAX) length = FINDER_PATH_MAX - 1;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

static int child_path(char destination[FINDER_PATH_MAX], const char *parent,
                      const char *name)
{
    size_t parent_length = strlen(parent);
    size_t name_length = strlen(name);
    size_t separator = strcmp(parent, "/") == 0 ? 0 : 1;

    if (parent_length + separator + name_length >= FINDER_PATH_MAX)
        return -1;
    memcpy(destination, parent, parent_length);
    if (separator != 0) destination[parent_length++] = '/';
    memcpy(destination + parent_length, name, name_length);
    destination[parent_length + name_length] = '\0';
    return 0;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static int path_exists(const char *path)
{
    int fd = open(path, O_RDONLY);

    if (fd < 0) return 0;
    close(fd);
    return 1;
}

static int remove_empty_directory(const char *path)
{
    register long result asm("rax") = 29;

    asm volatile("int $0x80" : "+a"(result) : "D"(path) : "memory");
    return (int)result;
}

static int copy_file_contents(const char *source, const char *destination)
{
    unsigned char buffer[4096];
    int input = -1;
    int output = -1;
    int result = -1;

    input = open(source, O_RDONLY);
    if (input < 0) goto done;
    output = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output < 0) goto done;
    for (;;) {
        int count = read(input, buffer, sizeof(buffer));
        int written = 0;

        if (count < 0) goto done;
        if (count == 0) break;
        while (written < count) {
            int amount = write(output, buffer + written, count - written);
            if (amount <= 0) goto done;
            written += amount;
        }
    }
    result = 0;
done:
    if (output >= 0) close(output);
    if (input >= 0) close(input);
    if (result < 0 && output >= 0) unlink(destination);
    return result;
}

static int is_short_filename(const char *name)
{
    int base = 0;
    int extension = 0;
    int saw_dot = 0;

    if (name == NULL || name[0] == '\0') return 0;
    for (; *name != '\0'; name++) {
        if (*name == '.') {
            if (saw_dot || base == 0) return 0;
            saw_dot = 1;
        } else if (!saw_dot) {
            if (++base > 8) return 0;
        } else if (++extension > 3) {
            return 0;
        }
    }
    return base > 0;
}

static int make_new_path(char destination[FINDER_PATH_MAX],
                         const char *directory, int folder)
{
    char name[16];

    for (int number = 0; number <= 9999; number++) {
        if (folder) {
            if (number == 0) strcpy(name, "newfold");
            else snprintf(name, sizeof(name), "fold%04d", number);
        } else {
            if (number == 0) strcpy(name, "new.txt");
            else snprintf(name, sizeof(name), "new%04d.txt", number);
        }
        if (child_path(destination, directory, name) == 0 &&
            !path_exists(destination))
            return 0;
    }
    return -1;
}

static int make_copy_path(char destination[FINDER_PATH_MAX],
                          const char *directory, const char *source)
{
    const char *source_name = base_name(source);
    const char *dot = strrchr(source_name, '.');
    char extension[5];
    char name[16];

    if (is_short_filename(source_name) &&
        child_path(destination, directory, source_name) == 0 &&
        strcmp(destination, source) != 0 && !path_exists(destination))
        return 0;
    extension[0] = '\0';
    if (dot != NULL && strlen(dot) <= 4)
        strcpy(extension, dot);
    for (int number = 1; number <= 9999; number++) {
        snprintf(name, sizeof(name), "copy%d%s", number, extension);
        if (child_path(destination, directory, name) == 0 &&
            !path_exists(destination))
            return 0;
    }
    return -1;
}

static void load_directory(const char *path, struct finder_response *response)
{
    struct kyleos_dirent entries[64];
    int fd;
    int count;

    memset(response, 0, sizeof(*response));
    response->status = -1;
    fd = open(path, O_RDONLY);
    if (fd < 0) return;
    count = get_directory_entries(fd, entries,
                                  (int)(sizeof(entries) / sizeof(entries[0])));
    close(fd);
    if (count < 0) return;

    strncpy(response->path, path, sizeof(response->path) - 1);
    strncpy(response->title, path_name(path), sizeof(response->title) - 1);
    for (int i = 0; i < count && response->item_count < FINDER_MAX_ITEMS;
         i++) {
        int slot;

        if (strcmp(entries[i].name, ".") == 0 ||
            strcmp(entries[i].name, "..") == 0)
            continue;
        slot = response->item_count++;
        strncpy(response->item_names[slot], entries[i].name,
                FINDER_NAME_MAX - 1);
        response->item_names[slot][FINDER_NAME_MAX - 1] = '\0';
        response->item_types[slot] = entries[i].type;
    }
    response->status = 0;
}

static void history_push(char history[FINDER_HISTORY_MAX][FINDER_PATH_MAX],
                         int *history_count, int *history_index,
                         const char *path)
{
    if (*history_index >= 0 &&
        strcmp(history[*history_index], path) == 0)
        return;
    if (*history_index + 1 < *history_count)
        *history_count = *history_index + 1;
    if (*history_count == FINDER_HISTORY_MAX) {
        memmove(history[0], history[1],
                (FINDER_HISTORY_MAX - 1) * FINDER_PATH_MAX);
        (*history_count)--;
        (*history_index)--;
    }
    strncpy(history[*history_count], path, FINDER_PATH_MAX - 1);
    history[*history_count][FINDER_PATH_MAX - 1] = '\0';
    *history_index = *history_count;
    (*history_count)++;
}

static void parent_path(const char *path, char *parent)
{
    char *slash;

    copy_path(parent, path);
    slash = strrchr(parent, '/');
    if (slash == parent)
        parent[1] = '\0';
    else if (slash != NULL)
        *slash = '\0';
    else
        strcpy(parent, "/");
}

static int finder_columns(int width)
{
    int columns = (width - 18) / 88;
    return columns > 0 ? columns : 1;
}

static int finder_visible_rows(int height)
{
    int rows = (height - 65) / 82;
    return rows > 0 ? rows : 1;
}

static int finder_max_scroll(const struct finder_response *state,
                             int width, int height)
{
    int rows = (state->item_count + finder_columns(width) - 1) /
               finder_columns(width);
    int maximum = rows - finder_visible_rows(height);
    return maximum > 0 ? maximum : 0;
}

static void draw_folder(struct kui_surface *surface, int x, int y)
{
    kui_fill_rect(surface, x + 4, y, 22, 6, 0x00f2b632U);
    kui_fill_rect(surface, x, y + 5, 48, 32, 0x00ffd45bU);
    kui_outline_rect(surface, x, y + 5, 48, 32, 0x00956a13U);
}

static void draw_document(struct kui_surface *surface, int x, int y)
{
    kui_fill_rect(surface, x + 7, y, 34, 40, KUI_WHITE);
    kui_outline_rect(surface, x + 7, y, 34, 40, KUI_SHADOW);
    for (int row = 0; row < 4; row++)
        kui_fill_rect(surface, x + 13, y + 9 + row * 6, 22, 1,
                      0x0089a1bdU);
}

static void finder_render(struct kui_client *client,
                          const struct kui_event *event,
                          const struct finder_response *state,
                          int scroll_row, int context_kind,
                          int context_item, int context_x, int context_y,
                          const char *status_message)
{
    int columns = finder_columns(event->width);
    char location[148];

    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  KUI_WHITE);
    kui_fill_rect(&client->surface, 0, 0, event->width, 58, KUI_LIGHT);
    kui_draw_button(&client->surface, 5, 5, 55, 24, "Back",
                    !state->can_back);
    kui_draw_button(&client->surface, 64, 5, 70, 24, "Forward",
                    !state->can_forward);
    kui_draw_button(&client->surface, 138, 5, 40, 24, "Up",
                    !state->can_up);
    snprintf(location, sizeof(location), "Location: %s", state->path);
    kui_draw_text(&client->surface, 7, 36, location,
                  KUI_BLACK, KUI_LIGHT, 0);
    for (int item = 0; item < state->item_count; item++) {
        int visible = item - scroll_row * columns;
        int row;
        int column;
        int x;
        int y;
        int label_width;
        uint32_t label_background = KUI_WHITE;

        if (visible < 0) continue;
        row = visible / columns;
        column = visible % columns;
        if (row >= finder_visible_rows(event->height)) break;
        x = 9 + column * 88;
        y = 65 + row * 82;
        if (context_kind == FINDER_CONTEXT_ITEM && item == context_item) {
            label_background = 0x00cde8ffU;
            kui_fill_rect(&client->surface, x + 2, y - 3, 76, 68,
                          label_background);
            kui_outline_rect(&client->surface, x + 2, y - 3, 76, 68,
                             0x00316ac5U);
        }
        if (state->item_types[item] == FINDER_ITEM_DIRECTORY)
            draw_folder(&client->surface, x + 16, y);
        else
            draw_document(&client->surface, x + 16, y);
        label_width = kui_text_width(state->item_names[item]);
        kui_draw_text(&client->surface,
                      x + 40 - label_width / 2, y + 47,
                      state->item_names[item], KUI_BLACK,
                      label_background, 0);
    }
    if (status_message != NULL && status_message[0] != '\0') {
        kui_fill_rect(&client->surface, 0, event->height - 21,
                      event->width, 21, KUI_LIGHT);
        kui_fill_rect(&client->surface, 0, event->height - 21,
                      event->width, 1, KUI_SHADOW);
        kui_draw_text(&client->surface, 7, event->height - 16,
                      status_message, KUI_BLACK, KUI_LIGHT, 0);
    }
    if (context_kind != FINDER_CONTEXT_NONE) {
        static const char *item_labels[] = { "Open", "Copy", "Delete" };
        static const char *background_labels[] = {
            "New Folder", "New Text File", "Paste"
        };
        const char **labels = context_kind == FINDER_CONTEXT_ITEM ?
                              item_labels : background_labels;

        kui_fill_rect(&client->surface, context_x + 4, context_y + 4,
                      CONTEXT_MENU_WIDTH, CONTEXT_MENU_HEIGHT,
                      0x00606060U);
        kui_fill_rect(&client->surface, context_x, context_y,
                      CONTEXT_MENU_WIDTH, CONTEXT_MENU_HEIGHT, KUI_FACE);
        kui_outline_rect(&client->surface, context_x, context_y,
                         CONTEXT_MENU_WIDTH, CONTEXT_MENU_HEIGHT, KUI_BLACK);
        for (int row = 0; row < 3; row++) {
            int disabled = 0;
            uint32_t color;

            if (context_kind == FINDER_CONTEXT_ITEM && row == 1 &&
                context_item >= 0 &&
                state->item_types[context_item] == FINDER_ITEM_DIRECTORY)
                disabled = 1;
            if (context_kind == FINDER_CONTEXT_BACKGROUND && row == 2 &&
                event->clipboard[0] == '\0')
                disabled = 1;
            color = disabled ? 0x00909090U : KUI_BLACK;
            kui_draw_text(&client->surface, context_x + 12,
                          context_y + 9 + row * CONTEXT_MENU_ROW_HEIGHT,
                          labels[row], color, KUI_FACE, 0);
        }
    }
}

static int finder_item_at(const struct finder_response *state,
                          int width, int height, int scroll_row,
                          int x, int y)
{
    int columns;
    int item;

    if (x < 9 || y < 60 || x >= width - 4 || y >= height) return -1;
    columns = finder_columns(width);
    item = scroll_row * columns + (y - 60) / 82 * columns + (x - 9) / 88;
    return item >= 0 && item < state->item_count ? item : -1;
}

static void activate_item(struct finder_response *state, int item,
                          char history[FINDER_HISTORY_MAX][FINDER_PATH_MAX],
                          int *history_count, int *history_index,
                          int *scroll_row, uint32_t *shell_action,
                          char action_path[FINDER_PATH_MAX])
{
    char path[FINDER_PATH_MAX];
    struct finder_response response;

    if (item < 0 || item >= state->item_count ||
        child_path(path, state->path, state->item_names[item]) < 0)
        return;
    if (state->item_types[item] == FINDER_ITEM_DIRECTORY) {
        load_directory(path, &response);
        if (response.status == 0) {
            history_push(history, history_count, history_index,
                         response.path);
            *state = response;
            *scroll_row = 0;
        }
    } else {
        copy_path(action_path, path);
        *shell_action = KUI_ACTION_OPEN_PATH;
    }
}

static int reload_current_directory(struct finder_response *state)
{
    struct finder_response response;

    load_directory(state->path, &response);
    if (response.status < 0) return -1;
    *state = response;
    return 0;
}

static int context_menu_row(int menu_x, int menu_y, int x, int y)
{
    int row;

    if (!kui_in_rect(x, y, menu_x + 1, menu_y + 1,
                     CONTEXT_MENU_WIDTH - 2, CONTEXT_MENU_HEIGHT - 2))
        return -1;
    row = (y - menu_y - 2) / CONTEXT_MENU_ROW_HEIGHT;
    return row >= 0 && row < 3 ? row : -1;
}

static int embedded_main(void)
{
    struct finder_response state;
    struct kui_input_dialog create_dialog;
    char history[FINDER_HISTORY_MAX][FINDER_PATH_MAX];
    int history_count = 0;
    int history_index = -1;
    int scroll_row = 0;
    int context_kind = FINDER_CONTEXT_NONE;
    int context_item = -1;
    int context_x = 0;
    int context_y = 0;
    int create_kind = FINDER_CREATE_NONE;
    char status_message[96];
    struct kui_client client;
    struct kui_event event;

    memset(&state, 0, sizeof(state));
    memset(&create_dialog, 0, sizeof(create_dialog));
    memset(history, 0, sizeof(history));
    status_message[0] = '\0';
    kui_client_init(&client);

    for (;;) {
        struct finder_response response;
        char action_path[FINDER_PATH_MAX];
        uint32_t shell_action = KUI_ACTION_NONE;

        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        response = state;
        response.status = -1;
        response.action = FINDER_ACTION_NONE;
        response.activated_path[0] = '\0';
        action_path[0] = '\0';
        if (event.type == KUI_EVENT_INIT) {
            const char *path = event.argument[0] != '\0' ? event.argument : "/";
            load_directory(path, &response);
            if (response.status == 0) {
                history_push(history, &history_count, &history_index,
                             response.path);
                state = response;
                scroll_row = 0;
                context_kind = FINDER_CONTEXT_NONE;
                create_kind = FINDER_CREATE_NONE;
                kui_input_dialog_close(&create_dialog);
            }
        } else if (create_dialog.active) {
            int result = kui_input_dialog_handle(&create_dialog, &event);

            if (result == KUI_INPUT_DIALOG_ACCEPT) {
                char path[FINDER_PATH_MAX];
                const char *name = create_dialog.value;
                int created = -1;

                if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 ||
                    strchr(name, '/') != NULL || strchr(name, '\\') != NULL) {
                    kui_input_dialog_set_error(&create_dialog,
                                               "Invalid name");
                } else if (child_path(path, state.path, name) < 0) {
                    kui_input_dialog_set_error(&create_dialog,
                                               "Path is too long");
                } else if (path_exists(path)) {
                    kui_input_dialog_set_error(&create_dialog,
                                               "That name already exists");
                } else if (create_kind == FINDER_CREATE_FOLDER) {
                    created = mkdir(path, 0755);
                    if (created < 0)
                        kui_input_dialog_set_error(
                            &create_dialog,
                            "Create failed (try an 8.3 name)");
                } else if (create_kind == FINDER_CREATE_TEXT_FILE) {
                    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

                    if (fd >= 0) {
                        close(fd);
                        created = 0;
                    } else {
                        kui_input_dialog_set_error(
                            &create_dialog,
                            "Create failed (try an 8.3 name)");
                    }
                }
                if (created == 0) {
                    snprintf(status_message, sizeof(status_message),
                             create_kind == FINDER_CREATE_FOLDER ?
                             "Created folder %s" : "Created %s", name);
                    kui_input_dialog_close(&create_dialog);
                    create_kind = FINDER_CREATE_NONE;
                    reload_current_directory(&state);
                    shell_action = KUI_ACTION_FILE_SAVED;
                }
            } else if (result == KUI_INPUT_DIALOG_CANCEL) {
                create_kind = FINDER_CREATE_NONE;
            }
        } else if (event.type == KUI_EVENT_REFRESH) {
            load_directory(state.path, &response);
            if (response.status == 0) state = response;
            context_kind = FINDER_CONTEXT_NONE;
        } else if (event.type == KUI_EVENT_RESIZE) {
            context_kind = FINDER_CONTEXT_NONE;
        } else if (event.type == KUI_EVENT_POINTER && event.wheel != 0) {
            scroll_row -= event.wheel * 3;
            context_kind = FINDER_CONTEXT_NONE;
        } else if (event.type == KUI_EVENT_POINTER && event.button == 2) {
            if (event.y < 58) {
                context_kind = FINDER_CONTEXT_NONE;
            } else {
                context_item = finder_item_at(&state, event.width,
                                              event.height, scroll_row,
                                              event.x, event.y);
                context_kind = context_item >= 0 ? FINDER_CONTEXT_ITEM :
                                                   FINDER_CONTEXT_BACKGROUND;
                context_x = event.x;
                context_y = event.y;
                if (context_x + CONTEXT_MENU_WIDTH + 5 > event.width)
                    context_x = event.width - CONTEXT_MENU_WIDTH - 5;
                if (context_y + CONTEXT_MENU_HEIGHT + 5 > event.height)
                    context_y = event.height - CONTEXT_MENU_HEIGHT - 5;
                if (context_x < 2) context_x = 2;
                if (context_y < 2) context_y = 2;
            }
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1 &&
                   context_kind != FINDER_CONTEXT_NONE) {
            int menu_kind = context_kind;
            int menu_item = context_item;
            int row = context_menu_row(context_x, context_y,
                                       event.x, event.y);

            context_kind = FINDER_CONTEXT_NONE;
            if (row >= 0 && menu_kind == FINDER_CONTEXT_ITEM &&
                menu_item >= 0 && menu_item < state.item_count) {
                char path[FINDER_PATH_MAX];

                path[0] = '\0';
                if (row == 0) {
                    activate_item(&state, menu_item, history,
                                  &history_count, &history_index,
                                  &scroll_row, &shell_action, action_path);
                    status_message[0] = '\0';
                } else if (row == 1) {
                    if (child_path(path, state.path,
                                   state.item_names[menu_item]) < 0) {
                        strcpy(status_message, "Path is too long");
                    } else if (state.item_types[menu_item] ==
                               FINDER_ITEM_DIRECTORY) {
                        strcpy(status_message,
                               "Folder copy is not supported yet");
                    } else {
                        copy_path(action_path, path);
                        shell_action = KUI_ACTION_COPY_PATH;
                        snprintf(status_message, sizeof(status_message),
                                 "Copied %s", state.item_names[menu_item]);
                    }
                } else if (row == 2 &&
                           child_path(path, state.path,
                                      state.item_names[menu_item]) == 0) {
                    int result = state.item_types[menu_item] ==
                                 FINDER_ITEM_DIRECTORY ?
                                 remove_empty_directory(path) : unlink(path);

                    if (result == 0) {
                        snprintf(status_message, sizeof(status_message),
                                 "Deleted %s", state.item_names[menu_item]);
                        reload_current_directory(&state);
                        shell_action = KUI_ACTION_FILE_SAVED;
                    } else if (state.item_types[menu_item] ==
                               FINDER_ITEM_DIRECTORY) {
                        strcpy(status_message,
                               "Delete failed: folder must be empty");
                    } else {
                        strcpy(status_message, "Delete failed");
                    }
                }
            } else if (row >= 0 &&
                       menu_kind == FINDER_CONTEXT_BACKGROUND) {
                char path[FINDER_PATH_MAX];
                int result = -1;

                path[0] = '\0';
                if (row == 0 && make_new_path(path, state.path, 1) == 0) {
                    create_kind = FINDER_CREATE_FOLDER;
                    kui_input_dialog_open(&create_dialog, "New Folder",
                                          "Folder name:", base_name(path),
                                          FINDER_NAME_MAX - 1);
                } else if (row == 1 &&
                           make_new_path(path, state.path, 0) == 0) {
                    create_kind = FINDER_CREATE_TEXT_FILE;
                    kui_input_dialog_open(&create_dialog, "New Text File",
                                          "File name:", base_name(path),
                                          FINDER_NAME_MAX - 1);
                } else if (row == 2 && event.clipboard[0] != '\0') {
                    struct stat source_stat;

                    if (stat(event.clipboard, &source_stat) < 0) {
                        strcpy(status_message, "Copied file no longer exists");
                    } else if (source_stat.st_mode == FINDER_ITEM_DIRECTORY) {
                        strcpy(status_message,
                               "Folder copy is not supported yet");
                    } else if (make_copy_path(path, state.path,
                                              event.clipboard) < 0) {
                        strcpy(status_message, "Could not choose a copy name");
                    } else if (copy_file_contents(event.clipboard, path) < 0) {
                        strcpy(status_message, "Paste failed");
                    } else {
                        result = 0;
                        snprintf(status_message, sizeof(status_message),
                                 "Pasted %s", base_name(path));
                    }
                }
                if (result == 0) {
                    reload_current_directory(&state);
                    shell_action = KUI_ACTION_FILE_SAVED;
                }
            }
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1 &&
                   event.y < 31) {
            if (event.x < 62 && state.can_back && history_index > 0) {
                load_directory(history[history_index - 1], &response);
                if (response.status == 0) { history_index--; state = response; }
            } else if (event.x < 136 && state.can_forward &&
                       history_index + 1 < history_count) {
                load_directory(history[history_index + 1], &response);
                if (response.status == 0) { history_index++; state = response; }
            } else if (event.x < 180 && state.can_up) {
                char path[FINDER_PATH_MAX];
                parent_path(state.path, path);
                load_directory(path, &response);
                if (response.status == 0) {
                    history_push(history, &history_count, &history_index,
                                 response.path);
                    state = response;
                }
            }
            scroll_row = 0;
            context_kind = FINDER_CONTEXT_NONE;
            status_message[0] = '\0';
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1) {
            int item = finder_item_at(&state, event.width, event.height,
                                      scroll_row, event.x, event.y);
            activate_item(&state, item, history, &history_count,
                          &history_index, &scroll_row, &shell_action,
                          action_path);
            status_message[0] = '\0';
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   event.key_extended &&
                   (event.key_code == 73 || event.key_code == 81)) {
            scroll_row += event.key_code == 73 ?
                -finder_visible_rows(event.height) :
                 finder_visible_rows(event.height);
        }
        state.can_back = history_index > 0;
        state.can_forward = history_index + 1 < history_count;
        state.can_up = state.path[0] != '\0' && strcmp(state.path, "/") != 0;
        if (scroll_row < 0) scroll_row = 0;
        if (scroll_row > finder_max_scroll(&state, event.width, event.height))
            scroll_row = finder_max_scroll(&state, event.width, event.height);
        kui_client_begin(&client, &event,
                         state.title[0] != '\0' ? state.title : "Finder");
        kui_client_set_constraints(&client, 360, 220, 700, 481);
        finder_render(&client, &event, &state, scroll_row,
                      context_kind, context_item, context_x, context_y,
                      status_message);
        kui_input_dialog_render(&client.surface, &event, &create_dialog);
        if (shell_action != KUI_ACTION_NONE)
            kui_client_set_action(&client, shell_action, action_path);
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
    puts("Finder is a desktop application. Run 'desktop' first.");
    return 0;
}
