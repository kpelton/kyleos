#include <fs/vfs.h>
#include <output/output.h>
#include <mm/mm.h>
#define VFS_MAX_OPEN 1024
struct vfs_device vfs_devices[VFS_MAX_DEVICES];

static int current_device = 0;
static struct file open_files[VFS_MAX_OPEN];

void vfs_init()
{
    int i;
    for (i = 0; i < VFS_MAX_OPEN; i++)
    {
        open_files[i].refcount = 0;
        open_files[i].dev = NULL;
        open_files[i].flags = 0;
    }
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
    kprintf("VFS Device registered %d\n", dev->devicenum);
    current_device += 1;
    return 0;
}

struct dnode *vfs_read_inode_dir(struct inode *i_node)
{

    int idev;
    idev = i_node->dev->devicenum;
    return vfs_devices[idev].ops->read_inode_dir(i_node);
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
    char *ptr = path;
    char dev[4] = {0};
    int i = 0;
    int idev;
    struct vfs_device *vdevice;

    dev[i] = '0';
    if (*ptr != '/')
    {
        kprintf("Incorrect path name");
        goto error;
    }
    idev = atoi(dev);
    if (idev > VFS_MAX_DEVICES || idev > current_device - 1)
    {
        kprintf("Invalid device %d\n", idev);
        goto error;
    }
    vdevice = &vfs_devices[idev];
    return vdevice->ops->read_root_dir(vdevice);
error:

    return 0;
}
static struct file *vfs_get_open_file(struct inode *i_node)
{
    int i = 0;

    //  look for slot and open file
    for (i = 0; i < VFS_MAX_OPEN; i++)
    {
        if (open_files[i].refcount == 0)
        {
            open_files[i].dev = i_node->dev;
            vfs_copy_inode(i_node, &open_files[i].i_node);
            open_files[i].refcount++;
            return &(open_files[i]);
        }
    }
    panic("File table full");
    return NULL;
}

struct file *vfs_open_file(struct inode *i_node, uint32_t flags)
{
    struct file *retfile = NULL;
    if (flags > MAX_FILE_FLAGS && (flags != O_RDONLY && flags != O_RDWR && flags != O_WRONLY))
    {
        kprintf("fail");
        goto done;
    }
    if (i_node)
    {
        retfile = vfs_get_open_file(i_node);
        retfile->pos = 0;
        retfile->flags = flags;
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

struct inode *vfs_walk_path(char *path, struct dnode *pwd, enum inode_type type)
{
    int end = 0;
    char *blah = path;
    char buffer[1024];
    struct inode_list *ptr;
    struct inode *iptr;
    struct dnode *dptr = pwd;
    // if (*blah == '/')
    //     return;

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
        while (ptr)
        {
            // kprintf("%s %s\n",ptr->current->i_name,buffer);

            if (kstrcmp(ptr->current->i_name, buffer) == 0 && ptr->current->i_type == I_DIR)
            {
                if (dptr != pwd)
                    vfs_free_dnode(dptr);
                dptr = vfs_read_inode_dir(ptr->current);
                break;
            }
            ptr = ptr->next;
        }
    }

    ptr = dptr->head;
    while (ptr)
    {
        // kprintf("%s\n",ptr->current->i_name);
        if (kstrcmp(ptr->current->i_name, blah) == 0 && ptr->current->i_type == (int)type)
        {
            iptr = kmalloc(sizeof(struct inode));
            vfs_copy_inode(ptr->current,iptr);
            vfs_free_dnode(dptr);
            return iptr;
        }
        ptr = ptr->next;
    }
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

int vfs_read_file(struct file *rfile, void *buf, int count)
{
    int idev;
    idev = rfile->dev->devicenum;
    if (rfile->flags != O_RDONLY && rfile->flags != O_RDWR)
    {
        goto error;
    }

    return vfs_devices[idev].ops->read_file(rfile, buf, count);

error:
    kprintf("Read error %d\n", rfile->flags);
    return -1;
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
