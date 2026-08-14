#ifndef KYLEOS_KUI_FILE_DIALOG_H
#define KYLEOS_KUI_FILE_DIALOG_H

#include <stddef.h>
#include <stdint.h>

#include "extras/kui/kui.h"

#define KUI_FILE_DIALOG_PATH_MAX 128
#define KUI_FILE_DIALOG_NAME_MAX 32
#define KUI_FILE_DIALOG_STATUS_MAX 96
#define KUI_FILE_DIALOG_MAX_ITEMS 48

#define KUI_FILE_DIALOG_DIRECTORY 40000

enum kui_file_dialog_result {
    KUI_FILE_DIALOG_NONE,
    KUI_FILE_DIALOG_ACCEPT,
    KUI_FILE_DIALOG_CANCEL
};

enum kui_file_dialog_mode {
    KUI_FILE_DIALOG_OPEN,
    KUI_FILE_DIALOG_SAVE
};

struct kui_file_dialog_item {
    char name[KUI_FILE_DIALOG_NAME_MAX];
    int type;
};

struct kui_file_dialog {
    int active;
    int mode;
    int confirm_overwrite;
    int item_count;
    int selected_item;
    int scroll_row;
    int filename_selected;
    char directory[KUI_FILE_DIALOG_PATH_MAX];
    char filename[KUI_FILE_DIALOG_NAME_MAX];
    char status[KUI_FILE_DIALOG_STATUS_MAX];
    struct kui_file_dialog_item items[KUI_FILE_DIALOG_MAX_ITEMS];
};

void kui_file_dialog_open(struct kui_file_dialog *dialog, int mode,
                          const char *current_path);
void kui_file_dialog_close(struct kui_file_dialog *dialog);
void kui_file_dialog_set_error(struct kui_file_dialog *dialog,
                               const char *message);
int kui_file_dialog_handle(struct kui_file_dialog *dialog,
                           const struct kui_event *event,
                           char *selected_path, size_t selected_path_size);
void kui_file_dialog_render(struct kui_surface *surface,
                            const struct kui_event *event,
                            const struct kui_file_dialog *dialog);

#endif
