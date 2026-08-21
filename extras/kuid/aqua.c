#include <unistd.h>

int main(void)
{
    char *argv[] = { "kuid", "/bin/aqua-shell", 0 };

    execve("/bin/kuid", argv, 0);
    return 127;
}
