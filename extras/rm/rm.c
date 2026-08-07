#include <stdio.h>
#include <unistd.h>

static int kyleos_rmdir(const char *path)
{
    long result;

    __asm__ volatile("mov $29, %%rax; int $0x80"
                     : "=a"(result)
                     : "D"(path)
                     : "memory");
    return (int)result;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: rm <file-or-empty-directory>\n");
        return 1;
    }

    if (unlink(argv[1]) < 0 && kyleos_rmdir(argv[1]) < 0) {
        perror("rm");
        return 1;
    }
    return 0;
}
