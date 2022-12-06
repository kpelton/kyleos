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
    
}
