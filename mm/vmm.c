#include <mm/vmm.h>
#include <mm/mm.h>
#include <mm/pmem.h>
#include <utils/llist.h>
#include <output/output.h>

bool vmm_init()
{
    kprintf("User VMM init\n");
    return true;
}

// create new vmm map; return NULL if no memory from kmalloc
struct vmm_map *vmm_map_new()
{
    struct vmm_map *new_map;
    int i = 0;
    new_map = kmalloc(sizeof(struct vmm_map));
    if (new_map == NULL)
        return NULL;
    new_map->pagetable.pml4 = (uint64_t *)KERN_PHYS_TO_PVIRT(pmem_alloc_zero_page());

    for (i = 0; i < VMM_SECTION_CNT; ++i)
        new_map->vmm_areas[i] = llist_new();

    new_map->total_pages = 0;
    return new_map;
}

bool vmm_free(struct vmm_map *map)
{
    for (uint64_t i = 0; i < VMM_SECTION_CNT; ++i)
    {
        llist_free(map->vmm_areas[i], vmm_map_free_block);
    }
    paging_free_pg_tbl(&map->pagetable);
    kfree(map);
    return true;
}

uint64_t vmm_get_page_count(struct vmm_map *map, enum vmm_block_type btype)
{
    return map->vmm_areas[btype]->count;
}

void vmm_map_free_block(void *data)
{
    struct vmm_block *block = data;
    /* Leaf mappings own the physical-page references and release them while
     * their page tables are torn down. */
    kfree(block);
}

void *vmm_copy_block(void *data, void *user_data)
{
    struct vmm_block *block = (struct vmm_block *)data;
    struct vmm_map *map = (struct vmm_map *)user_data;
    struct vmm_block *new_block;
    uint64_t *phys_ptr_old;
    uint64_t *phys_ptr_new;

    new_block = vmm_add_new_mapping(map, block->type, block->vaddr, block->size, block->page_ops, false, false);
    if (!new_block)
        return NULL;
    phys_ptr_old = (uint64_t *)KERN_PHYS_TO_PVIRT(block->paddr);
    phys_ptr_new = (uint64_t *)KERN_PHYS_TO_PVIRT(new_block->paddr);
    // copy data over to new block
    memcpy64(phys_ptr_new, phys_ptr_old, (block->size * PAGE_SIZE));

    return new_block;
}

bool vmm_copy_section(struct vmm_map *src, struct vmm_map *dst, enum vmm_block_type btype)
{
    llist_copy(src->vmm_areas[btype], dst->vmm_areas[btype], vmm_copy_block, dst);
    return true;
}

struct vmm_share_context {
    struct vmm_map *src;
    struct vmm_map *dst;
};

static void *vmm_share_block(void *data, void *user_data)
{
    struct vmm_block *block = (struct vmm_block *)data;
    struct vmm_share_context *context = (struct vmm_share_context *)user_data;
    struct vmm_map *dst = context->dst;
    struct vmm_block *new_block;
    uint64_t shared_pages = 0;

    new_block = kmalloc(sizeof(struct vmm_block));
    if (!new_block)
        return NULL;
    *new_block = *block;

    for (uint64_t i = 0; i < block->size; i++) {
        uint64_t va = (uint64_t)block->vaddr + i * PAGE_SIZE;
        uint64_t pa;
        uint64_t *parent_pte = paging_walk(&context->src->pagetable, va);
        uint64_t flags;

        if (!parent_pte || (*parent_pte & PAGE_PRESENT) == 0) {
            if (block->demand)
                continue;
            kfree(new_block);
            return NULL;
        }
        /* A prior COW fault may have replaced this one page, so block->paddr
         * can no longer describe the live physical backing. */
        pa = *parent_pte & 0xffffffffff000;
        /* The block's original flags are not sufficient here: after one
         * fork a writable page is already read-only+COW.  Preserve those
         * live PTE flags when sharing it with another child. */
        flags = *parent_pte & 0xfff;
        if ((*parent_pte & READ_WRITE) != 0) {
            *parent_pte = (*parent_pte & ~READ_WRITE) | PAGE_COW;
            flags = (*parent_pte & 0xfff);
            asm volatile("invlpg (%0)" :: "r"(va));
        }
        pmem_retain_page(pa);
        if (!paging_map_range(&dst->pagetable, pa, va, 1, flags))
            return NULL;
        shared_pages++;
    }
    dst->total_pages += shared_pages;
    return new_block;
}

bool vmm_share_section(struct vmm_map *src, struct vmm_map *dst, enum vmm_block_type btype)
{
    struct vmm_share_context context = { src, dst };
    llist_copy(src->vmm_areas[btype], dst->vmm_areas[btype], vmm_share_block, &context);
    return true;
}

struct vmm_block *vmm_add_new_mapping(struct vmm_map *map, enum vmm_block_type block_type,
                                      uint64_t *vaddr, uint64_t size, uint64_t page_ops, bool zero, bool add_to_list)
{
    if (block_type > VMM_SECTION_CNT)
        panic("Error invalid section ");
    struct vmm_block *block = kmalloc(sizeof(struct vmm_block));
    uint64_t *phys_ptr;
    block->vaddr = vaddr;
    block->size = size;
    block->page_ops = page_ops;
    block->free = false;
    block->type = block_type;
    block->demand = false;

    if (size > 1)
        block->paddr = pmem_alloc_block(size);
    else
        block->paddr = pmem_alloc_page();
    if (add_to_list)
        llist_append(map->vmm_areas[block_type], block);
    // map it in the page table
    if (!paging_map_range(&(map->pagetable), (uint64_t)block->paddr,
                               (uint64_t)block->vaddr, size, page_ops ))
        return NULL;

    // bss section zero pages
    if (zero)
    {
        phys_ptr = (uint64_t *)KERN_PHYS_TO_PVIRT(block->paddr);
        memzero64(phys_ptr, size * PAGE_SIZE);
    }
    // kprintf("vmm returning 0x%x\n",block->paddr);
    map->total_pages += size;
    return block;
}

struct vmm_block *vmm_reserve_mapping(struct vmm_map *map,
                                      enum vmm_block_type block_type,
                                      uint64_t *vaddr, uint64_t size,
                                      uint64_t page_ops)
{
    struct vmm_block *block;

    if (map == NULL || block_type >= VMM_SECTION_CNT || size == 0)
        return NULL;
    block = kmalloc(sizeof(*block));
    if (block == NULL)
        return NULL;
    block->vaddr = vaddr;
    block->paddr = NULL;
    block->size = size;
    block->page_ops = page_ops & ~PAGE_PRESENT;
    block->type = block_type;
    block->free = false;
    block->demand = true;
    if (llist_append(map->vmm_areas[block_type], block) == NULL) {
        kfree(block);
        return NULL;
    }
    return block;
}

static struct vmm_block *vmm_find_block(struct vmm_map *map, uint64_t address)
{
    for (int section = 0; section < VMM_SECTION_CNT; section++) {
        struct llist_node *node = map->vmm_areas[section]->head;
        while (node != NULL) {
            struct vmm_block *block = node->data;
            uint64_t start = (uint64_t)block->vaddr;
            uint64_t end = start + block->size * PAGE_SIZE;
            if (address >= start && address < end)
                return block;
            node = node->next;
        }
    }
    return NULL;
}

bool vmm_handle_page_fault(struct vmm_map *map, uint64_t address,
                           uint64_t error_code)
{
    struct vmm_block *block;
    uint64_t page = address & ~(PAGE_SIZE - 1);
    uint64_t physical;
    uint64_t *pte;

    if (map == NULL || (error_code & PF_PRESENT) != 0)
        return false;
    block = vmm_find_block(map, page);
    if (block == NULL || !block->demand)
        return false;
    if ((error_code & PF_WRITE) != 0 &&
        (block->page_ops & READ_WRITE) == 0)
        return false;
    pte = paging_walk(&map->pagetable, page);
    if (pte != NULL && (*pte & PAGE_PRESENT) != 0)
        return false;
    physical = (uint64_t)pmem_try_alloc_zero_page();
    if (physical == 0)
        return false;
    if (!paging_map_range(&map->pagetable, physical, page, 1,
                          block->page_ops | PAGE_PRESENT)) {
        pmem_free_block(physical);
        return false;
    }
    map->total_pages++;
    asm volatile("invlpg (%0)" :: "r"(page));
    return true;
}

bool vmm_populate_page(struct vmm_map *map, uint64_t address)
{
    return vmm_handle_page_fault(map, address, PF_USER | PF_WRITE);
}
