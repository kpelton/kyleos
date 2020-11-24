#include "mm.h"
char * kernel_heap;
unsigned long int used_bytes;
void * kmalloc(unsigned int size) {
    void * ret = (void *) kernel_heap;
    kernel_heap+=size;
    used_bytes += size;
    return ret;

}

void mm_init() {
    kernel_heap =  (char *) &_kernel_end + 0x8;
}

