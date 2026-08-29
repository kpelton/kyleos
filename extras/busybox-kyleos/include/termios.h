#ifndef KYLEOS_BUSYBOX_TERMIOS_WRAPPER_H
#define KYLEOS_BUSYBOX_TERMIOS_WRAPPER_H

#include_next <termios.h>

#define TCIFLUSH 0
#define TCOFLUSH 1
#define TCIOFLUSH 2

#define B0 0
#define B50 1
#define B75 2
#define B110 3
#define B134 4
#define B150 5
#define B200 6
#define B300 7
#define B600 8
#define B1200 9
#define B1800 10
#define B2400 11
#define B4800 12
#define B9600 13

int tcflush(int fd, int queue);

#endif
