#include "mm.h"
#include <output/output.h>

char * kernel_heap;
#define FREE 1
#define USED 1
#define MIN_SIZE 64

static struct  mm_block * head = 0;
static struct  mm_block * tail = 0;
void * kmalloc(unsigned int p_size)
{
    //kprint_hex("MM Allocating ",size)
    unsigned long * ret;
    struct  mm_block *lptr = head;
    unsigned int size = p_size;
    //first loop for a free block that is already allocated
    if (p_size < MIN_SIZE)
        size = MIN_SIZE;

    while(lptr != 0) {
        if (size <= lptr->size && lptr->free == 1 ) {
            lptr->free = 0;
            if ((unsigned long)lptr->addr  < 0xffffffff80000000) {
                kprint_hex("old alloc ",(unsigned long) &lptr->addr);
                kprint_hex("old alloc ",(unsigned long) &lptr->addr);
                kprint_hex("mem corrution detected ",(unsigned long) &lptr->addr);
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
        kprintf("Memory courrption detected on free\n");
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
           // kprint_hex("Used Memory        0x", lptr->addr);
           // kprint_hex("   Size            0x", lptr->size);
          //  kprint_hex("   free          0x", lptr->free);
            ll_size +=1;
            lptr = lptr->next;

        }
    kprint_hex("Total Used Memory     0x", size);
    kprint_hex("LL nodes              0x", ll_size);
    kprint_hex("LL node size          0x", ll_size*sizeof(struct mm_block));
    kprint_hex("End of kernel         0x", (unsigned long) &_kernel_end);
    kprint_hex("Start of kernel       0x",0xffffffff80000000);
}

void mm_init()
{
    kernel_heap =  (char *) &_kernel_end + 0x8;
}

