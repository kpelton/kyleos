#include <asm/asm.h>
#include <irq/irq.h>
#include <output/output.h>
#include <output/input.h>
#include <output/tty.h>
#include <locks/spinlock.h>
#include <locks/mutex.h>
static struct spinlock uart_spinlock;
static struct mutex uart_print_mutex;
#define PORT 0x3f8   /* COM1 */
#define MAX_CHARS 512

static void serial_write_byte(uint8_t byte)
{
    /* Do not overwrite the transmit holding register during large VT100
     * redraws.  The bounded loop also keeps early panic output from hanging
     * forever if COM1 is unavailable. */
    for (uint32_t attempt = 0; attempt < 100000; attempt++)
        if ((inb(PORT + 5) & 0x20) != 0) {
            outb(PORT, byte);
            return;
        }
}

void serial_kprintf(char* str)
{
   //cquire_mutex(&uart_print_mutex);

    char* strp = str;
    while (*strp != '\0')  {

        serial_write_byte((uint8_t)*strp);

        strp++;
        
    }
     //  release_mutex(&uart_print_mutex);

}

void serial_write(const void *buffer, uint32_t count)
{
    const char *bytes = buffer;

    for (uint32_t index = 0; index < count; index++)
        serial_write_byte((uint8_t)bytes[index]);
}

void serial_init()
{
   init_mutex(&uart_print_mutex);
   init_spinlock(&uart_spinlock);
   acquire_spinlock(&uart_spinlock);
   outb(PORT + 1, 0x00);    // Disable all interrupts
   outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)mutulu
   outb(PORT + 0, 0x01);    // Set divisor to 3 (lo byte) 38400 baud
   outb(PORT + 1, 0x00);    //                  (hi byte)
   outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
   /* Interrupt as soon as one byte arrives. Interactive commands are often
    * shorter than the UART's 14-byte trigger and must not remain buffered. */
   outb(PORT + 2, 0x07);    // Enable FIFO, clear both FIFOs, 1-byte threshold
   outb(PORT + 4, 0x0b);   // DTR, RTS, and OUT2 (route UART IRQ)
   outb(PORT + 1, 0x01);   // Enable received-data interrupt
      release_spinlock(&uart_spinlock);

#ifndef KYLEOS_DISABLE_SERIAL_OUTPUT
   serial_kprintf("\033c");
#endif
}

void serial_irq()
{
    acquire_spinlock(&uart_spinlock);
    /* Keep DLAB clear without changing the configured 8-N-1 format, then
     * drain the FIFO. Reading only one byte strands burst input because the
     * UART's receive interrupt is edge-triggered through the legacy PIC. */
    outb(PORT + 3, 0x03);
    while ((inb(PORT + 5) & 0x01) != 0)
        tty_receive_serial_char((char)inb(PORT));
    PIC_sendEOI(1);
    release_spinlock(&uart_spinlock);
}
