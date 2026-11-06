#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (unlink(argv[1]) < 0) {
        perror("rm");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
