#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int checks;
static int failures;

static int kyleos_rmdir(const char *path)
{
    long result;

    __asm__ volatile("mov $29, %%rax; int $0x80"
                     : "=a"(result)
                     : "D"(path)
                     : "memory");
    return (int)result;
}

static void check(const char *name, int passed)
{
    checks++;
    if (passed) {
        printf("PASS %s\n", name);
    } else {
        printf("FAIL %s\n", name);
        failures++;
    }
}

static int file_contains(const char *path, const char *expected, int length)
{
    char buffer[32];
    int fd = open(path, O_RDONLY);
    int count;

    if (fd < 0)
        return 0;
    count = read(fd, buffer, sizeof(buffer));
    close(fd);
    return count == length && memcmp(buffer, expected, length) == 0;
}

static int copy_file(const char *source, const char *destination)
{
    char buffer[4096];
    int input = open(source, O_RDONLY);
    int output = -1;
    int result = -1;

    if (input < 0)
        return -1;
    output = open(destination, O_WRONLY | O_TRUNC);
    if (output < 0)
        goto out;
    for (;;) {
        int count = read(input, buffer, sizeof(buffer));
        int written = 0;

        if (count < 0)
            goto out;
        if (count == 0)
            break;
        while (written < count) {
            int amount = write(output, buffer + written, count - written);

            if (amount <= 0)
                goto out;
            written += amount;
        }
    }
    result = 0;

out:
    close(input);
    if (output >= 0)
        close(output);
    return result;
}

static int run_relative(const char *path)
{
    pid_t child = fork();
    int status = 0;

    if (child < 0)
        return 0;
    if (child == 0) {
        char *arguments[] = { (char *)path, "relative-path", NULL };

        execve(path, arguments, NULL);
        _exit(127);
    }
    return waitpid(child, &status, 0) == child && WIFEXITED(status) &&
           WEXITSTATUS(status) == 0;
}

int main(void)
{
    const char *fat_file = "/fstest";
    const char *fat_moved = "/fstmoved";
    const char *fat_empty = "/fsempty";
    const char *fat_full = "/fsfull";
    const char *fat_child = "/fsfull/child";
    const char *tmp_dir = "/tmp/fstest";
    const char *tmp_file = "/tmp/fstest/data";
    const char *tmp_moved = "/tmp/fstest/moved";
    const char *tmp_empty = "/tmp/fstest/empty";
    const char *tmp_full = "/tmp/fstest/full";
    const char *tmp_child = "/tmp/fstest/full/child";
    const char *tmp_runner = "/tmp/fstest/runner";
    struct stat st;
    char zeros[16];
    int fd;
    int count;
    int all_zero;

    unlink(fat_file);
    unlink(fat_moved);
    kyleos_rmdir(fat_empty);
    kyleos_rmdir(fat_child);
    kyleos_rmdir(fat_full);
    unlink(tmp_file);
    unlink(tmp_moved);
    unlink(tmp_runner);
    kyleos_rmdir(tmp_empty);
    kyleos_rmdir(tmp_child);
    kyleos_rmdir(tmp_full);
    kyleos_rmdir(tmp_dir);

    printf("KyleOS filesystem regression suite\n");

    fd = open(fat_file, O_WRONLY | O_TRUNC);
    check("FAT create", fd >= 0);
    if (fd >= 0) {
        check("FAT write", write(fd, "hello", 5) == 5);
        close(fd);
    }
    check("FAT read", file_contains(fat_file, "hello", 5));
    check("FAT stat size", stat(fat_file, &st) == 0 && st.st_size == 5);

    fd = open(fat_file, O_RDWR);
    check("FAT open read-write", fd >= 0);
    if (fd >= 0) {
        check("FAT seek", lseek(fd, 1, SEEK_SET) == 1);
        check("FAT overwrite", write(fd, "X", 1) == 1);
        close(fd);
    }
    check("FAT seek result", file_contains(fat_file, "hXllo", 5));
    check("FAT rename", rename(fat_file, fat_moved) == 0);
    check("FAT rename source gone", stat(fat_file, &st) < 0);
    check("FAT rename destination", file_contains(fat_moved, "hXllo", 5));

    fd = open(fat_moved, O_WRONLY | O_TRUNC);
    check("FAT truncate open", fd >= 0);
    if (fd >= 0)
        close(fd);
    check("FAT truncate size", stat(fat_moved, &st) == 0 && st.st_size == 0);
    check("FAT unlink", unlink(fat_moved) == 0);
    check("FAT unlink gone", stat(fat_moved, &st) < 0);
    check("FAT empty directory mkdir", mkdir(fat_empty, 0) == 0);
    check("FAT empty directory rmdir", kyleos_rmdir(fat_empty) == 0);
    check("FAT non-empty parent mkdir", mkdir(fat_full, 0) == 0);
    check("FAT non-empty child mkdir", mkdir(fat_child, 0) == 0);
    check("FAT non-empty directory refused", kyleos_rmdir(fat_full) < 0);
    check("FAT nested child rmdir", kyleos_rmdir(fat_child) == 0);
    check("FAT nested parent rmdir", kyleos_rmdir(fat_full) == 0);

    check("tmp mkdir", mkdir(tmp_dir, 0) == 0);
    fd = open(tmp_file, O_WRONLY | O_TRUNC);
    check("tmp create", fd >= 0);
    if (fd >= 0) {
        check("tmp write", write(fd, "ram", 3) == 3);
        close(fd);
    }
    check("tmp read", file_contains(tmp_file, "ram", 3));
    check("tmp chdir", chdir(tmp_dir) == 0);
    check("tmp relative rename", rename("data", "moved") == 0);
    check("tmp relative rename source gone", stat("data", &st) < 0);
    check("tmp relative stat destination",
          stat("moved", &st) == 0 && st.st_size == 3);
    check("tmp relative rename destination", file_contains("moved", "ram", 3));
    check("tmp relative executable copy", copy_file("/bin/echo", "runner") == 0);
    check("tmp relative executable stat",
          stat("runner", &st) == 0 && st.st_size > 0);
    check("tmp bare relative exec", run_relative("runner"));
    check("tmp dot-slash relative exec", run_relative("./runner"));
    check("tmp relative executable unlink", unlink("runner") == 0);
    check("tmp relative executable gone", stat("runner", &st) < 0);
    check("tmp relative empty mkdir", mkdir("empty", 0) == 0);
    check("tmp relative empty stat", stat("empty", &st) == 0);
    check("tmp relative empty rmdir", kyleos_rmdir("empty") == 0);
    check("tmp relative empty gone", stat("empty", &st) < 0);
    check("tmp relative parent mkdir", mkdir("full", 0) == 0);
    check("tmp relative child mkdir", mkdir("full/child", 0) == 0);
    check("tmp relative non-empty refused", kyleos_rmdir("full") < 0);
    check("tmp relative child stat", stat("full/child", &st) == 0);
    check("tmp relative chdir", chdir("full") == 0);
    check("tmp absolute chdir restore", chdir(tmp_dir) == 0);
    check("tmp relative child rmdir", kyleos_rmdir("full/child") == 0);
    check("tmp relative parent rmdir", kyleos_rmdir("full") == 0);
    check("tmp relative unlink", unlink("moved") == 0);
    check("tmp relative unlink gone", stat("moved", &st) < 0);
    check("root chdir", chdir("/") == 0);
    check("tmp base rmdir", kyleos_rmdir(tmp_dir) == 0);

    fd = open("/dev/zero", O_RDONLY);
    count = fd < 0 ? -1 : read(fd, zeros, sizeof(zeros));
    if (fd >= 0)
        close(fd);
    all_zero = 1;
    for (int i = 0; i < (int)sizeof(zeros); i++)
        if (zeros[i] != 0)
            all_zero = 0;
    check("/dev/zero", count == (int)sizeof(zeros) && all_zero);

    fd = open("/dev/null", O_WRONLY | O_TRUNC);
    count = fd < 0 ? -1 : write(fd, "x", 1);
    if (fd >= 0)
        close(fd);
    check("/dev/null redirect write", count == 1);

    fd = open("/dev/null", O_RDONLY);
    count = fd < 0 ? -1 : read(fd, zeros, sizeof(zeros));
    if (fd >= 0)
        close(fd);
    check("/dev/null read EOF", count == 0);

    printf("FS REPORT: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
