#ifndef KYLEOS_BUSYBOX_SYS_SYSMACROS_H
#define KYLEOS_BUSYBOX_SYS_SYSMACROS_H

#define major(device) (((unsigned long)(device) >> 8) & 0xfff)
#define minor(device) ((unsigned long)(device) & 0xff)
#define makedev(major_number, minor_number) \
    ((((unsigned long)(major_number) & 0xfff) << 8) | \
     ((unsigned long)(minor_number) & 0xff))

#endif
