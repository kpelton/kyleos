#ifndef PAGING_H
#define PAGING_H
#include <include/types.h>
#define addr_start 0xffffffff80000000

void early_setup_paging();
bool setup_paging();
bool paging_map_kernel_range(uint64_t start, uint64_t len);

#define KERN_PHYS_TO_VIRT(x) ((uint64_t)addr_start + (uint64_t)x)
#define KERN_VIRT_TO_PHYS(x) ((uint64_t) x - (uint64_t)addr_start)

#endif
