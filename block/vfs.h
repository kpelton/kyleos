#ifndef VFS_H
#define VFS_H
#include <block/fat.h>
#define VFS_MAX_DEVICES 10
#define FAT_FS 0
enum inode_type {
    I_DIR,
    I_FILE,
};

union fsinfo {
    struct fatFS* fat;
};

struct vfs_device {   
    unsigned int devicenum;
    struct  vfs_ops* ops;
    int fstype;
    union fsinfo finfo;
};

struct inode {
    char i_name[128];
    int i_type;
    struct vfs_device* dev;
    unsigned long i_ino;
};

struct inode_list {
    struct inode* current;
    struct inode_list* next;
};

struct dnode {
    char i_name[128];
    struct inode* root_inode;
    //children files and folders
    struct inode_list* head;
};

struct vfs_ops {
    int (*read)(char* path,union fsinfo);
    struct dnode* (*read_root_dir)(struct vfs_device * dev);
    struct dnode* (*read_inode_dir)(struct inode* i_node);
    void (*read_inode_file)(struct inode* i_node);

};

void vfs_init();
struct dnode* vfs_read_root_dir(char * path);
struct dnode* vfs_read_inode_dir(struct inode * i_node);
void vfs_read_inode_file(struct inode * i_node);
int vfs_register_device(struct vfs_device newdev);
#endif
