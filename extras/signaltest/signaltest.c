#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t handled;

static void handle_signal(int signal_number)
{
    handled = signal_number;
}

static int fail(const char *message)
{
    printf("FAIL signals: %s\n", message);
    return 1;
}

int main(void)
{
    struct sigaction action;
    sigset_t blocked;
    sigset_t pending;
    pid_t child;
    int status = 0;

    if (getpid() <= 0)
        return fail("getpid");

    (void)sigemptyset(&action.sa_mask);
    action.sa_handler = handle_signal;
    action.sa_flags = 0;
    if (sigaction(SIGUSR1, &action, NULL) < 0)
        return fail("sigaction");

    (void)sigemptyset(&blocked);
    (void)sigaddset(&blocked, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &blocked, NULL) < 0)
        return fail("block");
    if (kill(getpid(), SIGUSR1) < 0 || handled != 0)
        return fail("queue blocked signal");
    if (sigpending(&pending) < 0 || !sigismember(&pending, SIGUSR1))
        return fail("pending set");
    if (sigprocmask(SIG_UNBLOCK, &blocked, NULL) < 0)
        return fail("unblock");
    if (handled != SIGUSR1)
        return fail("handler delivery or sigreturn");

    handled = 0;
    if (signal(SIGUSR2, handle_signal) == SIG_ERR)
        return fail("signal wrapper");
    if (kill(getpid(), SIGUSR2) < 0 || handled != SIGUSR2)
        return fail("signal wrapper delivery");

    child = fork();
    if (child < 0)
        return fail("fork");
    if (child == 0) {
        kill(getpid(), SIGTERM);
        _exit(99);
    }
    if (waitpid(child, &status, 0) != child ||
        !WIFSIGNALED(status) || WTERMSIG(status) != SIGTERM)
        return fail("default termination status");

    child = fork();
    if (child < 0)
        return fail("stop child fork");
    if (child == 0) {
        for (;;)
            (void)getpid();
    }
    if (kill(child, SIGSTOP) < 0 ||
        waitpid(child, &status, WUNTRACED) != child ||
        !WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP)
        return fail("stopped child status");
    if (kill(child, SIGCONT) < 0 || kill(child, SIGTERM) < 0 ||
        waitpid(child, &status, 0) != child ||
        !WIFSIGNALED(status) || WTERMSIG(status) != SIGTERM)
        return fail("continue and terminate child");

    if (getpgrp() <= 0 || getpgid(0) != getpgrp())
        return fail("get process group");
    child = fork();
    if (child < 0)
        return fail("process group child fork");
    if (child == 0) {
        for (;;)
            (void)getpid();
    }
    if (setpgid(child, child) < 0 || getpgid(child) != child)
        return fail("create child process group");
    if (kill(-child, SIGTERM) < 0 ||
        waitpid(child, &status, 0) != child ||
        !WIFSIGNALED(status) || WTERMSIG(status) != SIGTERM)
        return fail("signal process group");

    child = fork();
    if (child < 0)
        return fail("session child fork");
    if (child == 0)
        _exit(setsid() == getpid() && getpgrp() == getpid() ? 0 : 30);
    if (waitpid(child, &status, 0) != child ||
        !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return fail("create session");

    puts("PASS signals handlers masks groups sessions stop continue");
    return 0;
}
