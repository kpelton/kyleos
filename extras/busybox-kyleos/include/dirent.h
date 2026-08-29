#ifndef KYLEOS_BUSYBOX_DIRENT_H
#define KYLEOS_BUSYBOX_DIRENT_H

#include <stdint.h>
#include <string.h>

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10

struct dirent {
    uint64_t d_ino;
    char d_name[257];
    int d_type;
    uint64_t d_size;
};

typedef struct {
    int fd;
    int count;
    int index;
    struct dirent entries[256];
} DIR;

#define _D_EXACT_NAMLEN(entry) strlen((entry)->d_name)

DIR *opendir(const char *path);
DIR *fdopendir(int fd);
struct dirent *readdir(DIR *directory);
int closedir(DIR *directory);
void rewinddir(DIR *directory);
int dirfd(DIR *directory);

#endif
