#include <asm/asm.h>
#include <irq/irq.h>
#include <output/output.h>
#include <output/input.h>
#include <locks/spinlock.h>
#include <locks/mutex.h>
static struct spinlock uart_spinlock;
static struct mutex uart_print_mutex;
#define PORT 0x3f8   /* COM1 */
#define MAX_CHARS 512

void serial_kprintf(char* str)
{
   acquire_mutex(&uart_print_mutex);

    char* strp = str;
    while (*strp != '\0')  {

        outb(PORT,*strp);

        strp++;
        
    }
       release_mutex(&uart_print_mutex);

}

void serial_init()
{
   init_mutex(&uart_print_mutex);
   init_spinlock(&uart_spinlock);
   acquire_spinlock(&uart_spinlock);
   outb(PORT + 1, 0x00);    // Disable all interrupts
   outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
   outb(PORT + 0, 0x01);    // Set divisor to 3 (lo byte) 38400 baud
   outb(PORT + 1, 0x00);    //                  (hi byte)
   outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
   outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
   outb(PORT + 1, 0x1);    // IRQs enabled, RTS/DSR set
      release_spinlock(&uart_spinlock);

   serial_kprintf("\033c");
}

void serial_irq()
{
    acquire_spinlock(&uart_spinlock);
    char in;
    outb(PORT + 3, 0x00); //dlab = 0 
    in = inb(PORT);
    input_add_char(in);
    PIC_sendEOI(1);
    release_spinlock(&uart_spinlock);
 
}


