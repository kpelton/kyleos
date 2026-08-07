#ifndef MM_H
#define MM_H
#define MM_MIN_SIZE 64
#define MM_ALIGNMENT 16

void * kmalloc(unsigned int size);
void mm_init();
void mm_print_stats();
void kfree(void *ptr) ;
extern unsigned long _kernel_end;

struct mm_block {
    unsigned long size;
    struct mm_block* next;
    struct mm_block* prev;
    unsigned long magic;
    unsigned int free;
}__attribute__((aligned(MM_ALIGNMENT)));
#endif
