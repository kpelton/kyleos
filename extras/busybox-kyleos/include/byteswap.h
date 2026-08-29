#ifndef KYLEOS_BUSYBOX_BYTESWAP_H
#define KYLEOS_BUSYBOX_BYTESWAP_H

#define bswap_16(value) __builtin_bswap16(value)
#define bswap_32(value) __builtin_bswap32(value)
#define bswap_64(value) __builtin_bswap64(value)

#endif
