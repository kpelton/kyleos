#ifndef KYLEOS_BUSYBOX_SIGNAL_WRAPPER_H
#define KYLEOS_BUSYBOX_SIGNAL_WRAPPER_H

#include_next <signal.h>

#ifndef SA_RESTART
# define SA_RESTART 0
#endif

#endif
