#include "doomkeys.h"
#include "doomgeneric.h"

#include "extras/doom-aqua/doom_aqua_ui.h"
#include "extras/kui/kui_client.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef KYLEOS_HOST
#include <sys/time.h>
#endif

#define DOOM_AQUA_FRAME_MS 28U
#define DOOM_AQUA_STARTUP_MS 5000U
#define DOOM_AQUA_KEY_CAPACITY 128U

struct doom_aqua_key {
    int pressed;
    unsigned char key;
};

static struct doom_aqua_key key_queue[DOOM_AQUA_KEY_CAPACITY];
static unsigned int key_head;
static unsigned int key_count;
static int frame_ready;
static char window_title[KUI_PROTOCOL_TITLE_MAX] = "Doom";
static int protocol_output = 1;
static uint32_t timer_pause_started;
static uint32_t timer_pause_total;
static int timer_paused;

static uint32_t raw_ticks_ms(void)
{
#ifdef KYLEOS_HOST
    struct timeval now;

    gettimeofday(&now, NULL);
    return (uint32_t)((uint64_t)now.tv_sec * 1000U +
                      (uint32_t)now.tv_usec / 1000U);
#else
    return kui_ticks_ms();
#endif
}

static int separate_protocol_output(void)
{
    int reserved[2];
    int log_fd;

    if (pipe(reserved) < 0) return -1;
    protocol_output = reserved[0];
    close(reserved[1]);
    if (dup2(1, protocol_output) < 0) {
        close(protocol_output);
        protocol_output = 1;
        return -1;
    }
    log_fd = open("/tmp/doom-aqua.log",
                  O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd < 0) log_fd = open("/dev/null", O_WRONLY);
    if (log_fd < 0 || dup2(log_fd, 1) < 0 || dup2(log_fd, 2) < 0) {
        if (log_fd >= 0) close(log_fd);
        close(protocol_output);
        protocol_output = 1;
        return -1;
    }
    if (log_fd != 1 && log_fd != 2) close(log_fd);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    return 0;
}

static void queue_key_event(const struct kui_event *event)
{
    unsigned char key = doom_aqua_convert_key(event->key_code,
                                               event->key_extended);
    unsigned int tail;

    if (key == 0) return;
    if (key_count == DOOM_AQUA_KEY_CAPACITY) {
        key_head = (key_head + 1U) % DOOM_AQUA_KEY_CAPACITY;
        key_count--;
    }
    tail = (key_head + key_count) % DOOM_AQUA_KEY_CAPACITY;
    key_queue[tail].pressed = event->key_pressed != 0;
    key_queue[tail].key = key;
    key_count++;
}

static int send_loading(struct kui_client *client,
                        const struct kui_event *event)
{
    const char *message = "Loading Doom...";
    int x = (event->width - kui_text_width(message)) / 2;
    int y = event->height / 2 - 8;

    kui_client_begin(client, event, "Doom");
    kui_client_set_constraints(client, 320, 200, 640, 400);
    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  KUI_BLACK);
    kui_draw_text(&client->surface, x, y, message,
                  KUI_WHITE, KUI_BLACK, 0);
    kui_client_set_timer(client, DOOM_AQUA_STARTUP_MS);
    return kui_client_send_fd(client, protocol_output);
}

void DG_Init(void)
{
}

void DG_DrawFrame(void)
{
    frame_ready = 1;
}

void DG_SleepMs(uint32_t milliseconds)
{
    kui_sleep_ms(milliseconds);
}

uint32_t DG_GetTicksMs(void)
{
    uint32_t now = timer_paused ? timer_pause_started : raw_ticks_ms();

    return now - timer_pause_total;
}

int DG_GetKey(int *pressed, unsigned char *doom_key)
{
    if (key_count == 0 || pressed == NULL || doom_key == NULL) return 0;
    *pressed = key_queue[key_head].pressed;
    *doom_key = key_queue[key_head].key;
    key_head = (key_head + 1U) % DOOM_AQUA_KEY_CAPACITY;
    key_count--;
    return 1;
}

void DG_SetWindowTitle(const char *title)
{
    if (title == NULL || title[0] == '\0') return;
    strncpy(window_title, title, sizeof(window_title) - 1);
    window_title[sizeof(window_title) - 1] = '\0';
}

int main(int argc, char **argv)
{
    static char *default_argv[] = {
        "doom-aqua", "-iwad", "/usr/share/doom/doom.wad", NULL
    };
    struct kui_client client;
    struct kui_event event;
    int first_event = 1;

    if (separate_protocol_output() < 0) return 1;
    kui_client_init(&client);
    if (kui_client_read_event(&event) < 0 || event.type != KUI_EVENT_INIT ||
        send_loading(&client, &event) < 0) {
        kui_client_free(&client);
        return 1;
    }

    if (argc == 1 ||
        (argc == 2 && strcmp(argv[1], "--kui-client") == 0)) {
        argc = 3;
        argv = default_argv;
    }
    doomgeneric_Create(argc, argv);
    puts("doom-aqua: engine initialization complete");
    timer_pause_started = raw_ticks_ms();
    timer_paused = 1;

    while (kui_client_read_event(&event) == 0) {
        int redraw = frame_ready || event.type == KUI_EVENT_TIMER ||
                     event.type == KUI_EVENT_RESIZE ||
                     event.type == KUI_EVENT_REFRESH;

        if (event.type == KUI_EVENT_CLOSE) break;
        if (timer_paused) {
            timer_pause_total += raw_ticks_ms() - timer_pause_started;
            timer_paused = 0;
        }
        if (first_event)
            fprintf(stdout, "doom-aqua: first event type=%u\n", event.type);
        if (event.type == KUI_EVENT_KEY) queue_key_event(&event);
        if (event.type == KUI_EVENT_TIMER) {
            if (first_event) puts("doom-aqua: first tick begin");
            doomgeneric_Tick();
            if (first_event) puts("doom-aqua: first tick complete");
        }

        kui_client_begin(&client, &event, window_title);
        kui_client_set_constraints(&client, 320, 200, 640, 400);
        if (redraw) {
            doom_aqua_draw_frame(&client.surface, DG_ScreenBuffer,
                                 DOOMGENERIC_RESX, DOOMGENERIC_RESY);
            frame_ready = 0;
        } else {
            kui_client_set_no_display(&client);
        }
        if (event.type == KUI_EVENT_TIMER)
            kui_client_set_timer(&client, DOOM_AQUA_FRAME_MS);
        if (first_event)
            fprintf(stdout, "doom-aqua: first frame bytes=%u\n",
                    client.display.length);
        if (kui_client_send_fd(&client, protocol_output) < 0) {
            puts("doom-aqua: KUI response failed");
            break;
        }
        if (first_event) puts("doom-aqua: first frame sent");
        first_event = 0;
    }
    puts("doom-aqua: event loop ended");
    kui_client_free(&client);
    close(protocol_output);
    return 0;
}
