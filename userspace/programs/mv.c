#include <stdio.h>

int rename(const char *old_path, const char *new_path);

int main(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: mv source destination\n");
        return 1;
    }
    if (rename(argv[1], argv[2]) < 0) {
        printf("mv: rename failed\n");
        return 1;
    }
    return 0;
}
