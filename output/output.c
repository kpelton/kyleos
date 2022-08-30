#include <output/vga.h>
#include <output/uart.h>
#include <output/output.h>
#include <output/input.h>
#include <asm/asm.h>
#include <include/stdarg.h>
#include <include/types.h>

void output_init() {
    vga_clear();
    serial_init();
    input_init();
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

uint8_t * memcpy8(uint8_t *dest, const uint8_t *src,uint64_t bytes) {
    uint64_t i;
    for (i=0; i <bytes; i++) {
        dest[i] = src[i];
    }
    return dest;
}

uint8_t * memzero8(uint8_t *dest,uint64_t bytes) {
    uint64_t i;
    for (i=0; i <=bytes; i++) {
        dest[i] = 0;
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

int kstrstr(char *base, char *delim)
{
    char *baseptr = base;
    int kstart = 0;
    if (!base || !delim || !*base || !*delim)
        return -1;
    int val = 0;
    while (*baseptr)
    {
        if (*baseptr && *delim && *baseptr == *delim)
        {
            val = kstart;
        }
        while (*baseptr && *delim && *baseptr == *delim)
        {

            baseptr++;
            delim++;
            if (!*delim)
            {
                return val;
            }
            val++;
        }
        baseptr++;
        kstart++;
    }
    return -1;
}


char * itoa( unsigned long value, char * str, int base ) {
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

static void print_reg(char * name,unsigned long val)
{
    char buffer[50];
    itoa(val,buffer,16);
    kprintf("%s:0x%x ",name,val);
}
void print_regs(unsigned long exception,unsigned long rip) {
    struct RegDump dump;
    char buffer[50];
    kprintf("\n\nUnhandled Exception\nREGISTER DUMP\n=============\n");
    kprintf("EXCEPTION:");
    itoa(exception,buffer,16);
    kprintf("0x");
    kprintf(buffer);
    kprintf("\n");
    dump = dump_regs();
    print_reg("rip",rip);
    kprintf("\n");
    print_reg("rax",dump.rax);
    print_reg("rbx",dump.rbx);
    print_reg("rcx",dump.rcx);
    kprintf("\n");
    print_reg("rdx",dump.rdx);
    print_reg("rsp",dump.rsp);
    print_reg("rbp",dump.rbp);
    print_reg("rdi",dump.rdi);
    kprintf("\n");
    print_reg("rsi",dump.rsi);
    print_reg("r8",dump.r8);
    print_reg("r9",dump.r9);
    print_reg("r10",dump.r10);
    kprintf("\n");
    print_reg("r11",dump.r11);
    print_reg("r12",dump.r12);
    print_reg("r13",dump.r13);
    kprintf("\n");
    print_reg("r14",dump.r14);
    print_reg("r15",dump.r15);
    kprintf("\n=============\n");
    print_reg("cr0",dump.cr0);
    kprintf("\n");
    print_reg("cr2",dump.cr2);
    kprintf("\n");
    print_reg("cr3",dump.cr3);
    kprintf("\n");
    print_reg("cr4",dump.cr4);
    kprintf("\n");
    print_reg("flags",dump.flags);
    kprintf("\n");
}

static void puts( char *str) {
   // vga_kprintf(str);
    serial_kprintf(str);
}

static void putc( char c) {
    char buffer[2] = {'\0'};
    buffer[0] = c;

   // vga_kprintf(buffer);
    serial_kprintf(buffer);

}

void kprintf(char *format, ...)
{
    char *ptr = format;
    unsigned long x;
    char * str_ptr;
    char buffer[20];
    va_list arguments;
    va_start(arguments, format);
    while(*ptr != '\0') {
        if (*ptr == '%') {
            ptr++;
            switch (*ptr) {
                case 'x':
                    x = va_arg(arguments,unsigned long);
                    itoa(x,buffer,16);
                    puts(buffer);
                break;
                case 'd':
                    x = va_arg(arguments,unsigned long);
                    itoa(x,buffer,10);
                    puts(buffer);
                break;
                case 's':
                    str_ptr = va_arg(arguments,char *);
                    puts(str_ptr);
                break;
                default:
                panic("unable to handle kprintf type");
            }
        } else {
            putc(*ptr);
        }
        ptr++;
    }
    va_end(arguments);
}

void read_input(char * dest) {
     input_read(dest);
}

int kstrlen(char *str) {
    char *str_p;
    str_p= str;
    int len=0;
    for(;*str_p !='\0'; str_p++)
        len++;
    return len;
}

void panic(char *msg)
{
    kprintf(msg);
    print_regs(0xdeadbeef,0xdeadbeef);
    asm("cli");
    asm("hlt");

}
