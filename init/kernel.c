#include <asm/asm.h>
#include <output/output.h>
#include <init/tables.h>
#include <block/ata.h>
#include <irq/irq.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <timer/pit.h>
#include <block/vfs.h>
#include <init/dshell.h>

#define HALT() asm("cli; hlt")
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

void kernel(void)
{
    kprintf("Ted Wheeler OS has booted\n");
    start_dshell();
    HALT();
}

void kinit(void)
{
    output_init();
    kprintf("Booting.......\n");
    kprintf("Ted Wheeler OS.......\n");
    kprintf("Copyright:Kyle Pelton 2020 all rights reserved\n");
    gdt_install();
    kprintf("Installing idt\n");
    idt_install();
    PIC_init();
    pit_init();
    kprintf("PIC init done..\n");
    setup_paging();
    mm_init();
    //need to setup kernel stack after paging is setup
    asm("mov $0xffffffffbf000000,%rsp");
    kprintf("Switch to kernel tables/stack done.\n");
    ata_init();
    kernel();
}
void kmain(void)
{
    kinit();
}



