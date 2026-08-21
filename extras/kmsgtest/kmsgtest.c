#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define KMSG_BUFFER_SIZE (64 * 1024 + 1)

static char kmsg[KMSG_BUFFER_SIZE];

int main(void)
{
    static const char message[] = "kmsg-write-test";
    int fd = open("/dev/kmsg", O_WRONLY);
    int count;

    if (fd < 0 || write(fd, message, sizeof(message) - 1) !=
                       (int)sizeof(message) - 1) {
        puts("FAIL kmsg write");
        if (fd >= 0)
            close(fd);
        return 1;
    }
    close(fd);

    fd = open("/dev/kmsg", O_RDONLY);
    if (fd < 0) {
        puts("FAIL kmsg open for read");
        return 1;
    }
    count = read(fd, kmsg, sizeof(kmsg) - 1);
    close(fd);
    if (count <= 0) {
        puts("FAIL kmsg read");
        return 1;
    }
    kmsg[count] = '\0';
    if (strstr(kmsg, "userspace: kmsg-write-test") == NULL) {
        puts("FAIL kmsg userspace record");
        return 1;
    }
    if (strstr(kmsg, "Kyle OS has booted") == NULL) {
        puts("FAIL kmsg boot record");
        return 1;
    }
    puts("PASS kmsg write read records");
    return 0;
}
