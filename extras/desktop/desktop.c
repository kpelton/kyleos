/* Compatibility launcher for the classic KyleOS desktop environment. */
#include <unistd.h>

int main(void)
{
    char *argv[] = { "kuid", "/bin/win95-shell", 0 };

    execve("/bin/kuid", argv, 0);
    return 127;
}
