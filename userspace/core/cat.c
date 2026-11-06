#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    FILE *file;
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    if (argc == 1) {
        file = stdin;
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0)
            fwrite(buffer, 1, bytes_read, stdout);
    } else {
        for (int i = 1; i < argc; i++) {
            file = fopen(argv[i], "r");
            if (file == NULL) {
                fprintf(stderr, "Could not open file: %s\n", argv[i]);
                return EXIT_FAILURE;
            }
            while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0)
                fwrite(buffer, 1, bytes_read, stdout);
            fclose(file);
        }
    }

    return 0;
}
