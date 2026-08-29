#ifndef KYLEOS_BUSYBOX_NETDB_H
#define KYLEOS_BUSYBOX_NETDB_H

#include <sys/socket.h>

#define AI_CANONNAME 0x0002
#define AI_NUMERICHOST 0x0004
#define NI_NUMERICHOST 0x0002
#define NI_NAMEREQD 0x0004
#define NI_NUMERICSERV 0x0008

struct hostent {
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};

extern int h_errno;
const char *hstrerror(int error);

struct hostent *gethostbyname(const char *name);
int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **result);
void freeaddrinfo(struct addrinfo *result);
int getnameinfo(const struct sockaddr *address, socklen_t address_length,
                char *host, socklen_t host_length,
                char *service, socklen_t service_length, int flags);

#endif
