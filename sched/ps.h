#ifndef _PS_H
#define _PS_H
#include <include/types.h>
#include <sched/sched.h>
#include <fs/vfs.h>
#include <fs/vfs.h>
int user_process_open_fd(struct ktask *t, struct inode *iptr,uint32_t flags);
int user_process_close_fd(struct ktask *t,int fd);
int user_process_read_fd(struct ktask *t,int fd, void *buf, int count);
void user_process_exit(struct ktask *t, int code);
void *user_process_sbrk(struct ktask *t, uint64_t increment);

int process_wait(int pid);
#endif