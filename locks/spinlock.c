
#include <locks/spinlock.h>
#include <asm/asm.h>
#include <output/output.h>
int acquire_spinlock(struct spinlock *s)
{
    while (__sync_val_compare_and_swap (&(s->lock), 0, 1) != 0)
    {
    }

    if ((get_flags_reg() & INTERRUPT_ENABLE_FLAG) == INTERRUPT_ENABLE_FLAG) {
        s->int_enabled = true;
         asm("cli");

    }else{
        s->int_enabled = false;
    }
    return 0;
}

int release_spinlock(struct spinlock *s)
{

    __sync_val_compare_and_swap (&(s->lock), 1, 0);
      if(s->int_enabled)
        asm("sti");  
    return 0;
}

void init_spinlock(struct spinlock *s)
{
    //clear lock atomically
    __sync_fetch_and_and (&(s->lock),0);
}