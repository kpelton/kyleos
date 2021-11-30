#ifndef RTC_H
#define RTC_H
#include <include/types.h>
void rtc_init();
void rtc_irq();
struct sys_time {
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
};

struct sys_time get_time();
#endif

