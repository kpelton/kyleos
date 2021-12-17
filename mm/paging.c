#include <mm/paging.h>
#include <mm/pmem.h>
#include <output/output.h>
#include <include/types.h>
// 512 entries
#define INITIAL_PAGE_TAB 511 //MAP 128mb TO KERNEL SPACE


#define VIRT_TO_PML4_ADDR(x) ((0xff8000000000 & x) >> 39) & 0x1ff
#define VIRT_TO_PAGE_DIR_TAB_ADDR(x) (((0x7fc0000000 & x) >> 30) & 0x1ff)
#define VIRT_TO_PAGE_DIR(x) ((0x3fe00000 & x) >> 21) & 0x1ff
#define VIRT_TO_PAGE_TAB(x) (((0x1ff000 & x) >> 12) & 0x1ff)
#define VIRT_TO_PTE(x) ((0xfff & x) & 0xfff)
#define KERN_PHYS_TO_VIRT(x) ((uint64_t)addr_start + (uint64_t)x)
#define KERN_VIRT_TO_PHYS(x) ((uint64_t) x - (uint64_t)addr_start)

uint64_t initial_pml4[512] __attribute__((aligned(0x20)));         // must be aligned to (at least)0x20, ...
uint64_t initial_page_dir_tab[512] __attribute__((aligned(0x20))); // must be aligned to (at least)0x20, ...
uint64_t initial_page_dir[512] __attribute__((aligned(0x1000)));   // must be aligned to page boundary
uint64_t initial_page_tab[INITIAL_PAGE_TAB][512] __attribute__((aligned(0x1000)));



uint64_t *kernel_pml4;
uint64_t *kernel_page_dir_tab;
uint64_t *kernel_page_dir;
uint64_t  **page_tbl;

static void early_fill_dir(uint64_t startaddr, uint64_t *dir)
{
    uint64_t address = startaddr;
    int i = 0;
    for (i = 0; i < 512; i++)
    {
        dir[i] = address | 0x7; // map address and mark it present/writable
        address = address + 0x1000;
    }
}

void early_setup_paging()
{
    //set each entry to not present
    uint64_t i = 0;
    uint64_t j = 0;
    uint64_t address = 0;


    initial_pml4[511] = ((uint64_t)&initial_page_dir_tab - addr_start) | 7;     // set the page directory into the PDPT and mark it present
    initial_page_dir_tab[510] = ((uint64_t)&initial_page_dir - addr_start) | 7; // set the page directory into the PDPT and mark it present

    j = 0;
    for (i = 0; i < INITIAL_PAGE_TAB; i++)
    {
        initial_page_dir[i] = ((uint64_t)&initial_page_tab[j] - addr_start) | 7; //set the page table into the PD and mark it present/writable
        early_fill_dir((0x200000 * j), initial_page_tab[j]);
        j += 1;
    }

    address = (uint64_t)&initial_pml4 - addr_start;
    asm volatile("cli; \
            movq %0 ,%%cr3; \
            "
                 :
                 : "r"(address));
}

bool paging_map_kernel_range(uint64_t start, uint64_t len) {

    uint64_t virt_curr_addr =KERN_PHYS_TO_VIRT(start);
    uint64_t phys_curr_addr = start;
    uint16_t tbl;
    uint16_t offset;

    while (phys_curr_addr < start + (len*4096)) {
        tbl = VIRT_TO_PAGE_DIR(virt_curr_addr);
        offset =  VIRT_TO_PAGE_TAB(virt_curr_addr);
        //kprintf("%x, %x  %x %x\n",virt_curr_addr,phys_curr_addr,tbl,offset);
        kernel_page_dir[tbl] = KERN_VIRT_TO_PHYS(page_tbl[tbl]) &0xfffffffffffff000 | 3;
        page_tbl[tbl][offset] = phys_curr_addr |3;
        phys_curr_addr +=0x1000;
        virt_curr_addr +=0x1000;

    }
    return true;
}


bool paging_map_user_range(struct pg_tbl *pg,uint64_t start,uint64_t virt_start, uint64_t len) {

    uint64_t virt_curr_addr =virt_start;
    uint64_t phys_curr_addr = KERN_VIRT_TO_PHYS(start);
    uint16_t tbl;
    uint16_t offset;
    asm("cli");
    while (phys_curr_addr < KERN_VIRT_TO_PHYS(start) + (len*4096)) {
        tbl = VIRT_TO_PAGE_DIR(virt_curr_addr);
        offset =  VIRT_TO_PAGE_TAB(virt_curr_addr);
        //kprintf("%x, %x  %x %x\n",virt_curr_addr,phys_curr_addr,tbl,offset);
        pg->page_dir[tbl] = KERN_VIRT_TO_PHYS(pg->page_tbl[tbl]) | 7;
        pg->page_tbl[tbl][offset] = phys_curr_addr &0xfffffffffffff000  |7;
        phys_curr_addr +=0x1000;
        virt_curr_addr +=0x1000;

    }
    return true;
}

bool user_setup_paging(struct pg_tbl *pg,uint64_t start, uint64_t virt_start,uint64_t len) 
{
    int i;
    int j;
    uint64_t count;
    uint64_t curr_addr = start - addr_start;
    pg->page_dir_tab = (uint64_t *) KERN_PHYS_TO_VIRT(pmem_alloc_page());
    pg->page_dir = (uint64_t *) KERN_PHYS_TO_VIRT(pmem_alloc_page());
    pg->page_tbl = (uint64_t **) KERN_PHYS_TO_VIRT(pmem_alloc_page());
    paging_map_kernel_range(KERN_VIRT_TO_PHYS(pg->page_tbl), 1);
    paging_map_kernel_range(KERN_VIRT_TO_PHYS(pg->page_dir), 1);
    paging_map_kernel_range(KERN_VIRT_TO_PHYS(pg->page_dir_tab), 1);
    pg->page_dir_tab[0] = KERN_VIRT_TO_PHYS(pg->page_dir) | 7;

    for (i=0; i<512; i++) {
        pg->page_tbl[i] = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_page());
        paging_map_kernel_range(KERN_VIRT_TO_PHYS(pg->page_tbl[i]), 1);

    }


    j = VIRT_TO_PAGE_TAB(virt_start) ;
    i = VIRT_TO_PAGE_DIR(virt_start) ;
    while (curr_addr <= (start - addr_start)+len) {
        pg->page_dir[i] = KERN_VIRT_TO_PHYS(pg->page_tbl[i]) | 7;
        pg->page_tbl[i][j] = curr_addr |7;
        curr_addr +=0x1000;
        if (curr_addr % 0x200000 == 0) {
            i += 1;
            j = 0;
        }else{
            j += 1;
        }
    }
    return true;
}

void user_switch_paging(    struct pg_tbl *pg) {
        kernel_pml4[0] =  KERN_VIRT_TO_PHYS(pg->page_dir_tab) | 7;

        uint64_t address = (uint64_t)kernel_pml4 - addr_start;

    //kprintf("%x\n",KERN_VIRT_TO_PHYS(pg->page_dir));

}
void kernel_switch_paging() {
        kernel_pml4[0] =  0;

        uint64_t address = (uint64_t)kernel_pml4 - addr_start;

    //kprintf("%x\n",KERN_VIRT_TO_PHYS(pg->page_dir));

}
bool setup_paging()
{
    uint64_t address;
    uint64_t curr_addr = 0;
    uint64_t virt_curr_addr=addr_start;
    int j,i = 0;

    kernel_pml4 =(uint64_t *) KERN_PHYS_TO_VIRT(pmem_alloc_block(1));
    kprintf("0xpml4:%x\n",kernel_pml4);
    kprintf("0xpml4:%x\n",KERN_VIRT_TO_PHYS(kernel_pml4));
    kernel_page_dir_tab = (uint64_t *) KERN_PHYS_TO_VIRT(pmem_alloc_block(1));
    kprintf("page_dir_tab  %x\n",kernel_page_dir_tab);
    kprintf("page_dir_tab %x\n",KERN_VIRT_TO_PHYS(kernel_page_dir_tab));
    kernel_page_dir = (uint64_t *) KERN_PHYS_TO_VIRT(pmem_alloc_block(1));

    kernel_pml4[VIRT_TO_PML4_ADDR(virt_curr_addr)] =  KERN_VIRT_TO_PHYS(kernel_page_dir_tab) | 3;
    kernel_page_dir_tab[VIRT_TO_PAGE_DIR_TAB_ADDR(virt_curr_addr)] = KERN_VIRT_TO_PHYS(kernel_page_dir) | 3;
    page_tbl = (uint64_t **) KERN_PHYS_TO_VIRT(pmem_alloc_block(1));
    for (i=0; i!=511; i++) {
        address =(uint64_t) pmem_alloc_page();
        page_tbl[i] = (uint64_t *)KERN_PHYS_TO_VIRT(address);
    }
    i = 0;
    j = 0;
    

    while (curr_addr <= address) {
       // kprintf("%x\n",curr_addr);
        kernel_page_dir[i] = KERN_VIRT_TO_PHYS(page_tbl[i]) | 3;
        page_tbl[i][j] = curr_addr |3;
        curr_addr +=0x1000;
        if (curr_addr % 0x200000 == 0) {
            i += 1;
            j = 0;
        }else{
            j += 1;
        }
    }
    address = (uint64_t)kernel_pml4 - addr_start;

    asm volatile("cli; \
            movq %0 ,%%cr3; \
            "
                 :
                 : "r"(address));

    kprintf("Switch to late kernel pages done\n");
    return true;
}
