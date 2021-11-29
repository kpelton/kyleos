#include <timer/rtc.h>
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

void rtc_init() {
   outb(CMOS_ADDR,REG_B);
   outb(CMOS_DATA,IE);
   outb(CMOS_ADDR,REG_C);
   inb(CMOS_DATA);
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
    itoa_8(hour, buffer, 10);
    kprintf(buffer);
    kprintf(":");
    ptr = itoa_8(min, buffer2, 16);
    kprintf(ptr);
    kprintf(":");
    itoa_8(sec, buffer, 16);
    kprintf(buffer);
    kprintf("\n");
}
