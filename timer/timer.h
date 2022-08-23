#ifndef TIMER_H
#define TIMER_H
#include <include/types.h>
#include <locks/spinlock.h>
enum timer_states{
    TIMER_EXPIRED,
    TIMER_RUNNING,
    TIMER_STOPPED,
    TIMER_UNUSED,
    TIMER_MAX_STATE
};

extern const char *str_timer_states[];

struct basic_timer {
    //Start time in jiffies
    uint32_t start_time;
    uint32_t end_time;
    uint8_t state;
    struct spinlock timer_lock;
};

int update_timer(struct basic_timer *t);
struct basic_timer new_timer(uint32_t ms);
void timer_system_init();
#endif
