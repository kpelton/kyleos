#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int check_terminal(const char *path)
{
    int fd = open(path, O_RDWR);

    if (fd < 0) {
        printf("ttytest: unable to open %s\n", path);
        return 1;
    }
    if (!isatty(fd)) {
        printf("ttytest: %s is not reported as a terminal\n", path);
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}

int main(void)
{
    static const char *terminals[] = {
        "/dev/console", "/dev/tty0", "/dev/tty1",
        "/dev/tty2", "/dev/tty3", "/dev/tty4", "/dev/ttyS0",
    };
    int null_fd;
    int pipe_fds[2];

    if (!isatty(STDOUT_FILENO)) {
        puts("ttytest: stdout is not a terminal");
        return 1;
    }
    for (unsigned int index = 0;
         index < sizeof(terminals) / sizeof(terminals[0]); index++)
        if (check_terminal(terminals[index]) != 0)
            return 1;

    null_fd = open("/dev/null", O_RDWR);
    if (null_fd < 0 || isatty(null_fd)) {
        puts("ttytest: /dev/null incorrectly reported as a terminal");
        return 1;
    }
    close(null_fd);
    if (pipe(pipe_fds) < 0 || isatty(pipe_fds[0]) || isatty(pipe_fds[1])) {
        puts("ttytest: pipe incorrectly reported as a terminal");
        return 1;
    }
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    puts("ttytest: console and serial terminal devices pass");
    return 0;
}
