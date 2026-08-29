#ifndef KYLEOS_BUSYBOX_SYS_WAIT_WRAPPER_H
#define KYLEOS_BUSYBOX_SYS_WAIT_WRAPPER_H

#include_next <sys/wait.h>

#ifndef WCOREDUMP
# define WCOREDUMP(status) ((status) & 0x80)
#endif

#endif
