#include <fs/vfs.h>
#include <include/types.h>
#include <mm/mm.h>
#include <output/output.h>
#include <sched/exec.h>
#include <sched/ps.h>
#include <sched/sched.h>
typedef int (*sys_call)(void);

static int sleep(int msec)
{
    ksleepm(msec);
    return 0;
};

static int creat(char *path, uint32_t flags)
{
    int fd = -1;
    struct inode *iptr = NULL;
    struct ktask *pid = get_current_process();
    struct dnode *dptr = vfs_read_root_dir("/");
#ifdef DEBUG_SYS_CREATE
    kprintf("creating file\n");
#endif
    char *last_dir = vfs_get_dir(path);
    vfs_strip_path(path);
    // kprintf("last_dir %s, fname %s\n",last_dir,fname);
    iptr = vfs_walk_path(last_dir, dptr);

    vfs_create_file(iptr, vfs_strip_path(path), flags);

    // File is created now open it
    iptr = vfs_walk_path(path, dptr);
    fd = user_process_open_fd(pid, iptr, flags);
    kfree(last_dir);
    if (iptr)
        vfs_free_inode(iptr);
    return fd;
}
#define DELIM '/'
#define ROOT "/"
// destructive
//  Also does not handle duplicate ///
static char *basename(char *path)
{
    char *str = path;
    char *last = str;
    if (kstrcmp(path, ROOT) == 0)
    {
        return path;
    }

    while (*str != '\0')
    {
        if (*str == DELIM && *(str + 1) != '\0')
        {
            last = str + 1;
        }
        // Trailing /
        if (*str == DELIM && *(str + 1) == '\0')
            *str = '\0';
        str++;
    }

    return last;
}

// destructive
//  Does not handle realtive directories
//  Also does not handle duplicate ///
static char *dirname(char *path)
{
    char *str = path;
    int place = 0;
    int last = 0;
    int found = 0;

    if (kstrcmp(path, ROOT) == 0)
    {
        return path;
    }

    while (*str != '\0')
    {
        if (*str == DELIM)
            found = 1;

        if (*str == DELIM && *(str + 1) != '\0')
        {
            last = place;
        }
        str++;
        place++;
    }

    // Not found case realtive directory
    if (!found)
    {
        path[0] = '.';
        path[1] = '\0';
        return path;
    }

    // root directory case
    if (found && last == 0)
    {
        last++;
    }

    path[last] = '\0';
    return path;
}

static int open(char *path, uint32_t flags)
{

    int fd = -1;
    //    if (flags > MAX_FILE_FLAGS)
    //        goto done;
     kprintf("open %s\n",path);
    struct dnode *dptr;
    struct ktask *pid = get_current_process();
    struct inode *iptr = NULL;
    int pathlen = kstrlen(path);
    char *s_basepath = (char *)kmalloc(pathlen + 1);
    char *s_dirname = (char *)kmalloc(pathlen + 1);
    kstrncpy(s_basepath, path, pathlen + 1);
    kstrncpy(s_dirname, path, pathlen + 1);
    char *last_dir = dirname(s_dirname);



    if (path[0] == '/') {
        dptr = vfs_read_root_dir("/");
    } else {
        dptr = vfs_read_inode_dir(pid->cwd);
    }

    // If we are in the root dir or cwd don't walk path
    if (kstrcmp(path,"/") != 0 && kstrcmp (path,".") != 0) {
        iptr = vfs_walk_path(path, dptr);
    }else{
        iptr = dptr->root_inode;
    }
    kprintf("iptr val %x\n",iptr);
    if (iptr != NULL)
    {
        fd = user_process_open_fd(pid, iptr, flags);
        vfs_free_inode(iptr);
        // vfs_free_dnode(dptr);
    }
    else if ((flags & O_WRONLY) == O_WRONLY)
    {
           // get a directory entry in order to walk
        if (path[0] == '/') {
            dptr = vfs_read_root_dir("/");
        } else {
            dptr = vfs_read_inode_dir(pid->cwd);
        }
        kprintf("last_dir%s\n",last_dir);    
        // get parent node
        if (kstrcmp(path,"/") != 0 && kstrcmp (path,".") != 0 && kstrcmp(last_dir,".") != 0 ) {
            iptr = vfs_walk_path(last_dir, dptr);
        }else{
            iptr =kmalloc(sizeof(struct inode));
            vfs_copy_inode(iptr,dptr->root_inode);
            vfs_free_dnode(dptr);
        }

        kprintf("iptr2 val %x\n",iptr);

         if (iptr)
        {

            iptr = vfs_create_file(iptr, vfs_strip_path(path), flags);
            if (iptr)
            {
                fd = user_process_open_fd(pid, iptr, flags);
                vfs_free_inode(iptr);
            }
        }
    }
    kprintf("Returning %d for  path %s\n",fd,path);
    return fd;
}

static int read(int fd, void *buf, int count)
{
    int countr = 0;
    if (count < 0 || fd < 0)
        return -1;
    if (buf >= (void *)KERN_SPACE_BOUNDRY)
    {
        return -1;
    }

    struct ktask *pid = get_current_process();
    countr = user_process_read_fd(pid, fd, buf, count);
#ifdef DEBUG_READ_SYS
    kprintf("READ %d\n", countr);
#endif
    return countr;
}

static int write(int fd, void *buf, int count)
{
    int countr = 0;
    if (count < 0 || fd < 0)
        return -1;
    if (buf >= (void *)KERN_SPACE_BOUNDRY)
    {
        return -1;
    }

    struct ktask *pid = get_current_process();
    countr = user_process_write_fd(pid, fd, buf, count);
    return countr;
}

static int fork()
{
    return user_process_fork();
}

static int close(int fd)
{
    struct ktask *pid = get_current_process();
    return user_process_close_fd(pid, fd);
}

static void *sbrk(uint64_t increment)
{
    struct ktask *pid = get_current_process();
    return user_process_sbrk(pid, increment);
}

static void exit(int code)
{
    struct ktask *pid = get_current_process();
    user_process_exit(pid, code);
}

static int wait(int pid)
{
    return process_wait(pid);
}

static int debugprint(char *msg)
{
    kprintf("%s", msg);
    return 0;
}

static void debug_read_input(char *dst)
{
    read_input(dst);
}
/*
  +----------------------+
  |                    |
  |                    |
  |  Command-Line      |
  |  Arguments (argv)  |
  |                    |
  |  [0] ------------> |   argv[0] (Program Name)
  |  [1] ------------> |   argv[1] (First Argument)
  |  [2] ------------> |   argv[2] (Second Argument)
  |        ...         |
  |  [argc] ----------> |   argv[argc] (Last Argument)
  |                    |
  +----------------------+
  |                    |
  |  argc              |
  |  (Argument Count)  |
  |                    |
  +----------------------+
  |  Local Variables   |
  |  and Stack Data    |
  |                    |
  +----------------------+
  |  Return Address    |
  |                    |
  |                    |
  +----------------------+
  |  Previous Stack    |
  |  Frames (if any)   |
  |                    |
  +----------------------+

*/
// TODO add support for arguments
static int exec(char *path)
{
    int retval = -1;
    struct dnode *dptr = vfs_read_root_dir("/");
    struct inode *iptr = vfs_walk_path(path, dptr);
    if (iptr != NULL)
    {
        retval = exec_from_inode(iptr, true, NULL);
    }
    return retval;
}

static int exec_args(char *path, char *argv[])
{
    int retval = -1;
    if (!path)
        return -1;
    struct dnode *dptr = vfs_read_root_dir("/");
    struct inode *iptr = vfs_walk_path(path, dptr);
    char **user_argv = NULL;
    int i;
    int j;
    if (iptr != NULL)
    {
        for (i = 0; i < MAX_ARGS && argv[i]; i++)
            ;

        // Too many arguments
        if (i == MAX_ARGS)
            return -1;

        if (i > 0)
        {
            user_argv = kmalloc(sizeof(uint64_t *) * i + 1);
            for (j = 0; j < i; j++)
            {
                user_argv[j] = kmalloc(kstrlen(argv[j]) + 1);
                kstrcpy(user_argv[j], argv[j]);
            }
            user_argv[j] = NULL;
        }
        retval = exec_from_inode(iptr, true, user_argv);
    }
    return retval;
}

static int stat(const char *file, struct stat *st)
{
    struct dnode *dptr = vfs_read_root_dir("/");
    struct inode *iptr = vfs_walk_path(file, dptr);
    struct stat;
    if (iptr != NULL) {
	
    	struct file *fptr = vfs_open_file(iptr, O_RDONLY);
	vfs_stat_file(fptr,st);
	vfs_close_file(fptr);
	return 0;
    }

    return -1;
}

static int chdir(const char *path)
{
    struct dnode *dptr;
    struct inode *iptr;
    
    struct ktask *pid = get_current_process();
    if (path[0] == '/') {
        dptr = vfs_read_root_dir("/");
    } else {
        dptr = vfs_read_inode_dir(pid->cwd);
    }
    //TODO refactor this to iname
    // If we are in the root dir or cwd don't walk path
    if (kstrcmp(path,"/") != 0 && kstrcmp (path,".") != 0 ) {
        iptr = vfs_walk_path(path, dptr);
    }else{
        iptr =kmalloc(sizeof(struct inode));
        vfs_copy_inode(iptr,dptr->root_inode);
        vfs_free_dnode(dptr);
    }

    if (iptr != NULL ) {
        if (iptr->i_type == I_DIR) {
            vfs_free_inode(pid->cwd);
            struct inode *mnt = fs_is_mount_point(iptr);
            if(mnt) {
                kprintf("MOUNT");
                vfs_free_inode(iptr);
                pid->cwd = mnt;
            }else {
                pid->cwd = iptr;
            }
        }else {
            vfs_free_inode(iptr);
            goto error;
        }
        return 0;
    }
    error:
        return -1;
}

static int fstat(int fd, struct stat *st)
{
    struct ktask *pid = get_current_process();
    struct file *fptr = NULL;
    if (pid->open_fds[fd] == NULL) {
    	return -1;
    }
    fptr = pid->open_fds[fd];
    return vfs_stat_file(fptr,st);
}

static int getdents(int fd, struct dirent *dir_arr, uint64_t count)
{
    struct ktask *pid = get_current_process();
    struct file *fptr = NULL;
    if (pid->open_fds[fd] == NULL) {
	    return -1;
    }
    fptr = pid->open_fds[fd];
 
    return vfs_getdents(fptr,dir_arr,count);
}

static int mkdir(const char *pathname, int mode)
{
    struct ktask *pid = get_current_process();
    return vfs_create_dir(pid->cwd,pathname);
}

void *syscall_tbl[] = {
    (void *)&sleep,            // 0
    (void *)&debugprint,       // 1
    (void *)&open,             // 2
    (void *)&close,            // 3
    (void *)&read,             // 4
    (void *)&fork,             // 5
    (void *)&exit,             // 6
    (void *)&wait,             // 7
    (void *)&exec,             // 8
    (void *)&sbrk,             // 9
    (void *)&debug_read_input, // 10
    (void *)&exec_args,        // 11
    (void *)&write,            // 12
    (void *)&creat,            // 13
    (void *)&stat,             // 14
    (void *)&fstat,            // 15
    (void *)&getdents,         // 16
    (void *)&chdir,            // 17
    (void *)&mkdir,            // 18
};

const int NR_syscall = sizeof(syscall_tbl);
