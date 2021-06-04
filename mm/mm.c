#include "mm.h"
#include <output/output.h>

char * kernel_heap;
unsigned long int used_bytes;
void * kmalloc(unsigned int size) {
    void * ret = (void *) kernel_heap;
    kprint_hex("MM Allocating ",size);
    kernel_heap+=size;
    used_bytes += size;

    kprint_hex("MM used_bytes",used_bytes);
    return ret;

}

void mm_init() {
    kernel_heap =  (char *) &_kernel_end + 0x8;
}

