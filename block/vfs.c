#include <block/vfs.h>
#include <output/output.h>
#include <mm/mm.h>

struct vfs_device vfs_devices[VFS_MAX_DEVICES];
int current_device = 0;

int vfs_register_device(struct vfs_device newdev) {

    struct vfs_device* dev;
    if (current_device == VFS_MAX_DEVICES-1) {
        kprintf("Max devices allocated;");
        return -1;
    }
    dev = &vfs_devices[current_device];
    *dev = newdev;
    dev->devicenum = current_device;
    kprint_hex("VFS Device registered ",dev->devicenum);
    current_device+=1;
    return 0;
}
void read_dir(char *path) {

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
        kprint_hex("Invalid device",idev);
        goto error;
    }
    vdevice = &vfs_devices[idev];
    vdevice->ops->read_path(ptr,vdevice->finfo);
    error:

    return;
}
