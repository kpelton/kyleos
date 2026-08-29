#ifndef KYLEOS_BUSYBOX_TIME_WRAPPER_H
#define KYLEOS_BUSYBOX_TIME_WRAPPER_H

#include_next <time.h>

int nanosleep(const struct timespec *request, struct timespec *remaining);

#endif
