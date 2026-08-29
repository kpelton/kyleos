#ifndef KYLEOS_BUSYBOX_SYS_MMAN_H
#define KYLEOS_BUSYBOX_SYS_MMAN_H

#include <stddef.h>
#include <sys/types.h>

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *)-1)

void *mmap(void *address, size_t length, int protection, int flags,
           int fd, off_t offset);
int munmap(void *address, size_t length);

#endif
