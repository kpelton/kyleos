#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct signal_name {
    const char *name;
    int number;
};

static const struct signal_name signal_names[] = {
    { "HUP", SIGHUP }, { "INT", SIGINT }, { "QUIT", SIGQUIT },
    { "ILL", SIGILL }, { "TRAP", SIGTRAP }, { "ABRT", SIGABRT },
    { "FPE", SIGFPE }, { "KILL", SIGKILL }, { "BUS", SIGBUS },
    { "SEGV", SIGSEGV }, { "PIPE", SIGPIPE }, { "ALRM", SIGALRM },
    { "TERM", SIGTERM }, { "STOP", SIGSTOP }, { "TSTP", SIGTSTP },
    { "CONT", SIGCONT }, { "CHLD", SIGCHLD }, { "TTIN", SIGTTIN },
    { "TTOU", SIGTTOU }, { "WINCH", SIGWINCH },
    { "USR1", SIGUSR1 }, { "USR2", SIGUSR2 }
};

static int parse_number(const char *text, int minimum, int maximum,
                        int *result)
{
    char *end;
    long value = strtol(text, &end, 10);

    if (text[0] == '\0' || *end != '\0' || value < minimum ||
        value > maximum)
        return -1;
    *result = (int)value;
    return 0;
}

static int parse_signal(const char *text, int *result)
{
    if (strncmp(text, "SIG", 3) == 0)
        text += 3;
    if (parse_number(text, 0, NSIG - 1, result) == 0)
        return 0;
    for (unsigned int i = 0;
         i < sizeof(signal_names) / sizeof(signal_names[0]); i++)
        if (strcmp(text, signal_names[i].name) == 0) {
            *result = signal_names[i].number;
            return 0;
        }
    return -1;
}

static void list_signals(void)
{
    for (unsigned int i = 0;
         i < sizeof(signal_names) / sizeof(signal_names[0]); i++)
        printf("%2d) SIG%s%s", signal_names[i].number,
               signal_names[i].name,
               (i + 1) % 4 == 0 ? "\n" : "  ");
    if (sizeof(signal_names) / sizeof(signal_names[0]) % 4 != 0)
        putchar('\n');
}

int main(int argc, char **argv)
{
    int signal_number = SIGTERM;
    int first_pid = 1;
    int failures = 0;

    if (argc == 2 && strcmp(argv[1], "-l") == 0) {
        list_signals();
        return 0;
    }
    if (argc > 2 && strcmp(argv[1], "-s") == 0) {
        if (parse_signal(argv[2], &signal_number) < 0) {
            fprintf(stderr, "kill: invalid signal: %s\n", argv[2]);
            return 2;
        }
        first_pid = 3;
    } else if (argc > 1 && argv[1][0] == '-' && argv[1][1] != '\0') {
        if (parse_signal(argv[1] + 1, &signal_number) < 0) {
            fprintf(stderr, "kill: invalid signal: %s\n", argv[1] + 1);
            return 2;
        }
        first_pid = 2;
    }
    if (first_pid >= argc) {
        fprintf(stderr, "Usage: kill [-signal | -s signal] pid...\n");
        return 2;
    }
    for (int i = first_pid; i < argc; i++) {
        int pid;

        if (parse_number(argv[i], 1, 0x7fffffff, &pid) < 0) {
            fprintf(stderr, "kill: invalid pid: %s\n", argv[i]);
            failures = 1;
            continue;
        }
        if (kill(pid, signal_number) < 0) {
            fprintf(stderr, "kill: unable to signal pid %d\n", pid);
            failures = 1;
        }
    }
    return failures;
}
