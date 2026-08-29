#ifndef RAMFS_H
#define RAMFS_H

#include <include/types.h>
#include <include/uapi/termios.h>
#include <fs/ramfs.h>
#include <fs/vfs.h>
struct file;
#define RAMFS_MAX_DIRECTORY 1024
int ramfs_init(void);
bool ramfs_file_is_tty(struct file *file);
int ramfs_tty_get_foreground_group(struct file *file);
int ramfs_tty_set_foreground_group(struct file *file, int session,
                                   int process_group);
int ramfs_tty_get_attributes(struct file *file,
                             struct kyleos_termios *termios);
int ramfs_tty_set_attributes(struct file *file, int action,
                             const struct kyleos_termios *termios);
int ramfs_tty_status(struct file *file, uint32_t *readable,
                     uint32_t *writable, uint32_t *hung_up);

struct ramFS
{
    uint64_t allocated_blocks;

};

struct ramfs_block {
    uint64_t *block;
    struct ramfs_block *next;
    uint64_t size;
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
