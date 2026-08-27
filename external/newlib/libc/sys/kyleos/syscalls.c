#include <sys/errno.h>
#include <sys/fcntl.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <termios.h>
#include <reent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>

/* Keep the syscall helper and the public getdents declaration on the same
 * file-scope dirent tag.  Without this forward declaration, C gives the
 * parameter-list tag prototype scope and treats the two pointers as distinct
 * incompatible types under newer compilers. */
struct dirent;

#define KYLEOS_SYS_GETPID 36
#define KYLEOS_SYS_KILL 37
#define KYLEOS_SYS_SIGACTION 38
#define KYLEOS_SYS_SIGPROCMASK 39
#define KYLEOS_SYS_SIGPENDING 40
#define KYLEOS_SYS_SIGRETURN 41
#define KYLEOS_SYS_ISATTY 42
#define KYLEOS_SYS_OPENPTY 43
#define KYLEOS_SYS_GETPGID 44
#define KYLEOS_SYS_SETPGID 45
#define KYLEOS_SYS_SETSID 46
#define KYLEOS_SYS_TCGETPGRP 47
#define KYLEOS_SYS_TCSETPGRP 48
#define KYLEOS_SYS_TCGETATTR 49
#define KYLEOS_SYS_TCSETATTR 50

struct kyleos_sigaction {
    uintptr_t handler;
    unsigned long mask;
    uintptr_t restorer;
    uint32_t flags;
    uint32_t reserved;
};

extern void __kyleos_signal_return(void);

__asm__(
    ".text\n"
    ".global __kyleos_signal_return\n"
    ".type __kyleos_signal_return, @function\n"
    "__kyleos_signal_return:\n"
    "mov %rsp, %rdi\n"
    "mov $41, %rax\n"
    "int $0x80\n"
    "ud2\n"
    ".size __kyleos_signal_return, .-__kyleos_signal_return\n");

static long sys_signal_call3(long number, long first, long second, long third)
{
    register long result asm("rax") = number;

    asm volatile("int $0x80" : "+a"(result)
                 : "D"(first), "S"(second), "d"(third)
                 : "memory");
    return result;
}

static int sys_sleep(unsigned int msec)
{
    register long result asm("rax") = 0;

    asm volatile("int $0x80" : "+a"(result) : "D"((long)msec)
                 : "memory");
    return (int)result;
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
    register long result asm("rax") = 7;

    asm volatile("int $0x80" : "+a"(result)
                 : "D"((long)pid), "S"(status), "d"((long)options)
                 : "memory");
    return (int)result;
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

static int sys_openpty(int ptyfd[2])
{
    return (int)sys_signal_call3(KYLEOS_SYS_OPENPTY, (long)ptyfd, 0, 0);
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
    return (int)sys_signal_call3(KYLEOS_SYS_GETPID, 0, 0, 0);
};
pid_t getpgid(pid_t pid)
{
    return (pid_t)sys_signal_call3(KYLEOS_SYS_GETPGID, pid, 0, 0);
}
pid_t getpgrp(void)
{
    return getpgid(0);
}
int setpgid(pid_t pid, pid_t process_group)
{
    return (int)sys_signal_call3(KYLEOS_SYS_SETPGID, pid, process_group, 0);
}
pid_t setsid(void)
{
    return (pid_t)sys_signal_call3(KYLEOS_SYS_SETSID, 0, 0, 0);
}
pid_t tcgetpgrp(int fd)
{
    return (pid_t)sys_signal_call3(KYLEOS_SYS_TCGETPGRP, fd, 0, 0);
}
int tcsetpgrp(int fd, pid_t process_group)
{
    return (int)sys_signal_call3(KYLEOS_SYS_TCSETPGRP, fd,
                                 process_group, 0);
}
int tcgetattr(int fd, struct termios *termios_pointer)
{
    return (int)sys_signal_call3(KYLEOS_SYS_TCGETATTR, fd,
                                 (long)termios_pointer, 0);
}
int tcsetattr(int fd, int action, const struct termios *termios_pointer)
{
    return (int)sys_signal_call3(KYLEOS_SYS_TCSETATTR, fd, action,
                                 (long)termios_pointer);
}
int isatty(int file)
{
    long result = sys_signal_call3(KYLEOS_SYS_ISATTY, file, 0, 0);

    if (result < 0) {
        errno = EBADF;
        return 0;
    }
    if (result == 0)
        errno = ENOTTY;
    return result != 0;
};
int kill(int pid, int sig)
{
    return (int)sys_signal_call3(KYLEOS_SYS_KILL, pid, sig, 0);
};

int _raise_r(struct _reent *reent, int signal_number)
{
    int result = kill(getpid(), signal_number);

    if (result < 0 && reent != NULL)
        _REENT_ERRNO(reent) = EINVAL;
    return result;
}

int raise(int signal_number)
{
    return _raise_r(_REENT, signal_number);
}

int sigaction(int signal_number, const struct sigaction *action,
              struct sigaction *old_action)
{
    struct kyleos_sigaction kernel_action;
    struct kyleos_sigaction kernel_old_action;
    struct kyleos_sigaction *kernel_action_pointer = NULL;
    struct kyleos_sigaction *kernel_old_action_pointer = NULL;
    int result;

    if (action != NULL) {
        kernel_action.handler = (uintptr_t)action->sa_handler;
        kernel_action.mask = action->sa_mask;
        kernel_action.restorer = (uintptr_t)__kyleos_signal_return;
        kernel_action.flags = (uint32_t)action->sa_flags;
        kernel_action.reserved = 0;
        kernel_action_pointer = &kernel_action;
    }
    if (old_action != NULL)
        kernel_old_action_pointer = &kernel_old_action;
    result = (int)sys_signal_call3(KYLEOS_SYS_SIGACTION, signal_number,
                                   (long)kernel_action_pointer,
                                   (long)kernel_old_action_pointer);
    if (result == 0 && old_action != NULL) {
        old_action->sa_handler = (_sig_func_ptr)kernel_old_action.handler;
        old_action->sa_mask = kernel_old_action.mask;
        old_action->sa_flags = (int)kernel_old_action.flags;
    }
    return result;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *old_set)
{
    return (int)sys_signal_call3(KYLEOS_SYS_SIGPROCMASK, how,
                                 (long)set, (long)old_set);
}

int sigpending(sigset_t *set)
{
    return (int)sys_signal_call3(KYLEOS_SYS_SIGPENDING, (long)set, 0, 0);
}

_sig_func_ptr _signal_r(struct _reent *reent, int signal_number,
                        _sig_func_ptr handler)
{
    struct sigaction action;
    struct sigaction old_action;

    sigemptyset(&action.sa_mask);
    action.sa_handler = handler;
    action.sa_flags = 0;
    if (sigaction(signal_number, &action, &old_action) < 0) {
        if (reent != NULL)
            _REENT_ERRNO(reent) = EINVAL;
        return SIG_ERR;
    }
    return old_action.sa_handler;
}

_sig_func_ptr signal(int signal_number, _sig_func_ptr handler)
{
    return _signal_r(_REENT, signal_number, handler);
}

int _init_signal_r(struct _reent *reent)
{
    (void)reent;
    return 0;
}

int _init_signal(void)
{
    return 0;
}
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

int openpty(int *master, int *slave, char *name,
            const struct termios *termios_pointer,
            const struct winsize *window_size)
{
    int descriptors[2];

    (void)window_size;
    if (master == NULL || slave == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (sys_openpty(descriptors) < 0) {
        errno = EMFILE;
        return -1;
    }
    if (termios_pointer != NULL &&
        tcsetattr(descriptors[1], TCSANOW, termios_pointer) < 0) {
        close(descriptors[0]);
        close(descriptors[1]);
        errno = EINVAL;
        return -1;
    }
    *master = descriptors[0];
    *slave = descriptors[1];
    if (name != NULL)
        name[0] = '\0';
    return 0;
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
