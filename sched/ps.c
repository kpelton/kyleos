#include <sched/ps.h>
#include <output/output.h>

int user_process_open_fd(struct ktask *t, struct inode *iptr)
{
	int fd = -1;
	struct file *fptr=vfs_open_file(iptr);
	for (int j; j<MAX_TASK_OPEN_FILES; j++) {

		if (t->open_fds[j] == NULL) {
			t->open_fds[j] = fptr;
			fd = j;
			return fd;
		}

	}
	kprintf("Returning %d\n",fd);
	return fd;
}

int user_process_close_fd(struct ktask *t,int fd)
{
	if(fd >= 0 && fd < MAX_TASK_OPEN_FILES && t->open_fds[fd] != NULL){
		vfs_close_file(t->open_fds[fd]);
		t->open_fds[fd] = NULL;
		return 0;
	}
	return -1;
}

int user_process_read_fd(struct ktask *t,int fd, void *buf, int count)
{
	kprintf("buffer loc = %x\n",buf);
	if(fd >= 0 && fd < MAX_TASK_OPEN_FILES && t->open_fds[fd] != NULL){
		return vfs_read_file(t->open_fds[fd],buf,count);
	}
	return -1;
}

