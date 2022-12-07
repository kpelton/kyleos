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
    panic("unable to open file");
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

void user_process_exit(struct ktask *t, int code)
{
    kprintf("Exit called");
    t->exit_code = code;
    sched_process_kill(t->pid,false);
    schedule();
}

void *user_process_sbrk(struct ktask *t, uint64_t increment) 
{
    void *ret = NULL;
    uint64_t *newblock;
    uint64_t inc;
    //kprintf("Sbrk called with %x %x\n", increment,t->user_heap_loc);

    if (increment == 0){
        ret = t->user_heap_loc;

    }else if (increment > 0) {
        //return previous break
        ret = t->user_heap_loc;
        inc = ((uint64_t) t->user_heap_loc) + increment;
        t->user_heap_loc = (uint64_t *) inc;
    }
    else{
        panic("can't handle negative sbrk");
    }
    //kprintf("%x %x\n",(uint64_t)t->user_start_heap + t->heap_size*PAGE_SIZE,t->user_heap_loc);
    if((uint64_t)t->user_heap_loc >= (uint64_t)t->user_start_heap + t->heap_size*PAGE_SIZE){
        uint64_t delta = (uint64_t)t->user_heap_loc - ((uint64_t)t->user_start_heap + t->heap_size*PAGE_SIZE);
        uint64_t end_heap = ((uint64_t)t->user_start_heap + t->heap_size*PAGE_SIZE);
        uint64_t pages = delta/PAGE_SIZE + 1;

        vmm_add_new_mapping(t->mm,VMM_DATA,(uint64_t *)end_heap,pages,USER_PAGE,false);
        t->heap_size += pages; 
    }
    //kprintf("sbrk returning %x\n",ret);
    return ret;
}

int process_wait(int pid)
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
