#include <asm/asm.h>
#include <output/output.h>
#include <init/tables.h>
#include <block/ata.h>
#include <irq/irq.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <mm/pmem.h>
#include <timer/pit.h>
#include <fs/vfs.h>
#include <sched/sched.h>
#include <timer/timer.h>
#include <init/dshell.h>
#include <include/multiboot.h>
#include <include/types.h>
#include <sched/exec.h>
#define STACK_PAGES 256
//#define DSHELL_EN
static void idle_loop()
{
    for(;;) {
        asm("sti;hlt");
    }
}

static void kernel(void)
{
    kprintf("Kyle OS has booted\n");
    kthread_add(idle_loop, "Idle loop");
#ifdef DSHELL_EN    
    kthread_add(start_dshell,"D Shell");
#else
    int retval = -1;
    struct dnode *dptr = vfs_read_root_dir("/");
    struct inode *iptr = vfs_walk_path("nushell", dptr);

    if (iptr != NULL)
    {
        retval = exec_from_inode(iptr,false,NULL);
        if(retval <0) {
            panic("Unable to start userspace shell");
        }
    }
#endif
    //Should never return after this point since scheduler will take over
    for(;;)
        asm("sti;hlt");
}

static void kinit(void)
{
    kprintf("Booting.......\n");
    kprintf("Kyle OS.......\n");
    kprintf("Copyright:Kyle Pelton 2020-2025 all rights reserved\n");
    kprintf("Install GDT\n");
    gdt_install();
    tss_flush();
    idt_install();
    kprintf("interrupts init done\n");
    early_setup_paging();
    kprintf("Early page init done\n");
    phys_mem_init();
    kprintf("Phys mem init done\n");
    paging_enable_protected();
    mm_init();
    kprintf("Allocating stack\n");
    uint64_t kernel_stack = KERN_PHYS_TO_VIRT(pmem_alloc_block(STACK_PAGES));
    paging_map_kernel_range(KERN_VIRT_TO_PHYS(kernel_stack),STACK_PAGES);
    kprintf("Stack start %x Stack %x\n",kernel_stack,kernel_stack+4096*STACK_PAGES);
    asm volatile("movq %0,%%rsp " : : "r"(kernel_stack+4096*STACK_PAGES));
    kprintf("MM init done\n");
    PIC_init();
    kprintf("PIC init done\n");
    ata_init();
    timer_system_init();
    exec_init();
    sched_init();
    fpu_init();
    ramfs_init();
    kernel();
}

void kmain(uint64_t  mb_info, uint64_t multiboot_magic)
{
    //First c code
    output_init();
    kprintf("Multiboot header_loc:%x magic:%x\n",mb_info,multiboot_magic);

    if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC)
        panic("MULTIBOOT_BOOTLOADER_MAGIC was not passed to kernel correctly");

    phys_mem_early_init(mb_info);
    kinit();
}



