#ifndef KYLEOS_DIRENT_H
#define KYLEOS_DIRENT_H

#include <stdint.h>
#include <string.h>

#define KYLEOS_DIRENT_NAME_MAX 257
#define KYLEOS_DIRENT_MAX 1024

struct dirent {
    uint64_t d_ino;
    char d_name[KYLEOS_DIRENT_NAME_MAX];
    int d_type;
    uint64_t d_size;
};

typedef struct {
    int fd;
    int count;
    int index;
    struct dirent entries[KYLEOS_DIRENT_MAX];
} DIR;

#define _D_EXACT_NAMLEN(entry) strlen((entry)->d_name)

DIR *opendir(const char *path);
DIR *fdopendir(int fd);
struct dirent *readdir(DIR *directory);
int closedir(DIR *directory);
void rewinddir(DIR *directory);
int dirfd(DIR *directory);

#endif
