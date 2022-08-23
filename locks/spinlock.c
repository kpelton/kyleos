
#include <locks/spinlock.h>
#include <output/output.h>
int acquire_spinlock(struct spinlock *s)
{
    while (__sync_val_compare_and_swap (&(s->lock), 0, 1) != 0)
    {
        kprintf("waiting for spinlock\n");
    }
    return 0;
}

int release_spinlock(struct spinlock *s)
{
    return __sync_val_compare_and_swap (&(s->lock), 1, 0);
}

void init_spinlock(struct spinlock *s)
{
    //clear lock atomically
    __sync_fetch_and_and (&(s->lock),0);
}