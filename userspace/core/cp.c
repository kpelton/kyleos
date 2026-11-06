#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define BUFFER_SIZE 4096
#define PATH_BUFFER_SIZE 1024
#define KYLEOS_DIRECTORY_MODE 40000

static int is_directory(const char *path)
{
    struct stat status;

    return stat(path, &status) == 0 &&
           (S_ISDIR(status.st_mode) ||
            status.st_mode == KYLEOS_DIRECTORY_MODE);
}

static int destination_path(char *output, size_t output_size,
                            const char *source, const char *destination)
{
    const char *base_end;
    const char *base_start;
    size_t destination_length;
    size_t base_length;
    int needs_slash;

    if (!is_directory(destination)) {
        destination_length = strlen(destination);
        if (destination_length >= output_size)
            return -1;
        memcpy(output, destination, destination_length + 1);
        return 0;
    }

    base_end = source + strlen(source);
    while (base_end > source && base_end[-1] == '/')
        base_end--;
    base_start = base_end;
    while (base_start > source && base_start[-1] != '/')
        base_start--;
    base_length = (size_t)(base_end - base_start);
    destination_length = strlen(destination);
    needs_slash = destination_length > 0 &&
                  destination[destination_length - 1] != '/';
    if (base_length == 0 || destination_length + needs_slash + base_length >=
                            output_size)
        return -1;

    memcpy(output, destination, destination_length);
    if (needs_slash)
        output[destination_length++] = '/';
    memcpy(output + destination_length, base_start, base_length);
    output[destination_length + base_length] = '\0';
    return 0;
}

static int same_file(const char *left, const char *right)
{
    struct stat left_status;
    struct stat right_status;

    return stat(left, &left_status) == 0 &&
           stat(right, &right_status) == 0 &&
           left_status.st_dev == right_status.st_dev &&
           left_status.st_ino == right_status.st_ino;
}

int main(int argc, char *argv[])
{
    char destination[PATH_BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    FILE *source_file;
    FILE *destination_file;
    size_t bytes_read;
    int source_close;
    int destination_close;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source> <destination>\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (destination_path(destination, sizeof(destination), argv[1], argv[2]) < 0) {
        fprintf(stderr, "cp: destination path is too long or invalid\n");
        return EXIT_FAILURE;
    }
    if (same_file(argv[1], destination)) {
        fprintf(stderr, "cp: source and destination are the same file\n");
        return EXIT_FAILURE;
    }

    source_file = fopen(argv[1], "rb");
    if (source_file == NULL) {
        perror("cp: failed to open source");
        return EXIT_FAILURE;
    }
    destination_file = fopen(destination, "wb");
    if (destination_file == NULL) {
        perror("cp: failed to open destination");
        fclose(source_file);
        return EXIT_FAILURE;
    }

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source_file)) > 0) {
        if (fwrite(buffer, 1, bytes_read, destination_file) != bytes_read) {
            perror("cp: write error");
            fclose(source_file);
            fclose(destination_file);
            return EXIT_FAILURE;
        }
    }
    if (ferror(source_file)) {
        perror("cp: read error");
        fclose(source_file);
        fclose(destination_file);
        return EXIT_FAILURE;
    }
    source_close = fclose(source_file);
    destination_close = fclose(destination_file);
    if (source_close != 0 || destination_close != 0) {
        perror("cp: close error");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
