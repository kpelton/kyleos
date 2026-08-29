#ifndef KYLEOS_BUSYBOX_STDLIB_WRAPPER_H
#define KYLEOS_BUSYBOX_STDLIB_WRAPPER_H

/* BusyBox provides one-argument itoa/utoa helpers with names that collide
 * with Newlib's non-standard three-argument declarations. */
#define itoa __kyleos_newlib_itoa
#define utoa __kyleos_newlib_utoa
#include_next <stdlib.h>
#undef itoa
#undef utoa

#endif
