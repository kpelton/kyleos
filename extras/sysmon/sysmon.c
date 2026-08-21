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

static int sysmon_total_rows(const struct sysmon_response *state)
{
    int rows = 1;

    for (int i = 0; i < state->text_length; i++)
        if (state->text[i] == '\n') rows++;
    return rows;
}

static int sysmon_visible_rows(int height)
{
    int rows = (height - 27) / 16;

    return rows > 0 ? rows : 1;
}

static void sysmon_render(struct kui_client *client,
                          const struct kui_event *event,
                          const struct sysmon_response *state, int scroll,
                          const struct kui_scrollbar_state *scrollbar)
{
    int position = 0;
    int row = 0;
    int source_line = 0;
    int maximum_rows = sysmon_visible_rows(event->height);
    int maximum_columns =
        (event->width - 20 - KUI_SCROLLBAR_SIZE) / 8;

    if (maximum_columns < 1) maximum_columns = 1;

    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  KUI_WHITE);
    while (position < state->text_length && row < maximum_rows) {
        char text_line[96];
        int length = 0;
        uint32_t color = KUI_BLACK;

        while (position < state->text_length && state->text[position] != '\n') {
            if (length < maximum_columns &&
                length < (int)sizeof(text_line) - 1)
                text_line[length++] = state->text[position];
            position++;
        }
        if (position < state->text_length) position++;
        if (source_line++ < scroll) continue;
        text_line[length] = '\0';
        if (strncmp(text_line, "Processes:", 10) == 0 ||
            strncmp(text_line, "MemTotal:", 9) == 0)
            color = KUI_TITLE_BLUE;
        else if (strncmp(text_line, "PID  ", 5) == 0)
            color = 0x00316ac5U;
        kui_draw_mono_text(&client->surface, 9, 5 + row * 16,
                           text_line, color, KUI_WHITE, 0);
        row++;
    }
    kui_draw_mono_text(&client->surface, 9, event->height - 19,
                       "Live data from /proc - refreshes every second",
                       0x00545c68U, KUI_WHITE, 0);
    kui_draw_vscrollbar(&client->surface,
                        event->width - KUI_SCROLLBAR_SIZE, 0,
                        event->height - 23, scroll, maximum_rows,
                        sysmon_total_rows(state), scrollbar);
}

static int embedded_main(void)
{
    struct sysmon_response state;
    struct kui_scrollbar_state scrollbar;
    struct kui_client client;
    struct kui_event event;
    int scroll = 0;

    memset(&state, 0, sizeof(state));
    memset(&scrollbar, 0, sizeof(scrollbar));
    kui_client_init(&client);
    for (;;) {
        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        scroll = kui_vscrollbar_handle(
            &scrollbar, &event, event.width - KUI_SCROLLBAR_SIZE, 0,
            event.height - 23, scroll, sysmon_visible_rows(event.height),
            sysmon_total_rows(&state));
        if (event.type == KUI_EVENT_POINTER && event.wheel != 0)
            scroll = kui_scrollbar_wheel(
                scroll, event.wheel, sysmon_visible_rows(event.height),
                sysmon_total_rows(&state));
        if (event.type == KUI_EVENT_INIT || event.type == KUI_EVENT_TIMER ||
            event.type == KUI_EVENT_REFRESH)
            refresh(&state);
        {
            int maximum = sysmon_total_rows(&state) -
                          sysmon_visible_rows(event.height);

            if (maximum < 0) maximum = 0;
            if (scroll < 0) scroll = 0;
            if (scroll > maximum) scroll = maximum;
        }
        kui_client_begin(&client, &event, "System Monitor");
        kui_client_set_constraints(&client, 360, 220, 599, 441);
        {
            uint32_t timer = kui_scrollbar_timer_ms(&scrollbar);

            if (timer == 0) timer = 1000;
            kui_client_set_timer(&client, timer);
        }
        sysmon_render(&client, &event, &state, scroll, &scrollbar);
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
