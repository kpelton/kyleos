#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define MAX_PIPE_COMMANDS 8
#define MAX_JOBS 32
#define JOB_COMMAND_SIZE 256
#define I_DEV 400000

struct kyleos_dirent {
    unsigned long long i_no;
    char i_name[257];
    int i_type;
    unsigned long long i_size;
};

int getdents(int fd, struct kyleos_dirent *entries, int count);

static int last_status;

enum job_state {
    JOB_RUNNING,
    JOB_STOPPED
};

struct shell_job {
    int id;
    pid_t leader;
    pid_t pids[MAX_PIPE_COMMANDS];
    int pid_count;
    enum job_state state;
    char command[JOB_COMMAND_SIZE];
};

static struct shell_job jobs[MAX_JOBS];
static int next_job_id = 1;
static int shell_terminal = -1;
static pid_t shell_process_group;

static int decode_wait_status(int status);

static void give_terminal_to(pid_t process_group)
{
    if (shell_terminal >= 0)
        (void)tcsetpgrp(shell_terminal, process_group);
}

static void reclaim_terminal(void)
{
    give_terminal_to(shell_process_group);
}

static void reset_child_signals(void)
{
    (void)signal(SIGINT, SIG_DFL);
    (void)signal(SIGQUIT, SIG_DFL);
    (void)signal(SIGTSTP, SIG_DFL);
    (void)signal(SIGTTIN, SIG_DFL);
    (void)signal(SIGTTOU, SIG_DFL);
}

static pid_t wait_for_process(pid_t pid, int *status, int options)
{
    pid_t result;

    do {
        result = waitpid(pid, status, options);
    } while (result < 0 && errno == EINTR);
    return result;
}

static const char *job_state_name(enum job_state state)
{
    return state == JOB_STOPPED ? "Stopped" : "Running";
}

static int job_has_processes(const struct shell_job *job)
{
    for (int i = 0; i < job->pid_count; i++)
        if (job->pids[i] > 0)
            return 1;
    return 0;
}

static void clean_job_command(char *destination, size_t size,
                              const char *command)
{
    size_t length;

    snprintf(destination, size, "%s", command != NULL ? command : "");
    length = strlen(destination);
    while (length > 0 && destination[length - 1] == ' ')
        destination[--length] = '\0';
    if (length > 0 && destination[length - 1] == '&') {
        destination[--length] = '\0';
        while (length > 0 && destination[length - 1] == ' ')
            destination[--length] = '\0';
    }
}

static struct shell_job *add_job(const pid_t *pids, int pid_count,
                                 enum job_state state, const char *command)
{
    for (int slot = 0; slot < MAX_JOBS; slot++) {
        struct shell_job *job = &jobs[slot];

        if (job->id != 0)
            continue;
        memset(job, 0, sizeof(*job));
        job->id = next_job_id++;
        job->state = state;
        job->pid_count = pid_count > MAX_PIPE_COMMANDS
                       ? MAX_PIPE_COMMANDS : pid_count;
        for (int i = 0; i < job->pid_count; i++) {
            job->pids[i] = pids[i];
            if (job->leader == 0 && pids[i] > 0)
                job->leader = pids[i];
        }
        clean_job_command(job->command, sizeof(job->command), command);
        printf("[%d] %d\n", job->id, (int)job->leader);
        return job;
    }
    fprintf(stderr, "jobs: job table is full\n");
    return NULL;
}

static void remove_job(struct shell_job *job)
{
    memset(job, 0, sizeof(*job));
}

static struct shell_job *find_job(const char *specification)
{
    struct shell_job *latest = NULL;
    char *end;
    long value;

    if (specification == NULL) {
        for (int i = 0; i < MAX_JOBS; i++)
            if (jobs[i].id != 0 &&
                (latest == NULL || jobs[i].id > latest->id))
                latest = &jobs[i];
        return latest;
    }
    if (specification[0] == '%') {
        value = strtol(specification + 1, &end, 10);
        if (specification[1] == '\0' || *end != '\0' || value <= 0)
            return NULL;
        for (int i = 0; i < MAX_JOBS; i++)
            if (jobs[i].id == value)
                return &jobs[i];
        return NULL;
    }
    value = strtol(specification, &end, 10);
    if (specification[0] == '\0' || *end != '\0' || value <= 0)
        return NULL;
    for (int i = 0; i < MAX_JOBS; i++)
        if (jobs[i].id != 0)
            for (int process = 0; process < jobs[i].pid_count; process++)
                if (jobs[i].pids[process] == value)
                    return &jobs[i];
    return NULL;
}

static void signal_job(struct shell_job *job, int signal_number)
{
    for (int i = 0; i < job->pid_count; i++)
        if (job->pids[i] > 0)
            (void)kill(job->pids[i], signal_number);
}

static void reap_jobs(int notify)
{
    for (int slot = 0; slot < MAX_JOBS; slot++) {
        struct shell_job *job = &jobs[slot];
        enum job_state old_state;

        if (job->id == 0)
            continue;
        old_state = job->state;
        for (int i = 0; i < job->pid_count; i++) {
            int status;
            pid_t result;

            if (job->pids[i] <= 0)
                continue;
            result = waitpid(job->pids[i], &status, WNOHANG | WUNTRACED);
            if (result <= 0)
                continue;
            if (WIFSTOPPED(status))
                job->state = JOB_STOPPED;
            else
                job->pids[i] = 0;
        }
        if (!job_has_processes(job)) {
            if (notify)
                printf("[%d] Done       %s\n", job->id, job->command);
            remove_job(job);
        } else if (notify && old_state != job->state) {
            printf("[%d] %-10s %s\n", job->id,
                   job_state_name(job->state), job->command);
        }
    }
}

static int wait_for_job(struct shell_job *job)
{
    int result_status = 0;
    int stopped = 0;

    for (int i = 0; i < job->pid_count; i++) {
        int status;
        pid_t result;

        if (job->pids[i] <= 0)
            continue;
        result = wait_for_process(job->pids[i], &status, WUNTRACED);
        if (result < 0)
            continue;
        if (WIFSTOPPED(status)) {
            stopped = 1;
            job->state = JOB_STOPPED;
            signal_job(job, SIGSTOP);
            result_status = 128 + WSTOPSIG(status);
        } else {
            job->pids[i] = 0;
            result_status = decode_wait_status(status);
        }
    }
    reclaim_terminal();
    if (stopped) {
        printf("[%d] Stopped    %s\n", job->id, job->command);
        return result_status;
    }
    remove_job(job);
    return result_status;
}

static int wildcard_match(const char *pattern, const char *name)
{
    while (*pattern != '\0') {
        if (*pattern == '*') {
            pattern++;
            if (*pattern == '\0')
                return 1;
            while (*name != '\0') {
                if (wildcard_match(pattern, name))
                    return 1;
                name++;
            }
            return wildcard_match(pattern, name);
        }
        if (*name == '\0' || (*pattern != '?' && *pattern != *name))
            return 0;
        pattern++;
        name++;
    }
    return *name == '\0';
}

static int expand_command_globs(char **args, int *argc, char **owned,
                                int *owned_count)
{
    char *expanded[MAX_ARGS];
    int expanded_count = 0;

    for (int i = 0; i < *argc; i++) {
        int matches = 0;

        if (i > 0 && strchr(args[i], '/') == NULL &&
            (strchr(args[i], '*') != NULL || strchr(args[i], '?') != NULL)) {
            FILE *directory = fopen(".", "r");
            struct kyleos_dirent *entries =
                malloc(sizeof(*entries) * 1024);
            int count = -1;

            if (directory != NULL && entries != NULL)
                count = getdents(fileno(directory), entries, 1024);
            if (directory != NULL)
                fclose(directory);
            for (int j = 0; j < count && expanded_count < MAX_ARGS - 1; j++) {
                char *copy;

                if (strcmp(entries[j].i_name, ".") == 0 ||
                    strcmp(entries[j].i_name, "..") == 0 ||
                    !wildcard_match(args[i], entries[j].i_name))
                    continue;
                copy = strdup(entries[j].i_name);
                if (copy == NULL)
                    break;
                expanded[expanded_count++] = copy;
                owned[(*owned_count)++] = copy;
                matches++;
            }
            free(entries);
        }
        if (matches == 0 && expanded_count < MAX_ARGS - 1)
            expanded[expanded_count++] = args[i];
    }
    for (int i = 0; i < expanded_count; i++)
        args[i] = expanded[i];
    args[expanded_count] = NULL;
    *argc = expanded_count;
    return 0;
}

static int decode_wait_status(int status)
{
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 1;
}

static int parse_repeat_count(const char *text, int *count)
{
    char *end;
    long value = strtol(text, &end, 10);

    if (text[0] == '\0' || *end != '\0' || value < 0 || value > 10000)
        return -1;
    *count = (int)value;
    return 0;
}

static int parse_sleep_seconds(const char *text, unsigned int *seconds)
{
    char *end;
    long value = strtol(text, &end, 10);

    if (text[0] == '\0' || *end != '\0' || value < 0 || value > 86400)
        return -1;
    *seconds = (unsigned int)value;
    return 0;
}

static int parse_signal_number(const char *text, int *signal_number)
{
    struct named_signal {
        const char *name;
        int number;
    };
    static const struct named_signal names[] = {
        { "HUP", SIGHUP }, { "INT", SIGINT }, { "QUIT", SIGQUIT },
        { "KILL", SIGKILL }, { "PIPE", SIGPIPE }, { "TERM", SIGTERM },
        { "STOP", SIGSTOP }, { "TSTP", SIGTSTP }, { "CONT", SIGCONT },
        { "CHLD", SIGCHLD }, { "USR1", SIGUSR1 }, { "USR2", SIGUSR2 }
    };
    char *end;
    long value;

    if (strncmp(text, "SIG", 3) == 0)
        text += 3;
    value = strtol(text, &end, 10);
    if (text[0] != '\0' && *end == '\0' && value >= 0 && value < NSIG) {
        *signal_number = (int)value;
        return 0;
    }
    for (unsigned int i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (strcmp(text, names[i].name) == 0) {
            *signal_number = names[i].number;
            return 0;
        }
    return -1;
}

static int execute_jobs_builtin(char **args)
{
    if (args[1] != NULL) {
        fprintf(stderr, "Usage: jobs\n");
        return 2;
    }
    reap_jobs(0);
    for (int i = 0; i < MAX_JOBS; i++)
        if (jobs[i].id != 0)
            printf("[%d] %-10s %d %s\n", jobs[i].id,
                   job_state_name(jobs[i].state), (int)jobs[i].leader,
                   jobs[i].command);
    return 0;
}

static int execute_fg_builtin(char **args)
{
    struct shell_job *job;

    if (args[2] != NULL) {
        fprintf(stderr, "Usage: fg [%%job|pid]\n");
        return 2;
    }
    reap_jobs(0);
    job = find_job(args[1]);
    if (job == NULL) {
        fprintf(stderr, "fg: no such job\n");
        return 1;
    }
    printf("%s\n", job->command);
    give_terminal_to(job->leader);
    signal_job(job, SIGCONT);
    job->state = JOB_RUNNING;
    return wait_for_job(job);
}

static int execute_bg_builtin(char **args)
{
    struct shell_job *job;

    if (args[2] != NULL) {
        fprintf(stderr, "Usage: bg [%%job|pid]\n");
        return 2;
    }
    reap_jobs(0);
    job = find_job(args[1]);
    if (job == NULL) {
        fprintf(stderr, "bg: no such job\n");
        return 1;
    }
    signal_job(job, SIGCONT);
    job->state = JOB_RUNNING;
    printf("[%d] %-10s %s\n", job->id, "Running", job->command);
    return 0;
}

static int execute_kill_builtin(char **args)
{
    int signal_number = SIGTERM;
    int first_target = 1;
    int failures = 0;

    if (args[1] != NULL && strcmp(args[1], "-l") == 0) {
        if (args[2] != NULL) {
            fprintf(stderr, "Usage: kill -l\n");
            return 2;
        }
        puts("HUP INT QUIT KILL PIPE TERM STOP TSTP CONT CHLD USR1 USR2");
        return 0;
    }
    if (args[1] != NULL && strcmp(args[1], "-s") == 0) {
        if (args[2] == NULL || parse_signal_number(args[2], &signal_number) < 0) {
            fprintf(stderr, "kill: invalid or missing signal\n");
            return 2;
        }
        first_target = 3;
    } else if (args[1] != NULL && args[1][0] == '-' && args[1][1] != '\0') {
        if (parse_signal_number(args[1] + 1, &signal_number) < 0) {
            fprintf(stderr, "kill: invalid signal: %s\n", args[1] + 1);
            return 2;
        }
        first_target = 2;
    }
    if (args[first_target] == NULL) {
        fprintf(stderr, "Usage: kill [-signal | -s signal] pid|%%job...\n");
        return 2;
    }
    for (int i = first_target; args[i] != NULL; i++) {
        char *end;
        long pid;

        if (args[i][0] == '%') {
            struct shell_job *job = find_job(args[i]);
            if (job == NULL) {
                fprintf(stderr, "kill: no such job: %s\n", args[i]);
                failures = 1;
            } else {
                signal_job(job, signal_number);
                if (signal_number == SIGCONT)
                    job->state = JOB_RUNNING;
            }
            continue;
        }
        pid = strtol(args[i], &end, 10);
        if (args[i][0] == '\0' || *end != '\0' || pid <= 0 ||
            kill((pid_t)pid, signal_number) < 0) {
            fprintf(stderr, "kill: unable to signal %s\n", args[i]);
            failures = 1;
        }
    }
    return failures;
}

int execute_command(char **args, int background, char *in_file,
                    char *out_file, const char *command_text) {
    int val;
    int fd;
    if( strcmp(args[0],"exit") == 0) {
        int status = 0;
        if (args[1] != NULL && args[2] == NULL &&
            parse_repeat_count(args[1], &status) < 0) {
            fprintf(stderr, "Usage: exit [status]\n");
            return 2;
        }
        exit(status);
    }

    if (strcmp(args[0], "jobs") == 0)
        return execute_jobs_builtin(args);
    if (strcmp(args[0], "fg") == 0)
        return execute_fg_builtin(args);
    if (strcmp(args[0], "bg") == 0)
        return execute_bg_builtin(args);
    if (strcmp(args[0], "kill") == 0)
        return execute_kill_builtin(args);

    if (strcmp(args[0], "if") == 0) {
        char **then_word = NULL;
        char **else_word = NULL;
        int condition_status;

        for (int i = 1; args[i] != NULL; i++) {
            if (strcmp(args[i], "then") == 0 && then_word == NULL)
                then_word = &args[i];
            else if (strcmp(args[i], "else") == 0 && then_word != NULL) {
                else_word = &args[i];
                break;
            }
        }
        if (then_word == NULL || then_word == args + 1 ||
            (else_word != NULL && else_word == then_word + 1)) {
            fprintf(stderr, "Usage: if command then command [else command]\n");
            return 2;
        }
        *then_word = NULL;
        condition_status = execute_command(&args[1], 0, in_file, out_file,
                                           command_text);
        if (else_word != NULL)
            *else_word = NULL;
        if (condition_status == 0)
            return execute_command(then_word + 1, 0, in_file, out_file,
                                   command_text);
        if (else_word != NULL)
            return execute_command(else_word + 1, 0, in_file, out_file,
                                   command_text);
        return condition_status;
    }

    if( strcmp(args[0],"cd") == 0 && args[1] != NULL )
    {
        val = chdir(args[1]);
        if (val < 0)
        {
            printf("Error with cd \n");
        }
        return val < 0 ? 1 : 0;
    }

    if (strcmp(args[0], "repeat") == 0) {
        int count;

        if (args[1] == NULL || args[2] == NULL ||
            parse_repeat_count(args[1], &count) < 0) {
            fprintf(stderr, "Usage: repeat count command [args...]\n");
            return 2;
        }
        val = 0;
        for (int i = 0; i < count; i++) {
            val = execute_command(&args[2], 0, in_file, out_file,
                                  command_text);
            if (val == 128 + SIGINT)
                break;
        }
        return val;
    }

    if (strcmp(args[0], "sleep") == 0) {
        unsigned int seconds;
        pid_t sleep_pid;

        if (args[1] == NULL || args[2] != NULL ||
            parse_sleep_seconds(args[1], &seconds) < 0) {
            fprintf(stderr, "Usage: sleep seconds\n");
            return 2;
        }
        sleep_pid = fork();
        if (sleep_pid < 0) {
            perror("fork");
            return 1;
        }
        if (sleep_pid == 0) {
            (void)setpgid(0, 0);
            reset_child_signals();
            sleep(seconds);
            exit(0);
        }
        (void)setpgid(sleep_pid, sleep_pid);
        if (!background) {
            int status;

            give_terminal_to(sleep_pid);
            if (wait_for_process(sleep_pid, &status, WUNTRACED) < 0) {
                reclaim_terminal();
                return 1;
            }
            reclaim_terminal();
            if (WIFSTOPPED(status)) {
                pid_t pids[1] = {sleep_pid};
                (void)add_job(pids, 1, JOB_STOPPED, command_text);
            }
            return decode_wait_status(status);
        }
        {
            pid_t pids[1] = {sleep_pid};
            (void)add_job(pids, 1, JOB_RUNNING, command_text);
        }
        return 0;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        (void)setpgid(0, 0);
        reset_child_signals();

        if (out_file) {
            fd = open(out_file, O_WRONLY | O_TRUNC, 0666);
            if (fd < 0) { perror("open"); exit(1); }
            dup2(fd, 1); // redirect stdout
            close(fd);
        }

        if (in_file) {
            fd = open(in_file, O_RDONLY);
            if (fd < 0) { perror("open"); exit(1); }
            dup2(fd, 0); // redirect stdin
            //close(fd);
        }

        // Child process
        if (execve(args[0], args,NULL) == -1) {
            char system_path[MAX_INPUT_SIZE];
            snprintf(system_path, sizeof(system_path), "/bin/%s", args[0]);
            execve(system_path, args, NULL);
            snprintf(system_path, sizeof(system_path), "/usr/bin/%s", args[0]);
            execve(system_path, args, NULL);
            perror("execvp");
            exit(EXIT_FAILURE);
        }


    } else {
        // Parent process
        (void)setpgid(pid, pid);
        if (!background) {
            int status;

            give_terminal_to(pid);
            if (wait_for_process(pid, &status, WUNTRACED) < 0) {
                reclaim_terminal();
                return 1;
            }
            reclaim_terminal();
            if (WIFSTOPPED(status)) {
                pid_t pids[1] = {pid};
                (void)add_job(pids, 1, JOB_STOPPED, command_text);
            }
            return decode_wait_status(status);
        }
        {
            pid_t pids[1] = {pid};
            (void)add_job(pids, 1, JOB_RUNNING, command_text);
        }
        return 0;
    }
    return 0;
}

static int execute_pipeline(char *commands[][MAX_ARGS], int command_count,
                            int background, const char *command_text)
{
    int previous_read = -1;
    int pids[MAX_PIPE_COMMANDS] = {0};
    int status;
    int pipeline_status = 1;
    pid_t pipeline_group = 0;

    for (int i = 0; i < command_count; i++) {
        int next_pipe[2] = {-1, -1};
        if (i + 1 < command_count && pipe(next_pipe) < 0) {
            perror("pipe");
            break;
        }
        pids[i] = fork();
        if (pids[i] == 0) {
            (void)setpgid(0, pipeline_group == 0 ? getpid() :
                                                   pipeline_group);
            reset_child_signals();
            if (previous_read >= 0) dup2(previous_read, 0);
            if (next_pipe[1] >= 0) dup2(next_pipe[1], 1);
            if (previous_read >= 0) close(previous_read);
            if (next_pipe[0] >= 0) close(next_pipe[0]);
            if (next_pipe[1] >= 0) close(next_pipe[1]);
            if (execve(commands[i][0], commands[i], NULL) == -1) {
                char system_path[MAX_INPUT_SIZE];
                snprintf(system_path, sizeof(system_path), "/bin/%s", commands[i][0]);
                execve(system_path, commands[i], NULL);
                snprintf(system_path, sizeof(system_path), "/usr/bin/%s", commands[i][0]);
                execve(system_path, commands[i], NULL);
            }
            perror("exec");
            exit(1);
        }
        if (pids[i] < 0) {
            perror("fork");
            if (next_pipe[0] >= 0) close(next_pipe[0]);
            if (next_pipe[1] >= 0) close(next_pipe[1]);
            break;
        }
        if (pipeline_group == 0)
            pipeline_group = pids[i];
        (void)setpgid(pids[i], pipeline_group);
        if (previous_read >= 0) close(previous_read);
        if (next_pipe[1] >= 0) close(next_pipe[1]);
        previous_read = next_pipe[0];
    }
    if (previous_read >= 0) close(previous_read);
    if (background) {
        (void)add_job(pids, command_count, JOB_RUNNING, command_text);
        return 0;
    }
    give_terminal_to(pipeline_group);
    for (int i = 0; i < command_count; i++) {
        if (pids[i] > 0 &&
            wait_for_process(pids[i], &status, WUNTRACED) > 0) {
            if (WIFSTOPPED(status)) {
                for (int process = 0; process < command_count; process++)
                    if (pids[process] > 0 && process != i)
                        (void)kill(pids[process], SIGSTOP);
                pipeline_status = 128 + WSTOPSIG(status);
            } else {
                pids[i] = 0;
                if (i == command_count - 1)
                    pipeline_status = decode_wait_status(status);
            }
        }
    }
    reclaim_terminal();
    for (int i = 0; i < command_count; i++)
        if (pids[i] > 0) {
            (void)add_job(pids, command_count, JOB_STOPPED, command_text);
            break;
        }
    return pipeline_status;
}

int main() {
    char input[MAX_INPUT_SIZE];
    struct stat input_status;
    int interactive = fstat(STDIN_FILENO, &input_status) == 0 &&
                      input_status.st_mode == I_DEV;

    setbuf(stdout,0);
    if (interactive) {
        shell_process_group = getpid();
        if (getpgrp() != shell_process_group)
            (void)setpgid(0, shell_process_group);
        shell_terminal = STDIN_FILENO;
        if (tcsetpgrp(shell_terminal, shell_process_group) < 0)
            shell_terminal = -1;
        (void)signal(SIGINT, SIG_IGN);
        (void)signal(SIGQUIT, SIG_IGN);
        (void)signal(SIGTSTP, SIG_IGN);
        (void)signal(SIGTTIN, SIG_IGN);
        (void)signal(SIGTTOU, SIG_IGN);
    }
    while (1) {
        reap_jobs(interactive);
        if (interactive)
            printf("$ ");
        if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL) {
            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            break;
        }

        // Replace newline character with null terminator
        size_t input_length = strlen(input);
        if (input_length > 0 && input[input_length - 1] == '\n') {
            input[input_length - 1] = '\0';
        }

        char *commands[MAX_PIPE_COMMANDS][MAX_ARGS];
        char command_text[MAX_INPUT_SIZE];
        int background = 0;

        snprintf(command_text, sizeof(command_text), "%s", input);
        char *token = strtok(input, " ");
        int command_count = 1;
        int arg_counts[MAX_PIPE_COMMANDS] = {0};
        char *in_file = NULL;
        char *out_file = NULL;
        char status_text[16];
        char *glob_allocations[MAX_PIPE_COMMANDS * MAX_ARGS];
        int glob_allocation_count = 0;

        snprintf(status_text, sizeof(status_text), "%d", last_status);

        while (token != NULL) {
            if (strcmp(token, "$?") == 0)
                token = status_text;
            if (strcmp(token, "|") == 0) {
                if (arg_counts[command_count - 1] == 0 || command_count == MAX_PIPE_COMMANDS) {
                    fprintf(stderr, "pipe: missing command\n");
                    command_count = 0;
                    break;
                }
                commands[command_count - 1][arg_counts[command_count - 1]] = NULL;
                command_count++;
                token = strtok(NULL, " ");
                continue;
            }
            if (strcmp(token, "<") == 0) {
                token = strtok(NULL, " ");
                in_file = token;
            } else if (strcmp(token, ">") == 0) {
                token = strtok(NULL, " ");
                out_file = token;
            } else {
                if (arg_counts[command_count - 1] < MAX_ARGS - 1)
                    commands[command_count - 1][arg_counts[command_count - 1]++] = token;
            }
            token = strtok(NULL, " ");
        }

        if (command_count == 0) continue;
        commands[command_count - 1][arg_counts[command_count - 1]] = NULL;

        for (int i = 0; i < command_count; i++)
            expand_command_globs(commands[i], &arg_counts[i], glob_allocations,
                                 &glob_allocation_count);

        if (arg_counts[0] > 0) {
            if (arg_counts[command_count - 1] > 0 &&
                strcmp(commands[command_count - 1]
                               [arg_counts[command_count - 1] - 1], "&") == 0) {
                background = 1;
                commands[command_count - 1]
                        [--arg_counts[command_count - 1]] = NULL;
            }
            if (command_count > 1) {
                if (arg_counts[command_count - 1] == 0) {
                    fprintf(stderr, "pipe: missing command\n");
                } else {
                    last_status = execute_pipeline(commands, command_count,
                                                   background, command_text);
                }
                for (int i = 0; i < glob_allocation_count; i++)
                    free(glob_allocations[i]);
                continue;
            }
            if (arg_counts[0] > 0)
                last_status = execute_command(commands[0], background,
                                              in_file, out_file,
                                              command_text);
        }
        for (int i = 0; i < glob_allocation_count; i++)
            free(glob_allocations[i]);
    }

    return 0;
}
