#include "extras/kui/kui_dialog.h"

#include <stdint.h>
#include <string.h>

struct input_dialog_layout {
    int x;
    int y;
    int width;
    int height;
    int field_x;
    int field_y;
    int field_width;
    int ok_x;
    int cancel_x;
    int button_y;
};

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

static void input_dialog_layout(const struct kui_event *event,
                                struct input_dialog_layout *layout)
{
    layout->width = event->width - 24;
    if (layout->width > 400) layout->width = 400;
    if (layout->width < 280) layout->width = 280;
    layout->height = 154;
    if (layout->height > event->height - 16)
        layout->height = event->height - 16;
    layout->x = (event->width - layout->width) / 2;
    layout->y = (event->height - layout->height) / 2;
    layout->field_x = layout->x + 12;
    layout->field_y = layout->y + 62;
    layout->field_width = layout->width - 24;
    layout->ok_x = layout->x + layout->width - 174;
    layout->cancel_x = layout->x + layout->width - 90;
    layout->button_y = layout->y + layout->height - 35;
}

void kui_input_dialog_open(struct kui_input_dialog *dialog,
                           const char *title, const char *prompt,
                           const char *initial_value, size_t maximum_length)
{
    memset(dialog, 0, sizeof(*dialog));
    dialog->active = 1;
    dialog->value_selected = 1;
    if (maximum_length == 0 || maximum_length >= sizeof(dialog->value))
        maximum_length = sizeof(dialog->value) - 1;
    dialog->maximum_length = maximum_length;
    copy_text(dialog->title, sizeof(dialog->title), title);
    copy_text(dialog->prompt, sizeof(dialog->prompt), prompt);
    copy_text(dialog->value, maximum_length + 1, initial_value);
}

void kui_input_dialog_close(struct kui_input_dialog *dialog)
{
    dialog->active = 0;
}

void kui_input_dialog_set_error(struct kui_input_dialog *dialog,
                                const char *message)
{
    copy_text(dialog->status, sizeof(dialog->status), message);
}

int kui_input_dialog_handle(struct kui_input_dialog *dialog,
                            const struct kui_event *event)
{
    struct input_dialog_layout layout;

    if (!dialog->active) return KUI_INPUT_DIALOG_NONE;
    input_dialog_layout(event, &layout);
    if (event->type == KUI_EVENT_POINTER && event->button == 1) {
        if (kui_in_rect(event->x, event->y, layout.field_x,
                        layout.field_y, layout.field_width, 24)) {
            dialog->value_selected = 0;
        } else if (kui_in_rect(event->x, event->y, layout.ok_x,
                               layout.button_y, 76, 25)) {
            if (dialog->value[0] != '\0') return KUI_INPUT_DIALOG_ACCEPT;
            copy_text(dialog->status, sizeof(dialog->status),
                      "Type a name first");
        } else if (kui_in_rect(event->x, event->y, layout.cancel_x,
                               layout.button_y, 76, 25)) {
            kui_input_dialog_close(dialog);
            return KUI_INPUT_DIALOG_CANCEL;
        }
    } else if (event->type == KUI_EVENT_KEY && event->key_pressed) {
        size_t length = strlen(dialog->value);
        size_t visible_limit = layout.field_width > 16 ?
                               (size_t)(layout.field_width - 16) / 8 : 1;
        size_t limit = dialog->maximum_length;

        if (visible_limit < limit) limit = visible_limit;
        if (!event->key_extended && event->key_code == 1) {
            kui_input_dialog_close(dialog);
            return KUI_INPUT_DIALOG_CANCEL;
        }
        if (!event->key_extended && event->key_code == 28) {
            if (dialog->value[0] != '\0') return KUI_INPUT_DIALOG_ACCEPT;
            copy_text(dialog->status, sizeof(dialog->status),
                      "Type a name first");
        } else if (!event->key_extended && event->key_code == 14) {
            if (dialog->value_selected) {
                dialog->value[0] = '\0';
                dialog->value_selected = 0;
            } else if (length > 0) {
                dialog->value[length - 1] = '\0';
            }
            dialog->status[0] = '\0';
        } else if (event->character >= 32 && event->character < 127 &&
                   !(event->modifiers & (KUI_MOD_CTRL | KUI_MOD_ALT)) &&
                   (dialog->value_selected || length < limit)) {
            if (dialog->value_selected) {
                dialog->value[0] = '\0';
                dialog->value_selected = 0;
                length = 0;
            }
            dialog->value[length] = event->character;
            dialog->value[length + 1] = '\0';
            dialog->status[0] = '\0';
        }
    }
    return KUI_INPUT_DIALOG_NONE;
}

void kui_input_dialog_render(struct kui_surface *surface,
                             const struct kui_event *event,
                             const struct kui_input_dialog *dialog)
{
    struct input_dialog_layout layout;
    uint32_t text_background;
    uint32_t text_color;

    if (!dialog->active) return;
    input_dialog_layout(event, &layout);
    kui_fill_rect(surface, layout.x + 5, layout.y + 5,
                  layout.width, layout.height, 0x00505050U);
    kui_fill_rect(surface, layout.x, layout.y,
                  layout.width, layout.height, KUI_FACE);
    kui_outline_rect(surface, layout.x, layout.y,
                     layout.width, layout.height, KUI_BLACK);
    kui_fill_rect(surface, layout.x + 3, layout.y + 3,
                  layout.width - 6, 22, KUI_TITLE_BLUE);
    kui_draw_text(surface, layout.x + 8, layout.y + 8,
                  dialog->title, KUI_WHITE, KUI_TITLE_BLUE, 0);
    kui_draw_text(surface, layout.x + 12, layout.y + 38,
                  dialog->prompt, KUI_BLACK, KUI_FACE, 0);
    kui_fill_rect(surface, layout.field_x, layout.field_y,
                  layout.field_width, 24, KUI_WHITE);
    kui_outline_rect(surface, layout.field_x, layout.field_y,
                     layout.field_width, 24, KUI_SHADOW);
    text_background = dialog->value_selected ? 0x000a64adU : KUI_WHITE;
    text_color = dialog->value_selected ? KUI_WHITE : KUI_BLACK;
    if (dialog->value_selected && dialog->value[0] != '\0')
        kui_fill_rect(surface, layout.field_x + 4, layout.field_y + 4,
                      kui_text_width(dialog->value) + 3, 16,
                      text_background);
    kui_draw_text(surface, layout.field_x + 5, layout.field_y + 7,
                  dialog->value, text_color, text_background, 0);
    if (!dialog->value_selected)
        kui_fill_rect(surface,
                      layout.field_x + 5 + kui_text_width(dialog->value),
                      layout.field_y + 5, 1, 15, KUI_BLACK);
    if (dialog->status[0] != '\0')
        kui_draw_text(surface, layout.x + 12, layout.field_y + 31,
                      dialog->status, 0x00b00000U, KUI_FACE, 0);
    kui_draw_button(surface, layout.ok_x, layout.button_y,
                    76, 25, "OK", 0);
    kui_draw_button(surface, layout.cancel_x, layout.button_y,
                    76, 25, "Cancel", 0);
}

void kui_confirm_dialog_open(struct kui_confirm_dialog *dialog,
                             const char *title, const char *message,
                             const char *accept_label,
                             const char *cancel_label)
{
    memset(dialog, 0, sizeof(*dialog));
    dialog->active = 1;
    copy_text(dialog->title, sizeof(dialog->title), title);
    copy_text(dialog->message, sizeof(dialog->message), message);
    copy_text(dialog->accept_label, sizeof(dialog->accept_label),
              accept_label != NULL ? accept_label : "OK");
    copy_text(dialog->cancel_label, sizeof(dialog->cancel_label),
              cancel_label != NULL ? cancel_label : "Cancel");
}

void kui_confirm_dialog_close(struct kui_confirm_dialog *dialog)
{
    dialog->active = 0;
}

int kui_confirm_dialog_handle(struct kui_confirm_dialog *dialog,
                              const struct kui_event *event)
{
    int width = event->width - 24;
    int height = 130;
    int x;
    int y;
    int accept_x;
    int cancel_x;
    int button_y;

    if (!dialog->active) return KUI_CONFIRM_DIALOG_NONE;
    if (width > 380) width = 380;
    if (width < 280) width = 280;
    if (height > event->height - 16) height = event->height - 16;
    x = (event->width - width) / 2;
    y = (event->height - height) / 2;
    accept_x = x + width - 174;
    cancel_x = x + width - 90;
    button_y = y + height - 35;
    if (event->type == KUI_EVENT_KEY && event->key_pressed) {
        if (!event->key_extended && event->key_code == 28)
            return KUI_CONFIRM_DIALOG_ACCEPT;
        if (!event->key_extended && event->key_code == 1) {
            kui_confirm_dialog_close(dialog);
            return KUI_CONFIRM_DIALOG_CANCEL;
        }
    } else if (event->type == KUI_EVENT_POINTER && event->button == 1) {
        if (kui_in_rect(event->x, event->y, accept_x, button_y, 76, 25))
            return KUI_CONFIRM_DIALOG_ACCEPT;
        if (kui_in_rect(event->x, event->y, cancel_x, button_y, 76, 25)) {
            kui_confirm_dialog_close(dialog);
            return KUI_CONFIRM_DIALOG_CANCEL;
        }
    }
    return KUI_CONFIRM_DIALOG_NONE;
}

void kui_confirm_dialog_render(struct kui_surface *surface,
                               const struct kui_event *event,
                               const struct kui_confirm_dialog *dialog)
{
    int width = event->width - 24;
    int height = 130;
    int x;
    int y;
    int accept_x;
    int cancel_x;
    int button_y;

    if (!dialog->active) return;
    if (width > 380) width = 380;
    if (width < 280) width = 280;
    if (height > event->height - 16) height = event->height - 16;
    x = (event->width - width) / 2;
    y = (event->height - height) / 2;
    accept_x = x + width - 174;
    cancel_x = x + width - 90;
    button_y = y + height - 35;
    kui_fill_rect(surface, x + 5, y + 5, width, height, 0x00505050U);
    kui_fill_rect(surface, x, y, width, height, KUI_FACE);
    kui_outline_rect(surface, x, y, width, height, KUI_BLACK);
    kui_fill_rect(surface, x + 3, y + 3, width - 6, 22, KUI_TITLE_BLUE);
    kui_draw_text(surface, x + 8, y + 8, dialog->title,
                  KUI_WHITE, KUI_TITLE_BLUE, 0);
    kui_draw_text(surface, x + 16, y + 45, dialog->message,
                  KUI_BLACK, KUI_FACE, 0);
    kui_draw_button(surface, accept_x, button_y, 76, 25,
                    dialog->accept_label, 0);
    kui_draw_button(surface, cancel_x, button_y, 76, 25,
                    dialog->cancel_label, 0);
}
