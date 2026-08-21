#include "extras/kui/kui_file_dialog.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct kyleos_dirent {
    uint64_t inode;
    char name[257];
    int type;
    uint64_t size;
};

struct dialog_layout {
    int x;
    int y;
    int width;
    int height;
    int list_x;
    int list_y;
    int list_width;
    int list_height;
    int filename_x;
    int filename_y;
    int filename_width;
    int save_x;
    int cancel_x;
    int button_y;
};

static int get_directory_entries(int fd, struct kyleos_dirent *entries,
                                 int count)
{
    register long result asm("rax") = 16;

    asm volatile("int $0x80" : "+a"(result)
                 : "D"((long)fd), "S"(entries), "d"((long)count)
                 : "memory");
    return (int)result;
}

static void copy_text(char *destination, size_t size, const char *source)
{
    size_t length;

    if (size == 0) return;
    if (source == NULL) source = "";
    length = strlen(source);
    if (length >= size) length = size - 1;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

static int child_path(char *destination, size_t size, const char *directory,
                      const char *name)
{
    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);
    int separator = strcmp(directory, "/") == 0 ? 0 : 1;

    if (directory_length + separator + name_length >= size) return -1;
    memcpy(destination, directory, directory_length);
    if (separator) destination[directory_length++] = '/';
    memcpy(destination + directory_length, name, name_length);
    destination[directory_length + name_length] = '\0';
    return 0;
}

static void parent_path(char path[KUI_FILE_DIALOG_PATH_MAX])
{
    char *slash;

    if (strcmp(path, "/") == 0) return;
    slash = strrchr(path, '/');
    if (slash == path) path[1] = '\0';
    else if (slash != NULL) *slash = '\0';
    else strcpy(path, "/");
}

static int path_exists(const char *path)
{
    int fd = open(path, O_RDONLY);

    if (fd < 0) return 0;
    close(fd);
    return 1;
}

static void load_directory(struct kui_file_dialog *dialog)
{
    struct kyleos_dirent entries[64];
    int fd;
    int count;

    dialog->item_count = 0;
    dialog->selected_item = -1;
    dialog->scroll_row = 0;
    dialog->status[0] = '\0';
    fd = open(dialog->directory, O_RDONLY);
    if (fd < 0) {
        copy_text(dialog->status, sizeof(dialog->status),
                  "Could not open this folder");
        return;
    }
    count = get_directory_entries(fd, entries,
                                  (int)(sizeof(entries) / sizeof(entries[0])));
    close(fd);
    if (count < 0) {
        copy_text(dialog->status, sizeof(dialog->status),
                  "Could not read this folder");
        return;
    }
    for (int i = 0; i < count &&
         dialog->item_count < KUI_FILE_DIALOG_MAX_ITEMS; i++) {
        struct kui_file_dialog_item *item;

        if (strcmp(entries[i].name, ".") == 0 ||
            strcmp(entries[i].name, "..") == 0)
            continue;
        item = &dialog->items[dialog->item_count++];
        copy_text(item->name, sizeof(item->name), entries[i].name);
        item->type = entries[i].type;
    }
}

static void dialog_layout(const struct kui_event *event,
                          struct dialog_layout *layout)
{
    layout->width = event->width - 30;
    if (layout->width > 540) layout->width = 540;
    if (layout->width < 410) layout->width = 410;
    layout->height = event->height - 30;
    if (layout->height > 360) layout->height = 360;
    if (layout->height < 270) layout->height = 270;
    layout->x = (event->width - layout->width) / 2;
    layout->y = (event->height - layout->height) / 2;
    layout->list_x = layout->x + 12;
    layout->list_y = layout->y + 62;
    layout->list_width = layout->width - 24;
    layout->list_height = layout->height - 142;
    layout->filename_x = layout->x + 82;
    layout->filename_y = layout->y + layout->height - 70;
    layout->filename_width = layout->width - 94;
    layout->save_x = layout->x + layout->width - 174;
    layout->cancel_x = layout->x + layout->width - 90;
    layout->button_y = layout->y + layout->height - 35;
}

static int visible_items(const struct dialog_layout *layout)
{
    int count = layout->list_height / 20;
    return count > 0 ? count : 1;
}

static int maximum_scroll(const struct kui_file_dialog *dialog,
                          const struct dialog_layout *layout)
{
    int maximum = dialog->item_count - visible_items(layout);
    return maximum > 0 ? maximum : 0;
}

static int finish_selection(struct kui_file_dialog *dialog,
                            char *selected_path,
                            size_t selected_path_size,
                            int skip_confirmation)
{
    char path[KUI_FILE_DIALOG_PATH_MAX];

    if (dialog->filename[0] == '\0') {
        copy_text(dialog->status, sizeof(dialog->status),
                  "Type a file name first");
        return KUI_FILE_DIALOG_NONE;
    }
    if (strchr(dialog->filename, '/') != NULL ||
        strchr(dialog->filename, '\\') != NULL) {
        copy_text(dialog->status, sizeof(dialog->status),
                  "Invalid file name");
        return KUI_FILE_DIALOG_NONE;
    }
    if (child_path(path, sizeof(path), dialog->directory,
                   dialog->filename) < 0) {
        copy_text(dialog->status, sizeof(dialog->status), "Path is too long");
        return KUI_FILE_DIALOG_NONE;
    }
    if (dialog->mode == KUI_FILE_DIALOG_OPEN) {
        struct stat file_status;

        if (stat(path, &file_status) < 0) {
            copy_text(dialog->status, sizeof(dialog->status),
                      "File not found");
            return KUI_FILE_DIALOG_NONE;
        }
        if (file_status.st_mode == KUI_FILE_DIALOG_DIRECTORY) {
            copy_text(dialog->directory, sizeof(dialog->directory), path);
            load_directory(dialog);
            return KUI_FILE_DIALOG_NONE;
        }
    } else if (!skip_confirmation && path_exists(path)) {
        dialog->confirm_overwrite = 1;
        return KUI_FILE_DIALOG_NONE;
    }
    copy_text(selected_path, selected_path_size, path);
    dialog->confirm_overwrite = 0;
    return KUI_FILE_DIALOG_ACCEPT;
}

void kui_file_dialog_open(struct kui_file_dialog *dialog, int mode,
                          const char *current_path)
{
    const char *slash;

    memset(dialog, 0, sizeof(*dialog));
    dialog->active = 1;
    dialog->mode = mode;
    dialog->selected_item = -1;
    dialog->filename_selected = 1;
    if (current_path == NULL || current_path[0] == '\0') {
        strcpy(dialog->directory, "/");
        strcpy(dialog->filename, "new.txt");
    } else {
        slash = strrchr(current_path, '/');
        if (slash == current_path) {
            strcpy(dialog->directory, "/");
            copy_text(dialog->filename, sizeof(dialog->filename), slash + 1);
        } else if (slash != NULL) {
            size_t length = (size_t)(slash - current_path);

            if (length >= sizeof(dialog->directory))
                length = sizeof(dialog->directory) - 1;
            memcpy(dialog->directory, current_path, length);
            dialog->directory[length] = '\0';
            copy_text(dialog->filename, sizeof(dialog->filename), slash + 1);
        } else {
            strcpy(dialog->directory, "/");
            copy_text(dialog->filename, sizeof(dialog->filename), current_path);
        }
    }
    if (dialog->filename[0] == '\0') strcpy(dialog->filename, "new.txt");
    load_directory(dialog);
}

void kui_file_dialog_close(struct kui_file_dialog *dialog)
{
    dialog->active = 0;
    dialog->confirm_overwrite = 0;
}

void kui_file_dialog_set_error(struct kui_file_dialog *dialog,
                               const char *message)
{
    dialog->confirm_overwrite = 0;
    copy_text(dialog->status, sizeof(dialog->status), message);
}

int kui_file_dialog_handle(struct kui_file_dialog *dialog,
                           const struct kui_event *event,
                           char *selected_path, size_t selected_path_size)
{
    struct dialog_layout layout;
    int maximum;

    if (!dialog->active) return KUI_FILE_DIALOG_NONE;
    dialog_layout(event, &layout);
    if (dialog->confirm_overwrite) {
        int box_x = (event->width - 300) / 2;
        int box_y = (event->height - 112) / 2;

        if (event->type == KUI_EVENT_KEY && event->key_pressed) {
            if (!event->key_extended && event->key_code == 28)
                return finish_selection(dialog, selected_path,
                                        selected_path_size, 1);
            if (!event->key_extended && event->key_code == 1)
                dialog->confirm_overwrite = 0;
        } else if (event->type == KUI_EVENT_POINTER && event->button == 1) {
            if (kui_in_rect(event->x, event->y, box_x + 124, box_y + 75,
                            70, 25))
                return finish_selection(dialog, selected_path,
                                        selected_path_size, 1);
            if (kui_in_rect(event->x, event->y, box_x + 208, box_y + 75,
                            70, 25))
                dialog->confirm_overwrite = 0;
        }
        return KUI_FILE_DIALOG_NONE;
    }
    if (event->type == KUI_EVENT_POINTER && event->wheel != 0 &&
        kui_in_rect(event->x, event->y, layout.list_x, layout.list_y,
                    layout.list_width, layout.list_height)) {
        dialog->scroll_row -= event->wheel * 3;
    } else if (event->type == KUI_EVENT_POINTER && event->button == 1) {
        if (kui_in_rect(event->x, event->y,
                        layout.x + layout.width - 42, layout.y + 31,
                        30, 24)) {
            parent_path(dialog->directory);
            load_directory(dialog);
        } else if (kui_in_rect(event->x, event->y,
                               layout.list_x, layout.list_y,
                               layout.list_width, layout.list_height)) {
            int item = dialog->scroll_row +
                       (event->y - layout.list_y) / 20;

            if (item >= 0 && item < dialog->item_count) {
                if (dialog->items[item].type == KUI_FILE_DIALOG_DIRECTORY) {
                    char path[KUI_FILE_DIALOG_PATH_MAX];

                    if (child_path(path, sizeof(path), dialog->directory,
                                   dialog->items[item].name) == 0) {
                        copy_text(dialog->directory,
                                  sizeof(dialog->directory), path);
                        load_directory(dialog);
                    }
                } else {
                    dialog->selected_item = item;
                    copy_text(dialog->filename, sizeof(dialog->filename),
                              dialog->items[item].name);
                    dialog->filename_selected = 1;
                    dialog->status[0] = '\0';
                }
            }
        } else if (kui_in_rect(event->x, event->y, layout.filename_x,
                               layout.filename_y,
                               layout.filename_width, 24)) {
            dialog->filename_selected = 0;
        } else if (kui_in_rect(event->x, event->y, layout.save_x,
                               layout.button_y, 76, 25)) {
            return finish_selection(dialog, selected_path,
                                    selected_path_size, 0);
        } else if (kui_in_rect(event->x, event->y, layout.cancel_x,
                               layout.button_y, 76, 25)) {
            kui_file_dialog_close(dialog);
            return KUI_FILE_DIALOG_CANCEL;
        }
    } else if (event->type == KUI_EVENT_KEY && event->key_pressed) {
        size_t length = strlen(dialog->filename);

        if (!event->key_extended && event->key_code == 1) {
            kui_file_dialog_close(dialog);
            return KUI_FILE_DIALOG_CANCEL;
        }
        if (!event->key_extended && event->key_code == 28)
            return finish_selection(dialog, selected_path,
                                    selected_path_size, 0);
        if (!event->key_extended && event->key_code == 14) {
            if (dialog->filename_selected) {
                dialog->filename[0] = '\0';
                dialog->filename_selected = 0;
            } else if (length > 0) {
                dialog->filename[length - 1] = '\0';
            }
            dialog->status[0] = '\0';
        } else if (event->character >= 32 && event->character < 127 &&
                   !(event->modifiers & (KUI_MOD_CTRL | KUI_MOD_ALT)) &&
                   (dialog->filename_selected ||
                    length + 1 < sizeof(dialog->filename)) &&
                   event->character != '/' && event->character != '\\') {
            if (dialog->filename_selected) {
                dialog->filename[0] = '\0';
                length = 0;
                dialog->filename_selected = 0;
            }
            dialog->filename[length] = event->character;
            dialog->filename[length + 1] = '\0';
            dialog->selected_item = -1;
            dialog->status[0] = '\0';
        }
    }
    maximum = maximum_scroll(dialog, &layout);
    if (dialog->scroll_row < 0) dialog->scroll_row = 0;
    if (dialog->scroll_row > maximum) dialog->scroll_row = maximum;
    return KUI_FILE_DIALOG_NONE;
}

static void draw_small_folder(struct kui_surface *surface, int x, int y)
{
    kui_fill_rect(surface, x + 2, y, 8, 3, 0x00e5aa24U);
    kui_fill_rect(surface, x, y + 2, 17, 12, 0x00ffd45bU);
    kui_outline_rect(surface, x, y + 2, 17, 12, 0x00956a13U);
}

static void draw_small_file(struct kui_surface *surface, int x, int y)
{
    kui_fill_rect(surface, x + 2, y, 12, 15, KUI_WHITE);
    kui_outline_rect(surface, x + 2, y, 12, 15, KUI_SHADOW);
    kui_fill_rect(surface, x + 5, y + 5, 6, 1, 0x0089a1bdU);
    kui_fill_rect(surface, x + 5, y + 8, 6, 1, 0x0089a1bdU);
}

void kui_file_dialog_render(struct kui_surface *surface,
                            const struct kui_event *event,
                            const struct kui_file_dialog *dialog)
{
    struct dialog_layout layout;
    int visible;

    if (!dialog->active) return;
    dialog_layout(event, &layout);
    if (kui_style_current()->flags & KUI_STYLE_AQUA)
        kui_draw_window(surface, layout.x, layout.y,
                        layout.width, layout.height,
                        dialog->mode == KUI_FILE_DIALOG_OPEN ?
                        "Open" : "Save As", 1);
    else {
        kui_fill_rect(surface, layout.x + 5, layout.y + 5,
                      layout.width, layout.height, 0x00505050U);
        kui_fill_rect(surface, layout.x, layout.y,
                      layout.width, layout.height, KUI_FACE);
        kui_outline_rect(surface, layout.x, layout.y,
                         layout.width, layout.height, KUI_BLACK);
        kui_fill_rect(surface, layout.x + 3, layout.y + 3,
                      layout.width - 6, 22, KUI_TITLE_BLUE);
        kui_draw_text(surface, layout.x + 8, layout.y + 8,
                      dialog->mode == KUI_FILE_DIALOG_OPEN ?
                      "Open" : "Save As", KUI_WHITE, KUI_TITLE_BLUE, 0);
    }
    kui_draw_text(surface, layout.x + 12, layout.y + 38,
                  "Save in:", KUI_BLACK, KUI_FACE, 0);
    kui_fill_rect(surface, layout.x + 76, layout.y + 31,
                  layout.width - 124, 24, KUI_WHITE);
    kui_outline_rect(surface, layout.x + 76, layout.y + 31,
                     layout.width - 124, 24, KUI_SHADOW);
    kui_draw_text(surface, layout.x + 82, layout.y + 38,
                  dialog->directory, KUI_BLACK, KUI_WHITE, 0);
    kui_draw_button(surface, layout.x + layout.width - 42,
                    layout.y + 31, 30, 24, "Up", 0);
    kui_fill_rect(surface, layout.list_x, layout.list_y,
                  layout.list_width, layout.list_height, KUI_WHITE);
    kui_outline_rect(surface, layout.list_x, layout.list_y,
                     layout.list_width, layout.list_height, KUI_SHADOW);
    visible = visible_items(&layout);
    for (int row = 0; row < visible; row++) {
        int item = dialog->scroll_row + row;
        int y;
        uint32_t background;

        if (item >= dialog->item_count) break;
        y = layout.list_y + row * 20 + 2;
        background = item == dialog->selected_item ?
                     0x000a64adU : KUI_WHITE;
        if (item == dialog->selected_item)
            kui_fill_rect(surface, layout.list_x + 2, y - 1,
                          layout.list_width - 4, 19, background);
        if (dialog->items[item].type == KUI_FILE_DIALOG_DIRECTORY)
            draw_small_folder(surface, layout.list_x + 6, y + 1);
        else
            draw_small_file(surface, layout.list_x + 6, y);
        kui_draw_text(surface, layout.list_x + 30, y + 3,
                      dialog->items[item].name,
                      item == dialog->selected_item ? KUI_WHITE : KUI_BLACK,
                      background, 0);
    }
    kui_draw_text(surface, layout.x + 12, layout.filename_y + 7,
                  "File name:", KUI_BLACK, KUI_FACE, 0);
    kui_fill_rect(surface, layout.filename_x, layout.filename_y,
                  layout.filename_width, 24, KUI_WHITE);
    kui_outline_rect(surface, layout.filename_x, layout.filename_y,
                     layout.filename_width, 24, KUI_SHADOW);
    if (dialog->filename_selected && dialog->filename[0] != '\0')
        kui_fill_rect(surface, layout.filename_x + 4, layout.filename_y + 4,
                      kui_text_width(dialog->filename) + 3, 16,
                      0x000a64adU);
    kui_draw_text(surface, layout.filename_x + 5, layout.filename_y + 7,
                  dialog->filename,
                  dialog->filename_selected ? KUI_WHITE : KUI_BLACK,
                  dialog->filename_selected ? 0x000a64adU : KUI_WHITE, 0);
    kui_fill_rect(surface,
                  layout.filename_x + 5 + kui_text_width(dialog->filename),
                  layout.filename_y + 5, 1, 15, KUI_BLACK);
    kui_draw_button(surface, layout.save_x, layout.button_y,
                    76, 25,
                    dialog->mode == KUI_FILE_DIALOG_OPEN ? "Open" : "Save",
                    0);
    kui_draw_button(surface, layout.cancel_x, layout.button_y,
                    76, 25, "Cancel", 0);
    if (dialog->status[0] != '\0')
        kui_draw_text(surface, layout.x + 12, layout.button_y + 7,
                      dialog->status, 0x00b00000U, KUI_FACE, 0);
    if (dialog->confirm_overwrite) {
        int box_x = (event->width - 300) / 2;
        int box_y = (event->height - 112) / 2;

        kui_fill_rect(surface, box_x + 4, box_y + 4, 300, 112,
                      0x00505050U);
        kui_fill_rect(surface, box_x, box_y, 300, 112, KUI_FACE);
        kui_outline_rect(surface, box_x, box_y, 300, 112, KUI_BLACK);
        kui_fill_rect(surface, box_x + 3, box_y + 3, 294, 22,
                      KUI_TITLE_BLUE);
        kui_draw_text(surface, box_x + 8, box_y + 8,
                      "Confirm Save As", KUI_WHITE, KUI_TITLE_BLUE, 0);
        kui_draw_text(surface, box_x + 16, box_y + 42,
                      "Replace the existing file?", KUI_BLACK, KUI_FACE, 0);
        kui_draw_button(surface, box_x + 124, box_y + 75,
                        70, 25, "Yes", 0);
        kui_draw_button(surface, box_x + 208, box_y + 75,
                        70, 25, "No", 0);
    }
}
