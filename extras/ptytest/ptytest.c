#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static int fail(const char *message)
{
    puts(message);
    return 1;
}

static int read_exact(int fd, char *buffer, int count)
{
    int total = 0;

    while (total < count) {
        struct pollfd descriptor = {fd, POLLIN, 0};
        int result = poll(&descriptor, 1, 1000);
        int received;

        if (result != 1 || (descriptor.revents & POLLIN) == 0)
            return -1;
        received = read(fd, buffer + total, count - total);
        if (received <= 0)
            return -1;
        total += received;
    }
    return total;
}

static int test_terminal_signal(void)
{
    int master = -1;
    int slave = -1;
    int child;
    int status = 0;
    char output[6] = {0};

    if (openpty(&master, &slave, NULL, NULL, NULL) < 0)
        return fail("ptytest: signal PTY open failed");
    child = fork();
    if (child == 0) {
        char input;

        close(master);
        if (setsid() < 0 || tcsetpgrp(slave, getpgrp()) < 0)
            _exit(20);
        if (write(slave, "ready\n", 6) != 6)
            _exit(21);
        (void)read(slave, &input, 1);
        _exit(22);
    }
    close(slave);
    if (child < 0)
        return fail("ptytest: signal child fork failed");
    if (read_exact(master, output, sizeof(output)) != (int)sizeof(output) ||
        memcmp(output, "ready\n", sizeof(output)) != 0)
        return fail("ptytest: signal child did not become ready");
    if (write(master, "\003", 1) != 1)
        return fail("ptytest: interrupt write failed");
    if (waitpid(child, &status, 0) != child ||
        !WIFSIGNALED(status) || WTERMSIG(status) != SIGINT)
        return fail("ptytest: VINTR did not signal foreground group");
    close(master);
    return 0;
}

int main(void)
{
    int master = -1;
    int slave = -1;
    int status = 0;
    int child;
    char buffer[16] = {0};
    struct pollfd descriptor;
    struct termios attributes;

    if (openpty(&master, &slave, NULL, NULL, NULL) < 0)
        return fail("ptytest: openpty failed");
    if (!isatty(master) || !isatty(slave))
        return fail("ptytest: endpoints are not terminals");
    if (tcgetattr(slave, &attributes) < 0 ||
        (attributes.c_lflag & (ISIG | ICANON | ECHO)) !=
            (ISIG | ICANON | ECHO) ||
        attributes.c_cc[VINTR] != 3 || attributes.c_cc[VEOF] != 4)
        return fail("ptytest: default terminal attributes mismatch");

    child = fork();
    if (child == 0) {
        char input[8] = {0};
        int count;

        close(master);
        if (dup2(slave, STDIN_FILENO) < 0 ||
            dup2(slave, STDOUT_FILENO) < 0 ||
            dup2(slave, STDERR_FILENO) < 0)
            _exit(10);
        if (slave > STDERR_FILENO)
            close(slave);
        if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
            _exit(11);
        count = read(STDIN_FILENO, input, sizeof(input));
        if (count != 5 || memcmp(input, "ping\n", 5) != 0)
            _exit(12);
        if (write(STDOUT_FILENO, "pong\n", 5) != 5)
            _exit(13);
        if (read(STDIN_FILENO, input, sizeof(input)) != 0)
            _exit(14);
        _exit(0);
    }
    close(slave);
    if (child < 0)
        return fail("ptytest: fork failed");
    if (write(master, "ping\n", 5) != 5)
        return fail("ptytest: master write failed");
    if (read_exact(master, buffer, 10) != 10 ||
        memcmp(buffer, "ping\npong\n", 10) != 0)
        return fail("ptytest: echo or slave output mismatch");
    if (write(master, "\004", 1) != 1)
        return fail("ptytest: EOF write failed");
    if (waitpid(child, &status, 0) != child ||
        !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return fail("ptytest: child failed");
    descriptor.fd = master;
    descriptor.events = POLLIN;
    descriptor.revents = 0;
    if (poll(&descriptor, 1, 0) != 1 ||
        (descriptor.revents & POLLHUP) == 0)
        return fail("ptytest: master did not report hangup");
    close(master);
    if (test_terminal_signal() != 0)
        return 1;
    puts("ptytest: line discipline, signals, EOF, poll, and HUP pass");
    return 0;
}
