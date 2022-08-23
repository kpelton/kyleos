#ifndef _MUTEX_H
#include <include/types.h>

#define _MUTEX_H
struct mutex {
    int lock;
};
int acquire_mutex(struct mutex *s);
int release_mutex(struct mutex *s);
void init_mutex(struct mutex *s);
#endif