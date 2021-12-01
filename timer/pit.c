#include <asm/asm.h>
#include <irq/irq.h>
#include <sched/sched.h>
#include <timer/pit.h>
#include <output/output.h>
#define PIT_DATA_PORT_0 0x40
#define PIT_CMD_REG 0x43
//Set PIT to rate generater mode and lo/hi acess byte
#define PIT_RATE_LO_HI_ACCESS 0x34
#define PIT_FREQ 1193182
#define PIT_DIVIDER_VAL PIT_FREQ/TICK_HZ
//jiffies will in increased at a interval of TICK_HZ/HZ should be ~1ms
static uint32_t jiffies=0;

void pit_init(void) {
    kprintf("PIT init\n");
    kprintf("PIT divider:%d\n",PIT_DIVIDER_VAL);
    kprintf("Tick interval:%d HZ\n",HZ);
    outb(PIT_CMD_REG,PIT_RATE_LO_HI_ACCESS);
    outb(PIT_DATA_PORT_0,PIT_DIVIDER_VAL &0xff);
    outb(PIT_DATA_PORT_0,(PIT_DIVIDER_VAL & 0xff00) >>8);
}

uint32_t read_jiffies()
{
    return jiffies;
}

void timer_irq()
{
   PIC_sendEOI(1);

   jiffies++;
   if (jiffies % (TICK_HZ/HZ) == 0)
        schedule();
}
