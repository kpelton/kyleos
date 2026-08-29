#ifndef KYLEOS_BUSYBOX_SCHED_WRAPPER_H
#define KYLEOS_BUSYBOX_SCHED_WRAPPER_H

#include_next <sched.h>

int sched_getaffinity(pid_t pid, size_t size, void *mask);

#endif
