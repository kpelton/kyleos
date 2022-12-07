#ifndef _VMM_H
#define _VMM_H

#include <include/types.h>
#include <utils/llist.h>
#include <mm/paging.h>

enum vmm_block_type {
    VMM_STACK,
    VMM_TEXT,
    VMM_DATA,
    VMM_SECTION_CNT
};

//each process will have a mm_area mapping all its pages
struct vmm_map {
    struct llist *vmm_areas[VMM_SECTION_CNT];
    uint64_t total_pages;
    struct pg_tbl pagetable;
    
};

struct vmm_block {
    uint64_t *vaddr;
    uint64_t *paddr;
    uint64_t size; // in pages
    uint64_t page_ops;
    bool free;
};

//create new vmm_map
struct vmm_map *vmm_map_new();
//Add new VMM block
bool vmm_init();
struct vmm_block* vmm_add_new_mapping(struct vmm_map* map,enum vmm_block_type  block_type ,uint64_t *vaddr,uint64_t size, uint64_t page_ops,bool zero);

#endif
