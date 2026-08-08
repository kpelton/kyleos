#include <stdlib.h>
#include <string.h>

extern int main();
extern void __sinit(struct _reent *);

/* KyleOS currently enters _start with a stack aligned differently from the
 * SysV process ABI.  Realign here so code emitted by a native compiler may
 * safely use aligned SSE stack operands. */
__attribute__((force_align_arg_pointer)) void _start(void)
{
    unsigned long frame_pointer;
    unsigned long *frame;
    int argc;
    char **argv;

    _REENT_INIT_PTR(_REENT);
    _REENT_INIT_PTR(_GLOBAL_REENT);
    __sinit(_GLOBAL_REENT);

    __asm__ volatile("movq %%rbp, %0" : "=r"(frame_pointer));
    frame = (unsigned long *)frame_pointer;
    argc = (int)frame[2];
    argv = (char **)frame[3];
    exit(main(argc, argv));
}
