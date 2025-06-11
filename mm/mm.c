#include <mm/mm.h>
#include <output/output.h>
#include <include/types.h>
#include <mm/pmem.h>
#include <mm/vmm.h>
#include <mm/paging.h>
#include <locks/spinlock.h>
static struct spinlock kmem_spinlock;
extern uint64_t *kernel_pml4;
char *kernel_heap;
#define FREE 1
#define USED 0
// heap size in pages
#define HEAP_SIZE 8192*20
#define KERNEL_HEAP_ADDR 0xffffc00000000000
// TODO: Align this on cacheline boundry

static struct mm_block *head = 0;
static struct mm_block *tail = 0;
static uint64_t total_used = 0;

void *kmalloc(unsigned int p_size)
{
    acquire_spinlock(&kmem_spinlock);

    uint64_t *ret;
    struct mm_block *lptr = head;
    unsigned int size = p_size;
    uint32_t running_size = 0;
    struct mm_block *running_block = NULL;
    if (size < MM_MIN_SIZE)
        size = MM_MIN_SIZE;

    while (lptr != 0)
    {
        //kprintf("MM  Trying to use 0x%x %x %x\n", lptr->addr, lptr->size,lptr->free);

       if (running_size >= size) {
            running_block->next = lptr;
            running_block->free = USED;
            running_block->size = running_size;
            //kprintf("Running block %x",running_block->addr);
            release_spinlock(&kmem_spinlock);
            return running_block->addr;
        }
        

        if (size == lptr->size && lptr->free == FREE)
        {
            if ((unsigned long)lptr->addr < KERNEL_HEAP_ADDR)
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
        if (lptr->free == FREE)
        {
            if(running_size == 0)
                running_block = lptr;
            running_size += lptr->size;
        }else{
            running_block = NULL;
            running_size = 0;
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
    //kprintf("kfree called %x\n",ptr);
    struct mm_block *lptr = (struct mm_block *)(((uint64_t)ptr) - (sizeof(struct mm_block)));
    if (lptr->addr == ptr)
    {
        if(lptr->free == FREE) {
            kprintf("block already freed:0x%x\n",lptr->addr);
            for(;;);
        }
        else
        {
            lptr->free = FREE;
        }
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

    setup_paging();
    struct pg_tbl pg;
    pg.pml4 = kernel_pml4;
    init_spinlock(&kmem_spinlock);
    char *heap_loc = pmem_alloc_block(HEAP_SIZE);
    kprintf("Heap Loc:0x%x\n", heap_loc);
    kernel_heap = (char *) KERNEL_HEAP_ADDR;
    paging_map_range(&pg,(uint64_t)heap_loc,KERNEL_HEAP_ADDR,HEAP_SIZE,KERNEL_PAGE);
    kernel_switch_paging();
    vmm_init();
}
