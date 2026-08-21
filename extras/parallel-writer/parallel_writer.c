#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"

#define WRITER_LINES 20
#define WRITER_LINE_LENGTH 56

#ifndef WRITER_SIDE
#define WRITER_SIDE 0
#endif

struct writer_state {
    char lines[WRITER_LINES][WRITER_LINE_LENGTH];
    int line_count;
    int sequence;
    int launched_peer;
};

static const char *left_messages[] = {
    "indexing city blocks", "writing road graph", "updating population",
    "saving simulation state", "drawing residential zone"
};

static const char *right_messages[] = {
    "streaming compositor frame", "painting dock layer", "servicing timer",
    "rendering window damage", "flushing independent output"
};

static void append_line(struct writer_state *state)
{
    const char **messages = WRITER_SIDE == 0 ? left_messages : right_messages;
    int message_count = 5;
    int slot;

    if (state->line_count < WRITER_LINES)
        slot = state->line_count++;
    else {
        memmove(state->lines[0], state->lines[1],
                (WRITER_LINES - 1) * WRITER_LINE_LENGTH);
        slot = WRITER_LINES - 1;
    }
    snprintf(state->lines[slot], WRITER_LINE_LENGTH,
             "[%04d] %s... done", state->sequence,
             messages[state->sequence % message_count]);
    state->sequence++;
}

static void render_writer(struct kui_client *client,
                          const struct kui_event *event,
                          const struct writer_state *state)
{
    const uint32_t accent = WRITER_SIDE == 0 ? 0x004dc3ffU : 0x006fe38bU;
    char heading[64];
    int first = state->line_count > 16 ? state->line_count - 16 : 0;

    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  0x0011171dU);
    kui_fill_rect(&client->surface, 0, 0, event->width, 38, 0x00212d38U);
    snprintf(heading, sizeof(heading), "WRITER %c   PID stream %04d",
             WRITER_SIDE == 0 ? 'A' : 'B', state->sequence);
    kui_draw_mono_text(&client->surface, 14, 11, heading,
                       accent, 0x00212d38U, 0);
    for (int index = first, row = 0;
         index < state->line_count && row < 16; index++, row++)
        kui_draw_mono_text(
            &client->surface, 14, 52 + row * 20, state->lines[index],
            index + 1 == state->line_count ? accent : 0x00d5dce3U,
            0x0011171dU, 0);
    kui_fill_rect(&client->surface, 14, event->height - 24,
                  event->width - 28, 8, 0x00303b45U);
    kui_fill_rect(&client->surface, 14, event->height - 24,
                  (event->width - 28) * (state->sequence % 20) / 19,
                  8, accent);
}

int main(void)
{
    struct writer_state state;
    struct kui_client client;
    struct kui_event event;

    memset(&state, 0, sizeof(state));
    kui_client_init(&client);
    for (;;) {
        int launch_peer = 0;

        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        if (event.type == KUI_EVENT_INIT) {
            append_line(&state);
        } else if (event.type == KUI_EVENT_TIMER) {
            append_line(&state);
#if WRITER_SIDE == 0
            if (!state.launched_peer) {
                state.launched_peer = 1;
                launch_peer = 1;
            }
#endif
        }
        kui_client_begin(&client, &event,
                         WRITER_SIDE == 0 ? "Parallel Writer A" :
                                            "Parallel Writer B");
        kui_client_set_constraints(&client, 400, 320, 500, 440);
        kui_client_set_timer(&client, WRITER_SIDE == 0 ? 140 : 190);
        render_writer(&client, &event, &state);
        if (launch_peer)
            kui_client_set_action(&client, KUI_ACTION_LAUNCH,
                                  "/bin/parallel-writer-b");
        if (kui_client_send(&client) < 0) break;
    }
    kui_client_free(&client);
    return 0;
}
