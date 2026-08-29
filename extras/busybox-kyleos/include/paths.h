#ifndef KYLEOS_BUSYBOX_PATHS_WRAPPER_H
#define KYLEOS_BUSYBOX_PATHS_WRAPPER_H

#include_next <paths.h>

#ifndef _PATH_TTY
# define _PATH_TTY "/dev/tty"
#endif

#endif
