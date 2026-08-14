#ifndef KYLEOS_KUI_DIALOG_H
#define KYLEOS_KUI_DIALOG_H

#include <stddef.h>

#include "extras/kui/kui.h"

#define KUI_INPUT_DIALOG_TITLE_MAX 64
#define KUI_INPUT_DIALOG_PROMPT_MAX 64
#define KUI_INPUT_DIALOG_VALUE_MAX 64
#define KUI_INPUT_DIALOG_STATUS_MAX 96
#define KUI_CONFIRM_DIALOG_MESSAGE_MAX 128
#define KUI_CONFIRM_DIALOG_LABEL_MAX 16

enum kui_input_dialog_result {
    KUI_INPUT_DIALOG_NONE,
    KUI_INPUT_DIALOG_ACCEPT,
    KUI_INPUT_DIALOG_CANCEL
};

enum kui_confirm_dialog_result {
    KUI_CONFIRM_DIALOG_NONE,
    KUI_CONFIRM_DIALOG_ACCEPT,
    KUI_CONFIRM_DIALOG_CANCEL
};

struct kui_input_dialog {
    int active;
    int value_selected;
    size_t maximum_length;
    char title[KUI_INPUT_DIALOG_TITLE_MAX];
    char prompt[KUI_INPUT_DIALOG_PROMPT_MAX];
    char value[KUI_INPUT_DIALOG_VALUE_MAX];
    char status[KUI_INPUT_DIALOG_STATUS_MAX];
};

struct kui_confirm_dialog {
    int active;
    char title[KUI_INPUT_DIALOG_TITLE_MAX];
    char message[KUI_CONFIRM_DIALOG_MESSAGE_MAX];
    char accept_label[KUI_CONFIRM_DIALOG_LABEL_MAX];
    char cancel_label[KUI_CONFIRM_DIALOG_LABEL_MAX];
};

void kui_input_dialog_open(struct kui_input_dialog *dialog,
                           const char *title, const char *prompt,
                           const char *initial_value, size_t maximum_length);
void kui_input_dialog_close(struct kui_input_dialog *dialog);
void kui_input_dialog_set_error(struct kui_input_dialog *dialog,
                                const char *message);
int kui_input_dialog_handle(struct kui_input_dialog *dialog,
                            const struct kui_event *event);
void kui_input_dialog_render(struct kui_surface *surface,
                             const struct kui_event *event,
                             const struct kui_input_dialog *dialog);

void kui_confirm_dialog_open(struct kui_confirm_dialog *dialog,
                             const char *title, const char *message,
                             const char *accept_label,
                             const char *cancel_label);
void kui_confirm_dialog_close(struct kui_confirm_dialog *dialog);
int kui_confirm_dialog_handle(struct kui_confirm_dialog *dialog,
                              const struct kui_event *event);
void kui_confirm_dialog_render(struct kui_surface *surface,
                               const struct kui_event *event,
                               const struct kui_confirm_dialog *dialog);

#endif
