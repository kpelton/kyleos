
#ifndef VFS_H
#define VFS_H
#include <fs/fat.h>
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
//File open flags
#define O_RDONLY 0x1
#define O_WRONLY 0x2
#define O_RDWR   0x4
#define MAX_FILE_FLAGS (O_RDONLY | O_WRONLY | O_RDWR)

struct file {
    uint32_t refcount;
    struct inode i_node;
    struct vfs_device* dev;
    uint64_t pos;
    uint32_t flags;
};


struct vfs_ops {
    int (*read)(char* path,union fsinfo);
    struct dnode* (*read_root_dir)(struct vfs_device * dev);
    struct dnode* (*read_inode_dir)(struct inode* i_node);
    void (*cat_inode_file)(struct inode* i_node);
    int (*read_file)(struct file * rfile,void *buf,uint32_t count);
    int (*create_dir)(struct inode* parent, char *name);
};

void vfs_init();
struct dnode* vfs_read_root_dir(char * path);
struct dnode* vfs_read_inode_dir(struct inode * i_node);
void vfs_read_inode_file(struct inode * i_node);
int vfs_register_device(struct vfs_device newdev);
void vfs_free_inode(struct inode * i_node); 
void vfs_free_inode_list(struct inode_list * list); 
void vfs_free_dnode(struct dnode * dn);
void vfs_copy_inode(struct inode *src,struct inode *dst);
int vfs_read_file(struct file * rfile,void *buf,int count);
int vfs_read_file_offset(struct file * rfile,void *buf,int count,uint32_t offset);
int vfs_create_dir(struct inode* parent, char *name);
struct file* vfs_open_file(struct inode * i_node,uint32_t flags);
void vfs_close_file(struct file *ofile);
struct inode * vfs_walk_path(char *path, struct dnode *pwd,enum inode_type type);
#endif