#ifndef _PS_H
#define _PS_H
#include <include/types.h>
#include <sched/sched.h>
#include <fs/vfs.h>
#include <fs/vfs.h>
int user_process_open_fd(struct ktask *t, struct inode *iptr,uint32_t flags);
int user_process_close_fd(struct ktask *t,int fd);
int user_process_read_fd(struct ktask *t,int fd, void *buf, int count);
int user_process_write_fd(struct ktask *t, int fd, void *buf, int count);
int user_process_seek_fd(struct ktask *t, int fd, long offset, int whence);
int user_process_fd_status(struct ktask *t, int fd, uint32_t *readable,
                           uint32_t *writable, uint32_t *hung_up);
int user_process_isatty(struct ktask *t, int fd);
int user_process_tcgetpgrp(struct ktask *t, int fd);
int user_process_tcsetpgrp(struct ktask *t, int fd, int process_group);
int user_process_tcgetattr(struct ktask *t, int fd,
                           struct kyleos_termios *termios);
int user_process_tcsetattr(struct ktask *t, int fd, int action,
                           const struct kyleos_termios *termios);
int user_process_poll(struct ktask *t, struct kyleos_pollfd *fds,
                      uint32_t count, int timeout_ms);
int user_process_pipe(struct ktask *t, int pipefd[2]);
int user_process_openpty(struct ktask *t, int ptyfd[2]);
void user_process_exit(struct ktask *t, int code);
void *user_process_sbrk(struct ktask *t, uint64_t increment);
int process_wait(int pid, int *status, int options);
#endif
