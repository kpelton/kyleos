#ifndef KYLEOS_BUSYBOX_NET_IF_H
#define KYLEOS_BUSYBOX_NET_IF_H

#define IFNAMSIZ 16

struct ifreq {
    char ifr_name[IFNAMSIZ];
};

#endif
