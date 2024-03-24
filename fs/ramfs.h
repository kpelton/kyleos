#ifndef RAMFS_H
#define RAMFS_H

#include <include/types.h>
#include <fs/ramfs.h>
#include <fs/vfs.h>
#define RAMFS_MAX_DIRECTORY 1024
#define RAMFS_BLOCK_SIZE 4096
int ramfs_init(void);

struct ramFS
{
    uint64_t allocated_blocks;

};

struct ramfs_block {
    uint64_t *block;
    struct ramfs_block *next;
};

struct ramfs_inode {
    char i_name[257];
    int i_type;
    struct vfs_device* dev;
    uint64_t  i_ino;
    uint64_t file_size;
    int children[RAMFS_MAX_DIRECTORY];
    int last_child;
    int parent;
    struct ramfs_block *blocks;
    uint64_t used_space;
};

#endif