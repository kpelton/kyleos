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
    outb(0x40,0x1);
    asm("sti");
}
void kinit(void)
{
    vga_clear();
    kprintf("SIL-OS 64b\n");
    kprintf("Copyright:Kyle Pelton 2020 \n");
    gdt_install();
    kprintf("Installing idt\n");
    idt_install();
    kprintf("PIC init done..\n");  
    PIC_init();
    pit_setup();
    setup_paging();
    kprintf("Switch to kernel tables done.");
    HALT();
}
void kmain(void)
{
    kinit();  
}



