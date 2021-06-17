#include <asm/asm.h>
#include <output/output.h>
#include <init/tables.h>
#include <block/ata.h>
#include <irq/irq.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <timer/pit.h>
#include <block/vfs.h>
#include <sched/sched.h>
#include <init/dshell.h>

#define HALT() asm("cli; hlt")
#define START() asm("sti; run: hlt; jmp run")
#define HANG() asm("cli; hlt")

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
void testf()
{
    int i=0;
    for(;;) {
        asm("sti");
        if ( i%1000000 == 1 )
          //  kprint_hex("RUN3 ",read_jiffies());
        i+=1;
    }

    asm("sti; run1: hlt; jmp run1");
}

void idle_loop()
{
    int i=0;
    for(;;) {
        asm("sti");
        asm("hlt");
    }

    asm("sti; run1: hlt; jmp run1");
}
void kernel(void)
{
    kprintf("Ted Wheeler OS has booted\n");
	kthread_add(&start_dshell,"D Shell");
    kthread_add(&idle_loop, "Idle loop");

    

    //start_dshell();
    START();
}

void kinit(void)
{
    output_init();
    kprintf("Booting.......\n");
    kprintf("Ted Wheeler OS.......\n");
    kprintf("Copyright:Kyle Pelton 2020 all rights reserved\n");
    kprintf("Install GDT\n");
    gdt_install();
        tss_flush();

    kprintf("Installing idt\n");
    idt_install();
    PIC_init();
    pit_init();
    kprintf("PIC init done..\n");
    kprintf("MM init\n");
    setup_paging();
    //user mode test
    mm_init();
    //need to setup kernel stack after paging is setup
    asm("mov $0xffffffffbf000000,%rsp");
    kprintf("Switch to kernel tables/stack done.\n");

    ata_init();
    //IRQ_set_mask(0);//timer
    //jump_usermode();

    kernel();
}
void kmain(void)
{
    kinit();
}



