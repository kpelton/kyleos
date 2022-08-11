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
                                kprintf("         refcount -> %d\n", t->open_fds[j]->refcount);

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
                                        kprintf("         refcount -> %d\n", t->open_fds[fd]->refcount);

        return vfs_read_file(t->open_fds[fd], buf, count);
    }
    return -1;
}
