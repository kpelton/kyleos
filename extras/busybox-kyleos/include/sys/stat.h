#ifndef KYLEOS_BUSYBOX_SYS_STAT_WRAPPER_H
#define KYLEOS_BUSYBOX_SYS_STAT_WRAPPER_H

#include_next <sys/stat.h>

/* KyleOS has no symbolic links or device-node creation yet.  The port
 * library supplies conservative implementations for these POSIX calls. */
int lstat(const char *path, struct stat *st);
int mknod(const char *path, mode_t mode, dev_t device);
int kyleos_stat(const char *path, struct stat *st);
int kyleos_fstat(int fd, struct stat *st);

/* Translate KyleOS's internal inode tags before BusyBox interprets st_mode. */
#define stat(path, st) kyleos_stat((path), (st))
#define fstat(fd, st) kyleos_fstat((fd), (st))

#endif
