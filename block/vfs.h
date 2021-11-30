#ifndef VFS_H
#define VFS_H
#include <block/fat.h>
#include <include/types.h>
#define VFS_MAX_DEVICES 10
#define FAT_FS 0
#define VFS_MAX_FNAME 256
enum inode_type {
    I_DIR,
    I_FILE,
};

union fsinfo {
    struct fatFS* fat;
};

struct vfs_device {   
    uint32_t devicenum;
    struct  vfs_ops* ops;
    int fstype;
    union fsinfo finfo;
};

struct inode {
    char i_name[VFS_MAX_FNAME];
    int i_type;
    struct vfs_device* dev;
    uint64_t  i_ino;
    uint64_t file_size;
};

struct inode_list {
    struct inode* current;
    struct inode_list* next;
};

struct dnode {
    char i_name[VFS_MAX_FNAME];
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
void vfs_free_inode(struct inode * i_node); 
void vfs_free_inode_list(struct inode_list * list); 
void vfs_free_dnode(struct dnode * dn);
#endif
