#ifndef MM_H
#define MM_H
unsigned long * kmalloc(unsigned int size);
void mm_init();
void mm_print_stats();
void kfree(unsigned long *ptr) ;
extern unsigned long _kernel_end;
#endif
