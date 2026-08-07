#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
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

int main(void)
{
    const char *fat_file = "/fstest";
    const char *fat_moved = "/fstmoved";
    const char *fat_empty = "/fsempty";
    const char *fat_full = "/fsfull";
    const char *fat_child = "/fsfull/child";
    const char *tmp_dir = "/tmp/fstest";
    const char *tmp_file = "/tmp/fstest/data";
    const char *tmp_empty = "/tmp/fstest/empty";
    const char *tmp_full = "/tmp/fstest/full";
    const char *tmp_child = "/tmp/fstest/full/child";
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
    check("tmp unlink", unlink(tmp_file) == 0);

    check("empty directory mkdir", mkdir(tmp_empty, 0) == 0);
    check("empty directory rmdir", kyleos_rmdir(tmp_empty) == 0);
    check("empty directory gone", stat(tmp_empty, &st) < 0);
    check("non-empty parent mkdir", mkdir(tmp_full, 0) == 0);
    check("non-empty child mkdir", mkdir(tmp_child, 0) == 0);
    check("non-empty directory refused", kyleos_rmdir(tmp_full) < 0);
    check("non-empty child remains", stat(tmp_child, &st) == 0);
    check("nested child rmdir", kyleos_rmdir(tmp_child) == 0);
    check("nested parent rmdir", kyleos_rmdir(tmp_full) == 0);
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

    fd = open("/dev/null", O_WRONLY);
    count = fd < 0 ? -1 : write(fd, "x", 1);
    if (fd >= 0)
        close(fd);
    check("/dev/null", count == 1);

    printf("FS REPORT: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
