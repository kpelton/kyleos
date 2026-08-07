#include <fs/vfs.h>
#include <include/types.h>
#include <mm/mm.h>
#include <output/output.h>
#include <output/input.h>
#include <sched/exec.h>
#include <sched/ps.h>
#include <sched/sched.h>
#include <timer/pit.h>
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
    struct dnode *dptr = vfs_read_root_dir(ROOT);
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
#ifdef DEBUG_SYSCALL
    kprintf("open %s\n", path);
#endif
    struct dnode *dptr;
    struct ktask *pid = get_current_process();
    struct inode *iptr = NULL;
    int pathlen = kstrlen(path);
    char *s_basepath = (char *)kmalloc(pathlen + 1);
    char *s_dirname = (char *)kmalloc(pathlen + 1);
    kstrncpy(s_basepath, path, pathlen + 1);
    kstrncpy(s_dirname, path, pathlen + 1);
    char *last_dir = dirname(s_dirname);
    bool walk = false;


    if (path[0] == '/') {
        dptr = vfs_read_root_dir(ROOT);
    } else {
        dptr = vfs_read_inode_dir(pid->cwd);
    }

    // If we are in the root dir or cwd don't walk path
    if (kstrcmp(path,ROOT) != 0 && kstrcmp (path,".") != 0) {
        iptr = vfs_walk_path(path, dptr);
        walk = true;
    }else{
        iptr = dptr->root_inode;
    }
#ifdef DEBUG_SYSCALL
    kprintf("iptr val %x\n", iptr);
#endif
    if (iptr != NULL)
    {
        fd = user_process_open_fd(pid, iptr, flags);
        if (walk)
            vfs_free_inode(iptr);
        else
            vfs_free_dnode(dptr);
    }
    else if ((flags & O_WRONLY) == O_WRONLY)
    {
           // get a directory entry in order to walk
        if (path[0] == DELIM) {
            dptr = vfs_read_root_dir(ROOT);
        } else {
            dptr = vfs_read_inode_dir(pid->cwd);
        }
#ifdef DEBUG_SYSCALL
        kprintf("last_dir%s\n", last_dir);
#endif
        // get parent node
        if (kstrcmp(path,ROOT) != 0 && kstrcmp (path,".") != 0 && kstrcmp(last_dir,".") != 0 ) {
            iptr = vfs_walk_path(last_dir, dptr);
        }else{
            iptr =kmalloc(sizeof(struct inode));
            vfs_copy_inode(iptr,dptr->root_inode);
            vfs_free_dnode(dptr);
        }

#ifdef DEBUG_SYSCALL
        kprintf("iptr2 val %x\n", iptr);
#endif
        // if this  is a valid directory
        if (iptr && iptr->i_type == I_DIR )
        {

            iptr = vfs_create_file(iptr, vfs_strip_path(path), flags);
            if (iptr)
            {
                fd = user_process_open_fd(pid, iptr, flags);
                vfs_free_inode(iptr);
            }
        }
    }
#ifdef DEBUG_SYSCALL
    kprintf("Returning %d for path %s\n", fd, path);
#endif
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

static int lseek(int fd, long offset, int whence)
{
    struct ktask *pid = get_current_process();
    return user_process_seek_fd(pid, fd, offset, whence);
}

static int pipe(int pipefd[2])
{
    struct ktask *pid = get_current_process();
    if (pipefd == NULL || pipefd >= (int *)KERN_SPACE_BOUNDRY)
        return -1;
    return user_process_pipe(pid, pipefd);
}

static int framebuffer_present_syscall(void *pixels, uint32_t bytes)
{
    if (pixels == NULL || pixels >= (void *)KERN_SPACE_BOUNDRY)
        return -1;
    return framebuffer_present(pixels, bytes);
}

static int key_poll_syscall(int *pressed, uint8_t *scancode, bool *extended)
{
    if (pressed == NULL || scancode == NULL || extended == NULL ||
        pressed >= (int *)KERN_SPACE_BOUNDRY ||
        scancode >= (uint8_t *)KERN_SPACE_BOUNDRY ||
        extended >= (bool *)KERN_SPACE_BOUNDRY)
        return -1;
    return input_poll_key(pressed, scancode, extended);
}

static int tty_raw_syscall(int enabled)
{
    input_set_raw(enabled != 0);
    return 0;
}

static uint32_t ticks_ms_syscall(void)
{
    return (read_jiffies() * 1000) / TICK_HZ;
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
    struct dnode *dptr = vfs_read_root_dir(ROOT);
    struct inode *iptr = vfs_walk_path(path, dptr);
    if (iptr != NULL && iptr->i_type == I_FILE)
    {
        retval = exec_from_inode(iptr, true, NULL);
        vfs_free_inode(iptr);
    }

    return retval;
}

static int exec_args(char *path, char *argv[])
{
    int retval = -1;
    if (!path)
        return -1;
    struct dnode *dptr = vfs_read_root_dir(ROOT);
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
        vfs_free_inode(iptr);
    }
    return retval;
}

static int stat(const char *file, struct stat *st)
{
    struct dnode *dptr = vfs_read_root_dir(ROOT);
    struct inode *iptr = vfs_walk_path(file, dptr);
    if (iptr != NULL) {
	
    	struct file *fptr = vfs_open_file(iptr, O_RDONLY);
	    vfs_stat_file(fptr,st);
	    vfs_close_file(fptr);
        vfs_free_inode(iptr);
	    return 0;
    }

    return -1;
}

static int chdir(const char *path)
{
    struct dnode *dptr;
    struct inode *iptr=NULL;
    struct inode *iptr2;
    bool walk=false; 
    struct ktask *pid = get_current_process();
    if (path[0] == DELIM) {
        dptr = vfs_read_root_dir(ROOT);
    } else {
        dptr = vfs_read_inode_dir(pid->cwd);
    }
    //TODO refactor this to iname
    // If we are in the root dir or cwd don't walk path
    if (kstrcmp(path,ROOT) != 0 && kstrcmp (path,".") != 0 ) {
        iptr2 = vfs_walk_path(path, dptr);
        walk = true;
        if (!iptr2) {
            goto error;
        }
        iptr = kmalloc(sizeof(struct inode));
        vfs_copy_inode(iptr,iptr2);
        vfs_free_inode(iptr2);
    }else{
        iptr = kmalloc(sizeof(struct inode));
        vfs_copy_inode(iptr,dptr->root_inode);
    }


    if (iptr != NULL ) {
        if (iptr->i_type == I_DIR) {
            vfs_free_inode(pid->cwd);
            pid->cwd = iptr;
        }else {
            vfs_free_inode(iptr);
            goto error;
        }
        if (!walk)
            vfs_free_dnode(dptr);
        return 0;
    }
    error:
      if (!walk)
            vfs_free_dnode(dptr);
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
    char *path;
    char *name;
    int len;
    (void)mode;

    if (pathname == NULL)
        return -1;
    len = kstrlen((char *)pathname);
    if (len == 0)
        return -1;
    path = kmalloc(len + 1);
    if (path == NULL)
        return -1;
    kstrncpy(path, pathname, len + 1);
    name = basename(path); /* also removes a trailing slash */
    if (name[0] == '\0') {
        kfree(path);
        return -1;
    }

    /* Doom creates ./savegame/.  Its parent is the current working
     * directory; passing the full path to FAT used to create a malformed
     * single directory entry instead. */
    int result = vfs_create_dir(pid->cwd, name);
    kfree(path);
    return result;
}

static int unlink(char *path)
{
    struct ktask *pid = get_current_process();
    struct dnode *dptr;
    struct inode *iptr;
    int retval;

    if (path == NULL || kstrcmp(path, ROOT) == 0 || kstrcmp(path, ".") == 0 ||
        kstrcmp(path, "..") == 0)
        return -1;
    dptr = path[0] == DELIM ? vfs_read_root_dir(ROOT) : vfs_read_inode_dir(pid->cwd);
    if (dptr == NULL)
        return -1;
    iptr = vfs_walk_path(path, dptr);
    if (iptr == NULL)
        return -1;
    retval = vfs_unlink(iptr);
    vfs_free_inode(iptr);
    return retval;
}

static int rename(char *old_path, char *new_path)
{
    struct ktask *pid = get_current_process();
    struct dnode *old_dir;
    struct dnode *new_dir;
    struct inode *source;
    struct inode *parent;
    char *new_copy;
    char *name_copy;
    char *parent_path;
    char *new_name;
    int length;
    int result;

    if (old_path == NULL || new_path == NULL || old_path[0] == '\0' ||
        new_path[0] == '\0' || kstrcmp(old_path, ROOT) == 0 ||
        kstrcmp(old_path, ".") == 0 || kstrcmp(old_path, "..") == 0)
        return -1;
    old_dir = old_path[0] == DELIM ? vfs_read_root_dir(ROOT) :
                                     vfs_read_inode_dir(pid->cwd);
    if (old_dir == NULL)
        return -1;
    source = vfs_walk_path(old_path, old_dir);
    if (kstrstr(old_path, ROOT) < 0)
        vfs_free_dnode(old_dir);
    if (source == NULL)
        return -1;

    length = kstrlen(new_path);
    new_copy = kmalloc(length + 1);
    name_copy = kmalloc(length + 1);
    if (new_copy == NULL || name_copy == NULL) {
        if (new_copy) kfree(new_copy);
        if (name_copy) kfree(name_copy);
        vfs_free_inode(source);
        return -1;
    }
    kstrncpy(new_copy, new_path, length + 1);
    kstrncpy(name_copy, new_path, length + 1);
    new_name = basename(name_copy);
    parent_path = dirname(new_copy);
    if (new_name[0] == '\0' || kstrcmp(new_name, ".") == 0 ||
        kstrcmp(new_name, "..") == 0) {
        kfree(new_copy);
        kfree(name_copy);
        vfs_free_inode(source);
        return -1;
    }
    new_dir = new_path[0] == DELIM ? vfs_read_root_dir(ROOT) :
                                     vfs_read_inode_dir(pid->cwd);
    if (new_dir == NULL) {
        kfree(new_copy);
        kfree(name_copy);
        vfs_free_inode(source);
        return -1;
    }
    if (kstrcmp(parent_path, ".") == 0) {
        parent = kmalloc(sizeof(struct inode));
        if (parent != NULL)
            vfs_copy_inode(parent, new_dir->root_inode);
        vfs_free_dnode(new_dir);
    } else {
        parent = vfs_walk_path(parent_path, new_dir);
        if (kstrstr(parent_path, ROOT) < 0)
            vfs_free_dnode(new_dir);
    }
    if (parent == NULL) {
        kfree(new_copy);
        kfree(name_copy);
        vfs_free_inode(source);
        return -1;
    }
    result = vfs_rename(source, parent, new_name);
    vfs_free_inode(parent);
    vfs_free_inode(source);
    kfree(new_copy);
    kfree(name_copy);
    return result;
}

static int dup(const int oldfd)
{
#ifdef DEBUG_SYSCALL
    kprintf("DUP\n");
#endif
    //TODO broken
    return oldfd;
}

static int dup2(const int oldfd,const int newfd)
{

#ifdef DEBUG_SYSCALL
    kprintf("DUP2 old:%d newfd:%d\n", oldfd, newfd);
#endif
    if (oldfd >= MAX_TASK_OPEN_FILES || oldfd < 0)
        return -1;

    if (newfd >= MAX_TASK_OPEN_FILES || newfd < 0)
        return -1;
    
    // See if oldfd is really open
    struct ktask *t = get_current_process();
    if (t->open_fds[oldfd] == NULL) {
        kprintf("ERROR dup2 oldfd is not open\n");
	    return -1;
    }

    // If newfd already open close it first
    if (t->open_fds[newfd] != NULL) {
        //TODO error handling on vfs close
        vfs_close_file(t->open_fds[newfd]);
        t->open_fds[newfd] = NULL;
    }
    
    // Now dup the fd
    //TODO lock fd    
    t->open_fds[newfd] = t->open_fds[oldfd];
    t->open_fds[newfd]->refcount++;
#ifdef DEBUG_SYSCALL
    kprintf("%x newfd\n", newfd);
#endif
    return newfd;
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
    (void *)&dup,              // 19
    (void *)&dup2,             // 20
    (void *)&unlink,           // 21
    (void *)&lseek,            // 22
    (void *)&framebuffer_present_syscall, // 23
    (void *)&key_poll_syscall,            // 24
    (void *)&ticks_ms_syscall,            // 25
    (void *)&pipe,                        // 26
    (void *)&tty_raw_syscall,             // 27
    (void *)&rename                       // 28
};

const int NR_syscall = sizeof(syscall_tbl);
