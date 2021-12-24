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


uint64_t initial_pml4[512] __attribute__((aligned(0x20)));         // must be aligned to (at least)0x20, ...
uint64_t initial_page_dir_tab[512] __attribute__((aligned(0x20))); // must be aligned to (at least)0x20, ...
uint64_t initial_page_dir[512] __attribute__((aligned(0x1000)));   // must be aligned to page boundary
uint64_t initial_page_tab[INITIAL_PAGE_TAB][512] __attribute__((aligned(0x1000)));

uint64_t *kernel_pml4;
uint64_t *kernel_page_dir_tab;
uint64_t *kernel_iden_page_dir_tab;
uint64_t *kernel_page_dir;
uint64_t **page_tbl;
uint64_t *pkernel_pml4;


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
    asm volatile("movq %0 ,%%cr3; \
            "
                 :
                 : "r"(address));
}

bool paging_map_kernel_range(uint64_t start, uint64_t len)
{

    uint64_t virt_curr_addr = KERN_PHYS_TO_VIRT(start);
    uint64_t phys_curr_addr = start;
    uint16_t offset;
    uint64_t *curr;

    while (phys_curr_addr < start + (len * 4096))
    {

        offset = VIRT_TO_PML4_ADDR(virt_curr_addr);
        if ((pkernel_pml4[offset] & 1) == 0) {
            pkernel_pml4[offset] = (uint64_t)pmem_alloc_zero_page() | 3;
        }
        curr = (uint64_t *)KERN_PHYS_TO_PVIRT((pkernel_pml4[offset] & 0xffffffffffffff00));
        offset = VIRT_TO_PAGE_DIR_TAB_ADDR(virt_curr_addr);
        //kprintf("0x%x\n",curr);
        if ((curr[offset] & 1) == 0) {
            curr[offset] = (uint64_t)pmem_alloc_zero_page() |3 ;
        }
        curr = (uint64_t *) KERN_PHYS_TO_PVIRT((curr[offset] & 0xffffffffffffff00));

        //kprintf("0x%x\n",curr);
        offset = VIRT_TO_PAGE_DIR(virt_curr_addr);
        if ((curr[offset] & 1) == 0) {
            curr[offset] = (uint64_t)pmem_alloc_zero_page() |3 ;
        }
        curr = (uint64_t *) KERN_PHYS_TO_PVIRT((curr[offset] & 0xffffffffffffff00));
        offset = VIRT_TO_PAGE_TAB(virt_curr_addr);
        //kprintf("%d\n",offset);
        curr[offset] = phys_curr_addr | 3;
        phys_curr_addr += 0x1000;
        virt_curr_addr += 0x1000;
    }
    return true;
}

bool paging_map_user_range(struct pg_tbl *pg, uint64_t start, uint64_t virt_start, uint64_t len)
{

    uint64_t virt_curr_addr = virt_start;
    uint64_t phys_curr_addr = KERN_VIRT_TO_PHYS(start);
    uint16_t offset;
    uint64_t *curr = pg->pml4;
    kprintf("call base pml4 %x\n",pg->pml4);
    while (phys_curr_addr < KERN_VIRT_TO_PHYS(start) + (len * 4096))
    {
        curr = pg->pml4;
        offset = VIRT_TO_PML4_ADDR(virt_curr_addr);
        kprintf("pml4 %x %x %x\n",curr,curr[offset],offset);

        //Add checks here if we are out of the user range
        if ((curr[offset] & 1) == 0) {
                    kprintf("Writing to %x\n",curr+offset);

            curr[offset] = (uint64_t)pmem_alloc_zero_page()| 7;
            kprintf("new pagedirtab %x %x\n",curr[offset],offset);
        }
        curr = (uint64_t *) KERN_PHYS_TO_PVIRT((curr[offset] & 0xffffffffffffff00));
        offset = VIRT_TO_PAGE_DIR_TAB_ADDR(virt_curr_addr);
        kprintf("page_tab %x %x %x\n",curr,curr[offset],offset);


        if ((curr[offset] & 1) == 0) {
                    kprintf("Writing to %x\n",curr+offset);

            curr[offset] = (uint64_t)pmem_alloc_zero_page() |7 ;
        }
        curr = (uint64_t *) KERN_PHYS_TO_PVIRT((curr[offset] & 0xffffffffffffff00));
        offset = VIRT_TO_PAGE_DIR(virt_curr_addr);
        kprintf("page_dir %x %x %x\n",curr,curr[offset],offset);
                kprintf("done1\n");
        if ((curr[offset] & 1) == 0) {
                            kprintf("done2\n");
        kprintf("Writing to %x\n",curr+offset);
            curr[offset] = (uint64_t)pmem_alloc_zero_page() |7 ;
        }
                kprintf("done\n");

        curr = (uint64_t *) KERN_PHYS_TO_PVIRT((curr[offset] & 0xffffffffffffff00));
        offset = VIRT_TO_PAGE_TAB(virt_curr_addr);
        kprintf("page\n");
        kprintf("Writing to %x\n",curr+offset);
        curr[offset] = phys_curr_addr | 7;

        phys_curr_addr += 0x1000;
        virt_curr_addr += 0x1000;
    }
    return true;

}
bool paging_free_pg_tbl(struct pg_tbl *pg)
{
    //TODO: implement this
    kprintf("free %x",pg);

    return true;
}


void user_switch_paging(struct pg_tbl *pg)
{

   // kprintf("switching to pml4 0x%x\n",pg->pml4);
    //return;
    int i = 0;
    for (i=0; i<20; i++) {
        if ((pg->pml4[i] & 1) == 1)
            kernel_pml4[i] = pg->pml4[i];
        else
            kernel_pml4[i] = 0;
    }
/*
    //    kprintf("pml4 0x%x %x\n",address,pg->pml4);
    asm volatile("movq %0 ,%%cr3; \
            "
                 :
                 : "r"(address));
/
     //  kprintf("user switch\n");
*/
}
void kernel_switch_paging()
{
    return;
    uint64_t address = (uint64_t)kernel_pml4 - addr_start;
    asm volatile("movq %0 ,%%cr3; \
            "
                 :
                 : "r"(address));
                 

    kprintf("kern switch\n");
}
bool setup_paging()
{
    uint64_t address;
    uint64_t curr_addr = 0;
    uint64_t virt_curr_addr = addr_start;
    int j, i = 0;

    kernel_pml4 = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_page());
    kprintf("0xpml4:%x\n", kernel_pml4);
    kprintf("0xpml4:%x\n", KERN_VIRT_TO_PHYS(kernel_pml4));
    kernel_page_dir_tab = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_page());
    kprintf("page_dir_tab  %x\n", kernel_page_dir_tab);
    kprintf("page_dir_tab %x\n", KERN_VIRT_TO_PHYS(kernel_page_dir_tab));

    kernel_page_dir = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_page());
    kernel_iden_page_dir_tab = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_page());

    kernel_pml4[VIRT_TO_PML4_ADDR(virt_curr_addr)] = KERN_VIRT_TO_PHYS(kernel_page_dir_tab) | 3;
    kernel_pml4[273] = KERN_VIRT_TO_PHYS(kernel_iden_page_dir_tab) | 3;
    address = 0;
    kprintf("%x\n",kernel_iden_page_dir_tab);
    for(i=0; i<512; i++) {
        kernel_iden_page_dir_tab[i] = address | 131;
        address+=0x40000000;
    }

    kernel_page_dir_tab[VIRT_TO_PAGE_DIR_TAB_ADDR(virt_curr_addr)] = KERN_VIRT_TO_PHYS(kernel_page_dir) | 3;
    page_tbl = (uint64_t **)KERN_PHYS_TO_VIRT(pmem_alloc_page());
    for (i = 0; i != 511; i++)
    {
        address = (uint64_t)pmem_alloc_page();
        page_tbl[i] = (uint64_t *)KERN_PHYS_TO_VIRT(address);
    }
    i = 0;
    j = 0;

    while (curr_addr <= address)
    {
        // kprintf("%x\n",curr_addr);
        kernel_page_dir[i] = KERN_VIRT_TO_PHYS(page_tbl[i]) | 3;
        page_tbl[i][j] = curr_addr | 3;
        curr_addr += 0x1000;
        if (curr_addr % 0x200000 == 0)
        {
            i += 1;
            j = 0;
        }
        else
        {
            j += 1;
        }
    }
    address = (uint64_t)kernel_pml4 - addr_start;
    asm volatile("movq %0 ,%%cr3; \
            "
                 :
                 : "r"(address));
    pkernel_pml4 = (uint64_t *) KERN_PHYS_TO_PVIRT(KERN_VIRT_TO_PHYS(kernel_pml4));
    kprintf("Switch to late kernel pages done\n");

    return true;
}
