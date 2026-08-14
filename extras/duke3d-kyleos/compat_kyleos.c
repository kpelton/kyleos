#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "kyleos_syscalls.h"

clock_t clock(void)
{
    uint64_t milliseconds = kyleos_ticks_ms();
    return (clock_t)(milliseconds * CLOCKS_PER_SEC / 1000U);
}

int access(const char *path, int mode)
{
    int fd;

    (void)mode;
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    close(fd);
    return 0;
}
