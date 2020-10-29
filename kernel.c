#include "asm.h"
#include "vga.h"
#include "tables.h"
#include "irq.h"
#include "paging.h"
char * heap = (char *)0x400000;
#define HALT() asm("hlt: jmp hlt")
#define HANG() asm("cli; hlt")
void setup_long_mode();

void * kmalloc(unsigned int size)
{
    heap+=size+1;
    return heap;
}

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
void pit_setup(void) {
    asm("cli");
    outb(0x43,0x34);
    //set 100hz 11931 0x2e9b
    outb(0x40,0x9b);
    outb(0x40,0x2e);
    asm("sti");
}
void ksleep(unsigned int sec) {
    unsigned int expires = read_jiffies()+(sec*100);
    while(read_jiffies() < expires);
        
}
void kernel(void)
{
    while (1) {
        ksleep(1);
        kprintf("Sleep test\n");
        ksleep(5);
        kprintf("Sleep test 5\n");
    }
    HALT();
}
void kinit(void)
{

    vga_clear();
    kprintf("Booting.......\n");
    kprintf("Ted Wheeler OS.......\n");
    kprintf("Copyright:Kyle Pelton 2020 \n");
    gdt_install();
    kprintf("Installing idt\n");
    idt_install();
    kprintf("PIC init done..\n");  
    PIC_init();
    pit_setup();
    setup_paging();
    //need to setup kernel stack after paging is setup
    asm("mov $0xffffffff84000000,%rsp");
    kprintf("Switch to kernel tables/stack done.\n");
    kernel();
}
void kmain(void)
{
    kinit();  
}



