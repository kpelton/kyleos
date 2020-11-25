#include <asm/asm.h>
#include <output/output.h>
#include <init/tables.h>
#include <block/ata.h>
#include <irq/irq.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <timer/pit.h>

#define HALT() asm("hlt: jmp hlt")
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


void kprompt(void)
{
    kprintf(">");
}
void ksleep(unsigned int sec) {
    unsigned int expires = read_jiffies()+(sec*100);
    while(read_jiffies() < expires);
        
}
void kernel(void)
{
    char buffer[10];
    char *test=0;
    
    test = kmalloc(0x400);
    itoa((unsigned long)test,buffer,16);
    kprintf("heap");
    kprintf(buffer);
    kprintf("\n");
    test = kmalloc(0x400);

    HALT();
}
void kinit(void)
{

    output_init();
    kprintf("Booting.......\n");
    kprintf("Ted Wheeler OS.......\n");
    kprintf("Copyright:Kyle Pelton 2020 \n");
    gdt_install();
    kprintf("Installing idt\n");
    idt_install();
    kprintf("PIC init done..\n");  
    PIC_init();
    pit_init();
    setup_paging();
    mm_init();
    //need to setup kernel stack after paging is setup
    asm("mov $0xffffffff84000000,%rsp");
    kprintf("Switch to kernel tables/stack done.\n");
    ata_init();
    kernel();
}
void kmain(void)
{
    kinit();  
}



