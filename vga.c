#include "vga.h"
#include "asm.h"
static int cursor_x = 0;
static int cursor_y = 0;

static unsigned char *vram = (unsigned char *)0x100B8000;

void update_cursor(int row, int col) {
    unsigned short position=(row*80) + col;
    // cursor LOW port to vga INDEX register
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(position&0xFF));
    // cursor HIGH port to vga INDEX register
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char )((position>>8)&0xFF));
}

void vga_clear(){
    int i;

    for(i=0; i<(80*25)*2; i++) {
        vram[i] = 0x00;
    }
}


void print_loc(const int x, const int y, 
        const int back, const int fore, 
        const char c, const int blink) {

    int i = (x*2)+((y*80)*2);
    vram[i] = c;
    vram[i+1] = (fore) | (back << 4) | ((blink &1)<<8) ;
}


void kprintf(char *str) {

    for(;*str !='\0'; str++) {

        if (*str == '\n') {
            cursor_y++;
            cursor_x=0; 
        } else {
            print_loc(cursor_x,cursor_y,0, 4,*str,0);
            cursor_x++;   
        }
        if(cursor_x >= 80){
            cursor_y++;
            cursor_x=0;
        }

        if (cursor_y >= 25){
            cursor_y = 4;
            cursor_x = 0;
        }
        update_cursor(cursor_y,cursor_x);
    }
}
char * itoa( unsigned int value, char * str, int base )
{
    char * rc;
    char * ptr;
    char * low;
    // Check for supported base.
    if ( base < 2 || base > 36 )
    {
        *str = '\0';
        return str;
    }
    rc = ptr = str;
    // Set '-' for negative decimals.
    if (  base == 10 )
    {
        *ptr++ = '-';
    }
    // Remember where the numbers start.
    low = ptr;
    // The actual conversion.
    do
    {
        // Modulo is negative for negative value. This trick makes abs() unnecessary.
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + value % base];
        value /= base;
    } while ( value );
    // Terminating the string.
    *ptr-- = '\0';
    // Invert the numbers.
    while ( low < ptr )
    {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }
    return rc;
}
void print_regs() {
    struct RegDump dump;
    char buffer[50];
    //kprintf("\nKERNEL PANIC\nREGISTER DUMP\n=============\n");
    dump = dump_regs();
    itoa(dump.rax,buffer,16);
    kprintf("rax:0x");
    kprintf(buffer);
    kprintf(" ");

    itoa(dump.rbx,buffer,16);
    kprintf("rbx:0x");
    kprintf(buffer);
    kprintf(" ");

    itoa(dump.rcx,buffer,16);
    kprintf("rcx:0x");
    kprintf(buffer);
    kprintf(" ");

    itoa(dump.rdi,buffer,16);
    kprintf("rdi:0x");
    kprintf(buffer);
    kprintf(" ");

    itoa(dump.rsi,buffer,16);
    kprintf("rsi:0x");
    kprintf(buffer);
    kprintf(" ");

    itoa(dump.rdx,buffer,16);
    kprintf("rdx:0x");
    kprintf(buffer);
    kprintf(" ");

    itoa(dump.rsp,buffer,16);
    kprintf("rsp:0x");
    kprintf(buffer);
    kprintf("\n");

    itoa(dump.cr0,buffer,16);
    kprintf("cr0:0x");
    kprintf(buffer);
    kprintf(" ");

    itoa(dump.cr3,buffer,16);
    kprintf("cr3:0x");
    kprintf(buffer);
    kprintf(" ");

    itoa(dump.cr4,buffer,16);
    kprintf("cr4:0x");
    kprintf(buffer);
    kprintf("\n");
}
