#ifndef _PIT_H
#define _PIT_H
#include <include/types.h>
extern uint64_t HZ;
extern uint64_t TICK_HZ;
void pit_init();
uint32_t read_jiffies();
void timer_irq();
#endif
