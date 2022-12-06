#include <mm/vmm.h>
#include <mm/mm.h>
#include <output/output.h>
bool vmm_init() {
    kprintf("User VMM init\n");
    return true;
}

//create new vmm map; return NULL if no memory from kmalloc
struct vmm_map* new_vmm_map() {
    struct vmm_map *new_map;
    int i = 0;
    new_map = kmalloc(sizeof(struct vmm_map));
    if (new_map == NULL)
        return NULL;
    
    for(i=0; i<VMM_BLOCK_TYPE_NUM; ++i)
        new_map->vmm_areas[i] = NULL;

    new_map->total_pages = 0;
    return new_map;
    
}

/*
// Free list of used blocks allocated for program pages
static void free_memblock_list(struct p_memblock *head)
{
    struct p_memblock *p = head;
    struct p_memblock *curr = NULL;

    while (p != NULL)
    {
        curr = p;
        p = p->next;

        for (uint32_t j = 0; j < curr->count; j++)
        {
#ifdef SCHED_DEBUG
            kprintf("Freeing 0x%x\n", (uint64_t)curr->block + (PAGE_SIZE * j));
#endif
            pmem_free_block((uint64_t)curr->block + (PAGE_SIZE * j));
        }
        curr->block = 0;
        curr->next = 0;
        kfree(curr);
    }
}
*/