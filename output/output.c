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

int kpow(int base,int exp) {
    int i;
    int val = 1;
    for (i=0; i<exp; i++)
        val*= base;
    return val;
}
//only handles >=0
int atoi(char* str) {
    char * ptr = str;
    int size = 1;
    int i;
    int place=0;
    int val = 0;
    int cval = 0;
    while (*(ptr+1) != '\0') {
        ptr++;
        size++;
    }

    place=0;
    for(i=size; i>0; i--) {

        cval = ((*ptr) -0x30);
        cval *= kpow(10,place);
        val += cval;
        ptr--;
        place+=1;
    }
    return val;
}
char * kstrcpy(char *dest, const char *src) {
    int i;
    for (i=0; src[i] != '\0'; i++) 
        dest[i] = src[i];
    dest[i] = '\0';
    return dest;
}

char * kstrncpy(char *dest, const char *src,int bytes) {
    int i;
    for (i=0; i <bytes; i++) {
        dest[i] = src[i];
        if (src[i] == '\0') {
            break;
        }
    }
     
    return dest;
}

int kstrcmp(char *dest, const char *src) {
    int i;
    for (i=0; (src[i] != '\0' || dest[i] !='\0' ) ; i++) 
        if (src[i] != dest[i]) {
            return -1;
        }
    return 0;
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
void kprint_hex(char *desc, unsigned long val) {
    char buffer[20];
    kprintf(desc);
    itoa(val,buffer,16);
    kprintf(buffer);
    kprintf("\n");
}

void read_input(char * dest) {
     serial_read_input(dest);
}
