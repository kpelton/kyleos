#include <timer/rtc.h>
#include <irq/irq.h>
#include <asm/asm.h>
#include <output/output.h>

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71
#define REG_C 0xC
#define REG_B 0xB
#define REG_SEC 0x0
#define REG_MIN 0x2
#define REG_HOUR 0x4
//Value to write to set the 1 second update register
#define UIR 0x10
#define UPDATE 0x40
#define IE UIR
static struct sys_time current_time;
static void set_time(unsigned char hour, unsigned char min, unsigned char sec);

void rtc_init() {
   outb(CMOS_ADDR,REG_B);
   outb(CMOS_DATA,IE);
   outb(CMOS_ADDR,REG_C);
   inb(CMOS_DATA);
}
static void set_time(unsigned char hour, unsigned char min, unsigned char sec) {
    current_time.hour = hour;
    current_time.min = min;
    current_time.sec = sec;
}

struct sys_time get_time() {
    return current_time;
}

void rtc_irq()
{
    unsigned char sec;
    unsigned char hour;
    unsigned char min;
    unsigned char registerB;
    char buffer[20];
    char buffer2[20];
    char *ptr;
    int i;

    //Clear interrupt RTC register
    outb(CMOS_ADDR, 0xc);
    inb(CMOS_DATA);

    outb(CMOS_ADDR, 0xb);
    registerB = inb(CMOS_DATA);

    outb(CMOS_ADDR, REG_SEC);
    sec = inb(CMOS_DATA);
    outb(CMOS_ADDR, REG_MIN);
    min = inb(CMOS_DATA);
    outb(CMOS_ADDR, REG_HOUR);
    hour = inb(CMOS_DATA);

    if (!(registerB & 0x02) && (hour & 0x80))
    {
        hour = ((hour & 0x7F) + 12) % 24;
    }

    for(i=8; i >=0; i--) {

        if (hour > 23)
            hour = 23;
        else
            hour -= 1;
    }
    //hour %=24;
    PIC_sendEOI(8);

    set_time(hour,min,sec);

}
