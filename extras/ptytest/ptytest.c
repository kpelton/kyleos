#include <poll.h>
#include <pty.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int fail(const char *message)
{
    puts(message);
    return 1;
}

int main(void)
{
    int master = -1;
    int slave = -1;
    int status = 0;
    int child;
    char buffer[16] = {0};
    struct pollfd descriptor;

    if (openpty(&master, &slave, NULL, NULL, NULL) < 0)
        return fail("ptytest: openpty failed");
    if (!isatty(master) || !isatty(slave))
        return fail("ptytest: endpoints are not terminals");

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
    descriptor.fd = master;
    descriptor.events = POLLIN;
    descriptor.revents = 0;
    if (poll(&descriptor, 1, 1000) != 1 ||
        (descriptor.revents & POLLIN) == 0)
        return fail("ptytest: output did not become readable");
    if (read(master, buffer, sizeof(buffer)) != 5 ||
        memcmp(buffer, "pong\n", 5) != 0)
        return fail("ptytest: slave output mismatch");
    if (write(master, "\004", 1) != 1)
        return fail("ptytest: EOF write failed");
    if (waitpid(child, &status, 0) != child ||
        !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return fail("ptytest: child failed");
    descriptor.revents = 0;
    if (poll(&descriptor, 1, 0) != 1 ||
        (descriptor.revents & POLLHUP) == 0)
        return fail("ptytest: master did not report hangup");
    close(master);
    puts("ptytest: bidirectional PTY, EOF, poll, and HUP pass");
    return 0;
}
