#ifndef KYLEOS_KUI_CLIENT_H
#define KYLEOS_KUI_CLIENT_H

#include "extras/kui/kui.h"

struct kui_client {
    struct kui_command_buffer display;
    struct kui_surface surface;
    struct kui_response response;
};

void kui_client_init(struct kui_client *client);
void kui_client_free(struct kui_client *client);
int kui_client_read_event(struct kui_event *event);
void kui_client_begin(struct kui_client *client, const struct kui_event *event,
                      const char *title);
void kui_client_set_constraints(struct kui_client *client,
                                int minimum_width, int minimum_height,
                                int preferred_width, int preferred_height);
void kui_client_set_action(struct kui_client *client, uint32_t action,
                           const char *argument);
void kui_client_set_timer(struct kui_client *client, uint32_t milliseconds);
void kui_client_set_damage(struct kui_client *client,
                           int x, int y, int width, int height);
void kui_client_set_no_display(struct kui_client *client);
int kui_client_send(struct kui_client *client);
int kui_client_send_fd(struct kui_client *client, int output_fd);

#endif
