#include <asm/asm.h>
#include <irq/irq.h>
#include <sched/sched.h>
#include <timer/pit.h>
#include <output/output.h>
#include <locks/spinlock.h>

#define PIT_DATA_PORT_0 0x40
#define PIT_CMD_REG 0x43
//Set PIT to rate generater mode and lo/hi acess byte
#define PIT_RATE_LO_HI_ACCESS 0x34
#define PIT_FREQ 1193182
//jiffies will in increased at a interval of TICK_HZ/HZ should be ~1ms
static uint32_t jiffies=0;
static struct spinlock spinlock_time;
uint64_t HZ=50;
uint64_t TICK_HZ=100;
void pit_init(void) {
    init_spinlock(&spinlock_time);

    uint64_t PIT_DIVIDER_VAL = PIT_FREQ/TICK_HZ;
    kprintf("PIT init\n");
    kprintf("PIT divider:%d\n",PIT_DIVIDER_VAL);
    kprintf("Sched Tick interval:%d HZ\n",HZ);
    kprintf("Timer interrtupt interval:%d HZ\n",TICK_HZ);
    outb(PIT_CMD_REG,PIT_RATE_LO_HI_ACCESS);
    outb(PIT_DATA_PORT_0,PIT_DIVIDER_VAL &0xff);
    outb(PIT_DATA_PORT_0,PIT_DIVIDER_VAL>>8);

}

uint32_t read_jiffies()
{
    return jiffies;
}

void timer_irq()
{
   acquire_spinlock(&spinlock_time);
   
   PIC_sendEOI(0);

   jiffies++;
   release_spinlock(&spinlock_time);
   PIC_sendEOI(0);
   if (jiffies % (TICK_HZ/HZ) == 0)
        schedule();

}
