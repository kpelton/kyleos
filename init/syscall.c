#include <include/types.h>
#include <output/output.h>
#include <fs/vfs.h>
#include <mm/mm.h>
#include <sched/sched.h>
#include <sched/ps.h>
#include <sched/exec.h>
typedef int (*sys_call)(void);

static int sleep(int msec)
{
    ksleepm(msec);
    return 0;
}

static int open(char *path, uint32_t flags)
{

    int fd = -1;
    if (flags > MAX_FILE_FLAGS)
        goto done;
    struct dnode *dptr = vfs_read_root_dir("/");
    struct inode *iptr = vfs_walk_path(path, dptr, I_FILE);
    struct ktask *pid = get_current_process();
    
    if (iptr != NULL)
    {
        fd = user_process_open_fd(pid, iptr, flags);
        vfs_free_inode(iptr);
    }
done:
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
    return user_process_sbrk(pid,increment);
}

static void exit(int code)
{
    struct ktask *pid = get_current_process();
    user_process_exit(pid,code);
}

static int wait(int pid)
{
    return process_wait(pid);
}

static int debugprint(char *msg)
{
    kprintf("%s",msg);
    //serial_kprintf(msg);
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
//TODO add support for arguments
static int exec(char *path)
{
    int retval = -1;
    struct dnode *dptr = vfs_read_root_dir("/");
    struct inode *iptr = vfs_walk_path(path, dptr, I_FILE);
    if (iptr != NULL)
    {
        retval = exec_from_inode(iptr,true,NULL);
    }
    return retval;
}


static int exec_args(char *path, char *argv[])
{
    int retval = -1;
    struct dnode *dptr = vfs_read_root_dir("/");
    struct inode *iptr = vfs_walk_path(path, dptr, I_FILE);
    if (iptr != NULL)
    {
        retval = exec_from_inode(iptr,true,NULL);
    }
    return retval;
}

void *syscall_tbl[] = {
    (void *)&sleep,      //0
    (void *)&debugprint, //1 
    (void *)&open,       //2
    (void *)&close,      //3
    (void *)&read,       //4
    (void *)&fork,       //5
    (void *)&exit,       //6
    (void *)&wait,       //7
    (void *)&exec,       //8
    (void *)&sbrk,       //9
    (void *)&debug_read_input,       //10
    (void *)&exec_args,       //11
};

const int NR_syscall = sizeof(syscall_tbl);
