#ifndef KYLEOS_BUSYBOX_NETINET_IN_H
#define KYLEOS_BUSYBOX_NETINET_IN_H

#include <stdint.h>
#include <sys/socket.h>

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

#define INADDR_ANY ((in_addr_t)0)

struct in_addr {
    in_addr_t s_addr;
};

struct in6_addr {
    unsigned char s6_addr[16];
};

struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
};

struct sockaddr_in6 {
    sa_family_t sin6_family;
    in_port_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t sin6_scope_id;
};

#endif
