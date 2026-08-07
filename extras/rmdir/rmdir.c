#include <stdio.h>

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
        fprintf(stderr, "Usage: rmdir <empty-directory>\n");
        return 1;
    }
    if (kyleos_rmdir(argv[1]) < 0) {
        perror("rmdir");
        return 1;
    }
    return 0;
}
