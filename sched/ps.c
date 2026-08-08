#include <sched/ps.h>
#include <output/output.h>
#include <mm/paging.h>
#include <mm/pmem.h>
#include <mm/mm.h>
#define START_USER_FD 3

int user_process_open_fd(struct ktask *t, struct inode *iptr, uint32_t flags)
{
    int fd = -1;
    struct file *fptr = vfs_open_file(iptr, flags);
    if (fptr == NULL)
    {
        kprintf("fail in user_process_open_fd\n");
        goto done;
    }
    if ((flags & O_TRUNC) != 0 && vfs_truncate_file(fptr) < 0) {
        vfs_close_file(fptr);
        return -1;
    }
    for (int j = START_USER_FD; j < MAX_TASK_OPEN_FILES; j++)
    {

        if (t->open_fds[j] == NULL)
        {
            t->open_fds[j] = fptr;
            fd = j;
            return fd;
        }
    }
done:
    return -1;
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

int user_process_write_fd(struct ktask *t, int fd, void *buf, int count)
{
    if (fd >= 0 && fd < MAX_TASK_OPEN_FILES && t->open_fds[fd] != NULL)
    {
        return vfs_write_file(t->open_fds[fd], buf, count);
    }
    return -1;
}

int user_process_seek_fd(struct ktask *t, int fd, long offset, int whence)
{
    if (fd >= 0 && fd < MAX_TASK_OPEN_FILES && t->open_fds[fd] != NULL)
        return vfs_seek_file(t->open_fds[fd], offset, whence);
    return -1;
}

int user_process_pipe(struct ktask *t, int pipefd[2])
{
    struct file *reader;
    struct file *writer;
    int readfd = -1;
    int writefd = -1;

    if (t == NULL || pipefd == NULL || vfs_create_pipe(&reader, &writer) < 0)
        return -1;
    for (int i = START_USER_FD; i < MAX_TASK_OPEN_FILES; i++)
        if (t->open_fds[i] == NULL) {
            readfd = i;
            break;
        }
    for (int i = readfd + 1; i < MAX_TASK_OPEN_FILES; i++)
        if (t->open_fds[i] == NULL) {
            writefd = i;
            break;
        }
    if (readfd < 0 || writefd < 0) {
        vfs_close_file(reader);
        vfs_close_file(writer);
        return -1;
    }
    t->open_fds[readfd] = reader;
    t->open_fds[writefd] = writer;
    pipefd[0] = readfd;
    pipefd[1] = writefd;
    return 0;
}

void user_process_exit(struct ktask *t, int code)
{
    //kprintf("Exit called");
    t->exit_code = code;
    sched_process_kill(t->pid,false,true);
    schedule();
}

void *user_process_sbrk(struct ktask *t, uint64_t increment) 
{
    uint64_t old_break = (uint64_t)t->user_heap_loc;
    uint64_t new_break;
    uint64_t required_pages;
#ifdef DEBUG_SYSCALL_SBRK
    kprintf("Sbrk called with %x %x\n", increment,t->user_heap_loc);
#endif
    if (increment == 0)
        return (void *)old_break;
    new_break = old_break + increment;
    if (new_break < old_break)
        return (void *)-1;
    required_pages = (new_break - (uint64_t)t->user_start_heap +
                      PAGE_SIZE - 1) / PAGE_SIZE;
    if (required_pages > t->heap_size) {
        uint64_t pages = required_pages - t->heap_size;
        uint64_t start = (uint64_t)t->user_start_heap +
                         t->heap_size * PAGE_SIZE;
        if (vmm_reserve_mapping(t->mm, VMM_DATA, (uint64_t *)start,
                                pages, READ_WRITE | SUPERVISOR) == NULL)
            return (void *)-1;
        t->heap_size = required_pages;
    }
    t->user_heap_loc = (uint64_t *)new_break;
    return (void *)old_break;
}

int process_wait(int pid, int *status, int options)
{
    struct ktask *current = get_current_process();

    /* KyleOS currently implements only the POSIX WNOHANG option. */
    if (pid == 0 || pid < -1 || (options & ~1) != 0)
        return -1;
    while (1) {
        int exit_code = 0;
        int result = (options & 1)
                   ? sched_reap_child(current->pid, pid, &exit_code)
                   : sched_wait_child(current->pid, pid, &exit_code);

        if (result != 0)
            {
                if (result > 0 && status != NULL)
                    *status = (exit_code & 0xff) << 8;
                return result;
            }
        if (options & 1)
            return 0;
    }
}
