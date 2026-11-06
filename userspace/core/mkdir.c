#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    // Check if correct number of arguments are provided
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <destination>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    

    return mkdir(argv[1],0);
}
