#ifndef PAGING_H
#define PAGING_H
#include <include/types.h>
#define addr_start 0xffffffff80000000
struct pg_tbl {
    uint64_t  *page_dir_tab;
    uint64_t  *page_dir;
    uint64_t  **page_tbl;
};
void early_setup_paging();
bool setup_paging();
bool paging_map_kernel_range(uint64_t start, uint64_t len);
bool user_setup_paging(struct pg_tbl *pg,uint64_t start, uint64_t virt_start,uint64_t len);
bool paging_map_user_range(struct pg_tbl *pg,uint64_t start,uint64_t virt_start, uint64_t len);
bool paging_free_pg_tbl(struct pg_tbl *pg);
void user_switch_paging(struct pg_tbl *pg);
void kernel_switch_paging();

#define KERN_PHYS_TO_VIRT(x) ((uint64_t)addr_start + (uint64_t)x)
#define KERN_VIRT_TO_PHYS(x) ((uint64_t) x - (uint64_t)addr_start)

#endif
