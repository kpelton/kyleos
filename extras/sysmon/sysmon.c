#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"
#include "extras/sysmon/sysmon_ipc.h"

static int append_file(char *buffer, int position, const char *path)
{
    int fd = open(path, O_RDONLY);
    int count;

    if (fd < 0) return -1;
    count = read(fd, buffer + position, SYSMON_TEXT_MAX - position - 1);
    close(fd);
    if (count < 0) return -1;
    position += count;
    buffer[position] = '\0';
    return position;
}

static void refresh(struct sysmon_response *response)
{
    int position;

    memset(response, 0, sizeof(*response));
    response->status = -1;
    position = append_file(response->text, 0, "/proc/processes");
    if (position < 0) return;
    if (position + 2 < SYSMON_TEXT_MAX) {
        response->text[position++] = '\n';
        response->text[position] = '\0';
    }
    position = append_file(response->text, position, "/proc/meminfo");
    if (position < 0) return;
    response->text_length = position;
    response->status = 0;
}

static void sysmon_render(struct kui_client *client,
                          const struct kui_event *event,
                          const struct sysmon_response *state)
{
    int position = 0;
    int row = 0;
    int maximum_rows = (event->height - 27) / 16;
    int maximum_columns = (event->width - 20) / 8;

    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  KUI_WHITE);
    while (position < state->text_length && row < maximum_rows) {
        char line[96];
        int length = 0;
        uint32_t color = KUI_BLACK;

        while (position < state->text_length && state->text[position] != '\n') {
            if (length < maximum_columns && length < (int)sizeof(line) - 1)
                line[length++] = state->text[position];
            position++;
        }
        if (position < state->text_length) position++;
        line[length] = '\0';
        if (strncmp(line, "Processes:", 10) == 0 ||
            strncmp(line, "MemTotal:", 9) == 0)
            color = KUI_TITLE_BLUE;
        else if (strncmp(line, "PID  ", 5) == 0)
            color = 0x00316ac5U;
        kui_draw_mono_text(&client->surface, 9, 5 + row * 16,
                           line, color, KUI_WHITE, 0);
        row++;
    }
    kui_draw_mono_text(&client->surface, 9, event->height - 19,
                       "Live data from /proc - refreshes every second",
                       0x00545c68U, KUI_WHITE, 0);
}

static int embedded_main(void)
{
    struct sysmon_response state;
    struct kui_client client;
    struct kui_event event;

    memset(&state, 0, sizeof(state));
    kui_client_init(&client);
    for (;;) {
        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        if (event.type == KUI_EVENT_INIT || event.type == KUI_EVENT_TIMER ||
            event.type == KUI_EVENT_REFRESH)
            refresh(&state);
        kui_client_begin(&client, &event, "System Monitor");
        kui_client_set_constraints(&client, 360, 220, 599, 441);
        kui_client_set_timer(&client, 1000);
        sysmon_render(&client, &event, &state);
        if (kui_client_send(&client) < 0) break;
    }
    kui_client_free(&client);
    return 0;
}

int main(int argc, char **argv)
{
    struct sysmon_response response;

    if (argc > 1 && (strcmp(argv[1], "--embedded") == 0 ||
                     strcmp(argv[1], "--kui-client") == 0))
        return embedded_main();
    refresh(&response);
    if (response.status < 0) {
        fprintf(stderr, "sysmon: unable to read /proc\n");
        return 1;
    }
    kui_write_all(1, response.text, response.text_length);
    return 0;
}
