#include <mm/vmm.h>
#include <mm/mm.h>
#include <mm/pmem.h>
#include <output/output.h>

bool vmm_init() {
    kprintf("User VMM init\n");
    return true;
}

//create new vmm map; return NULL if no memory from kmalloc
struct vmm_map* vmm_map_new() {
    struct vmm_map *new_map;
    int i = 0;
    new_map = kmalloc(sizeof(struct vmm_map));
    if (new_map == NULL)
        return NULL;
    new_map->pagetable.pml4 = KERN_PHYS_TO_PVIRT(pmem_alloc_zero_page());


    for(i=0; i<VMM_SECTION_CNT; ++i)
        new_map->vmm_areas[i] = llist_new();

    new_map->total_pages = 0;
    return new_map; 
}

struct vmm_block* vmm_add_new_mapping(struct vmm_map* map,enum vmm_block_type  block_type , 
                                      uint64_t *vaddr,uint64_t size, uint64_t page_ops,bool zero) 
{
    if (block_type > VMM_SECTION_CNT)
        panic("Error invalid section ");
    struct vmm_block *block = kmalloc(sizeof(struct vmm_block));
    uint64_t *phys_ptr;
    block->vaddr = vaddr;
    block->size = size;
    block->page_ops = page_ops;
    block->free = false;


    if(size >1)
        block->paddr = pmem_alloc_block(size);
    else
        block->paddr = pmem_alloc_page();

    llist_prepend(map->vmm_areas[block_type],block);
    //map it in the page table
    if ( ! paging_map_user_range(&(map->pagetable) ,(uint64_t) block->paddr,
                                (uint64_t) block->vaddr,size,page_ops))
            return NULL;

        //bss section zero pages
        if(zero)
        {
            phys_ptr = (uint64_t *) KERN_PHYS_TO_PVIRT(block->paddr);
            for(uint32_t j=0; j<(size*PAGE_SIZE)/sizeof(uint64_t); j++)
                phys_ptr[j] = 0;
        }
    kprintf("vmm returning 0x%x\n",block->paddr);
    map->total_pages += size;
    return block;
    
}
