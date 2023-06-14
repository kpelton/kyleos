#ifndef PIT_H
#define PIT_H

#include <include/types.h>
#define HZ 50
#define TICK_HZ 100
void pit_init();
uint32_t read_jiffies();
void timer_irq();
#endif
