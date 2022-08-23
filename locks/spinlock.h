#ifndef _SPINLOCK_H
#define _SPINLOCK_H
struct spinlock {
    int lock;
};
int acquire_spinlock(struct spinlock *s);
int release_spinlock(struct spinlock *s);
void init_spinlock(struct spinlock *s);
#endif