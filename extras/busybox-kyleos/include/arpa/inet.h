#ifndef KYLEOS_BUSYBOX_ARPA_INET_H
#define KYLEOS_BUSYBOX_ARPA_INET_H

#include <netinet/in.h>

#define htons(value) __builtin_bswap16((uint16_t)(value))
#define ntohs(value) __builtin_bswap16((uint16_t)(value))
#define htonl(value) __builtin_bswap32((uint32_t)(value))
#define ntohl(value) __builtin_bswap32((uint32_t)(value))

in_addr_t inet_addr(const char *text);
int inet_aton(const char *text, struct in_addr *address);
char *inet_ntoa(struct in_addr address);
const char *inet_ntop(int family, const void *address, char *text,
                      socklen_t size);

#endif
