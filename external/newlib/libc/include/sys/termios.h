#ifndef _SYS_TERMIOS_H
#define _SYS_TERMIOS_H

#include <sys/types.h>

#define NCCS 13

#define TCSANOW   1
#define TCSADRAIN 2
#define TCSAFLUSH 3

#define IGNBRK 000001
#define BRKINT 000002
#define IGNPAR 000004
#define INPCK  000020
#define ISTRIP 000040
#define INLCR  000100
#define IGNCR  000200
#define ICRNL  000400
#define IXON   002000
#define IXOFF  010000

#define OPOST 000001
#define ONLCR 000010

#define ISIG   0000001
#define ICANON 0000002
#define ECHO   0000010
#define ECHOE  0000020
#define ECHOK  0000040
#define ECHONL 0000100
#define NOFLSH 0000200
#define TOSTOP 0001000

#define VINTR  0
#define VQUIT  1
#define VERASE 2
#define VKILL  3
#define VEOF   4
#define VEOL   5
#define VMIN   4
#define VTIME  5
#define VSUSP  10
#define VSTART 11
#define VSTOP  12

typedef unsigned char cc_t;
typedef unsigned short tcflag_t;
typedef char speed_t;

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    char c_line;
    cc_t c_cc[NCCS];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

#define cfgetospeed(termios_pointer) ((termios_pointer)->c_ospeed)
#define cfgetispeed(termios_pointer) ((termios_pointer)->c_ispeed)
#define cfsetospeed(termios_pointer, speed) \
    (((termios_pointer)->c_ospeed = (speed)), 0)
#define cfsetispeed(termios_pointer, speed) \
    (((termios_pointer)->c_ispeed = (speed)), 0)

int tcgetattr(int fd, struct termios *termios_pointer);
int tcsetattr(int fd, int action, const struct termios *termios_pointer);

#endif
