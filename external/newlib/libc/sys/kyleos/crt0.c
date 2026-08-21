#include <stdlib.h>
#include <time.h>
#include <string.h>


extern char **environ;

//extern int main(int argc,char **argv,char **envp);
extern int main();

extern char _end;
extern char __bss_start;
extern void __sinit(struct _reent *);
void _start()
{
    /*
     * The argument block begins above the current stack frame, because we
     * have no return address. The calculation assumes that sizeof(int) ==
     * sizeof(void *). This is okay for i386 user space, but may be invalid in
     * other cases.
     */
     //reneable thius
/*
struct _reent reent;

    struct _reent global_reent;
*/
    _REENT_INIT_PTR(_REENT);
    _REENT_INIT_PTR(_GLOBAL_REENT);
    __sinit(_GLOBAL_REENT);
/*
   _REENT_INIT_PTR(&reent);
    _REENT_INIT_PTR(&global_reent);
    __sinit(&global_reent);

*/
    //int *params = &args-1;
    //int argc = *params;
    //char **argv = (char **) (params+1);
     unsigned long frame_pointer;
     unsigned long *rbp; 
    // Inline assembly to get the frame pointer (RBP)
    asm volatile("movq %%rbp, %0" : "=r" (frame_pointer));
    rbp = (unsigned long *) frame_pointer;
    int argc= (int) *(rbp+2);
    char **argv = (char **) *(rbp+3);
    //printf("Frame Pointer (RBP) within someFunction: 0x%016lx\n", frame_pointer);

    // Your function code here
    //environ = argv+argc+1;

    /* Note: do not clear the .bss section.  When running with shared
     *       libraries, certain data items such __mb_cur_max or environ
     *       may get placed in the .bss, even though they are initialized
     *       to non-zero values.  Clearing the .bss will end up zeroing
     *       out their initial values.  The .bss is already initialized
     *       by this time by Linux.  */

    //tzset(); /* initialize timezone info */
    exit(main(argc,argv));
}
