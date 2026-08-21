#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define LARGE_TRANSFER 12288

struct fd_status {
    uint32_t readable;
    uint32_t writable;
    uint32_t hung_up;
};

static unsigned char transfer[LARGE_TRANSFER];

static void sleep_ms(uint32_t milliseconds)
{
    register long rax asm("rax") = 0;

    asm volatile("int $0x80" : "+a"(rax)
                 : "D"((long)milliseconds) : "memory");
}

static int fd_status(int fd, struct fd_status *status)
{
    register long rax asm("rax") = 33;

    asm volatile("int $0x80" : "+a"(rax) : "D"((long)fd), "S"(status)
                 : "memory");
    return (int)rax;
}

static int child_succeeded(pid_t child)
{
    int status = 0;

    return waitpid(child, &status, 0) == child && WIFEXITED(status) &&
           WEXITSTATUS(status) == 0;
}

static int test_reader_wakeup_and_eof(void)
{
    static const char message[] = "wake";
    char buffer[sizeof(message) - 1];
    int descriptors[2];
    pid_t child;
    int count;
    int eof;
    int child_ok;

    if (pipe(descriptors) < 0)
        return 0;
    child = fork();
    if (child < 0) {
        close(descriptors[0]);
        close(descriptors[1]);
        return 0;
    }
    if (child == 0) {
        int ok;

        close(descriptors[0]);
        sleep_ms(20);
        ok = write(descriptors[1], message, sizeof(message) - 1) ==
             (int)sizeof(message) - 1;
        /* Leave the writer open so process teardown exercises the scheduler-
         * locked pipe-close wakeup path. */
        _exit(ok ? 0 : 1);
    }

    close(descriptors[1]);
    count = read(descriptors[0], buffer, sizeof(buffer));
    eof = read(descriptors[0], buffer, sizeof(buffer));
    close(descriptors[0]);
    child_ok = child_succeeded(child);
    return count == (int)sizeof(buffer) &&
           memcmp(buffer, message, sizeof(buffer)) == 0 && eof == 0 &&
           child_ok;
}

static int test_writer_wakeup(void)
{
    unsigned char buffer[512];
    int descriptors[2];
    pid_t child;
    int total = 0;
    int valid = 1;
    int count;
    int child_ok;

    for (int i = 0; i < LARGE_TRANSFER; i++)
        transfer[i] = (unsigned char)(i * 37 + 11);
    if (pipe(descriptors) < 0)
        return 0;
    child = fork();
    if (child < 0) {
        close(descriptors[0]);
        close(descriptors[1]);
        return 0;
    }
    if (child == 0) {
        close(descriptors[0]);
        count = write(descriptors[1], transfer, sizeof(transfer));
        close(descriptors[1]);
        _exit(count == (int)sizeof(transfer) ? 0 : 1);
    }

    close(descriptors[1]);
    sleep_ms(20);
    while ((count = read(descriptors[0], buffer, sizeof(buffer))) > 0) {
        for (int i = 0; i < count; i++)
            if (buffer[i] != (unsigned char)((total + i) * 37 + 11))
                valid = 0;
        total += count;
    }
    close(descriptors[0]);
    child_ok = child_succeeded(child);
    return count == 0 && total == LARGE_TRANSFER && valid && child_ok;
}

static int test_close_wakes_reader(void)
{
    char byte;
    int descriptors[2];
    pid_t child;
    int count;
    int child_ok;

    if (pipe(descriptors) < 0)
        return 0;
    child = fork();
    if (child < 0) {
        close(descriptors[0]);
        close(descriptors[1]);
        return 0;
    }
    if (child == 0) {
        close(descriptors[0]);
        sleep_ms(20);
        close(descriptors[1]);
        _exit(0);
    }

    close(descriptors[1]);
    count = read(descriptors[0], &byte, 1);
    close(descriptors[0]);
    child_ok = child_succeeded(child);
    return count == 0 && child_ok;
}

static int test_close_wakes_writer(void)
{
    int descriptors[2];
    pid_t child;
    int child_ok;

    memset(transfer, 0x5a, sizeof(transfer));
    if (pipe(descriptors) < 0)
        return 0;
    child = fork();
    if (child < 0) {
        close(descriptors[0]);
        close(descriptors[1]);
        return 0;
    }
    if (child == 0) {
        int count;

        close(descriptors[0]);
        count = write(descriptors[1], transfer, sizeof(transfer));
        close(descriptors[1]);
        _exit(count > 0 && count < (int)sizeof(transfer) ? 0 : 1);
    }

    close(descriptors[1]);
    sleep_ms(20);
    close(descriptors[0]);
    child_ok = child_succeeded(child);
    return child_ok;
}

static int test_close_wakes_multiple_readers(void)
{
    int descriptors[2];
    pid_t first;
    pid_t second;
    int first_ok;
    int second_ok;

    if (pipe(descriptors) < 0)
        return 0;
    first = fork();
    if (first == 0) {
        char byte;
        int count;

        close(descriptors[1]);
        count = read(descriptors[0], &byte, 1);
        close(descriptors[0]);
        _exit(count == 0 ? 0 : 1);
    }
    if (first < 0) {
        close(descriptors[0]);
        close(descriptors[1]);
        return 0;
    }
    second = fork();
    if (second == 0) {
        char byte;
        int count;

        close(descriptors[1]);
        count = read(descriptors[0], &byte, 1);
        close(descriptors[0]);
        _exit(count == 0 ? 0 : 1);
    }
    if (second < 0) {
        close(descriptors[0]);
        close(descriptors[1]);
        child_succeeded(first);
        return 0;
    }

    close(descriptors[0]);
    sleep_ms(20);
    close(descriptors[1]);
    first_ok = child_succeeded(first);
    second_ok = child_succeeded(second);
    return first_ok && second_ok;
}

static void blocked_writer(int read_fd, int write_fd)
{
    int count;

    close(read_fd);
    count = write(write_fd, transfer, sizeof(transfer));
    close(write_fd);
    _exit(count < (int)sizeof(transfer) ? 0 : 1);
}

static int test_close_wakes_multiple_writers(void)
{
    int descriptors[2];
    pid_t first;
    pid_t second;
    int first_ok;
    int second_ok;

    memset(transfer, 0xa5, sizeof(transfer));
    if (pipe(descriptors) < 0)
        return 0;
    first = fork();
    if (first == 0)
        blocked_writer(descriptors[0], descriptors[1]);
    if (first < 0) {
        close(descriptors[0]);
        close(descriptors[1]);
        return 0;
    }
    second = fork();
    if (second == 0)
        blocked_writer(descriptors[0], descriptors[1]);
    if (second < 0) {
        close(descriptors[0]);
        close(descriptors[1]);
        child_succeeded(first);
        return 0;
    }

    close(descriptors[1]);
    sleep_ms(20);
    close(descriptors[0]);
    first_ok = child_succeeded(first);
    second_ok = child_succeeded(second);
    return first_ok && second_ok;
}

static int test_zero_length_io(void)
{
    char byte = 0;
    int descriptors[2];
    int passed;

    if (pipe(descriptors) < 0)
        return 0;
    passed = read(descriptors[0], &byte, 0) == 0 &&
             write(descriptors[1], &byte, 0) == 0;
    close(descriptors[0]);
    close(descriptors[1]);
    return passed;
}

static int test_readiness_status(void)
{
    struct fd_status reader;
    struct fd_status writer;
    char bytes[3] = {1, 2, 3};
    int descriptors[2];
    int passed;

    if (pipe(descriptors) < 0)
        return 0;
    passed = fd_status(descriptors[0], &reader) == 0 &&
             fd_status(descriptors[1], &writer) == 0 &&
             reader.readable == 0 && !reader.hung_up &&
             writer.writable && !writer.hung_up &&
             write(descriptors[1], bytes, sizeof(bytes)) == sizeof(bytes) &&
             fd_status(descriptors[0], &reader) == 0 &&
             reader.readable == sizeof(bytes);
    close(descriptors[1]);
    passed = passed && fd_status(descriptors[0], &reader) == 0 &&
             reader.readable == sizeof(bytes) && reader.hung_up &&
             read(descriptors[0], bytes, sizeof(bytes)) == sizeof(bytes) &&
             fd_status(descriptors[0], &reader) == 0 &&
             reader.readable == 0 && reader.hung_up;
    close(descriptors[0]);
    return passed;
}

static int check(const char *name, int passed)
{
    printf("%s pipe %s\n", passed ? "PASS" : "FAIL", name);
    return passed ? 0 : 1;
}

int main(void)
{
    int failures = 0;

    failures += check("reader wakeup and EOF", test_reader_wakeup_and_eof());
    failures += check("full-buffer writer wakeup", test_writer_wakeup());
    failures += check("writer close wakes reader", test_close_wakes_reader());
    failures += check("reader close wakes writer", test_close_wakes_writer());
    failures += check("close wakes multiple readers",
                      test_close_wakes_multiple_readers());
    failures += check("close wakes multiple writers",
                      test_close_wakes_multiple_writers());
    failures += check("zero-length I/O", test_zero_length_io());
    failures += check("readiness status", test_readiness_status());
    printf("PIPE REPORT: %d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
