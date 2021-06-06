#include <asm/asm.h>
#include <irq/irq.h>
#include <output/output.h>
#define PORT 0x3f8   /* COM1 */
#define MAX_CHARS 512
static char SERIAL_BUFFER[MAX_CHARS] = {'\0'};
static char INTERNAL_SERIAL_BUFFER[MAX_CHARS];

int SERIAL_CURRENT_PLACE;

void serial_kprintf(char* str) {
    char* strp = str;
    while (*strp != '\0')  {
        outb(PORT,*strp);
        strp++;
    }
}

void serial_init() {
   outb(PORT + 1, 0x00);    // Disable all interrupts
   outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
   outb(PORT + 0, 0x01);    // Set divisor to 3 (lo byte) 38400 baud
   outb(PORT + 1, 0x00);    //                  (hi byte)
   outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
   outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
   outb(PORT + 1, 0x1);    // IRQs enabled, RTS/DSR set
   serial_kprintf("\033c");
   for (int i = 0; i<MAX_CHARS; i++ )
        SERIAL_BUFFER[i] = '\0';
}
void serial_read_input(char* dest) {
    asm("cli");
    kstrcpy(dest,SERIAL_BUFFER);
    for (int i=0; i<MAX_CHARS; i++) 
        SERIAL_BUFFER[i] = '\0';
    asm("sti");
}

void serial_irq() {
    asm("cli");

    char in;
    char output[1];
    outb(PORT + 3, 0x00); //dlab = 0 
    in = inb(PORT);
    if (in == '\r')
        in = '\n';

    INTERNAL_SERIAL_BUFFER[SERIAL_CURRENT_PLACE] = in;
    
    SERIAL_CURRENT_PLACE +=1;
    if (in == '\n') {
        INTERNAL_SERIAL_BUFFER[SERIAL_CURRENT_PLACE] = '\0';  
        kstrcpy(SERIAL_BUFFER,INTERNAL_SERIAL_BUFFER);
        for (int i=0; i<MAX_CHARS; i++) 
            INTERNAL_SERIAL_BUFFER[i] = '\0';
        SERIAL_CURRENT_PLACE=0;
    }
    if (SERIAL_CURRENT_PLACE == MAX_CHARS-1 )
        SERIAL_CURRENT_PLACE = 0;

    output[0] = in;
    output[1] = '\0';
    kprintf(output);
    PIC_sendEOI(1);
    asm("sti");

}


