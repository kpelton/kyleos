#ifndef RAMFS_H
#define RAMFS_H

#include <include/types.h>
#include <fs/ramfs.h>
#include <fs/vfs.h>
int ramfs_init(void);
struct ramFS
{
    uint64_t allocated_blocks;

};

struct ramfs_inode {
    char i_name[257];
    int i_type;
    struct vfs_device* dev;
    uint64_t  i_ino;
    uint64_t file_size;

    uint64_t children[1024];
    int last_child;
};

#endif