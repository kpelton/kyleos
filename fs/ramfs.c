#include <fs/ramfs.h>
#include <mm/mm.h>
#include <fs/vfs.h>
#include <locks/spinlock.h>
#include <output/output.h>
#define MAX_RAMFS 512

struct dnode *ramfs_read_root_dir(struct vfs_device *dev);
struct dnode *ramfs_read_inode_dir(struct dnode *parent,struct inode *inode);
int ramfs_create_dir(struct inode *parent, char *name);
int ramfs_create_file(struct inode *parent, char *name);
int ramfs_read_file (struct file * rfile,void *buf,uint32_t count);
int ramfs_write_file (struct file * rfile,void *buf,uint32_t count);

static uint64_t i_no = 0;
static int device_num;
static struct ramfs_inode ramfs_inodes[MAX_RAMFS];
static struct vfs_device vfs_dev;
static struct spinlock ramfs_lock;

static void create_root_dir(struct vfs_device *dev) {

    ramfs_inodes[0].dev = dev;
    kstrcpy(ramfs_inodes[0].i_name, dev->mountpoint_root);
    ramfs_inodes[0].i_type=I_DIR;
    ramfs_inodes[0].i_ino = 0;
    ramfs_inodes[0].children[0]=0;
    ramfs_inodes[0].last_child = 0;
    ramfs_inodes[0].parent = -1;
    ramfs_inodes[0].blocks = NULL;
    i_no++;
}

int ramfs_init(void) {
    struct vfs_ops *vfs_ops=0;

    vfs_ops = kmalloc(sizeof(struct vfs_ops));
    init_spinlock(&ramfs_lock);
    kprintf("RAMFS driver init 0x%x\n",vfs_ops);

    vfs_ops->read_root_dir = ramfs_read_root_dir;
    vfs_ops->read_inode_dir= ramfs_read_inode_dir;
    vfs_ops->create_dir = ramfs_create_dir;
    vfs_ops->create_file = ramfs_create_file;
    vfs_ops->read_file = ramfs_read_file;
    vfs_ops->write_file = ramfs_write_file;
    ////Sample code to mount device
    vfs_dev.fstype = RAM_FS;
    vfs_dev.ops = vfs_ops;
    vfs_dev.rootfs = false;
    kstrcpy(vfs_dev.mountpoint,"/2");
    kstrcpy(vfs_dev.mountpoint_root,"2");
    kstrcpy(vfs_dev.mountpoint_parent,"/");
    device_num = vfs_register_device(vfs_dev);
    create_root_dir(vfs_get_device(device_num));

    return 0;
}
int ramfs_read_file (struct file * rfile,void *buf,uint32_t count) { 

    struct ramfs_block *r_block = ramfs_inodes[rfile->i_node.i_ino].blocks;
    int check_count = 0;

    while(rfile->pos > check_count || r_block->block == NULL) {
        check_count += RAMFS_BLOCK_SIZE;
        r_block = r_block->next;
    }

    memcpy8(buf,r_block->block,count);
    return rfile->i_node.file_size;
}

int ramfs_write_file (struct file * rfile,void *buf,uint32_t count) {

    struct ramfs_block *r_block = ramfs_inodes[rfile->i_node.i_ino].blocks;
    int check_count = 0;
    int offset = 0;
    uint32_t total_count =count;
    uint8_t *buffer = buf;


    uint32_t copy_count = 0;
    


    int rel_pos = 0;
    if (rfile->pos > RAMFS_BLOCK_SIZE)
        rel_pos = rfile->pos / RAMFS_BLOCK_SIZE;
    else
        rel_pos = rfile->pos;


    if (count+rel_pos > RAMFS_BLOCK_SIZE)
        copy_count = RAMFS_BLOCK_SIZE;
    else
        copy_count = count;

    kprintf("ramfs pos %x\n",rfile->pos);
    
    for(int i=0; i<rfile->pos/RAMFS_BLOCK_SIZE; i+=RAMFS_BLOCK_SIZE) {
        r_block = r_block->next;
    }

    while(total_count != 0 ) {
        memcpy8(((uint8_t *)r_block->block)+rel_pos,buffer,copy_count);
        total_count -= copy_count;
        rel_pos=0;
        if (total_count != 0) {
            r_block->next = kmalloc(sizeof(struct ramfs_block));
            r_block = r_block->next;
            r_block->next = NULL;
            r_block->block = kmalloc(RAMFS_BLOCK_SIZE);
        }
        if (total_count > RAMFS_BLOCK_SIZE)
            copy_count = RAMFS_BLOCK_SIZE;
        else
            copy_count = total_count;
        buffer += copy_count;

    }

    ramfs_inodes[rfile->i_node.i_ino].file_size= rfile->pos + count;
    return count;
}

int ramfs_create_file(struct inode *parent, char *name) 
{

    if (ramfs_inodes[parent->i_ino].last_child >= RAMFS_MAX_DIRECTORY)
        return -1;
    acquire_spinlock(&ramfs_lock);
    ramfs_inodes[i_no].dev = parent->dev;
    kstrcpy(ramfs_inodes[i_no].i_name, name);
    ramfs_inodes[i_no].i_type=I_FILE;
    ramfs_inodes[i_no].i_ino = i_no;
    ramfs_inodes[i_no].last_child = 0;
    ramfs_inodes[i_no].blocks = kmalloc(sizeof(struct ramfs_block));
    ramfs_inodes[i_no].blocks->block = kmalloc(RAMFS_BLOCK_SIZE);
    kstrcpy(ramfs_inodes[i_no].blocks->block,"asdfasdf");
    ramfs_inodes[i_no].file_size = kstrlen(ramfs_inodes[i_no].blocks->block)+1;
    ramfs_inodes[i_no].blocks->next = NULL;
    ramfs_inodes[i_no].parent =  parent->i_ino;

    ramfs_inodes[parent->i_ino].children[ramfs_inodes[parent->i_ino].last_child]=i_no;
    ramfs_inodes[parent->i_ino].last_child++;
    
    i_no++;
    release_spinlock(&ramfs_lock);

    return 0;
}


int ramfs_create_dir(struct inode *parent, char *name) 
{
    if (ramfs_inodes[parent->i_ino].last_child >= RAMFS_MAX_DIRECTORY)
        return -1;

    acquire_spinlock(&ramfs_lock);

    ramfs_inodes[i_no].dev = parent->dev;
    kstrcpy(ramfs_inodes[i_no].i_name, name);
    ramfs_inodes[i_no].i_type=I_DIR;
    ramfs_inodes[i_no].i_ino = i_no;
    ramfs_inodes[i_no].last_child = 0;
    ramfs_inodes[i_no].parent =  parent->i_ino;
    ramfs_inodes[parent->i_ino].children[ramfs_inodes[parent->i_ino].last_child]=i_no;
    ramfs_inodes[parent->i_ino].last_child++;
    i_no++;
        release_spinlock(&ramfs_lock);

    return 0;
}

struct dnode *ramfs_read_inode_dir(struct dnode *parent,struct inode *i_node)
{
    struct dnode *dir;
    struct inode_list *list=NULL;
    struct inode_list *prev = NULL;
    acquire_spinlock(&ramfs_lock);
    kprintf("Reading %d\n",i_node->i_ino);
    kprintf("parent:%s parent:%d\n",parent->i_name,parent->root_inode->i_ino);

    dir = kmalloc(sizeof(struct dnode));
    dir->root_inode = kmalloc(sizeof(struct inode));
    dir->root_inode->i_ino = i_node->i_ino;
    dir->root_inode->i_type = i_node->i_type;
    dir->root_inode->dev = i_node->dev;
    kstrcpy(dir->root_inode->i_name,i_node->i_name);

    dir->head = kmalloc(sizeof(struct inode_list));
    dir->head->current = kmalloc(sizeof(struct inode));
    kstrcpy(dir->head->current->i_name,"..");
    if (ramfs_inodes[i_node->i_ino].parent != -1){
        dir->head->current->i_ino = ramfs_inodes[i_node->i_ino].parent;
        dir->head->current->i_type = I_DIR;
        dir->head->current->dev = ramfs_inodes[i_node->i_ino].dev;
    }
    else{
        //Cross mount point
        dir->head->current->i_ino = ramfs_inodes[i_node->i_ino].dev->mnt_node_parent->i_ino;
        dir->head->current->i_type = ramfs_inodes[i_node->i_ino].dev->mnt_node_parent->i_type;
        dir->head->current->dev = ramfs_inodes[i_node->i_ino].dev->mnt_node_parent->dev;
    }
    dir->head->next = kmalloc(sizeof(struct inode_list));

    dir->head->next->current = kmalloc(sizeof(struct inode));
    kstrcpy(dir->head->next->current->i_name,".");
    dir->head->next->current->i_ino = i_node->i_ino;
    dir->head->next->current->i_type = I_DIR;
    dir->head->next->current->dev = i_node->dev;
    dir->head->next->next=NULL;
    if( ramfs_inodes[i_node->i_ino].last_child)
        prev = dir->head->next;

    for(int i=0; i < ramfs_inodes[i_node->i_ino].last_child; i++){
        int child = ramfs_inodes[i_node->i_ino].children[i];
        list = kmalloc(sizeof(struct inode_list));
        list->current = kmalloc(sizeof(struct inode));
        kstrcpy(list->current->i_name,ramfs_inodes[child].i_name);
        list->current->i_type = ramfs_inodes[child].i_type;
        list->current->dev = ramfs_inodes[child].dev;
        list->current->file_size = ramfs_inodes[child].file_size;
        list->current->i_ino = ramfs_inodes[child].i_ino;
        prev->next = list;
        list->next = NULL;
        prev = list;
    }
    release_spinlock(&ramfs_lock);

    return dir;
}

struct dnode *ramfs_read_root_dir(struct vfs_device *dev)
{

    struct dnode *dir;
    kprintf("dev %x\n",dev);
    acquire_spinlock(&ramfs_lock);
    dir = kmalloc(sizeof(struct dnode));
    dir->root_inode = kmalloc(sizeof(struct inode));
    vfs_copy_inode((struct inode *)&ramfs_inodes[0],dir->root_inode);
    release_spinlock(&ramfs_lock);

    return dir;
}
