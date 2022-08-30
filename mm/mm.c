#include <mm/mm.h>
#include <output/output.h>
#include <include/types.h>
#include <mm/pmem.h>
#include <mm/paging.h>
#include <locks/spinlock.h>
static struct spinlock kmem_spinlock;

char *kernel_heap;
#define FREE 1
#define USED 0
// heap size in pages
#define HEAP_SIZE 4096
// TODO: Align this on cacheline boundry

static struct mm_block *head = 0;
static struct mm_block *tail = 0;
static uint64_t total_used = 0;

void *kmalloc(unsigned int p_size)
{
    uint64_t *ret;
    struct mm_block *lptr = head;
    unsigned int size = p_size;
    if (size < MM_MIN_SIZE)
        size = MM_MIN_SIZE;
    acquire_spinlock(&kmem_spinlock);

    while (lptr != 0)
    {
        //kprintf("MM  Trying to use 0x%x %x %x\n", lptr->addr, lptr->size,lptr->free);

        if (size == lptr->size && lptr->free == FREE)
        {
            if ((unsigned long)lptr->addr < 0xffffffff80000000)
            {
                kprintf("old alloc %x\n", (unsigned long)&lptr->addr);
                kprintf("old alloc %x\n", (unsigned long)&lptr->addr);
                kprintf("mem corrution detected %x\n", (unsigned long)&lptr->addr);

                continue;
            }
            lptr->free = USED;

            // kprintf("MM Allocating 0x%x 0x%x\n",p_size,lptr->addr);
            release_spinlock(&kmem_spinlock);

            //kprintf("MM reuse Alloc 0x%x %x\n", lptr->addr, size);

            return lptr->addr;
        }
        lptr = lptr->next;
    }
    // TODO: Add allignment
    struct mm_block *ptr = (void *)kernel_heap;
    kernel_heap += sizeof(struct mm_block);
    total_used += sizeof(struct mm_block);
    ret = (void *)kernel_heap;
    if (head == 0)
    {
        head = ptr;
        tail = head;
    }
    else
    {
        tail->next = ptr;
        tail = ptr;
    }
    ptr->size = size;
    ptr->next = 0;
    ptr->free = USED;
    ptr->addr = ret;
    // kprint_hex("Alloc ",size);
    //kprintf("MM Alloc 0x%x %x\n", ret, size);
    kernel_heap += size;
    total_used += size;
    if (total_used >= HEAP_SIZE*PAGE_SIZE)
        panic("Kernel Heap out of memory");
    release_spinlock(&kmem_spinlock);

    return ret;
}

void kfree(void *ptr)
{
    acquire_spinlock(&kmem_spinlock);

    struct mm_block *lptr = (struct mm_block *)(((uint64_t)ptr) - (sizeof(struct mm_block)));
    if (lptr->addr == ptr)
    {
        lptr->free = FREE;
      //  kprintf("MM Free %x size:%x\n", lptr->addr, lptr->size);
    }
    else
    {
        panic("Memory courrption detected on free\n");
    }
    release_spinlock(&kmem_spinlock);
}

void mm_print_stats()
{
    acquire_spinlock(&kmem_spinlock);
    struct mm_block *lptr = head;
    unsigned long size = 0;
    unsigned long ll_size = 0;
    int i = 0;
    while (lptr != 0)
    {
        kprintf("MM  List %d -> %x size:%x free:%x\n", i,lptr->addr, lptr->size,lptr->free);
        size += sizeof(struct mm_block);
        if (lptr->free == 0)
            size += lptr->size;

        ll_size += 1;
        lptr = lptr->next;
        i++;
    }

    release_spinlock(&kmem_spinlock);
    kprintf("Total Used Memory     %dK\n", size / 1024);
    kprintf("LL nodes              %d\n", ll_size);
    kprintf("LL node size          %dK\n", (ll_size * sizeof(struct mm_block)) / 1024);
    kprintf("End of kernel         0x%x\n", (unsigned long)&_kernel_end);
    kprintf("Start of kernel       0x%x\n", 0xffffffff80000000);
    kprintf("Total allocated         %d out of %d\n", total_used, HEAP_SIZE * 4096);
}

void mm_init()
{
    init_spinlock(&kmem_spinlock);
    setup_paging();
    char *heap_loc = pmem_alloc_block(HEAP_SIZE);
    kprintf("Heap Loc:0x%x\n", heap_loc);
    kernel_heap = (char *)KERN_PHYS_TO_VIRT(heap_loc);
    paging_map_kernel_range(KERN_VIRT_TO_PHYS(kernel_heap), HEAP_SIZE);
}
