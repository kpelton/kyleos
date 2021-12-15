#ifndef MM_H
#define MM_H
#define MM_MIN_SIZE 64

void * kmalloc(unsigned int size);
void mm_init();
void mm_print_stats();
void kfree(void *ptr) ;
extern unsigned long _kernel_end;

 struct mm_block {
    unsigned long * addr;
    unsigned long size;
    struct mm_block* next;
    short free;
}__attribute__((aligned(MM_MIN_SIZE)));
#endif
