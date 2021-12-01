#ifndef PIT_H
#define PIT_H

#include <include/types.h>
#define HZ 100
#define TICK_HZ 1000
void pit_init();
uint32_t read_jiffies();
void timer_irq();
#endif
