#include "extras/kui/kui_client.h"

#include <string.h>

void kui_client_init(struct kui_client *client)
{
    memset(client, 0, sizeof(*client));
    kui_command_buffer_init(&client->display);
}

void kui_client_free(struct kui_client *client)
{
    kui_command_buffer_free(&client->display);
}

int kui_client_read_event(struct kui_event *event)
{
    if (kui_read_all(0, event, sizeof(*event)) < 0) return -1;
    if (event->magic != KUI_PROTOCOL_MAGIC ||
        event->version != KUI_PROTOCOL_VERSION)
        return -1;
    if (event->style.id != 0)
        kui_style_apply(&event->style);
    event->argument[sizeof(event->argument) - 1] = '\0';
    event->clipboard[sizeof(event->clipboard) - 1] = '\0';
    return 0;
}

void kui_client_begin(struct kui_client *client, const struct kui_event *event,
                      const char *title)
{
    kui_command_buffer_reset(&client->display);
    memset(&client->response, 0, sizeof(client->response));
    client->response.magic = KUI_PROTOCOL_MAGIC;
    client->response.version = KUI_PROTOCOL_VERSION;
    client->response.serial = event->serial;
    client->response.flags = KUI_RESPONSE_DISPLAY;
    client->response.status = 0;
    if (title != NULL) {
        client->response.flags |= KUI_RESPONSE_TITLE;
        strncpy(client->response.title, title,
                sizeof(client->response.title) - 1);
    }
    kui_surface_init_commands(&client->surface, &client->display,
                              event->width, event->height);
}

void kui_client_set_constraints(struct kui_client *client,
                                int minimum_width, int minimum_height,
                                int preferred_width, int preferred_height)
{
    client->response.flags |= KUI_RESPONSE_CONSTRAINTS;
    client->response.min_width = minimum_width;
    client->response.min_height = minimum_height;
    client->response.preferred_width = preferred_width;
    client->response.preferred_height = preferred_height;
}

void kui_client_set_action(struct kui_client *client, uint32_t action,
                           const char *argument)
{
    client->response.action = action;
    if (argument != NULL) {
        strncpy(client->response.action_argument, argument,
                sizeof(client->response.action_argument) - 1);
    }
}

void kui_client_set_timer(struct kui_client *client, uint32_t milliseconds)
{
    client->response.flags |= KUI_RESPONSE_TIMER;
    client->response.timer_ms = milliseconds;
}

void kui_client_set_damage(struct kui_client *client,
                           int x, int y, int width, int height)
{
    client->response.flags |= KUI_RESPONSE_DISPLAY | KUI_RESPONSE_PARTIAL;
    client->response.damage_x = x;
    client->response.damage_y = y;
    client->response.damage_width = width;
    client->response.damage_height = height;
}

void kui_client_set_no_display(struct kui_client *client)
{
    client->response.flags &= ~(KUI_RESPONSE_DISPLAY | KUI_RESPONSE_PARTIAL);
    kui_command_buffer_reset(&client->display);
}

int kui_client_send(struct kui_client *client)
{
    if (client->display.failed) return -1;
    client->response.display_bytes = client->display.length;
    if (kui_write_all(1, &client->response, sizeof(client->response)) < 0)
        return -1;
    if (client->display.length != 0 &&
        kui_write_all(1, client->display.data, client->display.length) < 0)
        return -1;
    return 0;
}
