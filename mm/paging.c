#include <mm/paging.h>
#include <mm/pmem.h>
#include <output/output.h>
#include <include/types.h>
#include <sched/sched.h>
// 512 entries
#define INITIAL_PAGE_TAB 511 //MAP 128mb TO KERNEL SPACE

#define VIRT_TO_PML4_ADDR(x) ((0xff8000000000 & x) >> 39) & 0x1ff
#define VIRT_TO_PAGE_DIR_TAB_ADDR(x) (((0x7fc0000000 & x) >> 30) & 0x1ff)
#define VIRT_TO_PAGE_DIR(x) ((0x3fe00000 & x) >> 21) & 0x1ff
#define VIRT_TO_PAGE_TAB(x) (((0x1ff000 & x) >> 12) & 0x1ff)
#define VIRT_TO_PTE(x) ((0xfff & x) & 0xfff)
#define PHYS_ADDR_MASK 0xffffffffff000


#define PHYS_MEM_MAP_START 273
extern uint64_t _kernel_text_end;
extern uint64_t _kernel_text_start;
extern uint64_t _kernel_rodata_end;
extern uint64_t _kernel_rodata_start;

uint64_t initial_pml4[512] __attribute__((aligned(0x20)));         // must be aligned to (at least)0x20, ...
uint64_t initial_page_dir_tab[512] __attribute__((aligned(0x20))); // must be aligned to (at least)0x20, ...
uint64_t initial_page_dir[512] __attribute__((aligned(0x1000)));   // must be aligned to page boundary
uint64_t initial_page_tab[INITIAL_PAGE_TAB][512] __attribute__((aligned(0x1000)));
uint64_t initial_iden_page_dir_tab[512] __attribute__((aligned(0x1000)));
uint64_t initial_iden_page_dir[512] __attribute__((aligned(0x1000)));


uint64_t *kernel_pml4;
uint64_t *kernel_page_dir_tab;
uint64_t *kernel_iden_page_dir_tab;
uint64_t *kernel_iden_page_dir;
uint64_t *kernel_page_dir;
uint64_t **page_tbl;
uint64_t *pkernel_pml4;


static void early_fill_dir(uint64_t startaddr, uint64_t *dir)
{
    uint64_t address = startaddr;
    int i = 0;
    for (i = 0; i < 512; i++)
    {
        dir[i] = address | KERNEL_PAGE; // map address and mark it present/writable
        address = address + 0x1000;
    }
}

void early_setup_paging()
{
    //set each entry to not present
    uint64_t i = 0;
    uint64_t j = 0;
    uint64_t address = 0;
    

    for(i=0; i<512; i++){
        initial_pml4[i] = 0;
        initial_iden_page_dir_tab[i] = 0;
    }
    initial_pml4[511] = ((uint64_t)&initial_page_dir_tab - addr_start) | KERNEL_PAGE;     // set the page directory into the PDPT and mark it present
    initial_page_dir_tab[510] = ((uint64_t)&initial_page_dir - addr_start) | KERNEL_PAGE; // set the page directory into the PDPT and mark it present

    j = 0;
    for (i = 0; i < INITIAL_PAGE_TAB; i++)
    {
        initial_page_dir[i] = ((uint64_t)&initial_page_tab[j] - addr_start) | KERNEL_PAGE; //set the page table into the PD and mark it present/writable
        early_fill_dir((0x200000 * j), initial_page_tab[j]);
        j += 1;
    }
    //Setup initial 1-1 virt-to phys-mem mapping
    //Map first 512GB
    initial_pml4[PHYS_MEM_MAP_START] = KERN_VIRT_TO_PHYS(&initial_iden_page_dir_tab) | KERNEL_PAGE;
    initial_iden_page_dir_tab[0] = KERN_VIRT_TO_PHYS(&initial_iden_page_dir) | KERNEL_PAGE;
    address = 0;
    for(i=0; i<512; i++) {
        initial_iden_page_dir[i] = address | 131;
        address+=0x200000;
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

    while (phys_curr_addr < start + (len * PAGE_SIZE))
    {

        offset = VIRT_TO_PML4_ADDR(virt_curr_addr);

        //kprintf("1 pkernel_pml4:%x %d\n",pkernel_pml4,offset);
        if ((pkernel_pml4[offset] & PAGE_PRESENT) == 0) {
            pkernel_pml4[offset] = (uint64_t)pmem_alloc_zero_page() | KERNEL_PAGE;
        }
        curr = (uint64_t *)KERN_PHYS_TO_PVIRT((pkernel_pml4[offset] & PHYS_ADDR_MASK));
        offset = VIRT_TO_PAGE_DIR_TAB_ADDR(virt_curr_addr);

        //kprintf("2 curr:%x %d\n",curr,offset);
        //kprintf("0x%x\n",curr);
        if ((curr[offset] & PAGE_PRESENT) == 0) {
            curr[offset] = (uint64_t)pmem_alloc_zero_page() | KERNEL_PAGE ;
        }
        curr = (uint64_t *) KERN_PHYS_TO_PVIRT((curr[offset] & PHYS_ADDR_MASK));

        //kprintf("0x%x\n",curr);
        offset = VIRT_TO_PAGE_DIR(virt_curr_addr);

        //kprintf("3 curr:%x %d\n",curr,offset);
        if ((curr[offset] & PAGE_PRESENT) == 0) {
            curr[offset] = (uint64_t)pmem_alloc_zero_page() | KERNEL_PAGE ;
        }
        curr = (uint64_t *) KERN_PHYS_TO_PVIRT((curr[offset] & PHYS_ADDR_MASK));
        offset = VIRT_TO_PAGE_TAB(virt_curr_addr);
        //kprintf("4 curr:%x %d\n",curr,offset);
        //kprintf("Writing to %x\n",curr+offset);
        curr[offset] = phys_curr_addr | KERNEL_PAGE;
        phys_curr_addr += 0x1000;
        virt_curr_addr += 0x1000;
    }
    return true;
}

bool paging_map_user_range(struct pg_tbl *pg, uint64_t start, uint64_t virt_start,
                           uint64_t len, uint64_t page_ops)
{

    uint64_t virt_curr_addr = virt_start;
    uint64_t phys_curr_addr = start;
    uint16_t offset;
    uint64_t *curr = pg->pml4;
    //printf("call base pml4 %x\n",pg->pml4);
    while (phys_curr_addr < start + (len * PAGE_SIZE))
    {
        curr = pg->pml4;
        offset = VIRT_TO_PML4_ADDR(virt_curr_addr);
       // kprintf("pml4 %x %x %x\n",curr,curr[offset],offset);

        //Add checks here if we are out of the user range
        if ((curr[offset] & PAGE_PRESENT) == 0) {
               //     kprintf("Writing to %x\n",curr+offset);

            curr[offset] = (uint64_t)pmem_alloc_zero_page()| USER_PAGE;
            //kprintf("new pagedirtab %x %x\n",curr[offset],offset);
        }
        curr = (uint64_t *) KERN_PHYS_TO_PVIRT((curr[offset] & PHYS_ADDR_MASK));
        offset = VIRT_TO_PAGE_DIR_TAB_ADDR(virt_curr_addr);
       // kprintf("page_tab %x %x %x\n",curr,curr[offset],offset);


        if ((curr[offset] & PAGE_PRESENT) == 0) {
                    //kprintf("Writing to %x\n",curr+offset);

            curr[offset] = (uint64_t)pmem_alloc_zero_page() | USER_PAGE ;
        }
        curr = (uint64_t *) KERN_PHYS_TO_PVIRT((curr[offset] & PHYS_ADDR_MASK));
        offset = VIRT_TO_PAGE_DIR(virt_curr_addr);
       /// kprintf("page_dir %x %x %x\n",curr,curr[offset],offset);
        //        kprintf("done1\n");
        if ((curr[offset] & PAGE_PRESENT) == 0) {
       //                     kprintf("done2\n");
      //  kprintf("Writing to %x\n",curr+offset);
            curr[offset] = (uint64_t)pmem_alloc_zero_page() | USER_PAGE ;
        }
           //     kprintf("done\n");

        curr = (uint64_t *) KERN_PHYS_TO_PVIRT((curr[offset] & PHYS_ADDR_MASK));
        offset = VIRT_TO_PAGE_TAB(virt_curr_addr);
      //  kprintf("page\n");
      //  kprintf("Writing to %x\n",curr+offset);
       // curr[offset] = phys_curr_addr | (page_ops);
        curr[offset] = phys_curr_addr | (page_ops );
        phys_curr_addr += PAGE_SIZE;
        virt_curr_addr += PAGE_SIZE;
    }
    return true;

}

static void paging_free_page_dir(uint64_t* pgdir_addr) {
    int i;

    for (i=0; i<512; i++) {
        if((pgdir_addr[i]  & PAGE_PRESENT) != 0) {
#ifdef PAGING_DEBUG
            kprintf("free pgdir %x\n",pgdir_addr[i] & PHYS_ADDR_MASK );
#endif
            pmem_free_block(pgdir_addr[i] & PHYS_ADDR_MASK);
        } 
    }
}

static void paging_free_page_tab(uint64_t* pgtb_addr) {
    int i;

    for (i=0; i<512; i++) {
        if((pgtb_addr[i]  & PAGE_PRESENT) != 0) {
            paging_free_page_dir((uint64_t*)KERN_PHYS_TO_PVIRT((pgtb_addr[i] & PHYS_ADDR_MASK)));
#ifdef PAGING_DEBUG
            kprintf("free pgtab %x\n",pgtb_addr[i] & PHYS_ADDR_MASK );
#endif
            pmem_free_block(pgtb_addr[i] & PHYS_ADDR_MASK);
        } 
    }
}

static void paging_free_pml4(uint64_t* pml4_addr) {
    int i;

    for (i=0; i<512; i++) {
        if((pml4_addr[i]  & PAGE_PRESENT) != 0) {
            paging_free_page_tab((uint64_t*) KERN_PHYS_TO_PVIRT((pml4_addr[i] & PHYS_ADDR_MASK)));
#ifdef PAGING_DEBUG
            kprintf("free pml4 %x\n",pml4_addr[i] & PHYS_ADDR_MASK );
#endif
            pmem_free_block(pml4_addr[i] & PHYS_ADDR_MASK);
        } 

    }
    pmem_free_block((KERN_PVIRT_TO_PHYS(pml4_addr) & PHYS_ADDR_MASK));
}

bool paging_free_pg_tbl(struct pg_tbl *pg)
{
    //TODO: implement this
#ifdef PAGING_DEBUG
    kprintf("free %x\n",pg->pml4);
#endif
    paging_free_pml4(pg->pml4);
    return true;
}

void user_switch_paging(struct pg_tbl *pg)
{
    //return;
    int i = 0;
    for (i=0; i<PHYS_MEM_MAP_START; i++) {
        if ((pg->pml4[i] & PAGE_PRESENT) == 1)
            kernel_pml4[i] = pg->pml4[i];
        else
            kernel_pml4[i] = 0;
    }
    //Flush entire TLB on context switch... Not the best
        uint64_t address = (uint64_t)kernel_pml4 - addr_start;
    asm volatile("movq %0 ,%%cr3; \
            "
                 :
                 : "r"(address));
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

    kernel_pml4 = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_zero_page());
    kprintf("0xpml4:%x\n", kernel_pml4);
    kprintf("kyle test 0xpml4:%x\n", *(uint64_t *)kernel_pml4);
    kernel_page_dir_tab = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_zero_page());
    kprintf("page_dir_tab  %x\n", kernel_page_dir_tab);
    kprintf("page_dir_tab %x\n", KERN_VIRT_TO_PHYS(kernel_page_dir_tab));

    kernel_page_dir = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_zero_page());
    kernel_iden_page_dir_tab = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_zero_page());
    kernel_iden_page_dir = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_zero_page());

    kernel_pml4[VIRT_TO_PML4_ADDR(virt_curr_addr)] = KERN_VIRT_TO_PHYS(kernel_page_dir_tab) | KERNEL_PAGE;
    kernel_pml4[PHYS_MEM_MAP_START] = KERN_VIRT_TO_PHYS(kernel_iden_page_dir_tab) | KERNEL_PAGE;
    //TODO need check to only map physical memory that is present 
    //Use 2MB pages to map the first 1GB
    kernel_iden_page_dir_tab[0] = KERN_VIRT_TO_PHYS(kernel_iden_page_dir) | KERNEL_PAGE;
    address = 0;
    kprintf("%x\n",kernel_iden_page_dir);
    for(i=0; i<512; i++) {
        kernel_iden_page_dir[i] = address | 131;
        address+=0x200000;
    }

    kernel_page_dir_tab[VIRT_TO_PAGE_DIR_TAB_ADDR(virt_curr_addr)] = KERN_VIRT_TO_PHYS(kernel_page_dir) | KERNEL_PAGE;
    page_tbl = (uint64_t **)KERN_PHYS_TO_VIRT(pmem_alloc_zero_page());
    for (i = 0; i != 511; i++)
    {
        address = (uint64_t)pmem_alloc_zero_page();
        page_tbl[i] = (uint64_t *)KERN_PHYS_TO_VIRT(address);
    }
    i = 0;
    j = 0;

    while (curr_addr <= address)
    {
        // kprintf("%x\n",curr_addr);
        kernel_page_dir[i] = KERN_VIRT_TO_PHYS(page_tbl[i]) | KERNEL_PAGE;
        //kprintf("text %x\n",&_kernel_text_start);
        //        kprintf("text %x\n",&_kernel_text_end);
        if ((KERN_PHYS_TO_VIRT(curr_addr) >= (uint64_t) &_kernel_text_start &&
             KERN_PHYS_TO_VIRT(curr_addr)  <= (uint64_t) &_kernel_rodata_end))
            page_tbl[i][j] = curr_addr | KERNEL_PAGE_RO;
        else
            page_tbl[i][j] = curr_addr | KERNEL_PAGE;



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
    return true;
}

void paging_enable_protected()
{

    uint64_t cr0;
    asm volatile("movq %%cr0 ,%0" : "=g"(cr0));
    cr0 |= 1<<16;
    kprintf("cr0 %x\n",cr0);
    asm volatile("movq %0 ,%%cr0" :: "r"(cr0));
}

uint64_t walk(struct pg_tbl *pg, uint64_t va)
{

    uint64_t virt_curr_addr = va;
    uint64_t *curr = pg->pml4;
    uint64_t offset;

    //printf("call base pml4 %x\n",pg->pml4);
        curr = pg->pml4;
        offset = VIRT_TO_PML4_ADDR(va);
        if ((curr[offset] & PAGE_PRESENT) == 0)
            goto error;
        curr = (uint64_t *) KERN_PHYS_TO_PVIRT((curr[offset] & PHYS_ADDR_MASK));
        offset = VIRT_TO_PAGE_DIR_TAB_ADDR(virt_curr_addr);

        if ((curr[offset] & PAGE_PRESENT) == 0)
            goto error;
        
        curr = (uint64_t *) KERN_PHYS_TO_PVIRT((curr[offset] & PHYS_ADDR_MASK));
        offset = VIRT_TO_PAGE_DIR(virt_curr_addr);
        if ((curr[offset] & PAGE_PRESENT) == 0) 
            goto error;
        
        curr = (uint64_t *) KERN_PHYS_TO_PVIRT((curr[offset] & PHYS_ADDR_MASK));
        offset = VIRT_TO_PAGE_TAB(virt_curr_addr);
    
    //return pte;
    return &curr[offset];

    error:
        return NULL;

}



void pagefault() {
    uint64_t cr2;
    struct ktask *proc = get_current_process();
    asm volatile("movq %%cr2 ,%0" : "=g"(cr2));
    cr2 &=0xfffffffffffff000;
        if (cr2 == 0)
    {
        kprintf("Test123123\n");
    }
    kprintf("pagefault on 0x%x !\n",cr2);
    asm("cli; hlt");
    uint64_t *pte =(uint64_t *)walk(&(proc->mm->pagetable),cr2);
    kprintf("pte:%x\n",pte);

    if (pte) {
        *pte |= PAGE_PRESENT;
    }
    else{
        sched_process_kill(proc->pid,true);
        schedule();
    }
    kprintf("pte:%x\n",*pte);


}
