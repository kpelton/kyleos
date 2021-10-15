#include <output/output.h>
#include <timer/timer.h>
#include <irq/irq.h>

int update_timer(struct basic_timer* t) 
{
    if (t->state == TIMER_RUNNING) {
        unsigned int current = read_jiffies();
        if (current  >= t->end_time) {
            t->state = TIMER_EXPIRED;
            return 1;
        }
    }
    return 0;
}

struct basic_timer new_timer(unsigned int ms) 
{
    struct basic_timer t;

    t.start_time = read_jiffies();
    //TODO: expired time is realtive to PIT HZ this is garbage
    t.end_time = t.start_time + ms * 10;
    t.state = TIMER_RUNNING;
    return t;
}


