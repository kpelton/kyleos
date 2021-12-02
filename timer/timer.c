#include <output/output.h>
#include <timer/timer.h>
#include <irq/irq.h>
#include <timer/pit.h>
#include <timer/rtc.h>

//States the timer can be in
const char *str_timer_states[] = {
    "TIMER_EXPIRED",
    "TIMER_RUNNING",
    "TIMER_STOPPED",
    "TIMER_UNUSED",
    "TIMER_MAX_STATE",
};

void timer_system_init() {
    pit_init();
    rtc_init();
}

int update_timer(struct basic_timer* t) 
{
    if (t->state == TIMER_RUNNING) {
        uint32_t current = read_jiffies();
        if (current  >= t->end_time) {
            t->state = TIMER_EXPIRED;
            return 1;
        }
    }
    return 0;
}

struct basic_timer new_timer(uint32_t ms) 
{
    struct basic_timer t;

    t.start_time = read_jiffies();
    t.end_time = t.start_time + ms ;
    t.state = TIMER_RUNNING;
    return t;
}
