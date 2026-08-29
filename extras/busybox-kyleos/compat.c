#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libgen.h>
#include <poll.h>
#include <pwd.h>
#include <stdarg.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define KYLEOS_I_DIR  40000
#define KYLEOS_I_FILE 100000
#define KYLEOS_I_DEV  400000

extern int getdents(int fd, struct dirent *entries, int count);

/* KyleOS currently uses the bytewise C locale. */
int __collate_load_error = 1;

int __collate_range_cmp(int first, int second)
{
    return first - second;
}

static long kyleos_syscall3(long number, long first, long second, long third)
{
    register long result asm("rax") = number;

    asm volatile("int $0x80" : "+a"(result)
                 : "D"(first), "S"(second), "d"(third)
                 : "memory");
    return result;
}

DIR *fdopendir(int fd)
{
    DIR *directory;
    int i;

    if (fd < 0) {
        errno = EBADF;
        return NULL;
    }
    directory = malloc(sizeof(*directory));
    if (directory == NULL) {
        close(fd);
        return NULL;
    }
    directory->fd = fd;
    directory->index = 0;
    directory->count = getdents(fd, directory->entries,
                                sizeof(directory->entries) /
                                sizeof(directory->entries[0]));
    if (directory->count < 0) {
        close(fd);
        free(directory);
        return NULL;
    }
    for (i = 0; i < directory->count; i++) {
        switch (directory->entries[i].d_type) {
        case KYLEOS_I_DIR:
            directory->entries[i].d_type = DT_DIR;
            break;
        case KYLEOS_I_FILE:
            directory->entries[i].d_type = DT_REG;
            break;
        case KYLEOS_I_DEV:
            directory->entries[i].d_type = DT_CHR;
            break;
        default:
            directory->entries[i].d_type = DT_UNKNOWN;
            break;
        }
    }
    return directory;
}

DIR *opendir(const char *path)
{
    return fdopendir(open(path, O_RDONLY));
}

struct dirent *readdir(DIR *directory)
{
    if (directory == NULL || directory->index >= directory->count)
        return NULL;
    return &directory->entries[directory->index++];
}

int closedir(DIR *directory)
{
    int result;

    if (directory == NULL) {
        errno = EBADF;
        return -1;
    }
    result = close(directory->fd);
    free(directory);
    return result;
}

void rewinddir(DIR *directory)
{
    if (directory != NULL)
        directory->index = 0;
}

int dirfd(DIR *directory)
{
    if (directory == NULL) {
        errno = EBADF;
        return -1;
    }
    return directory->fd;
}

static void translate_stat_mode(struct stat *st)
{
    switch (st->st_mode) {
    case KYLEOS_I_DIR:
        st->st_mode = S_IFDIR | 0777;
        break;
    case KYLEOS_I_FILE:
        st->st_mode = S_IFREG | 0777;
        break;
    case KYLEOS_I_DEV:
        st->st_mode = S_IFCHR | 0666;
        break;
    }
}

int kyleos_stat(const char *path, struct stat *st)
{
    int result = (int)kyleos_syscall3(14, (long)path, (long)st, 0);

    if (result < 0) {
        errno = ENOENT;
        return -1;
    }
    translate_stat_mode(st);
    return result;
}

int kyleos_fstat(int fd, struct stat *st)
{
    int result = (int)kyleos_syscall3(15, fd, (long)st, 0);

    if (result < 0) {
        errno = EBADF;
        return -1;
    }
    translate_stat_mode(st);
    return result;
}

int lstat(const char *path, struct stat *st)
{
    return stat(path, st);
}

ssize_t readlink(const char *path, char *buffer, size_t size)
{
    (void)path;
    (void)buffer;
    (void)size;
    /* The current KyleOS filesystems do not implement symbolic links. */
    errno = EINVAL;
    return -1;
}

int mknod(const char *path, mode_t mode, dev_t device)
{
    (void)path;
    (void)mode;
    (void)device;
    errno = ENOSYS;
    return -1;
}

int sched_getaffinity(pid_t pid, size_t size, void *mask)
{
    (void)pid;
    (void)size;
    (void)mask;
    errno = ENOSYS;
    return -1;
}

int tcflush(int fd, int queue)
{
    (void)fd;
    (void)queue;
    return 0;
}

int nanosleep(const struct timespec *request, struct timespec *remaining)
{
    int milliseconds;

    if (request == NULL || request->tv_sec < 0 || request->tv_nsec < 0 ||
        request->tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return -1;
    }
    if (request->tv_sec > 2147483L)
        milliseconds = 2147483647;
    else
        milliseconds = (int)(request->tv_sec * 1000L +
                             (request->tv_nsec + 999999L) / 1000000L);
    if (remaining != NULL) {
        remaining->tv_sec = 0;
        remaining->tv_nsec = 0;
    }
    return poll(NULL, 0, milliseconds);
}

int usleep(useconds_t microseconds)
{
    struct timespec request;

    request.tv_sec = microseconds / 1000000UL;
    request.tv_nsec = (long)(microseconds % 1000000UL) * 1000L;
    return nanosleep(&request, NULL);
}

int ppoll(struct pollfd *descriptors, nfds_t count,
          const struct timespec *timeout, const sigset_t *signal_mask)
{
    int milliseconds = -1;

    (void)signal_mask;
    if (timeout != NULL) {
        if (timeout->tv_sec > 2147483L)
            milliseconds = 2147483647;
        else
            milliseconds = (int)(timeout->tv_sec * 1000L +
                                 (timeout->tv_nsec + 999999L) / 1000000L);
    }
    return poll(descriptors, count, milliseconds);
}

int h_errno;

const char *hstrerror(int error)
{
    (void)error;
    return "network lookup failed";
}

int uname(struct utsname *name)
{
    if (name == NULL) {
        errno = EFAULT;
        return -1;
    }
    strcpy(name->sysname, "KyleOS");
    strcpy(name->nodename, "kyleos");
    strcpy(name->release, "0");
    strcpy(name->version, "KyleOS");
    strcpy(name->machine, "x86_64");
    name->domainname[0] = '\0';
    return 0;
}

int getrlimit(int resource, struct rlimit *limit)
{
    (void)resource;
    if (limit == NULL) {
        errno = EFAULT;
        return -1;
    }
    limit->rlim_cur = RLIM_INFINITY;
    limit->rlim_max = RLIM_INFINITY;
    return 0;
}

int setrlimit(int resource, const struct rlimit *limit)
{
    (void)resource;
    if (limit != NULL && limit->rlim_cur == RLIM_INFINITY &&
        limit->rlim_max == RLIM_INFINITY)
        return 0;
    errno = ENOSYS;
    return -1;
}

int ioctl(int fd, unsigned long request, ...)
{
    va_list arguments;
    struct winsize *size;

    (void)fd;
    if (request != TIOCGWINSZ) {
        errno = ENOTTY;
        return -1;
    }
    va_start(arguments, request);
    size = va_arg(arguments, struct winsize *);
    va_end(arguments);
    if (size == NULL) {
        errno = EFAULT;
        return -1;
    }
    size->ws_row = 25;
    size->ws_col = 80;
    size->ws_xpixel = 0;
    size->ws_ypixel = 0;
    return 0;
}

int fcntl(int fd, int command, ...)
{
    va_list arguments;
    int minimum;

    switch (command) {
    case F_DUPFD:
#ifdef F_DUPFD_CLOEXEC
    case F_DUPFD_CLOEXEC:
#endif
        va_start(arguments, command);
        minimum = va_arg(arguments, int);
        va_end(arguments);
        if (minimum < 0) {
            errno = EINVAL;
            return -1;
        }
        return (int)kyleos_syscall3(20, fd, minimum, 0);
    case F_GETFD:
        return 0;
    case F_SETFD:
        return 0;
    case F_GETFL:
        return O_RDWR;
    case F_SETFL:
        return 0;
    default:
        errno = ENOSYS;
        return -1;
    }
}

char *getcwd(char *buffer, size_t size)
{
    static const char root[] = "/";

    if (buffer == NULL) {
        if (size == 0)
            size = sizeof(root);
        buffer = malloc(size);
        if (buffer == NULL)
            return NULL;
    }
    if (size < sizeof(root)) {
        errno = ERANGE;
        return NULL;
    }
    memcpy(buffer, root, sizeof(root));
    return buffer;
}

mode_t umask(mode_t mask)
{
    static mode_t current = 022;
    mode_t previous = current;

    current = mask & 0777;
    return previous;
}

int chmod(const char *path, mode_t mode)
{
    struct stat status;

    (void)mode;
    return stat(path, &status);
}

int rmdir(const char *path)
{
    return (int)kyleos_syscall3(29, (long)path, 0, 0);
}

int access(const char *path, int mode)
{
    struct stat status;

    (void)mode;
    return stat(path, &status);
}

char *dirname(char *path)
{
    char *end;

    if (path == NULL || path[0] == '\0')
        return ".";
    end = path + strlen(path);
    while (end > path + 1 && end[-1] == '/')
        *--end = '\0';
    while (end > path && end[-1] != '/')
        end--;
    if (end == path)
        return ".";
    while (end > path + 1 && end[-1] == '/')
        end--;
    *end = '\0';
    return path;
}

int vfork(void)
{
    return fork();
}

int sigsuspend(const sigset_t *mask)
{
    sigset_t previous;

    if (sigprocmask(SIG_SETMASK, mask, &previous) < 0)
        return -1;
    poll(NULL, 0, -1);
    sigprocmask(SIG_SETMASK, &previous, NULL);
    errno = EINTR;
    return -1;
}

pid_t getppid(void)
{
    return 1;
}

uid_t getuid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getgid(void) { return 0; }
gid_t getegid(void) { return 0; }

int getgroups(int size, gid_t list[])
{
    (void)list;
    if (size < 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

struct passwd *getpwnam(const char *name)
{
    (void)name;
    return NULL;
}

struct passwd *getpwuid(uid_t uid)
{
    static char name[] = "root";
    static char password[] = "x";
    static struct passwd entry = {
        name, password, 0, 0, name, name, "/", "/bin/sh"
    };

    if (uid != 0)
        return NULL;
    return &entry;
}

struct group *getgrnam(const char *name)
{
    static char group_name[] = "root";
    static char password[] = "x";
    static char *members[] = { NULL };
    static struct group entry = { group_name, password, 0, members };

    if (name == NULL || strcmp(name, group_name) != 0)
        return NULL;
    return &entry;
}

struct group *getgrgid(gid_t gid)
{
    return gid == 0 ? getgrnam("root") : NULL;
}

int execlp(const char *file, const char *arg, ...)
{
    char *argv[32];
    va_list ap;
    int count = 0;

    if (file == NULL || arg == NULL) {
        errno = EINVAL;
        return -1;
    }
    argv[count++] = (char *)arg;
    va_start(ap, arg);
    while (count < (int)(sizeof(argv) / sizeof(argv[0])) - 1) {
        char *value = va_arg(ap, char *);
        if (value == NULL)
            break;
        argv[count++] = value;
    }
    va_end(ap);
    argv[count] = NULL;
    return execvp(file, argv);
}

int chown(const char *path, uid_t owner, gid_t group)
{
    (void)path;
    (void)owner;
    (void)group;
    /* KyleOS currently has no ownership-changing filesystem syscall. */
    return 0;
}

int symlink(const char *target, const char *linkpath)
{
    (void)target;
    (void)linkpath;
    errno = EOPNOTSUPP;
    return -1;
}

int utimes(const char *path, const struct timeval times[2])
{
    (void)path;
    (void)times;
    /* Filesystems currently use their own creation/update timestamps. */
    return 0;
}

long sysconf(int name)
{
    if (name == _SC_CLK_TCK)
        return 100;
    errno = EINVAL;
    return -1;
}

int execvp(const char *file, char *const argv[])
{
    const char *path;
    const char *start;
    const char *end;
    char candidate[512];
    size_t directory_length;
    size_t file_length;

    if (strchr(file, '/') != NULL)
        return execve((char *)file, (char **)argv, environ);
    path = getenv("PATH");
    if (path == NULL || path[0] == '\0')
        path = "/bin:/usr/bin";
    file_length = strlen(file);
    for (start = path; ; start = end + 1) {
        end = strchr(start, ':');
        directory_length = end == NULL ? strlen(start) : (size_t)(end - start);
        if (directory_length + file_length + 2 <= sizeof(candidate)) {
            if (directory_length != 0) {
                memcpy(candidate, start, directory_length);
                candidate[directory_length] = '/';
                memcpy(candidate + directory_length + 1, file, file_length + 1);
            } else {
                memcpy(candidate, file, file_length + 1);
            }
            execve(candidate, (char **)argv, environ);
        }
        if (end == NULL)
            break;
    }
    errno = ENOENT;
    return -1;
}
