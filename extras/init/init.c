#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CONSOLE_SHELL "/bin/sh"
#define CONSOLE_SHELL_NAME "sh"
#define CONSOLE_COUNT 4

static pid_t start_console(int console_index)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("init: fork");
        return -1;
    }
    if (pid == 0) {
        char terminal[] = "/dev/tty1";
        char *argv[] = { CONSOLE_SHELL_NAME, NULL };
        char *environment[] = {
            "HOME=/",
            "PATH=/bin:/usr/bin",
            "SHELL=/bin/sh",
            "TERM=vt100",
            "COLUMNS=80",
            "LINES=25",
            NULL
        };
        int fd;

        if (setsid() < 0) {
            perror("init: create virtual-terminal session");
            _exit(126);
        }
        terminal[8] = '1' + console_index;
        fd = open(terminal, O_RDWR);
        if (fd < 0) {
            perror("init: open virtual terminal");
            _exit(126);
        }
        if (dup2(fd, STDIN_FILENO) < 0 ||
            dup2(fd, STDOUT_FILENO) < 0 ||
            dup2(fd, STDERR_FILENO) < 0) {
            perror("init: redirect virtual terminal");
            _exit(126);
        }
        if (fd > STDERR_FILENO)
            close(fd);
        if (tcsetpgrp(STDIN_FILENO, getpgrp()) < 0) {
            perror("init: set virtual-terminal foreground group");
            _exit(126);
        }
        printf("KyleOS virtual terminal %d (Alt+F%d)\n",
               console_index + 1, console_index + 1);

        execve(CONSOLE_SHELL, argv, environment);
        perror("init: exec /bin/sh");
        _exit(127);
    }
    return pid;
}

static pid_t start_serial_shell(void)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("init: fork serial shell");
        return -1;
    }
    if (pid == 0) {
        char *argv[] = { CONSOLE_SHELL_NAME, NULL };
        char *environment[] = {
            "HOME=/",
            "PATH=/bin:/usr/bin",
            "SHELL=/bin/sh",
            NULL
        };
        int fd;

        if (setsid() < 0) {
            perror("init: create serial session");
            _exit(126);
        }
        fd = open("/dev/ttyS0", O_RDWR);
        if (fd < 0) {
            perror("init: open serial tty");
            _exit(126);
        }
        if (dup2(fd, STDIN_FILENO) < 0 ||
            dup2(fd, STDOUT_FILENO) < 0 ||
            dup2(fd, STDERR_FILENO) < 0) {
            perror("init: redirect serial tty");
            _exit(126);
        }
        if (fd > STDERR_FILENO)
            close(fd);
        if (tcsetpgrp(STDIN_FILENO, getpgrp()) < 0) {
            perror("init: set serial foreground group");
            _exit(126);
        }
        puts("KyleOS serial terminal COM1");
        execve(CONSOLE_SHELL, argv, environment);
        perror("init: exec serial /bin/sh");
        _exit(127);
    }
    return pid;
}

int main(void)
{
    pid_t consoles[CONSOLE_COUNT];
    pid_t serial_shell;

    puts("KyleOS init: starting PID 1");
    for (int index = 0; index < CONSOLE_COUNT; index++)
        consoles[index] = start_console(index);
    serial_shell = start_serial_shell();

    for (;;) {
        int status = 0;
        pid_t exited = waitpid(-1, &status, 0);

        if (exited < 0) {
            sleep(1);
            continue;
        }
        for (int index = 0; index < CONSOLE_COUNT; index++) {
            if (consoles[index] != exited)
                continue;
            printf("KyleOS init: tty%d exited; restarting\n", index + 1);
            consoles[index] = start_console(index);
            break;
        }
        if (exited == serial_shell) {
            puts("KyleOS serial terminal exited; restarting");
            serial_shell = start_serial_shell();
        }
    }
}
