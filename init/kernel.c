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

void test_user_function ();
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
void kernel(void)
{
    kprintf("Kyle OS has booted\n");
    kthread_add(idle_loop, "Idle loop");
    for(int i=0; i<3; i++)
     user_process_add(&test_user_function,"Test userspace3");
 

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
    kprintf("Installing idt\n");
    idt_install();
    PIC_init();
    kprintf("PIC init done\n");
    kprintf("MM init\n");
    setup_paging();
    //user mode test
    mm_init();
    //need to setup kernel stack after paging is setup
    asm("mov $0xffffffffbf000000,%rsp");
    kprintf("Switch to kernel tables/stack done.\n");
    kprintf("RTC init done\n");
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



