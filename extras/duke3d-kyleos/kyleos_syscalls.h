#ifndef KYLEOS_DUKE3D_SYSCALLS_H
#define KYLEOS_DUKE3D_SYSCALLS_H

#include <stdint.h>

static inline int kyleos_framebuffer_present(const void *pixels,
                                              uint32_t bytes)
{
    register long rax asm("rax") = 23;
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"(pixels), "S"((long)bytes) : "memory");
    return (int)rax;
}

static inline int kyleos_key_poll(int *pressed, unsigned char *scancode,
                                  unsigned char *extended)
{
    register long rax asm("rax") = 24;
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"(pressed), "S"(scancode), "d"(extended) : "memory");
    return (int)rax;
}

static inline uint32_t kyleos_ticks_ms(void)
{
    register long rax asm("rax") = 25;
    asm volatile("int $0x80" : "+a"(rax) : : "memory");
    return (uint32_t)rax;
}

static inline void kyleos_tty_raw(int enabled)
{
    register long rax asm("rax") = 27;
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"((long)enabled) : "memory");
}

static inline void kyleos_sleep_ms(uint32_t ms)
{
    register long rax asm("rax") = 0;
    asm volatile("int $0x80" : "+a"(rax) : "D"((long)ms) : "memory");
}

#endif
