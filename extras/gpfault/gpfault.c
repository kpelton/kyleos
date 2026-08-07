#include <stdio.h>

/* HLT is privileged at CPL 3 and must raise #GP(0).  It is deliberately a
 * tiny regression binary: success means the kernel terminates this process
 * and the invoking shell remains usable. */
int main(void)
{
    puts("GP-FAULT: triggering user #GP");
    __asm__ volatile("hlt");
    puts("GP-FAULT: ERROR returned from hlt");
    return 1;
}
