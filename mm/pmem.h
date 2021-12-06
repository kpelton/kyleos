#ifndef PMEM_H
#define PMEM_H
#include <include/types.h>
#include <include/multiboot.h>

#define BLOCK_SIZE 4096

int pmem_addr_get_status(uint64_t addr, uint64_t *bitmap);
void pmem_addr_set_block(uint64_t addr, uint64_t *bitmap);
void pmem_addr_free_block(uint64_t addr, uint64_t *bitmap); 
uint64_t *pmem_alloc_bitmap(uint64_t size, uint64_t *size_allocated, void *memory_loc);
void phys_mem_early_init(uint64_t  mb_info);
void *pmem_alloc_block(unsigned int size_in_pages);
void phys_mem_init();
enum phys_type {
    PMEM_RESERVED,
    PMEM_AVALIABLE
};
enum phys_mem_lcation {
    PMEM_LOC_BELOW_1MB,
    PMEM_LOC_BELOW_4GB,
    PMEM_LOC_HI_MEM,
};

struct phys_mem_zone {
    //1 if in use
    uint8_t in_use; 
    uint64_t base_addr;
    uint64_t len;
    uint64_t *bitmap;
    enum phys_type type;
    enum phys_mem_lcation location;

};
#endif