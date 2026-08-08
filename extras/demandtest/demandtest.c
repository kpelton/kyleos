#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define HEAP_PAGES 256
#define STACK_PAGES 12

static const unsigned char file_backed_data[PAGE_SIZE * 3] = {
    [0] = 0x19,
    [PAGE_SIZE] = 0x42,
    [PAGE_SIZE * 3 - 1] = 0xa7,
};
static volatile unsigned char demand_bss[PAGE_SIZE * 2];

static int test_exec_pages(void)
{
    if (file_backed_data[0] != 0x19 ||
        file_backed_data[PAGE_SIZE] != 0x42 ||
        file_backed_data[PAGE_SIZE * 3 - 1] != 0xa7)
        return -1;
    if (demand_bss[0] != 0 || demand_bss[PAGE_SIZE * 2 - 1] != 0)
        return -1;
    demand_bss[PAGE_SIZE * 2 - 1] = 0x5c;
    return demand_bss[PAGE_SIZE * 2 - 1] == 0x5c ? 0 : -1;
}

static int test_stack(void)
{
    volatile unsigned char stack[STACK_PAGES * PAGE_SIZE];

    for (int i = 0; i < STACK_PAGES; i++)
        stack[i * PAGE_SIZE] = (unsigned char)(i + 1);
    for (int i = 0; i < STACK_PAGES; i++)
        if (stack[i * PAGE_SIZE] != (unsigned char)(i + 1))
            return -1;
    return 0;
}

int main(void)
{
    unsigned char *heap = sbrk(HEAP_PAGES * PAGE_SIZE);
    pid_t child;
    int status = 0;

    if (test_exec_pages() < 0) {
        puts("FAIL demand exec pages");
        return 1;
    }

    if (heap == (void *)-1) {
        puts("FAIL demand heap reserve");
        return 1;
    }
    if (heap[0] != 0 || heap[(HEAP_PAGES - 1) * PAGE_SIZE] != 0) {
        puts("FAIL demand zero fill");
        return 1;
    }
    heap[0] = 0x31;
    heap[(HEAP_PAGES - 1) * PAGE_SIZE] = 0x79;
    if (test_stack() < 0) {
        puts("FAIL demand stack growth");
        return 1;
    }

    child = fork();
    if (child < 0) {
        puts("FAIL demand fork");
        return 1;
    }
    if (child == 0) {
        if (heap[0] != 0x31 || heap[(HEAP_PAGES - 1) * PAGE_SIZE] != 0x79)
            _exit(2);
        heap[0] = 0x55;
        heap[HEAP_PAGES / 2 * PAGE_SIZE] = 0x66;
        _exit(0);
    }
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0 ||
        heap[0] != 0x31 || heap[HEAP_PAGES / 2 * PAGE_SIZE] != 0) {
        puts("FAIL demand COW");
        return 1;
    }
    puts("PASS demand paging heap stack COW");
    return 0;
}
