#ifndef _SPINLOCK_H
#include <include/types.h>

#define _SPINLOCK_H
struct spinlock {
    int lock;
    bool int_enabled;
};
int acquire_spinlock(struct spinlock *s);
int release_spinlock(struct spinlock *s);
void init_spinlock(struct spinlock *s);
#endif