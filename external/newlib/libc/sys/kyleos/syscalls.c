#include <sys/errno.h>
#include <sys/fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>

/* Keep the syscall helper and the public getdents declaration on the same
 * file-scope dirent tag.  Without this forward declaration, C gives the
 * parameter-list tag prototype scope and treats the two pointers as distinct
 * incompatible types under newer compilers. */
struct dirent;

static int sys_sleep(unsigned int msec)
{
    long val = 0;
    (void)msec;
    asm volatile("mov $0, %%rax; int $0x80\n nop\n; nop;nop;nop;movq %%rax ,%0" : "=g"(val));
    return val;
}

static int sys_gettimeofday(struct timeval *tv)
{
    long val = 0;
    (void)tv;
    asm volatile("mov $34, %%rax; int $0x80; movq %%rax, %0" : "=g"(val));
    return val;
}

static int sys_poll(struct pollfd *fds, nfds_t count, int timeout)
{
    register long result asm("rax") = 35;

    asm volatile("int $0x80" : "+a"(result)
                 : "D"(fds), "S"((long)count), "d"((long)timeout)
                 : "memory");
    return (int)result;
}

static int sys_stat(const char *path, struct stat *statbuf)
{
    long val = 0;
    (void)path;
    (void)statbuf;
    asm volatile("mov $14, %%rax; int $0x80;movq %%rax ,%0" : "=g"(val));
    return val;
}

static int sys_mkdir(const char *path, int mode)
{
    long val = 0;
    (void)path;
    (void)mode;
    asm volatile("mov $18, %%rax; int $0x80;movq %%rax ,%0" : "=g"(val));
    return val;
}


static int sys_fstat(int fd, struct stat *statbuf)
{
    long val = 0;
    (void)fd;
    (void)statbuf;
    asm volatile("mov $15, %%rax; int $0x80;movq %%rax ,%0" : "=g"(val));
    return val;
}
static int sys_getdents(int fd, struct dirent *dir,int count)
{
    long val = 0;
    (void)fd;
    (void)dir;
    (void)count;
    asm volatile("mov $16, %%rax; int $0x80;movq %%rax ,%0" : "=g"(val));
    return val;
}

static int sys_chdir(const char *path)
{
    long val = 0;
    (void)path;
    asm volatile("mov $17, %%rax; int $0x80;movq %%rax ,%0" : "=g"(val));
    return val;
}

static int sys_print(char *msg)
{
    long val = 0;
    (void)msg;
    asm volatile("mov $1, %%rax; int $0x80\n nop; nop;nop;movq %%rax ,%0" : "=g"(val));
    return val;
}

static int sys_open(const char *path, int flags)
{
    long val = 0;
    (void)path;
    (void)flags;
    asm volatile("mov $2, %%rax; int $0x80\n movq %%rax ,%0" : "=g"(val));
    return val;
}

static int sys_close(int fd)
{
    long val = 0;
    (void)fd;
    asm volatile("mov $3, %%rax; int $0x80\n nop;nop;nop;nop;nop;movq %%rax ,%0" : "=g"(val));
    return val;
}

static int sys_read(int fd, void *buf, int count)
{
    long val = 0;
    (void)fd;
    (void)count;
    (void)buf;
    asm volatile("mov $4, %%rax; int $0x80\n nop;nop;nop;nop;movq %%rax ,%0" : "=g"(val));
    return val;
}

static int sys_write(int fd, void *buf, int count)
{
    long val = 0;
    (void)fd;
    (void)count;
    (void)buf;
    asm volatile("mov $12, %%rax; int $0x80\n nop;nop;nop;nop;movq %%rax ,%0" : "=g"(val));
    return val;
}

static int sys_fork()
{
    long val = 0;
    asm volatile("mov $5, %%rax; int $0x80\n movq %%rax ,%0" : "=g"(val));
    return val;
}

static void sys_exit(int code)
{
    (void)code;
    long val = 0;
    asm volatile("mov $6, %%rax; int $0x80\n movq %%rax ,%0" : "=g"(val));
}

static int sys_wait(int pid, int *status, int options)
{
    long val = 0;
    (void)pid;
    (void)status;
    (void)options;
    asm volatile("mov $7, %%rax; int $0x80\n movq %%rax ,%0" : "=g"(val));
    return val;
}

static int sys_exec(char *path, char **argv)
{
    long val = 0;
    (void)path;
    (void)argv;
    asm volatile("mov $11, %%rax; int $0x80\n movq %%rax ,%0" : "=g"(val));
    return val;
}

static int sys_dup2(int oldfd, int newfd)
{
    long val = 0;
    (void)oldfd;
    (void)newfd;
    asm volatile("mov $20, %%rax; int $0x80\n movq %%rax ,%0" : "=g"(val));
    return val;
}


static int sys_lseek(int fd, long offset, int whence)
{
    long val = 0;
    (void)fd;
    (void)offset;
    (void)whence;
    asm volatile("mov $22, %%rax; int $0x80; movq %%rax, %0"
                 : "=g"(val));
    return (int)val;
}

static int sys_pipe(int pipefd[2])
{
    long val = 0;
    (void)pipefd;
    asm volatile("mov $26, %%rax; int $0x80; movq %%rax, %0"
                 : "=g"(val));
    return (int)val;
}

static int sys_unlink(char *path)
{
    long val = 0;
    (void)path;
    asm volatile("mov $21, %%rax; int $0x80; movq %%rax, %0" : "=g"(val));
    return val;
}

static int sys_rename(const char *old_path, const char *new_path)
{
    long val = 0;
    (void)old_path;
    (void)new_path;
    asm volatile("mov $28, %%rax; int $0x80; movq %%rax, %0" : "=g"(val));
    return val;
}

static void sys_read_debug_input(char *dst)
{
    long val = 0;
    (void)dst;
    asm volatile("mov $10, %%rax; int $0x80\n movq %%rax ,%0" : "=g"(val));
}

static void *sys_sbrk(int increment)
{
    long val = 0;
    (void)increment;
    asm volatile("mov $9, %%rax; int $0x80\n movq %%rax ,%0" : "=g"(val));
    return (void *)val;
}

void _exit(int status)
{
    sys_exit(status);
}
char **environ; /* pointer to array of char * strings that define the current environment variables */
int execve(char *name, char **argv, char **env)
{
    return sys_exec(name, argv);
};
int fork()
{
    return sys_fork();
}

int fstat(int file, struct stat *st)
{
    return sys_fstat(file,st);
};
int getpid()
{
    for (;;)
        ;
    return 0;
};
int isatty(int file)
{
    for (;;)
        ;
    return 0;
};
int kill(int pid, int sig)
{
    for (;;)
        ;
    return 0;
};
int link(char *old, char *new)
{
    for (;;)
        ;
    return 0;
};
int lseek(int file, int ptr, int dir)
{
    return sys_lseek(file, ptr, dir);
};
caddr_t sbrk(int incr)
{
    return sys_sbrk(incr);
};
int stat(const char *file, struct stat *st)
{
    return sys_stat(file,st);
};
clock_t times(struct tms *buf)
{
    for (;;)
        ;
    return 0;
};
int unlink(char *name)
{
    return sys_unlink(name);
};
int _rename(const char *old_path, const char *new_path)
{
    return sys_rename(old_path, new_path);
}

int wait(int *status)
{
    return sys_wait(-1, status, 0);
};

int waitpid(int pid, int *status, int options)
{
    return sys_wait(pid, status, options);
}
int sleep(int sec)
{
    return sys_sleep(sec * 1000);
};


int read(int file, char *ptr, int len)
{
    return sys_read(file, ptr, len);
}

int close(int file)
{
    return sys_close(file);
}

int open(const char *name, int flags, ...)
{
    return sys_open(name, flags);
}

int chdir(const char *path)
{
    return sys_chdir(path);
}

int dup2(int oldfd, int newfd)
{
    return sys_dup2(oldfd,newfd);
}

int pipe(int pipefd[2])
{
    return sys_pipe(pipefd);
}

int poll(struct pollfd fds[], nfds_t count, int timeout)
{
    return sys_poll(fds, count, timeout);
}

int mkdir(const char *_path,mode_t __mode)
{
    return sys_mkdir(_path,__mode);
}

int getdents(int fd, struct dirent *dir,int count)
{ 
    return sys_getdents(fd,dir,count);
}
int write(int file, char *ptr, int len)
{
    return sys_write(file, ptr, len);
}

int gettimeofday(struct timeval *__restrict __p, void *__restrict __tz)
{
    (void)__tz;
    return sys_gettimeofday(__p);
}
