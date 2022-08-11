#include <sched/ps.h>
#include <output/output.h>

int user_process_open_fd(struct ktask *t, struct inode *iptr, uint32_t flags)
{
    int fd = -1;
    struct file *fptr = vfs_open_file(iptr, flags);
    if (fptr == NULL)
    {
        kprintf("fail in user_process_open_fd\n");
        goto done;
    }
    for (int j; j < MAX_TASK_OPEN_FILES; j++)
    {

        if (t->open_fds[j] == NULL)
        {
            t->open_fds[j] = fptr;
            fd = j;
            return fd;
        }
    }
done:
    kprintf("Returning %d\n", fd);
    return fd;
}

int user_process_close_fd(struct ktask *t, int fd)
{
    if (fd >= 0 && fd < MAX_TASK_OPEN_FILES && t->open_fds[fd] != NULL)
    {
        vfs_close_file(t->open_fds[fd]);
        t->open_fds[fd] = NULL;
        return 0;
    }
    return -1;
}

int user_process_read_fd(struct ktask *t, int fd, void *buf, int count)
{
    if (fd >= 0 && fd < MAX_TASK_OPEN_FILES && t->open_fds[fd] != NULL)
    {
        return vfs_read_file(t->open_fds[fd], buf, count);
    }
    return -1;
}

void user_process_exit(struct ktask *t, int code)
{
    t->exit_code = code;
    sched_process_kill(t->pid,false);
    schedule();
}

int user_process_wait(struct ktask *t, int pid)
{
    struct ktask *child;
    while(1) {

        child = sched_get_process(pid);
        if (child == NULL)
            return -1;
        if(child->state == TASK_DONE){
            //TODO: Move this to sched.c
            //Reap child
            child->pid = -1;
            return child->exit_code;
        }
        //try again in 100 ms
        ksleepm(100);
    }
    return 0;
}