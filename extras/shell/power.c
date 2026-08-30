#include <include/uapi/syscall.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *name = argv != NULL ? argv[0] : NULL;
    (void)argc;
    if (name != NULL) {
        const char *slash = strrchr(name, '/');
        if (slash != NULL) name = slash + 1;
    }
    int result = name != NULL && strcmp(name, "reboot") == 0 ?
                 kyleos_reboot() : kyleos_poweroff();
    if (result < 0) {
        fprintf(stderr, "%s: poweroff unavailable or permission denied\n",
                argv != NULL && argv[0] != NULL ? argv[0] : "poweroff");
        return 1;
    }
    return 0;
}
