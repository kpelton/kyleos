#ifndef PIT_H
#define PIT_H
#include <include/types.h>
void pit_init();
uint32_t read_jiffies();
void timer_irq();
#endif
