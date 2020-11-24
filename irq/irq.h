#ifndef IRQ_H
#define IRQ_H
void PIC_init(void);
void kbd_irq(void);
void PIC_sendEOI(unsigned char irq);
unsigned int read_jiffies();
#endif
