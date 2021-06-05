#ifndef VFS_H
#define VFS_H
#include <block/fat.h>
#define VFS_MAX_DEVICES 10
#define FAT_FS 0

union fsinfo {
    struct fatFS* fat;
};

struct vfs_device {   
    unsigned int devicenum;
    struct  vfs_ops* ops;
    int fstype;
    union fsinfo finfo;
};

struct vfs_ops {
    int (*read)(char* path,union fsinfo);
    int (*read_path)(char* path,union fsinfo);
};

void vfs_init();
//TODO: return inode or node object
void read_dir(char * path);
int vfs_register_device(struct vfs_device newdev);
#endif
