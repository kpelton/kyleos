#ifndef PMEM_H
#define PMEM_H
#include <include/types.h>
int pmem_addr_get_status(uint64_t addr, uint64_t *bitmap);
void pmem_addr_set_block(uint64_t addr, uint64_t *bitmap);
void pmem_addr_free_block(uint64_t addr, uint64_t *bitmap); 
uint64_t *pmem_alloc_bitmap(int size, int *size_allocated, void *memory_loc);

#endif