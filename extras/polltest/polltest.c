#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static uint32_t ticks_ms(void)
{
    register long result asm("rax") = 25;

    asm volatile("int $0x80" : "+a"(result) : : "memory");
    return (uint32_t)result;
}

static void sleep_ms(uint32_t milliseconds)
{
    register long result asm("rax") = 0;

    asm volatile("int $0x80" : "+a"(result)
                 : "D"((long)milliseconds) : "memory");
}

static int child_succeeded(pid_t child)
{
    int status = 0;

    return waitpid(child, &status, 0) == child && WIFEXITED(status) &&
           WEXITSTATUS(status) == 0;
}

static int test_immediate_pipe_status(void)
{
    struct pollfd fds[2];
    int descriptors[2];
    int result;

    if (pipe(descriptors) < 0) return 0;
    fds[0].fd = descriptors[0];
    fds[0].events = POLLIN;
    fds[1].fd = descriptors[1];
    fds[1].events = POLLOUT;
    result = poll(fds, 2, 0);
    close(descriptors[0]);
    close(descriptors[1]);
    return result == 1 && fds[0].revents == 0 &&
           fds[1].revents == POLLOUT;
}

static int test_multiple_fds_and_wakeup(void)
{
    static const char byte = 'P';
    struct pollfd fds[2];
    int first[2];
    int second[2];
    pid_t child;
    char received = 0;
    int result;
    int child_ok;

    if (pipe(first) < 0 || pipe(second) < 0) return 0;
    child = fork();
    if (child < 0) return 0;
    if (child == 0) {
        int ok;

        close(first[0]);
        close(first[1]);
        close(second[0]);
        sleep_ms(30);
        ok = write(second[1], &byte, 1) == 1;
        close(second[1]);
        _exit(ok ? 0 : 1);
    }
    close(second[1]);
    fds[0].fd = first[0];
    fds[0].events = POLLIN;
    fds[1].fd = second[0];
    fds[1].events = POLLIN;
    result = poll(fds, 2, 500);
    if ((fds[1].revents & POLLIN) != 0)
        read(second[0], &received, 1);
    close(first[1]);
    close(first[0]);
    close(second[0]);
    child_ok = child_succeeded(child);
    return result >= 1 && fds[0].revents == 0 &&
           (fds[1].revents & POLLIN) != 0 && received == byte && child_ok;
}

static int test_timeout(void)
{
    struct pollfd fd;
    int descriptors[2];
    uint32_t start;
    uint32_t elapsed;
    int result;

    if (pipe(descriptors) < 0) return 0;
    fd.fd = descriptors[0];
    fd.events = POLLIN;
    start = ticks_ms();
    result = poll(&fd, 1, 40);
    elapsed = ticks_ms() - start;
    close(descriptors[0]);
    close(descriptors[1]);
    return result == 0 && fd.revents == 0 && elapsed >= 30 && elapsed < 500;
}

static int test_hangup_and_invalid(void)
{
    struct pollfd fds[3];
    int descriptors[2];
    int result;

    if (pipe(descriptors) < 0) return 0;
    close(descriptors[1]);
    fds[0].fd = descriptors[0];
    fds[0].events = POLLIN;
    fds[1].fd = 63;
    fds[1].events = POLLIN;
    fds[2].fd = -1;
    fds[2].events = POLLIN;
    result = poll(fds, 3, 0);
    close(descriptors[0]);
    return result == 2 && (fds[0].revents & POLLHUP) != 0 &&
           fds[1].revents == POLLNVAL && fds[2].revents == 0;
}

static int test_regular_file_and_empty_set(void)
{
    struct pollfd fd;
    int file = open("/bin/pipetest", O_RDONLY);
    uint32_t start;
    uint32_t elapsed;
    int file_result;
    int empty_result;

    if (file < 0) {
        printf("poll regular detail: open failed\n");
        return 0;
    }
    if (lseek(file, 0, SEEK_END) < 0) {
        close(file);
        return 0;
    }
    fd.fd = file;
    fd.events = POLLIN;
    file_result = poll(&fd, 1, 100);
    close(file);
    start = ticks_ms();
    empty_result = poll(NULL, 0, 30);
    elapsed = ticks_ms() - start;
    if (file_result != 1 || fd.revents != POLLIN || empty_result != 0 ||
        elapsed < 20 || elapsed >= 500)
        printf("poll regular detail: file=%d revents=0x%x empty=%d elapsed=%u\n",
               file_result, (unsigned int)fd.revents, empty_result, elapsed);
    return file_result == 1 && fd.revents == POLLIN &&
           empty_result == 0 && elapsed >= 20 && elapsed < 500;
}

static int check(const char *name, int passed)
{
    printf("%s poll %s\n", passed ? "PASS" : "FAIL", name);
    return passed ? 0 : 1;
}

int main(void)
{
    int failures = 0;

    failures += check("immediate pipe readiness",
                      test_immediate_pipe_status());
    failures += check("multiple descriptors and wakeup",
                      test_multiple_fds_and_wakeup());
    failures += check("finite timeout", test_timeout());
    failures += check("hangup, invalid, and ignored descriptors",
                      test_hangup_and_invalid());
    failures += check("regular file and empty descriptor set",
                      test_regular_file_and_empty_set());
    printf("POLL REPORT: %d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
