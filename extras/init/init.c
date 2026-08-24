#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CONSOLE_SHELL "/bin/nushell"
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
        char *argv[] = { "nushell", NULL };
        int fd;

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
        printf("KyleOS virtual terminal %d (Alt+F%d)\n",
               console_index + 1, console_index + 1);

        execve(CONSOLE_SHELL, argv, NULL);
        perror("init: exec /bin/nushell");
        _exit(127);
    }
    return pid;
}

int main(void)
{
    pid_t consoles[CONSOLE_COUNT];

    puts("KyleOS init: starting PID 1");
    for (int index = 0; index < CONSOLE_COUNT; index++)
        consoles[index] = start_console(index);

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
    }
}
