#ifndef PAGING_H
#define PAGING_H
#include <include/types.h>
#define addr_start 0xffffffff80000000
#define phys_memmap_start 0xffff888000000000
#define PAGE_PRESENT 1
#define READ_WRITE 1<<1
#define SUPERVISOR 1<<2
#define KERNEL_PAGE PAGE_PRESENT | READ_WRITE
#define KERNEL_PAGE_RO PAGE_PRESENT
#define USER_PAGE PAGE_PRESENT | READ_WRITE | SUPERVISOR
#define USER_PAGE_RO PAGE_PRESENT  | SUPERVISOR

struct pg_tbl {
    uint64_t  *pml4;
};
void early_setup_paging();
bool setup_paging();
bool paging_map_kernel_range(uint64_t start, uint64_t len);
bool paging_map_user_range(struct pg_tbl *pg, uint64_t start, uint64_t virt_start,
                           uint64_t len, uint64_t page_ops);
bool paging_free_pg_tbl(struct pg_tbl *pg);
void user_switch_paging(struct pg_tbl *pg);
void kernel_switch_paging();
void paging_enable_protected();

#define KERN_PHYS_TO_VIRT(x) ((uint64_t)addr_start + (uint64_t)x)
#define KERN_VIRT_TO_PHYS(x) ((uint64_t) x - (uint64_t)addr_start)

#define KERN_PHYS_TO_PVIRT(x) ((uint64_t)phys_memmap_start + (uint64_t)x)
#define KERN_PVIRT_TO_PHYS(x) ((uint64_t) x - (uint64_t)phys_memmap_start)

#endif
