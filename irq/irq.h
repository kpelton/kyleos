#ifndef IRQ_H
#define IRQ_H
#include <include/types.h>
void PIC_init(void);
void kbd_irq(void);
void PIC_sendEOI(uint8_t irq);
uint32_t read_jiffies();
#endif
