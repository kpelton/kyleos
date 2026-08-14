/* KyleOS desktop entry point.  Application behavior lives in clients. */
#include "extras/desktop/shell.h"

int main(void)
{
    return desktop_shell_run();
}
