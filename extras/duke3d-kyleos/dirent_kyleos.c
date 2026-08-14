#include "dirent.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

extern int getdents(int fd, struct dirent *entries, int count);

DIR *fdopendir(int fd)
{
    DIR *directory;

    if (fd < 0)
        return NULL;
    directory = malloc(sizeof(*directory));
    if (directory == NULL) {
        close(fd);
        return NULL;
    }
    directory->fd = fd;
    directory->index = 0;
    directory->count = getdents(fd, directory->entries, KYLEOS_DIRENT_MAX);
    if (directory->count < 0) {
        close(fd);
        free(directory);
        return NULL;
    }
    return directory;
}

DIR *opendir(const char *path)
{
    return fdopendir(open(path, O_RDONLY));
}

struct dirent *readdir(DIR *directory)
{
    if (directory == NULL || directory->index >= directory->count)
        return NULL;
    return &directory->entries[directory->index++];
}

int closedir(DIR *directory)
{
    int result;

    if (directory == NULL)
        return -1;
    result = close(directory->fd);
    free(directory);
    return result;
}

void rewinddir(DIR *directory)
{
    if (directory != NULL)
        directory->index = 0;
}

int dirfd(DIR *directory)
{
    return directory == NULL ? -1 : directory->fd;
}
