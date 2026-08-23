#ifndef KYLEOS_UAPI_POLL_H
#define KYLEOS_UAPI_POLL_H

#include <stdint.h>

typedef unsigned int kyleos_nfds_t;

struct kyleos_pollfd {
    int fd;
    int16_t events;
    int16_t revents;
};

#define KYLEOS_POLLIN   0x0001
#define KYLEOS_POLLPRI  0x0002
#define KYLEOS_POLLOUT  0x0004
#define KYLEOS_POLLERR  0x0008
#define KYLEOS_POLLHUP  0x0010
#define KYLEOS_POLLNVAL 0x0020

#define KYLEOS_POLL_REQUEST_MASK \
    (KYLEOS_POLLIN | KYLEOS_POLLPRI | KYLEOS_POLLOUT)
#define KYLEOS_POLL_RESULT_MASK \
    (KYLEOS_POLL_REQUEST_MASK | KYLEOS_POLLERR | KYLEOS_POLLHUP | \
     KYLEOS_POLLNVAL)

#define KYLEOS_POLL_MAX_FDS 64U

#endif
