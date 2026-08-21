#ifndef KYLEOS_UAPI_FD_H
#define KYLEOS_UAPI_FD_H

#include <stdint.h>

struct kyleos_fd_status {
    uint32_t readable;
    uint32_t writable;
    uint32_t hung_up;
};

#endif
