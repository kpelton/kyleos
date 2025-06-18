#include <fs/vfs.h>
#include <output/output.h>
#include <mm/mm.h>
struct vfs_device vfs_devices[VFS_MAX_DEVICES];

static int current_device = 0;
static struct file_table ftable ;


static struct inode* vfs_find_file_in_dir(const char *filename, struct dnode *dptr);
struct inode* top_root_inode;
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
struct inode *vfs_get_root_inode() 
{   
    return top_root_inode;
}

int vfs_getdents(struct file * rfile,void *dirp,int count) {
    
    if (rfile->i_node.i_type != I_DIR)
        return -1;
    
    struct inode_list *ptr;
    struct file *cfile = NULL;
    struct dirent *dir_arr = (struct dirent *) dirp;
    struct dnode *dptr = vfs_read_inode_dir(&rfile->i_node);
    int read_count = 0;
    if (dptr == 0)
        goto error;
    
    ptr = dptr->head;
    while (ptr != 0)
    {
        dir_arr[read_count].i_no = ptr->current->i_ino;
        dir_arr[read_count].i_type = ptr->current->i_type;
        kstrncpy(dir_arr[read_count].i_name, ptr->current->i_name,VFS_MAX_FNAME);
        ptr = ptr->next;
        read_count++;
        if (read_count >= count) {
            panic("unable to do anything here");
            vfs_free_dnode(dptr);
            return -1;
        }
    }
    vfs_free_dnode(dptr);
    return read_count;
error:
    return -1;
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
        dev->mnt_node = vfs_walk_path(dev->mountpoint,root);
        if(!dev->mnt_node)
            panic("failed to mount non rootfs");
        
        root =vfs_devices[0].ops->read_root_dir(&vfs_devices[0]);
        if (kstrcmp(dev->mountpoint_parent,"/") == 0)
            dev->mnt_node_parent = root->root_inode;
        else {
        dev->mnt_node_parent = vfs_walk_path(dev->mountpoint_parent,root);
            if(!dev->mnt_node)
                panic("failed to mount non rootfs");
        }
    //memory leak likely here
    } else{
        // Store top level ref to /
        struct dnode *root = vfs_devices[0].ops->read_root_dir(&vfs_devices[0]);
        top_root_inode=root->root_inode;
    }
    kprintf("VFS Device registered %d at %s\n", dev->devicenum,dev->mountpoint);
    current_device += 1;
    return dev->devicenum;
}
// Parent must free d_node
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
    // Assumption here that the dnode ptr is allocated on the stack
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
            vfs_copy_inode(&ftable.open_files[i].i_node,i_node);
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
    if (i_node)
    {
        retfile = vfs_get_open_file(i_node);
        retfile->pos = 0;
        retfile->flags = flags;
        //Release lock aquired in get open_file
        release_spinlock(&ftable.lock);
    }
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
//will free dnode internally
struct inode *vfs_walk_path(char *path, struct dnode *pwd)
{
    int end = 0;
    char *curr = path;
    char buffer[1024];
    struct inode_list *ptr;
    struct inode *iptr;
    struct inode *iptr_ret=NULL;
    struct dnode *dptr = pwd;
    struct dnode *prev_dptr;
    bool found = false;
    // handle ./
    // if this a a relative path in local dir
   if (kstrstr(path, "/") < 0  ) {
        iptr_ret = vfs_find_file_in_dir(path,dptr);
        return iptr_ret;
    }

    while (end != -1)
    {
        end = kstrstr(curr, "/");
        // kprintf("%d %s\n", end, curr);
        if (end >= 0)
        {
            kstrncpy(buffer, curr, end);
            buffer[end] = '\0';
            curr += end + 1;
        }
        // Skip check on local dir
        if (kstrcmp(buffer,".") == 0) {
            continue;
        }

        //kprintf("main comparison %s %s\n",curr,buffer);
        // printf("%s\n",curr);
        ptr = dptr->head;
        found = false;
        while (ptr)
        {
            //Traverse directories in path
            //kprintf("current:%s buffer:%s\n",ptr->current->i_name,buffer);
            if (kstrcmp(ptr->current->i_name, buffer) == 0 && ptr->current->i_type == I_DIR)
            {
                prev_dptr = dptr;
                // Something left in the path name and not the final node
                if (end > 0) {
                    struct inode *mnt = fs_is_mount_point(ptr->current);
                    if (mnt){
                        kprintf("mount\n");
                        dptr = vfs_read_inode_dir(mnt);
                        vfs_free_inode(mnt);
                        ptr = dptr->head;
                    } else{
                        dptr = vfs_read_inode_dir(ptr->current);
                    }
                    vfs_free_inode(prev_dptr);
                    found = true;
                    break;
                }//else {
                 //   kprintf("fail\n");
                //}
            }
            ptr = ptr->next;
        }
        //if we didn't find anything then the directories are bad and we can return NULL
        if(!found && end > 0) {
             if (dptr != NULL) {
                vfs_free_dnode(dptr);
             }
            return NULL;
        }
    }
    iptr_ret = vfs_find_file_in_dir(curr,dptr);
    vfs_free_dnode(dptr);
    return iptr_ret;
}

//Find a specific file given an dnode list and return a copy of the new inode
static struct inode* vfs_find_file_in_dir(const char *filename, struct dnode *dptr) {
    struct inode_list *ptr = dptr->head;
    struct inode *iptr = NULL;
    while (ptr)
    {
        //kprintf("find in file  %s\n",ptr->current->i_name);
         if (kstrcmp(ptr->current->i_name, filename) == 0)
         {
            iptr = kmalloc(sizeof(struct inode));
            struct inode *mnt = fs_is_mount_point(ptr->current);
            if(mnt) {
                vfs_copy_inode(iptr,mnt);
                vfs_free_inode(mnt);
            }else{
                vfs_copy_inode(iptr,ptr->current);
            }
            return iptr;
        }
        ptr = ptr->next;
    }
    return NULL;
}

void vfs_copy_inode(struct inode *dst, struct inode *src)
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

struct inode * fs_is_mount_point(struct inode *ptr) {
    for (int i=0; i<current_device; i++){
        if (! vfs_devices[i].rootfs && vfs_compare_inode(ptr,vfs_devices[i].mnt_node)) {
            kprintf("reading dev %d\n",i);
            struct dnode *dnode = vfs_devices[i].ops->read_root_dir(&vfs_devices[i]);
            struct inode *iptr = kmalloc(sizeof(struct inode));
            vfs_copy_inode(iptr,dnode->root_inode);
            vfs_free_dnode(dnode);
            kprintf("return %x\n",iptr);
            return(iptr);
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
    return rcount;

error:
    kprintf("Read error %d\n", rfile->flags);
    return -1;
}

int vfs_stat_file(struct file *rfile, struct stat *st)
{
    int idev;
    idev = rfile->dev->devicenum;
    return vfs_devices[idev].ops->stat_file(rfile, st);

error:
    kprintf("Read error %d\n", rfile->flags);
    return -1;
}


int vfs_write_file(struct file *rfile, void *buf, int count)
{
    int idev;
    idev = rfile->dev->devicenum;
    int rcount = vfs_devices[idev].ops->write_file(rfile, buf, count);
    rfile->pos += rcount;
    return count;

}

char * vfs_strip_path(char *ptr) {
    char *ret_ptr = ptr;
    char *check_ptr = ptr;
    while(*check_ptr != '\0') {

        check_ptr++;
        if(*check_ptr == '/')
            ret_ptr = check_ptr+1;
    }
    kprintf("returning %s\n",ret_ptr);
    return ret_ptr;
}

char *vfs_get_dir( char *filename) {
    char *directory = NULL;
    //char *last_slash = kstrrchr(filename, '/'); // Find the last occurrence of '/'
    // Hard coded for /2
    char *last_slash = filename+2;
    if (last_slash != NULL) {
        // Allocate memory for the directory string
        directory = (char *)kmalloc(last_slash - filename + 2);
        if (directory == NULL) {
            return NULL;
        }
        // Copy the directory part of the filename
        kstrncpy(directory, filename, last_slash - filename );
        directory[last_slash - filename + 1] = '\0'; // Null-terminate the string
    }
    
    return directory;
}


struct inode* vfs_create_file(struct inode* parent, char *name, uint32_t flags)
{
    //kprintf("FLAGS %x\n",flags);
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
