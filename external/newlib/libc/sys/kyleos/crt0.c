#include <stdlib.h>


extern int main(int argc, char **argv);
extern void __sinit(struct _reent *);

/* KyleOS enters userspace with this stack layout:
 *
 *   rsp + 0   sentinel return address
 *   rsp + 8   argc
 *   rsp + 16  argv
 *
 * Capture it before compiled C code can repurpose RBP or adjust RSP. */
__asm__(
    ".text\n"
    ".global _start\n"
    ".type _start, @function\n"
    "_start:\n"
    "movq 8(%rsp), %rdi\n"
    "movq 16(%rsp), %rsi\n"
    "andq $-16, %rsp\n"
    "call __kyleos_start\n"
    "ud2\n"
    ".size _start, .-_start\n");

__attribute__((noreturn, used))
void __kyleos_start(int argc, char **argv)
{
    _REENT_INIT_PTR(_REENT);
    _REENT_INIT_PTR(_GLOBAL_REENT);
    __sinit(_GLOBAL_REENT);
    exit(main(argc, argv));
    __builtin_unreachable();
}
