#include <fs/ramfs.h>
#include <mm/mm.h>
#include <fs/vfs.h>
#include <sched/sched.h>
#include <output/input.h>
#include <locks/spinlock.h>
#include <output/output.h>
#define MAX_RAMFS 512
#define CONSOLE_INO 1
struct dnode *ramfs_read_root_dir(struct vfs_device *dev);
struct dnode *ramfs_read_inode_dir(struct inode *inode);
int ramfs_create_dir(struct inode *parent, char *name);
struct inode* ramfs_create_file(struct inode *parent, char *name);
int ramfs_read_file (struct file * rfile,void *buf,uint32_t count);
int ramfs_write_file (struct file * rfile,void *buf,uint32_t count);
int ramfs_stat_file (struct file * rfile,struct stat *st);

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
    ramfs_inodes[0].children[0]=1;
    ramfs_inodes[0].last_child = 1;
    ramfs_inodes[0].file_size = 0;
    ramfs_inodes[0].parent = -1;
    ramfs_inodes[0].blocks = NULL;
    // create console node
    ramfs_inodes[1].dev = dev;
    kstrcpy(ramfs_inodes[1].i_name,"console");
    ramfs_inodes[1].i_type=I_DEV;
    ramfs_inodes[1].i_ino = 1;
    ramfs_inodes[1].children[0]=0;
    ramfs_inodes[1].last_child = 0;
    ramfs_inodes[1].file_size = 0;
    ramfs_inodes[1].parent = 0;
    ramfs_inodes[1].blocks = NULL;


    i_no+=2;
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
    vfs_ops->stat_file = ramfs_stat_file;
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
static int ramfs_read_console(void *buf, uint32_t count) {
    int val=0;
    while (val == 0)
    {
        input_read(buf);    
        val = kstrlen(buf);
        ksleepm(50);
    }
    return val;
}

static int ramfs_write_console(void *buf, uint32_t count) {
    char buffer[4096];
    if (count > 4095)
        panic("can't handle console write of this size");
    memcpy(buffer,buf,count);
    buffer[count]='\0';
    kprintf("%s",buffer); 
    return count;
}

int ramfs_read_file (struct file * rfile,void *buf,uint32_t count) { 

    struct ramfs_block *r_block = ramfs_inodes[rfile->i_node.i_ino].blocks;
    uint64_t file_size = ramfs_inodes[rfile->i_node.i_ino].file_size;
    int ret_count = count;
    if (rfile->i_node.i_ino == CONSOLE_INO) {
        return ramfs_read_console(buf,count);
    }
    acquire_spinlock(&ramfs_lock);
    // Approaching the end of the file truncate read bytes
#ifdef DEBUG_FS_RAMFS
    kprintf("%d %d\n",rfile->pos + count,file_size);
#endif
    
    if (rfile->pos + count > file_size) {
        ret_count = file_size - rfile->pos;
    }

    memcpy(((char *)buf),((uint8_t *)r_block->block)+rfile->pos,ret_count);

    release_spinlock(&ramfs_lock);

    return ret_count;
}

int ramfs_stat_file (struct file * rfile,struct stat *st) { 

    acquire_spinlock(&ramfs_lock);
    // Leave everything else default...
    st->st_dev = device_num;
    st->st_ino = rfile->i_node.i_ino;
    st->st_mode = ramfs_inodes[rfile->i_node.i_ino].i_type;
    st->st_size = ramfs_inodes[rfile->i_node.i_ino].file_size;


   release_spinlock(&ramfs_lock);

    return 0;
}


int ramfs_write_file (struct file * rfile,void *buf,uint32_t count) {

    if (rfile->i_node.i_ino == CONSOLE_INO) {
        return ramfs_write_console(buf,count);
    }
 
    acquire_spinlock(&ramfs_lock);
    struct ramfs_block *r_block = ramfs_inodes[rfile->i_node.i_ino].blocks;
    uint64_t file_size = ramfs_inodes[rfile->i_node.i_ino].file_size;

    //kprintf("Writing %d at %d\n",count,rfile->pos);
    if (file_size == 0) {
        uint64_t block =(uint64_t) kmalloc(count);
        r_block->block = (uint64_t *) block;
        ramfs_inodes[rfile->i_node.i_ino].file_size=count;
    }
    //we are appending to the block allocate new block

    else if (rfile->pos + count > file_size) {
         //kprintf("Allocating %d \n",rfile->pos + count);
        uint64_t block =(uint64_t) kmalloc(rfile->pos + count);

        memcpy((void *)block,r_block->block,file_size);
        kfree(r_block->block);
        r_block->block = (uint64_t *)  block;
        ramfs_inodes[rfile->i_node.i_ino].file_size= file_size + ( (rfile->pos + count) - file_size);

     } 
 
    memcpy(((uint8_t *)r_block->block)+rfile->pos,buf,count);

    release_spinlock(&ramfs_lock);

    return count ;
}

struct inode* ramfs_create_file(struct inode *parent, char *name) 
{

    if (ramfs_inodes[parent->i_ino].last_child >= RAMFS_MAX_DIRECTORY)
        return NULL;
    acquire_spinlock(&ramfs_lock);
    ramfs_inodes[i_no].dev = parent->dev;
    kstrcpy(ramfs_inodes[i_no].i_name, name);
    ramfs_inodes[i_no].i_type=I_FILE;
    ramfs_inodes[i_no].i_ino = i_no;
    ramfs_inodes[i_no].last_child = 0;
    ramfs_inodes[i_no].blocks = kmalloc(sizeof(struct ramfs_block));
    ramfs_inodes[i_no].file_size = 0;
    ramfs_inodes[i_no].parent =  parent->i_ino;

    ramfs_inodes[parent->i_ino].children[ramfs_inodes[parent->i_ino].last_child]=i_no;
    ramfs_inodes[parent->i_ino].last_child++;
    struct inode *i = kmalloc(sizeof(struct inode));
    vfs_copy_inode(i,(struct inode *)&ramfs_inodes[i_no]);
    i_no++;
    release_spinlock(&ramfs_lock);

    return i;
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
    ramfs_inodes[i_no].file_size = 0;  
    ramfs_inodes[i_no].parent =  parent->i_ino;
    ramfs_inodes[parent->i_ino].children[ramfs_inodes[parent->i_ino].last_child]=i_no;
    ramfs_inodes[parent->i_ino].last_child++;
    i_no++;
    release_spinlock(&ramfs_lock);

    return 0;
}

struct dnode *ramfs_read_inode_dir(struct inode *i_node)
{
    struct dnode *dir;
    struct inode_list *list=NULL;
    struct inode_list *prev = NULL;
    acquire_spinlock(&ramfs_lock);
    //kprintf("Reading %d\n",i_node->i_ino);

    dir = kmalloc(sizeof(struct dnode));
    dir->root_inode = kmalloc(sizeof(struct inode));
    dir->root_inode->i_ino = i_node->i_ino;
    dir->root_inode->i_type = i_node->i_type;
    dir->root_inode->dev = i_node->dev;

    kstrcpy(dir->root_inode->i_name,i_node->i_name);
    dir->head = kmalloc(sizeof(struct inode_list));
    dir->head->current = kmalloc(sizeof(struct inode));
    dir->head->current->file_size = 0;
    kstrcpy(dir->head->current->i_name,"..");
    if (ramfs_inodes[i_node->i_ino].parent != -1){
        dir->head->current->i_ino = ramfs_inodes[i_node->i_ino].parent;
        dir->head->current->i_type = I_DIR;
        dir->head->current->dev = ramfs_inodes[i_node->i_ino].dev;
    } else {
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
    dir->head->next->current->file_size = 0;
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
    acquire_spinlock(&ramfs_lock);

    struct dnode *dir;
#ifdef DEBUG_FS_RAMFS
    kprintf("dev %x\n",dev);
#endif
    dir = kmalloc(sizeof(struct dnode));
    dir->root_inode = kmalloc(sizeof(struct inode));
    vfs_copy_inode(dir->root_inode,(struct inode *)&ramfs_inodes[0]);

    dir->head = kmalloc(sizeof(struct inode_list));
    dir->head->current = kmalloc(sizeof(struct inode));
    dir->head->current->file_size = 0;
    kstrcpy(dir->head->current->i_name,"..");
    //Cross mount point since we are on mount 
    // Assumes we are not /
    dir->head->current->i_ino = ramfs_inodes[0].dev->mnt_node_parent->i_ino;
    dir->head->current->i_type = ramfs_inodes[0].dev->mnt_node_parent->i_type;
    dir->head->current->dev = ramfs_inodes[0].dev->mnt_node_parent->dev;
    dir->head->next = kmalloc(sizeof(struct inode_list));
    
    dir->head->next->current = kmalloc(sizeof(struct inode));
    kstrcpy(dir->head->next->current->i_name,".");
    dir->head->next->current->i_ino = dir->root_inode->i_ino;
    dir->head->next->current->i_type = I_DIR;
    dir->head->next->current->dev = dir->root_inode->dev;
    dir->head->next->current->file_size = 0;
    dir->head->next->next=NULL;

    release_spinlock(&ramfs_lock);

    return dir;
}
