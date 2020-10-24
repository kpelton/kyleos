#include "paging.h"
#include "vga.h"
// 512 entriesi
#define addr_start 0x10000000
typedef  unsigned long long uint64_t;

uint64_t pml4[4] __attribute__((aligned(0x20))); // must be aligned to (at least)0x20, ...
uint64_t page_dir_ptr_tab[512] __attribute__((aligned(0x20))); // must be aligned to (at least)0x20, ...
uint64_t page_dir[512] __attribute__((aligned(0x1000)));  // must be aligned to page boundary
uint64_t page_tab[100][512] __attribute__((aligned(0x1000)));


void fill_dir(uint64_t startaddr,uint64_t * dir) {
    uint64_t address = startaddr;
    int i=0;
    for(i = 0; i < 512; i++) {
        dir[i] = address | 3; // map address and mark it present/writable
        address = address + 0x1000;
    }
}

void setup_paging() {
    //set each entry to not present
    char buffer[200];
    uint64_t i = 0;
    uint64_t j = 0;
    uint64_t address = 0;

    pml4[0] = ((uint64_t)&page_dir_ptr_tab -addr_start)| 3; // set the page directory into the PDPT and mark it present
    page_dir_ptr_tab[0] = ((uint64_t)&page_dir -addr_start) | 3; // set the page directory into the PDPT and mark it present

    j=0;
    for(i = 128; i < 128+100; i++) {
        page_dir[i] = ((uint64_t)&page_tab[j] -addr_start)| 3; //set the page table into the PD and mark it present/writable
        fill_dir((0x200000*j),page_tab[j]);
        j+=1;
    }
    address =(uint64_t) &pml4 - 0x10000000;
    itoa(address,buffer,16);
    kprintf(buffer);
    kprintf("\n");
    asm volatile("cli; \
            movq %0 ,%%cr3; \
            sti; \
            " : : "r"(address));
}

