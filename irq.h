#ifndef IRQ_H
#define IRQ_H
void PIC_init(void);
void kbd_irq(void);
unsigned int read_jiffies();
#endif
