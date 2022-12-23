
#include <locks/mutex.h>
#include <asm/asm.h>
#include <output/output.h>
#include <sched/sched.h>
int acquire_mutex(struct mutex *s)
{
    while (__sync_val_compare_and_swap (&(s->lock), 0, 1) != 0)
    {
        ksleepm(1);
    }

        return 0;
}

int release_mutex(struct mutex *s)
{
    __sync_val_compare_and_swap (&(s->lock), 1, 0);
    return 0;
}

void init_mutex(struct mutex *s)
{
    //clear lock atomically
    __sync_fetch_and_and (&(s->lock),0);
}