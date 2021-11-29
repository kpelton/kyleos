#ifndef RTC_H
#define RTC_H
void rtc_init();
void rtc_irq();
struct sys_time {
    unsigned char hour;
    unsigned char min;
    unsigned char sec;
};

struct sys_time get_time();
#endif

