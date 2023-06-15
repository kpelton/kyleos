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
    //acquire_spinlock(&t->timer_lock);
    if (t->state == TIMER_RUNNING) {
        uint32_t current = read_jiffies();
        if (current  >= t->end_time) {
            t->state = TIMER_EXPIRED;
            //release_spinlock(&t->timer_lock);
            return 1;

        }
    }
    //release_spinlock(&t->timer_lock);
    return 0;
}

struct basic_timer new_timer(uint32_t ms) 
{
    //Tick must be less than 1000HZ
    uint32_t ms_tick_delta = 1000/TICK_HZ;
    struct basic_timer t;
    //init_spinlock(&t.timer_lock);
    t.start_time = read_jiffies();
    t.end_time = t.start_time + (ms  / ms_tick_delta);
    t.state = TIMER_RUNNING;
    return t;
}
