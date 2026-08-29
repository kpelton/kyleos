#ifndef KYLEOS_BUSYBOX_POLL_WRAPPER_H
#define KYLEOS_BUSYBOX_POLL_WRAPPER_H

#include_next <poll.h>
#include <signal.h>
#include <time.h>

int ppoll(struct pollfd *descriptors, nfds_t count,
          const struct timespec *timeout, const sigset_t *signal_mask);

#endif
