#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CONSOLE_SHELL "/bin/nushell"

static pid_t start_console(void)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("init: fork");
        return -1;
    }
    if (pid == 0) {
        char *argv[] = { "nushell", NULL };

        execve(CONSOLE_SHELL, argv, NULL);
        perror("init: exec /bin/nushell");
        _exit(127);
    }
    return pid;
}

int main(void)
{
    puts("KyleOS init: starting PID 1");

    for (;;) {
        int status = 0;
        pid_t console = start_console();

        if (console < 0) {
            sleep(1);
            continue;
        }
        waitpid(console, &status, 0);
        puts("KyleOS init: console exited; restarting");
    }
}
