
#ifndef VFS_H
#define VFS_H

#include <fs/fat.h>
#include <fs/ramfs.h>
#include <include/types.h>
#include <locks/spinlock.h>

#define VFS_MAX_DEVICES 10
#define FAT_FS 0
#define RAM_FS 1
#define VFS_MAX_FNAME 257
#define VFS_MAX_MOUNT_POINT 1024
#define I_DIR  40000
#define I_FILE 100000
#define I_DEV  400000
#define DELIM '/'
#define ROOT "/"

//File open flags
#define O_RDONLY 0x0
#define O_WRONLY 0x1
#define O_RDWR   0x2
#define MAX_FILE_FLAGS (O_RDONLY | O_WRONLY | O_RDWR)
#define VFS_MAX_OPEN 1024

union fsinfo {
    struct fatFS* fat;
    struct ramFS *ramfs;
};

struct vfs_device {   
    uint32_t devicenum;
    struct  vfs_ops* ops;
    int fstype;
    union fsinfo finfo;
    bool rootfs;
    char mountpoint[VFS_MAX_MOUNT_POINT];
    char mountpoint_root[VFS_MAX_MOUNT_POINT];
    char mountpoint_parent[VFS_MAX_MOUNT_POINT];

    struct inode *mnt_node;
    struct inode *mnt_node_parent;
};

struct inode {
    char i_name[VFS_MAX_FNAME];
    int i_type;
    struct vfs_device* dev;
    uint64_t  i_ino;
    uint64_t file_size;
};

// list of inodes
struct inode_list {
    struct inode* current;
    struct inode_list* next;
};
// directory entry
struct dnode {
    struct inode* root_inode;
    //children files and folders
    struct inode_list* head;
    struct dnode *parent;
};

struct file {
    uint32_t refcount;
    struct inode i_node;
    struct vfs_device* dev;
    uint64_t pos;
    uint32_t flags;
    struct spinlock lock;    
};

struct file_table {
    struct spinlock lock;    
    struct file open_files[VFS_MAX_OPEN];
};

// External API to userspace shell
struct dirent { 
    uint64_t i_no;
    char i_name[VFS_MAX_FNAME];
    int i_type;
};

struct vfs_ops {
    int (*read)(char* path,union fsinfo);
    struct dnode* (*read_root_dir)(struct vfs_device * dev);
    struct dnode* (*read_inode_dir)(struct inode* i_node);
    void (*cat_inode_file)(struct inode* i_node);
    int (*read_file)(struct file * rfile,void *buf,uint32_t count);
    int (*write_file)(struct file * rfile,void *buf,uint32_t count);
    int (*create_dir)(struct inode* parent, char *name);
    struct inode* (*create_file)(struct inode* parent, char *name);
    int (*stat_file)(struct file *rfile, struct stat *st);
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
struct vfs_device *vfs_get_device(int num);
int vfs_getdents(struct file * rfile,void *dirp,int count);
int vfs_read_file(struct file * rfile,void *buf,int count);
int vfs_write_file(struct file * rfile,void *buf,int count);
int vfs_stat_file(struct file * rfile,struct stat *st);
int vfs_read_file_offset(struct file * rfile,void *buf,int count,uint32_t offset);

int vfs_create_dir(struct inode* parent, char *name);
struct inode* vfs_create_file(struct inode* parent, char *name,uint32_t flags);

struct file* vfs_open_file(struct inode * i_node,uint32_t flags);
void vfs_close_file(struct file *ofile);
struct inode * vfs_walk_path(char *path, struct dnode *pwd);
struct inode * vfs_get_root_inode();
char * vfs_strip_path(char *ptr);
char * vfs_get_dir(char *ptr);
struct inode * fs_is_mount_point(struct inode *ptr);
#endif
