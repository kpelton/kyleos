#include <asm/asm.h>
#include <irq/irq.h>
#include <sched/sched.h>
#include <timer/pit.h>
static uint32_t jiffies=0;

void pit_init(void) {
    outb(0x43,0x34);
    //set 100hz 11931 0x2e9b
   // outb(0x40,0x9b);
    //outb(0x40,0x2e);
    outb(0x40,0xa9);
    outb(0x40,0x4);
}

uint32_t read_jiffies()
{
    return jiffies;
}

void timer_irq()
{
   jiffies+=1;
   PIC_sendEOI(1);
   //kprintf("timer\n");
   //Tick scheduler
   if (jiffies %10 == 0)
        schedule();
}
