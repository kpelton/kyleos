#ifndef _SYS_POLL_H
#define _SYS_POLL_H

#include <sys/cdefs.h>

typedef unsigned int nfds_t;

struct pollfd {
    int fd;
    short events;
    short revents;
};

#define POLLIN   0x0001
#define POLLPRI  0x0002
#define POLLOUT  0x0004
#define POLLERR  0x0008
#define POLLHUP  0x0010
#define POLLNVAL 0x0020

#define INFTIM (-1)

__BEGIN_DECLS
int poll(struct pollfd fds[], nfds_t nfds, int timeout);
__END_DECLS

#endif
