#ifndef KYLEOS_BUSYBOX_SYS_SOCKET_H
#define KYLEOS_BUSYBOX_SYS_SOCKET_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef uint16_t sa_family_t;
typedef unsigned int socklen_t;

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

struct sockaddr_storage {
    sa_family_t ss_family;
    char __padding[126];
};

#define AF_UNSPEC 0
#define AF_UNIX 1
#define AF_INET 2
#define AF_INET6 10
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
#define SOCK_RDM 4
#define SOCK_SEQPACKET 5
#define SOL_SOCKET 1
#define SO_TYPE 3
#define SO_REUSEADDR 4
#define SO_KEEPALIVE 9
#define SO_BROADCAST 6
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

int socket(int domain, int type, int protocol);
int bind(int fd, const struct sockaddr *address, socklen_t length);
int connect(int fd, const struct sockaddr *address, socklen_t length);
int getsockname(int fd, struct sockaddr *address, socklen_t *length);
int getpeername(int fd, struct sockaddr *address, socklen_t *length);
int getsockopt(int fd, int level, int option, void *value, socklen_t *length);
int setsockopt(int fd, int level, int option, const void *value,
               socklen_t length);
int listen(int fd, int backlog);
int shutdown(int fd, int how);
ssize_t recv(int fd, void *buffer, size_t length, int flags);
ssize_t recvfrom(int fd, void *buffer, size_t length, int flags,
                 struct sockaddr *address, socklen_t *address_length);
ssize_t sendto(int fd, const void *buffer, size_t length, int flags,
               const struct sockaddr *address, socklen_t address_length);

#endif
