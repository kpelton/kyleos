#include "mm.h"
#include <output/output.h>
#include <include/types.h>
#include <mm/pmem.h>

char * kernel_heap;
#define FREE 1
#define USED 1
#define MIN_SIZE 64

static struct  mm_block * head = 0;
static struct  mm_block * tail = 0;
void * kmalloc(unsigned int p_size)
{
    //kprint_hex("MM Allocating ",size)
    uint64_t* ret;
    struct  mm_block *lptr = head;
    unsigned int size = p_size;
    //first loop for a free block that is already allocated
    if (p_size < MIN_SIZE)
        size = MIN_SIZE;

    while(lptr != 0) {
        if (size <= lptr->size && lptr->free == 1 ) {
            lptr->free = 0;
            if ((unsigned long)lptr->addr  < 0xffffffff80000000) {
                kprintf("old alloc %x\n",(unsigned long) &lptr->addr);
                kprintf("old alloc %x\n",(unsigned long) &lptr->addr);
                kprintf("mem corrution detected %x\n",(unsigned long) &lptr->addr);
                continue;
            }
            return lptr->addr;
        }
        lptr = lptr->next;
    } 

    struct  mm_block *ptr = (void *) kernel_heap;
    kernel_heap+=sizeof(struct mm_block);
    ret = (void *) kernel_heap;
    if (head== 0) {
        head = ptr;
        tail = head;
    } else {
        tail->next = ptr;
        tail = ptr;
    }
    ptr->size = size;
    ptr->next = 0;
    ptr->free = 0;
    ptr->addr = ret;
    //kprint_hex("Alloc ",size);
    //kprint_hex("Alloc 0x",ret);

    kernel_heap+=size;
    return ret;

}

void kfree(void *ptr)
{
    struct  mm_block *lptr = ptr - (sizeof(struct mm_block));
    if (lptr->addr == ptr) {
            lptr->free = 1;
    } else {
        panic("Memory courrption detected on free\n");
    }
}

void mm_print_stats()
{

    struct  mm_block *lptr = head;
    unsigned long size = 0;
    unsigned long ll_size = 0;

    while(lptr != 0) {
        size +=sizeof(struct mm_block);
        if (lptr->free == 0)
            size += lptr->size;

        ll_size +=1;
        lptr = lptr->next;
    }

    kprintf("Total Used Memory     %dK\n", size/1024);
    kprintf("LL nodes              %d\n", ll_size);
    kprintf("LL node size          %dK\n", (ll_size*sizeof(struct mm_block)) /1024);
    kprintf("End of kernel         0x%x\n", (unsigned long) &_kernel_end);
    kprintf("Start of kernel       0x%x\n",0xffffffff80000000);
}

void mm_init()
{

    //alloc_block;
    kernel_heap =  (char *) &_kernel_end + 0x8;
}

