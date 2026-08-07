#include <fs/ramfs.h>
#include <mm/mm.h>
#include <fs/vfs.h>
#include <sched/sched.h>
#include <output/input.h>

static int ramfs_truncate_file(struct file *rfile);
static int ramfs_rename_file(struct inode *i_node, struct inode *new_parent,
                             char *new_name);
#include <locks/spinlock.h>
#include <output/output.h>
#define MAX_RAMFS 512
#define CONSOLE_INO 1
#define NULL_INO 2
#define ZERO_INO 3
#define RAMFS_INSTANCE_COUNT 2
struct ramfs_instance {
    struct ramfs_inode inodes[MAX_RAMFS];
    struct spinlock lock;
    bool has_console;
};
struct dnode *ramfs_read_root_dir(struct vfs_device *dev);
struct dnode *ramfs_read_inode_dir(struct inode *inode);
int ramfs_create_dir(struct inode *parent, char *name);
struct inode* ramfs_create_file(struct inode *parent, char *name);
int ramfs_remove_file(struct inode *i_node);
int ramfs_read_file (struct file * rfile,void *buf,uint32_t count);
int ramfs_write_file (struct file * rfile,void *buf,uint32_t count);
int ramfs_stat_file (struct file * rfile,struct stat *st);

static struct ramfs_instance ramfs_instances[RAMFS_INSTANCE_COUNT];
static int ramfs_mount_count;
static int ramfs_device_nums[RAMFS_INSTANCE_COUNT];
static struct vfs_ops ramfs_ops;

static struct ramfs_instance *ramfs_for_device(struct vfs_device *dev)
{
    for (int i = 0; i < ramfs_mount_count; i++)
        if (ramfs_device_nums[i] == (int)dev->devicenum)
            return &ramfs_instances[i];
    panic("unknown ramfs device");
    return NULL;
}

static int ramfs_alloc_inode(struct ramfs_instance *fs)
{
    int first = fs->has_console ? 4 : 2;
    for (int i = first; i < MAX_RAMFS; i++)
        if (fs->inodes[i].dev == NULL)
            return i;
    return -1;
}

static void create_root_dir(struct vfs_device *dev, bool with_console) {
    struct ramfs_instance *fs = ramfs_for_device(dev);

    fs->has_console = with_console;
    fs->inodes[0].dev = dev;
    kstrcpy(fs->inodes[0].i_name, dev->mountpoint_root);
    fs->inodes[0].i_type=I_DIR;
    fs->inodes[0].i_ino = 0;
    fs->inodes[0].last_child = with_console ? 3 : 0;
    fs->inodes[0].file_size = 0;
    fs->inodes[0].parent = -1;
    fs->inodes[0].blocks = NULL;
    if (with_console) {
        fs->inodes[0].children[0] = CONSOLE_INO;
        fs->inodes[CONSOLE_INO].dev = dev;
        kstrcpy(fs->inodes[CONSOLE_INO].i_name,"console");
        fs->inodes[CONSOLE_INO].i_type=I_DEV;
        fs->inodes[CONSOLE_INO].i_ino = CONSOLE_INO;
        fs->inodes[CONSOLE_INO].file_size = 0;
        fs->inodes[CONSOLE_INO].parent = 0;
        fs->inodes[CONSOLE_INO].blocks = NULL;
        fs->inodes[0].children[1] = NULL_INO;
        fs->inodes[NULL_INO].dev = dev;
        kstrcpy(fs->inodes[NULL_INO].i_name, "null");
        fs->inodes[NULL_INO].i_type = I_DEV;
        fs->inodes[NULL_INO].i_ino = NULL_INO;
        fs->inodes[NULL_INO].parent = 0;
        fs->inodes[0].children[2] = ZERO_INO;
        fs->inodes[ZERO_INO].dev = dev;
        kstrcpy(fs->inodes[ZERO_INO].i_name, "zero");
        fs->inodes[ZERO_INO].i_type = I_DEV;
        fs->inodes[ZERO_INO].i_ino = ZERO_INO;
        fs->inodes[ZERO_INO].parent = 0;
    }

}

static int ramfs_mount(const char *path, const char *name, bool with_console)
{
    struct vfs_device dev;
    struct dnode *root = vfs_read_root_dir(ROOT);
    int device_num;

    if (ramfs_mount_count >= RAMFS_INSTANCE_COUNT)
        return -1;

    /* Create the FAT mount point on first boot, so no image preparation is
     * required for /dev or /tmp. */
    if (root == NULL)
        return -1;
    if (vfs_create_dir(root->root_inode, (char *)name) < 0) {
        vfs_free_dnode(root);
        return -1;
    }
    vfs_free_dnode(root);

    memzero8((uint8_t *)&dev, sizeof(dev));
    dev.fstype = RAM_FS;
    dev.ops = &ramfs_ops;
    int instance = ramfs_mount_count++;
    dev.rootfs = false;
    kstrcpy(dev.mountpoint, path);
    kstrcpy(dev.mountpoint_root, name);
    kstrcpy(dev.mountpoint_parent, ROOT);
    device_num = vfs_register_device(dev);
    if (device_num < 0)
        return -1;
    ramfs_device_nums[instance] = device_num;
    init_spinlock(&ramfs_instances[instance].lock);
    create_root_dir(vfs_get_device(device_num), with_console);
    return 0;
}

int ramfs_init(void) {
    ramfs_mount_count = 0;
    for (int i = 0; i < RAMFS_INSTANCE_COUNT; i++)
        ramfs_device_nums[i] = -1;
    memzero8((uint8_t *)&ramfs_ops, sizeof(ramfs_ops));
    ramfs_ops.read_root_dir = ramfs_read_root_dir;
    ramfs_ops.read_inode_dir= ramfs_read_inode_dir;
    ramfs_ops.create_dir = ramfs_create_dir;
    ramfs_ops.create_file = ramfs_create_file;
    ramfs_ops.remove_file = ramfs_remove_file;
    ramfs_ops.rename_file = ramfs_rename_file;
    ramfs_ops.read_file = ramfs_read_file;
    ramfs_ops.write_file = ramfs_write_file;
    ramfs_ops.truncate_file = ramfs_truncate_file;
    ramfs_ops.stat_file = ramfs_stat_file;
#ifdef DEBUG_RAMFS
    kprintf("RAMFS: mounting /dev and /tmp\n");
#endif
    if (ramfs_mount("/dev", "dev", true) < 0 ||
        ramfs_mount("/tmp", "tmp", false) < 0)
        return -1;
    return 0;
}
static int ramfs_read_console(void *buf, uint32_t count) {
    (void)count;
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
    struct ramfs_instance *fs = ramfs_for_device(rfile->dev);

    struct ramfs_block *r_block = fs->inodes[rfile->i_node.i_ino].blocks;
    uint64_t file_size = fs->inodes[rfile->i_node.i_ino].file_size;
    int ret_count = count;
    if (fs->has_console && rfile->i_node.i_ino == CONSOLE_INO) {
        return ramfs_read_console(buf,count);
    }
    if (fs->has_console && rfile->i_node.i_ino == NULL_INO)
        return 0;
    if (fs->has_console && rfile->i_node.i_ino == ZERO_INO) {
        memzero8((uint8_t *)buf, count);
        return count;
    }
    acquire_spinlock(&fs->lock);
    // Approaching the end of the file truncate read bytes
#ifdef DEBUG_FS_RAMFS
    kprintf("%d %d\n",rfile->pos + count,file_size);
#endif
    
    if (rfile->pos + count > file_size) {
        ret_count = file_size - rfile->pos;
    }

    memcpy(((char *)buf),((uint8_t *)r_block->block)+rfile->pos,ret_count);

    release_spinlock(&fs->lock);

    return ret_count;
}

int ramfs_stat_file (struct file * rfile,struct stat *st) { 
    struct ramfs_instance *fs = ramfs_for_device(rfile->dev);

    acquire_spinlock(&fs->lock);
    // Leave everything else default...
    st->st_dev = rfile->dev->devicenum;
    st->st_ino = rfile->i_node.i_ino;
    st->st_mode = fs->inodes[rfile->i_node.i_ino].i_type;
    st->st_size = fs->inodes[rfile->i_node.i_ino].file_size;


   release_spinlock(&fs->lock);

    return 0;
}


int ramfs_write_file (struct file * rfile,void *buf,uint32_t count) {
    struct ramfs_instance *fs = ramfs_for_device(rfile->dev);

    if (fs->has_console && rfile->i_node.i_ino == CONSOLE_INO) {
        return ramfs_write_console(buf,count);
    }
    if (fs->has_console && (rfile->i_node.i_ino == NULL_INO ||
                            rfile->i_node.i_ino == ZERO_INO))
        return count;
 
    acquire_spinlock(&fs->lock);
    struct ramfs_block *r_block = fs->inodes[rfile->i_node.i_ino].blocks;
    uint64_t file_size = fs->inodes[rfile->i_node.i_ino].file_size;

    //kprintf("Writing %d at %d\n",count,rfile->pos);
    if (file_size == 0) {
        uint64_t block =(uint64_t) kmalloc(count);
        r_block->block = (uint64_t *) block;
        fs->inodes[rfile->i_node.i_ino].file_size=count;
    }
    //we are appending to the block allocate new block

    else if (rfile->pos + count > file_size) {
         //kprintf("Allocating %d \n",rfile->pos + count);
        uint64_t block =(uint64_t) kmalloc(rfile->pos + count);

        memcpy((void *)block,r_block->block,file_size);
        kfree(r_block->block);
        r_block->block = (uint64_t *)  block;
        fs->inodes[rfile->i_node.i_ino].file_size= file_size + ( (rfile->pos + count) - file_size);

     } 
 
    memcpy(((uint8_t *)r_block->block)+rfile->pos,buf,count);

    release_spinlock(&fs->lock);

    return count ;
}

static int ramfs_truncate_file(struct file *rfile)
{
    struct ramfs_instance *fs = ramfs_for_device(rfile->dev);
    struct ramfs_block *block;

    if (rfile->i_node.i_type != I_FILE ||
        (fs->has_console && rfile->i_node.i_ino <= ZERO_INO))
        return -1;
    acquire_spinlock(&fs->lock);
    block = fs->inodes[rfile->i_node.i_ino].blocks;
    if (block != NULL && block->block != NULL) {
        kfree(block->block);
        block->block = NULL;
    }
    fs->inodes[rfile->i_node.i_ino].file_size = 0;
    rfile->i_node.file_size = 0;
    rfile->pos = 0;
    release_spinlock(&fs->lock);
    return 0;
}

struct inode* ramfs_create_file(struct inode *parent, char *name) 
{
    struct ramfs_instance *fs = ramfs_for_device(parent->dev);

    if (fs->inodes[parent->i_ino].last_child >= RAMFS_MAX_DIRECTORY)
        return NULL;
    acquire_spinlock(&fs->lock);
    int inode_num = ramfs_alloc_inode(fs);
    if (inode_num < 0) {
        release_spinlock(&fs->lock);
        return NULL;
    }
    fs->inodes[inode_num].dev = parent->dev;
    kstrcpy(fs->inodes[inode_num].i_name, name);
    fs->inodes[inode_num].i_type=I_FILE;
    fs->inodes[inode_num].i_ino = inode_num;
    fs->inodes[inode_num].last_child = 0;
    fs->inodes[inode_num].blocks = kmalloc(sizeof(struct ramfs_block));
    if (fs->inodes[inode_num].blocks == NULL) {
        fs->inodes[inode_num].dev = NULL;
        release_spinlock(&fs->lock);
        return NULL;
    }
    memzero8((uint8_t *)fs->inodes[inode_num].blocks, sizeof(struct ramfs_block));
    fs->inodes[inode_num].file_size = 0;
    fs->inodes[inode_num].parent =  parent->i_ino;

    fs->inodes[parent->i_ino].children[fs->inodes[parent->i_ino].last_child]=inode_num;
    fs->inodes[parent->i_ino].last_child++;
    struct inode *i = kmalloc(sizeof(struct inode));
    vfs_copy_inode(i,(struct inode *)&fs->inodes[inode_num]);
    release_spinlock(&fs->lock);

    return i;
}


int ramfs_create_dir(struct inode *parent, char *name) 
{
    struct ramfs_instance *fs = ramfs_for_device(parent->dev);
    if (fs->inodes[parent->i_ino].last_child >= RAMFS_MAX_DIRECTORY)
        return -1;

    acquire_spinlock(&fs->lock);
    int inode_num = ramfs_alloc_inode(fs);
    if (inode_num < 0) {
        release_spinlock(&fs->lock);
        return -1;
    }

    fs->inodes[inode_num].dev = parent->dev;
    kstrcpy(fs->inodes[inode_num].i_name, name);
    fs->inodes[inode_num].i_type=I_DIR;
    fs->inodes[inode_num].i_ino = inode_num;
    fs->inodes[inode_num].last_child = 0;
    fs->inodes[inode_num].file_size = 0;
    fs->inodes[inode_num].parent =  parent->i_ino;
    fs->inodes[parent->i_ino].children[fs->inodes[parent->i_ino].last_child]=inode_num;
    fs->inodes[parent->i_ino].last_child++;
    release_spinlock(&fs->lock);

    return 0;
}

int ramfs_remove_file(struct inode *i_node)
{
    struct ramfs_instance *fs = ramfs_for_device(i_node->dev);
    int inode_num = i_node->i_ino;
    struct ramfs_block *block;

    if (inode_num < 2 || inode_num >= MAX_RAMFS)
        return -1;
    acquire_spinlock(&fs->lock);
    if (fs->inodes[inode_num].dev == NULL ||
        (fs->inodes[inode_num].i_type != I_FILE &&
         fs->inodes[inode_num].i_type != I_DIR) ||
        (fs->inodes[inode_num].i_type == I_DIR &&
         fs->inodes[inode_num].last_child != 0)) {
        release_spinlock(&fs->lock);
        return -1;
    }
    int parent = fs->inodes[inode_num].parent;
    int index = -1;
    for (int i = 0; i < fs->inodes[parent].last_child; i++)
        if (fs->inodes[parent].children[i] == inode_num) {
            index = i;
            break;
        }
    if (index < 0) {
        release_spinlock(&fs->lock);
        return -1;
    }
    for (int i = index; i + 1 < fs->inodes[parent].last_child; i++)
        fs->inodes[parent].children[i] = fs->inodes[parent].children[i + 1];
    fs->inodes[parent].last_child--;
    fs->inodes[parent].children[fs->inodes[parent].last_child] = 0;
    block = fs->inodes[inode_num].blocks;
    while (block != NULL) {
        struct ramfs_block *next = block->next;
        if (block->block != NULL)
            kfree(block->block);
        kfree(block);
        block = next;
    }
    memzero8((uint8_t *)&fs->inodes[inode_num], sizeof(struct ramfs_inode));
    release_spinlock(&fs->lock);
    return 0;
}

static int ramfs_rename_file(struct inode *i_node, struct inode *new_parent,
                             char *new_name)
{
    struct ramfs_instance *fs = ramfs_for_device(i_node->dev);
    int inode_num = i_node->i_ino;
    uint64_t old_parent;
    int index = -1;

    if (inode_num < 2 || inode_num >= MAX_RAMFS || new_name[0] == '\0' ||
        new_parent->dev != i_node->dev || new_parent->i_type != I_DIR)
        return -1;
    if (fs->has_console && inode_num <= ZERO_INO)
        return -1;
    acquire_spinlock(&fs->lock);
    if (fs->inodes[inode_num].dev == NULL ||
        fs->inodes[new_parent->i_ino].dev == NULL) {
        release_spinlock(&fs->lock);
        return -1;
    }
    old_parent = fs->inodes[inode_num].parent;
    if (old_parent != new_parent->i_ino) {
        if (fs->inodes[new_parent->i_ino].last_child >= RAMFS_MAX_DIRECTORY) {
            release_spinlock(&fs->lock);
            return -1;
        }
        for (int i = 0; i < fs->inodes[old_parent].last_child; i++)
            if (fs->inodes[old_parent].children[i] == inode_num) {
                index = i;
                break;
            }
        if (index < 0) {
            release_spinlock(&fs->lock);
            return -1;
        }
        for (int i = index; i + 1 < fs->inodes[old_parent].last_child; i++)
            fs->inodes[old_parent].children[i] = fs->inodes[old_parent].children[i + 1];
        fs->inodes[old_parent].last_child--;
        fs->inodes[new_parent->i_ino]
            .children[fs->inodes[new_parent->i_ino].last_child++] = inode_num;
        fs->inodes[inode_num].parent = new_parent->i_ino;
    }
    kstrcpy(fs->inodes[inode_num].i_name, new_name);
    release_spinlock(&fs->lock);
    return 0;
}

struct dnode *ramfs_read_inode_dir(struct inode *i_node)
{
    struct ramfs_instance *fs = ramfs_for_device(i_node->dev);
    struct dnode *dir;
    struct inode_list *list=NULL;
    struct inode_list *prev = NULL;
    acquire_spinlock(&fs->lock);
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
    if (fs->inodes[i_node->i_ino].parent != -1){
        dir->head->current->i_ino = fs->inodes[i_node->i_ino].parent;
        dir->head->current->i_type = I_DIR;
        dir->head->current->dev = fs->inodes[i_node->i_ino].dev;
    } else {
        //Cross mount point
        dir->head->current->i_ino = fs->inodes[i_node->i_ino].dev->mnt_node_parent->i_ino;
        dir->head->current->i_type = fs->inodes[i_node->i_ino].dev->mnt_node_parent->i_type;
        dir->head->current->dev = fs->inodes[i_node->i_ino].dev->mnt_node_parent->dev;
    }
    dir->head->next = kmalloc(sizeof(struct inode_list));

    dir->head->next->current = kmalloc(sizeof(struct inode));
    kstrcpy(dir->head->next->current->i_name,".");
    dir->head->next->current->i_ino = i_node->i_ino;
    dir->head->next->current->i_type = I_DIR;
    dir->head->next->current->dev = i_node->dev;
    dir->head->next->current->file_size = 0;
    dir->head->next->next=NULL;
    if( fs->inodes[i_node->i_ino].last_child)
        prev = dir->head->next;

    for(int i=0; i < fs->inodes[i_node->i_ino].last_child; i++){
        int child = fs->inodes[i_node->i_ino].children[i];
        list = kmalloc(sizeof(struct inode_list));
        list->current = kmalloc(sizeof(struct inode));
        kstrcpy(list->current->i_name,fs->inodes[child].i_name);
        list->current->i_type = fs->inodes[child].i_type;
        list->current->dev = fs->inodes[child].dev;
        list->current->file_size = fs->inodes[child].file_size;
        list->current->i_ino = fs->inodes[child].i_ino;
        prev->next = list;
        list->next = NULL;
        prev = list;
    }
    release_spinlock(&fs->lock);

    return dir;
}

struct dnode *ramfs_read_root_dir(struct vfs_device *dev)
{
    struct ramfs_instance *fs = ramfs_for_device(dev);
    acquire_spinlock(&fs->lock);

    struct dnode *dir;
#ifdef DEBUG_FS_RAMFS
    kprintf("dev %x\n",dev);
#endif
    dir = kmalloc(sizeof(struct dnode));
    dir->root_inode = kmalloc(sizeof(struct inode));
    vfs_copy_inode(dir->root_inode,(struct inode *)&fs->inodes[0]);

    dir->head = kmalloc(sizeof(struct inode_list));
    dir->head->current = kmalloc(sizeof(struct inode));
    dir->head->current->file_size = 0;
    kstrcpy(dir->head->current->i_name,"..");
    //Cross mount point since we are on mount 
    // Assumes we are not /
    dir->head->current->i_ino = fs->inodes[0].dev->mnt_node_parent->i_ino;
    dir->head->current->i_type = fs->inodes[0].dev->mnt_node_parent->i_type;
    dir->head->current->dev = fs->inodes[0].dev->mnt_node_parent->dev;
    dir->head->next = kmalloc(sizeof(struct inode_list));
    
    dir->head->next->current = kmalloc(sizeof(struct inode));
    kstrcpy(dir->head->next->current->i_name,".");
    dir->head->next->current->i_ino = dir->root_inode->i_ino;
    dir->head->next->current->i_type = I_DIR;
    dir->head->next->current->dev = dir->root_inode->dev;
    dir->head->next->current->file_size = 0;
    dir->head->next->next=NULL;

    release_spinlock(&fs->lock);

    return dir;
}
