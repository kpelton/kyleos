#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define OOM_GUARD_KB (64UL * 1024)

static unsigned long free_memory_kb(void)
{
    char report[256];
    char *free_line;
    int fd = open("/proc/meminfo", O_RDONLY);
    int count;

    if (fd < 0)
        return 0;
    count = read(fd, report, sizeof(report) - 1);
    close(fd);
    if (count <= 0)
        return 0;
    report[count] = '\0';
    free_line = strstr(report, "MemFree:");
    if (free_line == NULL)
        return 0;
    return strtoul(free_line + strlen("MemFree:"), NULL, 10);
}

int main(void)
{
    unsigned long free_kb = free_memory_kb();
    unsigned long bytes;
    unsigned long pages;
    volatile unsigned char *heap;

    if (free_kb == 0) {
        puts("OOM-TEST: FAIL unable to read MemFree");
        return 2;
    }
    if (free_kb > ULONG_MAX / 1024 - OOM_GUARD_KB) {
        puts("OOM-TEST: FAIL memory size overflow");
        return 2;
    }
    bytes = (free_kb + OOM_GUARD_KB) * 1024;
    pages = bytes / PAGE_SIZE;
    heap = sbrk(bytes);
    if (heap == (void *)-1) {
        puts("OOM-TEST: FAIL reservation rejected");
        return 2;
    }
    printf("OOM-TEST: touching %lu pages with %lu kB free\n",
           pages, free_kb);
    for (unsigned long i = 0; i < pages; i++)
        heap[i * PAGE_SIZE] = (unsigned char)i;
    puts("OOM-TEST: ERROR allocation unexpectedly completed");
    return 1;
}
