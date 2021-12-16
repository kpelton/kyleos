#include <asm/asm.h>
#include <output/output.h>
#include <init/tables.h>
#include <block/ata.h>
#include <irq/irq.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <mm/pmem.h>
#include <timer/pit.h>
#include <block/vfs.h>
#include <sched/sched.h>
#include <timer/timer.h>
#include <init/dshell.h>
#include <include/multiboot.h>
#include <include/types.h>
#define STACK_PAGES 128

void test_user_function ();
void test_user_function5 ();
void print_vendor()
{
    unsigned int  b,c,d;
    int * string = kmalloc(sizeof(int)*20);
    string[12]='\0';
    asm volatile( "cpuid;"
            :"=b"(b), "=c"(c),"=d"(d));

    string[0] = b;
    string[1] = d;
    string[2] = c;
    kprintf("\nCPU:\n");
    kprintf((char *)string);
    kprintf("\n\n");
}

int strlen(char *str)
{
    char *str_p;
    str_p= str;
    int len=0;
    for(;*str_p !='\0'; str_p++)
        len++;
    return len;
}

void ksleep(unsigned int sec)
{
    unsigned int expires = read_jiffies()+(sec*100);
    while(read_jiffies() < expires);

}
void test_sleep()
{
    for(;;) {
        asm("sti");
        ksleepm(1000);
    }

    asm("sti; run1: hlt; jmp run1");
}

void idle_loop()
{
    for(;;) {
        //schedule();
        asm("hlt");
    }
}
bool setup_kernel_stack(){
        kprintf("Allocating stack\n");
        uint64_t kernel_stack = KERN_PHYS_TO_VIRT(pmem_alloc_block(16));
        paging_map_kernel_range(KERN_VIRT_TO_PHYS(kernel_stack),16);
        asm volatile("movq %%rsp ,%0" : "=g"(kernel_stack));
        return true;
}
void kernel(void)
{
    kprintf("Kyle OS has booted\n");
    kthread_add(idle_loop, "Idle loop");
        user_process_add(&test_user_function5,"Test userspace3");
        user_process_add(&test_user_function,"Test userspace3");
        user_process_add(&test_user_function5,"Test userspace3");
        user_process_add(&test_user_function,"Test userspace3");
                user_process_add(&test_user_function5,"Test userspace3");
        user_process_add(&test_user_function,"Test userspace3");
        user_process_add(&test_user_function5,"Test userspace3");
        user_process_add(&test_user_function,"Test userspace3");
                user_process_add(&test_user_function5,"Test userspace3");
        user_process_add(&test_user_function5,"Test userspace3");
\

	kthread_add(&start_dshell,"D Shell");

    asm("sti");

    while(1) {
        asm("sti");

    }
}

void kinit(void)
{
    kprintf("Booting.......\n");
    kprintf("Kyle OS.......\n");
    kprintf("Copyright:Kyle Pelton 2020-2021 all rights reserved\n");
    kprintf("Install GDT\n");
    gdt_install();
    tss_flush();
    idt_install();

    kprintf("interrupts are done\n");

    early_setup_paging();
    kprintf("Early page init done\n");
    phys_mem_init();
    kprintf("Phys Init mem done\n");
    //user mode test
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

    kernel();
    
}

void kmain(uint64_t  mb_info, uint64_t multiboot_magic)
{

    output_init();
    kprintf("Multiboot header_loc:%x magic:%x\n",mb_info,multiboot_magic);

    if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC)
        panic("MULTIBOOT_BOOTLOADER_MAGIC was not passed to kernel correctly");

    phys_mem_early_init(mb_info);
    kinit();
}



