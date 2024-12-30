#ifndef TYPES_H
#define TYPES_H

#define true 1
#define false 0
#define NULL 0

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef uint8_t bool;

typedef short dev_t;
typedef unsigned short ino_t;
typedef unsigned short nlink_t;
typedef unsigned int mode_t;
typedef unsigned short gid_t;
typedef unsigned short uid_t;
typedef unsigned short off_t;
typedef long blksize_t;
typedef long blkcnt_t;
typedef long time_t;
struct timespec
{
    time_t tv_sec;
    long tv_nsec;
};

struct stat
{
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
};

#define asmlinkage __attribute__((regparm(0)))

#endif
