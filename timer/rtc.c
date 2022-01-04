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
#define REG_DAY 0x7
#define REG_MONTH 0x8
#define REG_YEAR 0x9
#define REG_CENTURY 0x32
//Value to write to set the 1 second update register
#define UIR 0x10
#define UPDATE 0x40
#define IE UIR | 0x10
#define HR24 0x2

static struct sys_time current_time;
static void set_time(uint8_t hour, uint8_t min, uint8_t sec,
                uint8_t day,uint8_t month, uint16_t year);
//Enable rtc interrupt
void rtc_init() {
   outb(CMOS_ADDR,REG_B);
   outb(CMOS_DATA,IE);
   outb(CMOS_ADDR,REG_C);
   inb(CMOS_DATA);
   outb(CMOS_ADDR,REG_B);
   uint8_t reg = inb(CMOS_DATA);
   outb(CMOS_DATA,reg|6);
}
static void set_time(uint8_t hour, uint8_t min, uint8_t sec,
                uint8_t day,uint8_t month, uint16_t year) {
    current_time.hour = hour;
    current_time.min = min;
    current_time.sec = sec;
    current_time.day = day;
    current_time.year = year;
    current_time.month = month;
}

struct sys_time get_time() {
    return current_time;
}

void rtc_irq() {
    uint8_t sec;
    uint8_t hour;
    uint8_t min;
    uint8_t month;
    uint8_t day;
    uint16_t year;
  //  uint8_t registerB;
    PIC_sendEOI(8);

    //Clear interrupt RTC register
    outb(CMOS_ADDR, 0xc);
    inb(CMOS_DATA);


    outb(CMOS_ADDR, REG_SEC);
    sec = inb(CMOS_DATA);
    outb(CMOS_ADDR, REG_MIN);
    min = inb(CMOS_DATA);
    outb(CMOS_ADDR, REG_HOUR);
    hour = inb(CMOS_DATA);
   // outb(CMOS_ADDR, REG_B);
   // registerB = inb(CMOS_DATA);
    outb(CMOS_ADDR, REG_MONTH);
    month = inb(CMOS_DATA);
    outb(CMOS_ADDR, REG_DAY);
    day = inb(CMOS_DATA);
    outb(CMOS_ADDR, REG_YEAR);
    year = inb(CMOS_DATA);
   //century hack
   if (year <85) {
       year = 2000 +year;
   }

    set_time(hour,min,sec,day,month,year);

}
