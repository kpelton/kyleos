#include <mm/mm.h>
#include <output/output.h>
#include <include/types.h>
#include <mm/pmem.h>
#include <mm/vmm.h>
#include <mm/paging.h>
#include <locks/spinlock.h>

static struct spinlock kmem_spinlock;
extern uint64_t *kernel_pml4;

#define FREE 1
#define USED 0
#define HEAP_SIZE 8192
#define KERNEL_HEAP_ADDR 0xffffc00000000000
#define MM_BLOCK_MAGIC 0x4b4d454d424c4f43UL

static struct mm_block *head;
static char *heap_start;
static char *heap_end;
static uint64_t live_bytes;
static uint64_t peak_live_bytes;

static unsigned long align_size(unsigned long size)
{
    return (size + MM_ALIGNMENT - 1) & ~(MM_ALIGNMENT - 1);
}

static void split_block(struct mm_block *block, unsigned long size)
{
    struct mm_block *remaining;

    if (block->size < size + sizeof(*block) + MM_MIN_SIZE)
        return;
    remaining = (struct mm_block *)((char *)(block + 1) + size);
    remaining->size = block->size - size - sizeof(*remaining);
    remaining->next = block->next;
    remaining->prev = block;
    remaining->magic = MM_BLOCK_MAGIC;
    remaining->free = FREE;
    if (remaining->next != NULL)
        remaining->next->prev = remaining;
    block->next = remaining;
    block->size = size;
}

static void merge_with_next(struct mm_block *block)
{
    struct mm_block *next = block->next;

    if (next == NULL || !next->free)
        return;
    block->size += sizeof(*next) + next->size;
    block->next = next->next;
    if (block->next != NULL)
        block->next->prev = block;
}

void *kmalloc(unsigned int requested)
{
    struct mm_block *block;
    unsigned long size = requested;

    if (size < MM_MIN_SIZE)
        size = MM_MIN_SIZE;
    size = align_size(size);

    acquire_spinlock(&kmem_spinlock);
    for (block = head; block != NULL; block = block->next) {
        if (block->magic != MM_BLOCK_MAGIC) {
            panic("Kernel heap metadata corruption");
        }
        if (!block->free || block->size < size)
            continue;
        split_block(block, size);
        block->free = USED;
        live_bytes += block->size;
        if (live_bytes > peak_live_bytes)
            peak_live_bytes = live_bytes;
        release_spinlock(&kmem_spinlock);
        return (void *)(block + 1);
    }
    release_spinlock(&kmem_spinlock);
    panic("Kernel Heap out of memory");
    return NULL;
}

void kfree(void *ptr)
{
    struct mm_block *block;

    if (ptr == NULL)
        return;
    acquire_spinlock(&kmem_spinlock);
    block = ((struct mm_block *)ptr) - 1;
    if ((char *)block < heap_start || (char *)block >= heap_end ||
        block->magic != MM_BLOCK_MAGIC || block->free == FREE)
        panic("Kernel heap corruption on free");
    live_bytes -= block->size;
    block->free = FREE;
    merge_with_next(block);
    if (block->prev != NULL && block->prev->free) {
        block = block->prev;
        merge_with_next(block);
    }
    release_spinlock(&kmem_spinlock);
}

void mm_print_stats()
{
    struct mm_block *block;
    unsigned long free_bytes = 0;
    unsigned long largest_free = 0;
    unsigned long blocks = 0;

    acquire_spinlock(&kmem_spinlock);
    for (block = head; block != NULL; block = block->next) {
        if (block->magic != MM_BLOCK_MAGIC)
            panic("Kernel heap metadata corruption");
        if (block->free) {
            free_bytes += block->size;
            if (block->size > largest_free)
                largest_free = block->size;
        }
        blocks++;
    }
    release_spinlock(&kmem_spinlock);

    kprintf("Kernel heap live       %dK\n", live_bytes / 1024);
    kprintf("Kernel heap peak       %dK\n", peak_live_bytes / 1024);
    kprintf("Kernel heap free       %dK\n", free_bytes / 1024);
    kprintf("Kernel heap largest    %dK\n", largest_free / 1024);
    kprintf("Kernel heap blocks     %d\n", blocks);
}

void mm_init()
{
    struct pg_tbl pg;
    char *heap_loc;

    setup_paging();
    pg.pml4 = kernel_pml4;
    init_spinlock(&kmem_spinlock);
    heap_loc = pmem_alloc_block(HEAP_SIZE);
    kprintf("Heap Loc:0x%x\n", heap_loc);
    paging_map_range(&pg, (uint64_t)heap_loc, KERNEL_HEAP_ADDR, HEAP_SIZE,
                     KERNEL_PAGE);
    kernel_switch_paging();

    heap_start = (char *)KERNEL_HEAP_ADDR;
    heap_end = heap_start + HEAP_SIZE * PAGE_SIZE;
    head = (struct mm_block *)heap_start;
    head->size = HEAP_SIZE * PAGE_SIZE - sizeof(*head);
    head->next = NULL;
    head->prev = NULL;
    head->magic = MM_BLOCK_MAGIC;
    head->free = FREE;
    live_bytes = 0;
    peak_live_bytes = 0;
    vmm_init();
}
