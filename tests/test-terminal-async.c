#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "extras/kui/kui_protocol.h"
#include "extras/kui/kui_style.h"
#ifdef KYLEOS_GUEST
#include "extras/kui/kui.h"
#endif

static int write_all(int fd, const void *buffer, size_t bytes)
{
    const unsigned char *cursor = buffer;

    while (bytes != 0) {
        ssize_t count = write(fd, cursor, bytes);

        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -1;
        cursor += count;
        bytes -= (size_t)count;
    }
    return 0;
}

static int read_all(int fd, void *buffer, size_t bytes)
{
    unsigned char *cursor = buffer;

    while (bytes != 0) {
        ssize_t count = read(fd, cursor, bytes);

        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -1;
        cursor += count;
        bytes -= (size_t)count;
    }
    return 0;
}

static long milliseconds(void)
{
    struct timeval time;

    gettimeofday(&time, NULL);
    return time.tv_sec * 1000L + time.tv_usec / 1000;
}

static int exchange(int input, int output, struct kui_event *event,
                    struct kui_response *response)
{
    unsigned char *display = NULL;

    if (write_all(input, event, sizeof(*event)) < 0 ||
        read_all(output, response, sizeof(*response)) < 0)
        return -1;
    if (response->magic != KUI_PROTOCOL_MAGIC ||
        response->version != KUI_PROTOCOL_VERSION ||
        response->serial != event->serial ||
        response->display_bytes > KUI_PROTOCOL_MAX_DISPLAY)
        return -1;
    if (response->display_bytes != 0) {
        display = malloc(response->display_bytes);
        if (display == NULL ||
            read_all(output, display, response->display_bytes) < 0) {
            free(display);
            return -1;
        }
        free(display);
    }
    return 0;
}

static int send_key(int input, int output, struct kui_event *event,
                    struct kui_response *response, int code, char character)
{
    event->type = KUI_EVENT_KEY;
    event->serial++;
    event->key_code = code;
    event->key_extended = 0;
    event->key_pressed = 1;
    event->modifiers = 0;
    event->character = character;
    return exchange(input, output, event, response);
}

static int send_text(int input, int output, struct kui_event *event,
                     struct kui_response *response, const char *text)
{
    for (size_t index = 0; text[index] != '\0'; index++)
        if (send_key(input, output, event, response, 0, text[index]) < 0)
            return -1;
    return 0;
}

static int poll_until_idle(int input, int output, struct kui_event *event,
                           struct kui_response *response, int attempts)
{
    for (int attempt = 0; attempt < attempts; attempt++) {
#ifdef KYLEOS_GUEST
        kui_sleep_ms(20);
#else
        usleep(20000);
#endif
        event->type = KUI_EVENT_TIMER;
        event->serial++;
        event->key_pressed = 0;
        event->character = 0;
        if (exchange(input, output, event, response) < 0) {
            fprintf(stderr, "terminal timer exchange failed at attempt %d\n",
                    attempt + 1);
            return -1;
        }
        if ((response->flags & KUI_RESPONSE_TIMER) != 0 &&
            response->timer_ms == 0)
            return 0;
    }
    fprintf(stderr, "terminal timer stayed active for %d attempts\n",
            attempts);
    return -1;
}

static void check_result(int failed, const char *name, int *failures)
{
    if (!failed) return;
    fprintf(stderr, "terminal async check failed: %s\n", name);
    (*failures)++;
}

int main(int argc, char **argv)
{
    const char *terminal_path;
#ifdef KYLEOS_GUEST
    terminal_path = "/bin/kterm";
#else
    const char *source_path = "/tmp/os3-terminal-hello.c";
    const char *binary_path = "/tmp/os3-terminal-hello";
    FILE *source;
#endif
    int to_child[2];
    int from_child[2];
    pid_t child;
    struct kui_event event;
    struct kui_response response;
    long started;
    long elapsed;
    int status = 0;
    int failures = 0;

    (void)argv;
#ifdef KYLEOS_GUEST
    if (argc != 1) return 2;
#else
    if (argc != 2) return 2;
    terminal_path = argv[1];
    source = fopen(source_path, "wb");
    if (source == NULL) return 2;
    fputs("int main(void) { return 0; }\n", source);
    fclose(source);
    remove(binary_path);
#endif

    if (pipe(to_child) < 0 || pipe(from_child) < 0) return 2;
    child = fork();
    if (child == 0) {
        char *child_argv[] = { (char *)terminal_path, NULL };

        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        execve(terminal_path, child_argv, NULL);
        _exit(127);
    }
    if (child < 0) return 2;
    close(to_child[0]);
    close(from_child[1]);

    memset(&event, 0, sizeof(event));
    event.magic = KUI_PROTOCOL_MAGIC;
    event.version = KUI_PROTOCOL_VERSION;
    event.type = KUI_EVENT_INIT;
    event.serial = 1;
    event.width = 700;
    event.height = 481;
    kui_style_aqua(&event.style);
    check_result(exchange(to_child[1], from_child[0], &event, &response) < 0,
                 "init exchange", &failures);

#ifdef KYLEOS_GUEST
    check_result(send_text(to_child[1], from_child[0], &event, &response,
                           "ls") < 0,
                 "type ls", &failures);
    puts("terminal async: starting ls");
    fflush(stdout);
    started = milliseconds();
    check_result(send_key(to_child[1], from_child[0], &event, &response,
                          28, '\n') < 0,
                 "start ls", &failures);
    elapsed = milliseconds() - started;
    if ((response.flags & KUI_RESPONSE_TIMER) == 0 || response.timer_ms == 0)
        check_result(1, "ls timer", &failures);
    check_result(poll_until_idle(to_child[1], from_child[0], &event,
                                 &response, 200) < 0,
                 "ls completion", &failures);
    puts("terminal async: ls completed");
    fflush(stdout);
    check_result(send_text(to_child[1], from_child[0], &event, &response,
                           "echo terminal-alive") < 0,
                 "type echo", &failures);
    check_result(send_key(to_child[1], from_child[0], &event, &response,
                          28, '\n') < 0,
                 "start echo", &failures);
    check_result(poll_until_idle(to_child[1], from_child[0], &event,
                                 &response, 200) < 0,
                 "echo completion", &failures);
#else
    check_result(send_text(to_child[1], from_child[0], &event, &response,
                           "sleep 1") < 0,
                 "type sleep", &failures);
    started = milliseconds();
    check_result(send_key(to_child[1], from_child[0], &event, &response,
                          28, '\n') < 0,
                 "start sleep", &failures);
    elapsed = milliseconds() - started;
    if (elapsed >= 500 || (response.flags & KUI_RESPONSE_TIMER) == 0 ||
        response.timer_ms == 0)
        check_result(1, "sleep response deadline/timer", &failures);
    check_result(poll_until_idle(to_child[1], from_child[0], &event,
                                 &response, 100) < 0,
                 "sleep completion", &failures);

    check_result(send_text(to_child[1], from_child[0], &event, &response,
                           "seq 1 2000") < 0,
                 "type large-output command", &failures);
    check_result(send_key(to_child[1], from_child[0], &event, &response,
                          28, '\n') < 0,
                 "start large-output command", &failures);
    check_result(poll_until_idle(to_child[1], from_child[0], &event,
                                 &response, 200) < 0,
                 "large-output completion", &failures);

    check_result(send_text(to_child[1], from_child[0], &event, &response,
                           "cc -o /tmp/os3-terminal-hello "
                           "/tmp/os3-terminal-hello.c") < 0,
                 "type cc", &failures);
    started = milliseconds();
    check_result(send_key(to_child[1], from_child[0], &event, &response,
                          28, '\n') < 0,
                 "start cc", &failures);
    elapsed = milliseconds() - started;
    check_result(elapsed >= 500, "cc response deadline", &failures);
    check_result(poll_until_idle(to_child[1], from_child[0], &event,
                                 &response, 200) < 0,
                 "cc completion", &failures);
    check_result(access(binary_path, F_OK) < 0,
                 "cc output binary", &failures);
#endif

    event.type = KUI_EVENT_CLOSE;
    event.serial++;
    write_all(to_child[1], &event, sizeof(event));
    close(to_child[1]);
    close(from_child[0]);
    waitpid(child, &status, 0);
#ifndef KYLEOS_GUEST
    remove(source_path);
    remove(binary_path);
#endif
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) failures++;

    if (failures != 0) {
        fprintf(stderr, "FAIL terminal async commands: %d failure(s), "
                        "response=%ldms\n", failures, elapsed);
        return 1;
    }
#ifdef KYLEOS_GUEST
    printf("PASS terminal stays responsive during ls (%ldms response)\n",
           elapsed);
#else
    printf("PASS terminal stays responsive during sleep and cc (%ldms response)\n",
           elapsed);
#endif
    return 0;
}
