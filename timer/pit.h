#ifndef PIT_H
#define PIT_H
void pit_init();
unsigned int read_jiffies();
void timer_irq();
#endif
