#include "asm.h"
#include "irq.h"
#include "output.h"
#define PORT 0x3f8   /* COM1 */

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
}
void serial_irq() {
   char in;
   char *output = "x";
   outb(PORT + 3, 0x00); //dlab = 0 
   in = inb(PORT);
   output[0] = in;
   kprintf(output);
   PIC_sendEOI(1);
}


