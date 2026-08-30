#ifndef KYLEOS_UAPI_SYSCALL_H
#define KYLEOS_UAPI_SYSCALL_H

#define KYLEOS_SYS_POWEROFF 52
#define KYLEOS_SYS_REBOOT 53

static inline int kyleos_poweroff(void)
{
    int result;
    asm volatile("mov $52, %%rax; int $0x80" : "=a"(result) :
                 : "memory");
    return result;
}

static inline int kyleos_reboot(void)
{
    int result;
    asm volatile("mov $53, %%rax; int $0x80" : "=a"(result) :
                 : "memory");
    return result;
}

#endif
