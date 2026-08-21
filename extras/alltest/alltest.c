#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define EXEC_STRESS_ITERATIONS 200
#define COPY_ITERATIONS 4
#define COPY_BUFFER_SIZE 4096

#ifndef ALLTEST_ROOT
#define ALLTEST_ROOT ""
#endif

#define TEST_PATH(path) ALLTEST_ROOT path

struct program_test {
    const char *name;
    const char *path;
    int expected_status;
};

static const struct program_test program_tests[] = {
    { "fs", TEST_PATH("/bin/fstest"), 0 },
    { "kmsg", TEST_PATH("/bin/kmsgtest"), 0 },
    { "wait", TEST_PATH("/bin/waittest"), 0 },
    { "pipe", TEST_PATH("/bin/pipetest"), 0 },
    { "demand", TEST_PATH("/bin/demandtest"), 0 },
    { "gp", TEST_PATH("/bin/gpfault"), 139 },
    { "oom", TEST_PATH("/bin/oomtest"), 139 },
};

static int wait_status_code(int status)
{
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 255;
}

static int run_process(const char *path, char *const argv[], int quiet)
{
    pid_t child = fork();
    int status = 0;

    if (child < 0)
        return 255;
    if (child == 0) {
        if (quiet) {
            int null_fd = open("/dev/null", O_WRONLY);

            if (null_fd < 0 || dup2(null_fd, STDOUT_FILENO) < 0)
                _exit(126);
            close(null_fd);
        }
        execve(path, argv, NULL);
        _exit(127);
    }
    if (waitpid(child, &status, 0) != child)
        return 255;
    return wait_status_code(status);
}

static int run_program_test(const struct program_test *test)
{
    char *argv[] = { (char *)test->path, NULL };
    int status;

    printf("TEST %s START\n", test->name);
    status = run_process(test->path, argv, 0);
    if (status != test->expected_status) {
        printf("TEST %s FAIL expected=%d actual=%d\n",
               test->name, test->expected_status, status);
        return 1;
    }
    printf("TEST %s PASS status=%d\n", test->name, status);
    return 0;
}

static int files_equal(const char *left_path, const char *right_path)
{
    unsigned char left[COPY_BUFFER_SIZE];
    unsigned char right[COPY_BUFFER_SIZE];
    struct stat left_stat;
    struct stat right_stat;
    int left_fd = -1;
    int right_fd = -1;
    int equal = 0;

    if (stat(left_path, &left_stat) < 0 ||
        stat(right_path, &right_stat) < 0 ||
        left_stat.st_size != right_stat.st_size)
        goto out;
    left_fd = open(left_path, O_RDONLY);
    right_fd = open(right_path, O_RDONLY);
    if (left_fd < 0 || right_fd < 0)
        goto out;
    for (;;) {
        int left_count = read(left_fd, left, sizeof(left));
        int right_count = read(right_fd, right, sizeof(right));

        if (left_count < 0 || right_count < 0 || left_count != right_count)
            goto out;
        if (left_count == 0) {
            equal = 1;
            break;
        }
        if (memcmp(left, right, left_count) != 0)
            goto out;
    }

out:
    if (left_fd >= 0)
        close(left_fd);
    if (right_fd >= 0)
        close(right_fd);
    return equal;
}

static int run_copy_test(void)
{
    static const char *destinations[COPY_ITERATIONS] = {
        TEST_PATH("/tmp/alltest-copy-a"),
        TEST_PATH("/tmp/alltest-copy-b"),
        TEST_PATH("/tmp/alltest-copy-c"),
        TEST_PATH("/tmp/alltest-copy-d"),
    };
    const char *source = TEST_PATH("/bin/nushell");

    puts("TEST copy START");
    for (int i = 0; i < COPY_ITERATIONS; i++) {
        char *copy_argv[] = {
            TEST_PATH("/bin/cp"), (char *)source,
            (char *)destinations[i], NULL
        };
        char *remove_argv[] = {
            TEST_PATH("/bin/rm"), (char *)destinations[i], NULL
        };
        struct stat removed;
        int copy_status = run_process(TEST_PATH("/bin/cp"), copy_argv, 1);

        if (copy_status != 0 || !files_equal(source, destinations[i])) {
            printf("TEST copy FAIL iteration=%d copy_status=%d\n",
                   i + 1, copy_status);
            unlink(destinations[i]);
            return 1;
        }
        if (run_process(TEST_PATH("/bin/rm"), remove_argv, 1) != 0 ||
            stat(destinations[i], &removed) == 0) {
            printf("TEST copy FAIL iteration=%d cleanup\n", i + 1);
            unlink(destinations[i]);
            return 1;
        }
    }
    printf("TEST copy PASS iterations=%d\n", COPY_ITERATIONS);
    return 0;
}

static int run_exec_stress_test(void)
{
    char *argv[] = { TEST_PATH("/bin/ls"), NULL };

    puts("TEST exec START");
    for (int i = 0; i < EXEC_STRESS_ITERATIONS; i++) {
        int status = run_process(TEST_PATH("/bin/ls"), argv, 1);

        if (status != 0) {
            printf("TEST exec FAIL iteration=%d status=%d\n", i + 1, status);
            return 1;
        }
    }
    printf("TEST exec PASS iterations=%d\n", EXEC_STRESS_ITERATIONS);
    return 0;
}

static int run_named_test(const char *name)
{
    if (strcmp(name, "copy") == 0)
        return run_copy_test();
    if (strcmp(name, "exec") == 0)
        return run_exec_stress_test();
    for (unsigned int i = 0;
         i < sizeof(program_tests) / sizeof(program_tests[0]); i++)
        if (strcmp(name, program_tests[i].name) == 0)
            return run_program_test(&program_tests[i]);
    fprintf(stderr, "alltest: unknown test: %s\n", name);
    return 2;
}

int main(int argc, char **argv)
{
    int completed = 0;

    if (argc == 2)
        return run_named_test(argv[1]);
    if (argc != 1) {
        fprintf(stderr,
                "Usage: alltest [fs|copy|kmsg|wait|pipe|demand|gp|oom|exec]\n");
        return 2;
    }

    puts("ALL-TESTS-START");
    for (unsigned int i = 0;
         i < sizeof(program_tests) / sizeof(program_tests[0]); i++) {
        if (i == 1) {
            if (run_copy_test() != 0) {
                puts("ALL-TESTS-FAILED copy");
                return 1;
            }
            completed++;
        }
        if (run_program_test(&program_tests[i]) != 0) {
            printf("ALL-TESTS-FAILED %s\n", program_tests[i].name);
            return 1;
        }
        completed++;
    }
    if (run_exec_stress_test() != 0) {
        puts("ALL-TESTS-FAILED exec");
        return 1;
    }
    completed++;
    printf("ALL-TESTS-COMPLETE count=%d\n", completed);
    return 0;
}
