#ifndef MM_H
#define MM_H
void * kmalloc(unsigned int size);
void mm_init();
extern unsigned long _kernel_end;
#endif
