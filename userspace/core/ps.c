#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    char buffer[1024];
    int fd;
    int count;

    (void)argv;
    if (argc != 1) {
        fprintf(stderr, "Usage: ps\n");
        return 2;
    }
    fd = open("/proc/processes", O_RDONLY);
    if (fd < 0) {
        perror("ps: /proc/processes");
        return 1;
    }
    while ((count = read(fd, buffer, sizeof(buffer))) > 0)
        if (write(STDOUT_FILENO, buffer, count) != count) {
            perror("ps: write");
            close(fd);
            return 1;
        }
    close(fd);
    if (count < 0) {
        perror("ps: read");
        return 1;
    }
    return 0;
}
