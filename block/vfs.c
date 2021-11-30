#include <block/vfs.h>
#include <output/output.h>
#include <mm/mm.h>

struct vfs_device vfs_devices[VFS_MAX_DEVICES];
static int current_device = 0;

int vfs_register_device(struct vfs_device newdev) {

    struct vfs_device* dev;
    if (current_device == VFS_MAX_DEVICES-1) {
        kprintf("Max devices allocated;");
        return -1;
    }
    dev = &vfs_devices[current_device];
    *dev = newdev;
    dev->devicenum = current_device;
    kprintf("VFS Device registered %d\n",dev->devicenum);
    current_device+=1;
    return 0;
}

struct dnode* vfs_read_inode_dir(struct inode * i_node) {
   
    int idev;
    idev = i_node->dev->devicenum;
    return vfs_devices[idev].ops->read_inode_dir(i_node);
}

void vfs_read_inode_file(struct inode * i_node) {
   
    int idev;
    idev = i_node->dev->devicenum;
    vfs_devices[idev].ops->read_inode_file(i_node);
}

void vfs_free_inode(struct inode * i_node) {
    kfree(i_node);
}

void vfs_free_inode_list(struct inode_list * list) {
    struct inode_list* curr=list;
    struct inode_list* prev=list;

    while(curr != 0) {
        vfs_free_inode(curr->current);
        curr = curr->next;
        kfree(prev);
        prev = curr;
    }
}

void vfs_free_dnode(struct dnode * dn) {
    vfs_free_inode_list(dn->head);
    vfs_free_inode(dn->root_inode);
    kfree(dn);
}

struct dnode* vfs_read_root_dir(char *path) {

    char *ptr = path;
    char dev[4];
    int i = 0;
    int idev;
    struct vfs_device* vdevice;
    while (*ptr != ':') {
        dev[i] = *ptr;
        i += 1;
        ptr += 1;
    }
    dev[i] = '\0';
    ptr+=1;
    if (*ptr != '/') {
        kprintf("Incorrect path name");
        goto error;
    }
    idev = atoi(dev);
    if (idev > VFS_MAX_DEVICES || idev > current_device-1) {
        kprintf("Invalid device %d\n",idev);
        goto error;
    }
    vdevice = &vfs_devices[idev];
    return vdevice->ops->read_root_dir(vdevice);
    error:

    return 0;
}
