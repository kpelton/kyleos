#include <stdio.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define OOM_PAGES (128 * 1024 * 1024 / PAGE_SIZE)

int main(void)
{
    volatile unsigned char *heap = sbrk(OOM_PAGES * PAGE_SIZE);

    if (heap == (void *)-1) {
        puts("OOM-TEST: reservation rejected safely");
        return 0;
    }
    puts("OOM-TEST: touching reserved pages");
    for (unsigned long i = 0; i < OOM_PAGES; i++)
        heap[i * PAGE_SIZE] = (unsigned char)i;
    puts("OOM-TEST: ERROR allocation unexpectedly completed");
    return 1;
}
