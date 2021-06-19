#ifndef TIMER_H
#define TIMER_H

enum timer_states{
    TIMER_EXPIRED,
    TIMER_RUNNING,
    TIMER_STOPPED,
    TIMER_UNUSED,
    TIMER_MAX_STATE
};

struct basic_timer {
    //Start time in jiffies
    unsigned int start_time;
    unsigned int end_time;
    unsigned char state;
};

int update_timer(struct basic_timer *t);
struct basic_timer new_timer(unsigned int ms);
#endif

