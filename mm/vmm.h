#ifndef _VMM_H
#define _VMM_H
#include <include/types.h>

enum vmm_block_type {
    VMM_STACK_BLOCK,
    VMM_TEXT_TYPE,
    VMM_DATA_TYPE,
    VMM_BLOCK_TYPE_NUM
};

//each process will have a mm_area mapping all its pages
struct vmm_map {
    struct vmm_block *vmm_areas[VMM_BLOCK_TYPE_NUM];
    uint64_t total_pages;
};

struct vmm_block {
    uint64_t *addr;
    uint64_t size; // in pages
    uint64_t page_ops;
    struct vmm_block *next;
    bool free;
};

//create new vmm_map
struct vmm_map *new_vmm_map();

#endif
