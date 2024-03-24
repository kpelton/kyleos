#include <fs/vfs.h>
#include <output/output.h>
#include <mm/mm.h>
struct vfs_device vfs_devices[VFS_MAX_DEVICES];

static int current_device = 0;
static struct file_table ftable ;

static struct inode * fs_is_mount_point(struct inode *ptr);

void vfs_init()
{
    init_spinlock(&ftable.lock);
    int i;
    acquire_spinlock(&ftable.lock);
    for (i = 0; i < VFS_MAX_OPEN; i++)
    {
        ftable.open_files[i].refcount = 0;
        ftable.open_files[i].dev = NULL;
        ftable.open_files[i].flags = 0; 
    }

    release_spinlock(&ftable.lock);
}

struct vfs_device *vfs_get_device(int num)
{
    return &vfs_devices[num];
}

int vfs_register_device(struct vfs_device newdev)
{

    struct vfs_device *dev;
    if (current_device == VFS_MAX_DEVICES - 1)
    {
        kprintf("Max devices allocated;");
        return -1;
    }
    dev = &vfs_devices[current_device];
    *dev = newdev;
    dev->devicenum = current_device;
    kstrcpy(dev->mountpoint,newdev.mountpoint);
    if(dev->rootfs == false) {
        struct dnode *root = vfs_devices[0].ops->read_root_dir(&vfs_devices[0]);
        dev->mnt_node = vfs_walk_path(dev->mountpoint,root,I_DIR);
        if(!dev->mnt_node)
            panic("failed to mount non rootfs");
        
        root =vfs_devices[0].ops->read_root_dir(&vfs_devices[0]);
        if (kstrcmp(dev->mountpoint_parent,"/") == 0)
            dev->mnt_node_parent = root->root_inode;
        else {
        dev->mnt_node_parent = vfs_walk_path(dev->mountpoint_parent,root,I_DIR);
            if(!dev->mnt_node)
                panic("failed to mount non rootfs");
        }
    //memory leak likely here
    }
    kprintf("VFS Device registered %d at %s\n", dev->devicenum,dev->mountpoint);
    current_device += 1;
    return dev->devicenum;
}

struct dnode *vfs_read_inode_dir(struct dnode *parent,struct inode *i_node)
{

    int idev;
    idev = i_node->dev->devicenum;

    return vfs_devices[idev].ops->read_inode_dir(parent,i_node);
}

void vfs_cat_inode_file(struct inode *i_node)
{

    int idev;
    idev = i_node->dev->devicenum;
    vfs_devices[idev].ops->cat_inode_file(i_node);
}

void vfs_free_inode(struct inode *i_node)
{
    kfree(i_node);
}

void vfs_free_inode_list(struct inode_list *list)
{
    struct inode_list *curr = list;
    struct inode_list *prev = list;

    while (curr != 0)
    {
        vfs_free_inode(curr->current);
        curr = curr->next;
        kfree(prev);
        prev = curr;
    }
}

void vfs_free_dnode(struct dnode *dn)
{
    vfs_free_inode_list(dn->head);
    vfs_free_inode(dn->root_inode);
    kfree(dn);
}

struct dnode *vfs_read_root_dir(char *path)
{
    int i = 0;
    struct vfs_device *vdevice;
    
    if (current_device == 0)
        goto error;

    for (i=0; i<current_device; i++){
        if (kstrcmp(vfs_devices[i].mountpoint,path) == 0)
            break;
    }
    if (i == current_device)
        goto error;

    vdevice = &vfs_devices[i];
    return vdevice->ops->read_root_dir(vdevice);
error:
    return 0;
}

//aquires ftable.lock which must be released in calling function
static struct file *vfs_get_open_file(struct inode *i_node)
{
    int i = 0;

    //  look for slot and open file
    acquire_spinlock(&ftable.lock);
    //Lock must be released in calling function
    for (i = 0; i < VFS_MAX_OPEN; i++)
    {
        if (ftable.open_files[i].refcount == 0)
        {
            ftable.open_files[i].dev = i_node->dev;
            vfs_copy_inode(i_node, &ftable.open_files[i].i_node);
            ftable.open_files[i].refcount++;
            return &(ftable.open_files[i]);
        }
    }
    release_spinlock(&ftable.lock);
    panic("File table full");
    return NULL;
}

struct file *vfs_open_file(struct inode *i_node, uint32_t flags)
{
    struct file *retfile = NULL;
    if (flags > MAX_FILE_FLAGS  )
    {
        kprintf("fail");
        goto done;
    }
    if (i_node)
    {
        retfile = vfs_get_open_file(i_node);
        retfile->pos = 0;
        retfile->flags = flags;
        //Release lock aquired in get open_file
        release_spinlock(&ftable.lock);
    }
done:
    return retfile;
}

void vfs_close_file(struct file *ofile)
{
    if (ofile && ofile->refcount)
    {
        ofile->refcount--;
        if (ofile->refcount == 0)
        {
            ofile->dev = NULL;
            ofile->flags = 0;
            ofile->pos = 0;
        }
    }
}
//walk path and find inode given absolute path
//TODO redo this monster
struct inode *vfs_walk_path(char *path, struct dnode *pwd, enum inode_type type)
{
    int end = 0;
    char *blah = path;
    char buffer[1024];
    struct inode_list *ptr;
    struct inode *iptr;
    struct dnode *dptr = pwd;
    bool found = false;
    // if (*blah == '/')
    //     return;
    kprintf("%s\n",path);

    while (end != -1)
    {
        end = kstrstr(blah, "/");
        // kprintf("%d %s\n", end, blah);
        if (end >= 0)
        {
            kstrncpy(buffer, blah, end);
            buffer[end] = '\0';
            blah += end + 1;
        }
        // printf("%s\n",blah);
        ptr = dptr->head;
        found = false;
        while (ptr)
        {
             
             //kprintf("%s %s\n",ptr->current->i_name,buffer);

            if (kstrcmp(ptr->current->i_name, buffer) == 0 && ptr->current->i_type == I_DIR)
            {
                vfs_free_dnode(dptr);
                struct inode *mnt = fs_is_mount_point(ptr->current);
                if (mnt){
                    kprintf("p1\n");

                    dptr = mnt->dev->ops->read_root_dir(mnt->dev);
                    kprintf("mount point");
                }
                else{
                    kprintf("p2\n");
                }
                    dptr = vfs_read_inode_dir(dptr,ptr->current);
                
                //Current entry on p ath was found
                found = true;
                break;
            }
            ptr = ptr->next;
        }
        //IF we didn't find anything and we are not on the last or first node then return NULL
        if(!found && end > 0) {
             if (dptr != NULL)
                vfs_free_dnode(dptr);
            return NULL;
        }
    }

    ptr = dptr->head;
    while (ptr)
    {
        // kprintf("%s\n",ptr->current->i_name);
        if (kstrcmp(ptr->current->i_name, blah) == 0 && ptr->current->i_type == (int)type)
        {
            iptr = kmalloc(sizeof(struct inode));
            struct inode *mnt = fs_is_mount_point(ptr->current);
            if(mnt) {
                kprintf("Mount point\n");
                vfs_copy_inode(mnt,iptr);
            }else{
                vfs_copy_inode(ptr->current,iptr);

            }
            //vfs_free_dnode(dptr);
            dptr = NULL;
            return iptr;
        }
        ptr = ptr->next;
    }
    if (dptr != NULL)
        vfs_free_dnode(dptr);
    return NULL;
}

void vfs_copy_inode(struct inode *src, struct inode *dst)
{
    kstrncpy(dst->i_name, src->i_name, VFS_MAX_FNAME);
    dst->file_size = src->file_size;
    dst->i_ino = src->i_ino;
    dst->dev = src->dev;
    dst->i_type = src->i_type;
}

static bool vfs_compare_inode(struct inode *src, struct inode *dst)
{
    if(dst->i_ino != src->i_ino || dst->dev != src->dev || src->i_type != dst->i_type)
        return false;
    return true;
}

static struct inode * fs_is_mount_point(struct inode *ptr) {
    int i;

    for (i=0; i<current_device; i++){
        if (! vfs_devices[i].rootfs && vfs_compare_inode(ptr,vfs_devices[i].mnt_node)) {
            struct dnode *dnode = vfs_devices[i].ops->read_root_dir(&vfs_devices[i]);
            struct inode *retval = dnode->root_inode;
            vfs_free_dnode(dnode);
            return(retval);
        }
    }
    return NULL;
}

int vfs_read_file(struct file *rfile, void *buf, int count)
{
    int idev;
    idev = rfile->dev->devicenum;
    if (rfile->flags != O_RDONLY && rfile->flags != O_RDWR)
    {
        goto error;
    }

    int rcount = vfs_devices[idev].ops->read_file(rfile, buf, count);
    rfile->pos += rcount;
    return count;

error:
    kprintf("Read error %d\n", rfile->flags);
    return -1;
}

int vfs_write_file(struct file *rfile, void *buf, int count)
{
    int idev;
    idev = rfile->dev->devicenum;
    if (rfile->flags != O_WRONLY && rfile->flags != O_RDWR)
    {
        goto error;
    }

    int rcount = vfs_devices[idev].ops->write_file(rfile, buf, count);
    rfile->pos += rcount;
    return count;

error:
    kprintf("Read error %d\n", rfile->flags);
    return -1;
}



int vfs_create_file(struct inode* parent, char *name, uint32_t flags)
{
    kprintf("FLAGS %x\n",flags);
    return vfs_devices[parent->dev->devicenum].ops->create_file(parent, name);
}

int vfs_read_file_offset(struct file *rfile, void *buf, int count, uint32_t offset)
{
    int idev;
    idev = rfile->dev->devicenum;

    // set offset of file -- No check if we are going over file
    rfile->pos = offset;
    return vfs_devices[idev].ops->read_file(rfile, buf, count);
}

int vfs_create_dir(struct inode *parent, char *name)
{
    return vfs_devices[parent->dev->devicenum].ops->create_dir(parent, name);
}
