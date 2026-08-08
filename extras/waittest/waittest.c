#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int fail(const char *message)
{
    printf("FAIL wait ABI: %s\n", message);
    return 1;
}

int main(void)
{
    pid_t child;
    pid_t result;
    int status = 0;

    child = fork();
    if (child < 0)
        return fail("fork");
    if (child == 0) {
        sleep(1);
        _exit(37);
    }

    result = waitpid(child, &status, WNOHANG);
    if (result != 0)
        return fail("WNOHANG");
    result = waitpid(child, &status, 0);
    if (result != child)
        return fail("return PID");
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 37)
        return fail("exit status");
    if (waitpid(child, &status, WNOHANG) != -1)
        return fail("double reap");

    child = fork();
    if (child < 0)
        return fail("second fork");
    if (child == 0)
        _exit(9);
    result = wait(&status);
    if (result != child || !WIFEXITED(status) || WEXITSTATUS(status) != 9)
        return fail("wait any child");

    puts("PASS wait ABI PID status WNOHANG");
    return 0;
}
