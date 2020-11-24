#include <output/vga.h>
#include <output/uart.h>
#include <asm/asm.h>


void kprintf(char *str) {
    vga_kprintf(str);
    serial_kprintf(str);
}
void output_init() {
    vga_clear();
    serial_init();
}

//refactor itoa
char * itoa_8( unsigned char value, char * str, int base )
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
char * itoa_16( unsigned short value, char * str, int base )
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
char * itoa( unsigned long value, char * str, int base )
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
void print_regs(unsigned long exception) {
    struct RegDump dump;
    char buffer[50];
    kprintf("\nKERNEL PANIC\nREGISTER DUMP\n=============\n");
    kprintf("EXCEPTION:");
    itoa(exception,buffer,16);
    kprintf(buffer);
    kprintf("\n");
    itoa(dump.rax,buffer,16);
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
