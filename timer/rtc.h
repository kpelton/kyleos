#ifndef RTC_H
#define RTC_H
#include <include/types.h>
void rtc_init();
void rtc_irq();
struct sys_time {
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t month;
    uint8_t day;
    uint16_t year;
};

struct sys_time get_time();
#endif

