#ifndef PATH_DESTINATION_H
#define PATH_DESTINATION_H

#include <string.h>
#include <sys/stat.h>

#define PATH_BUFFER_SIZE 1024
#define KYLEOS_DIRECTORY_MODE 40000

static int path_is_directory(const char *path)
{
    struct stat status;

    return stat(path, &status) == 0 &&
           (S_ISDIR(status.st_mode) ||
            status.st_mode == KYLEOS_DIRECTORY_MODE);
}

static int resolve_destination(char *output, size_t output_size,
                               const char *source, const char *destination)
{
    const char *base_end;
    const char *base_start;
    size_t destination_length;
    size_t base_length;
    int needs_slash;

    if (!path_is_directory(destination)) {
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

#endif
