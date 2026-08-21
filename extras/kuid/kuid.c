#include <stdio.h>

#include "extras/desktop/shell.h"

int main(int argc, char **argv)
{
    const char *shell = argc > 1 ? argv[1] : "/bin/win95-shell";

    if (argc > 2) {
        fputs("Usage: kuid [shell]\n", stderr);
        return 2;
    }
    return desktop_shell_run(shell);
}
