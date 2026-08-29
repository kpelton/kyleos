#include <stdio.h>
#include <unistd.h>

#include "path_destination.h"

#define COPY_BUFFER_SIZE 4096

int rename(const char *old_path, const char *new_path);

static int copy_and_unlink(const char *source, const char *destination)
{
    char buffer[COPY_BUFFER_SIZE];
    FILE *input = fopen(source, "rb");
    FILE *output;
    size_t count;
    int input_close;
    int output_close;

    if (input == NULL)
        return -1;
    output = fopen(destination, "wb");
    if (output == NULL) {
        fclose(input);
        return -1;
    }
    while ((count = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, count, output) != count) {
            fclose(input);
            fclose(output);
            return -1;
        }
    }
    if (ferror(input)) {
        fclose(input);
        fclose(output);
        return -1;
    }
    input_close = fclose(input);
    output_close = fclose(output);
    if (input_close != 0 || output_close != 0)
        return -1;
    return unlink(source);
}

int main(int argc, char **argv)
{
    char destination[PATH_BUFFER_SIZE];

    if (argc != 3) {
        fprintf(stderr, "Usage: mv source destination\n");
        return 1;
    }
    if (resolve_destination(destination, sizeof(destination),
                            argv[1], argv[2]) < 0) {
        fprintf(stderr, "mv: destination path is too long or invalid\n");
        return 1;
    }
    if (rename(argv[1], destination) < 0 &&
        copy_and_unlink(argv[1], destination) < 0) {
        fprintf(stderr, "mv: rename failed\n");
        return 1;
    }
    return 0;
}
