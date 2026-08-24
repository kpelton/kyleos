#ifndef KYLEOS_UAPI_SIGNAL_H
#define KYLEOS_UAPI_SIGNAL_H

#include <stdint.h>

#define KYLEOS_NSIG 32

#define KYLEOS_SIGHUP 1
#define KYLEOS_SIGINT 2
#define KYLEOS_SIGQUIT 3
#define KYLEOS_SIGILL 4
#define KYLEOS_SIGTRAP 5
#define KYLEOS_SIGABRT 6
#define KYLEOS_SIGFPE 8
#define KYLEOS_SIGKILL 9
#define KYLEOS_SIGBUS 10
#define KYLEOS_SIGSEGV 11
#define KYLEOS_SIGPIPE 13
#define KYLEOS_SIGALRM 14
#define KYLEOS_SIGTERM 15
#define KYLEOS_SIGSTOP 17
#define KYLEOS_SIGTSTP 18
#define KYLEOS_SIGCONT 19
#define KYLEOS_SIGCHLD 20
#define KYLEOS_SIGTTIN 21
#define KYLEOS_SIGTTOU 22
#define KYLEOS_SIGWINCH 28

#define KYLEOS_SIG_DFL 0ULL
#define KYLEOS_SIG_IGN 1ULL

#define KYLEOS_SIG_SETMASK 0
#define KYLEOS_SIG_BLOCK 1
#define KYLEOS_SIG_UNBLOCK 2

#define KYLEOS_SIGNAL_UNMASKABLE \
    ((1ULL << KYLEOS_SIGKILL) | (1ULL << KYLEOS_SIGSTOP))

struct kyleos_sigaction {
    uint64_t handler;
    uint64_t mask;
    uint64_t restorer;
    uint32_t flags;
    uint32_t reserved;
};

#endif
