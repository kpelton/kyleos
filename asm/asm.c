#include <asm/asm.h>
uint8_t inb( uint16_t port ) {

    uint8_t ret;
    asm volatile( "inb %1, %0"
            : "=a"(ret) : "Nd"(port) );
    return ret;
}
uint16_t inw( uint16_t port ) {

    uint16_t ret;
    asm volatile( "inw %1, %0"
            : "=a"(ret) : "Nd"(port) );
    return ret;
}
void outw( uint16_t port,uint16_t val ) {

    asm volatile( "outw %0, %1"
            : : "a"(val), "Nd"(port) );
}

void outb( uint16_t port, uint8_t val ) {

    asm volatile( "outb %0, %1"
            : : "a"(val), "Nd"(port) );
}
inline void wrmsr(uint32_t msr_id, uint32_t msr_value)
{
    asm volatile ( "wrmsr" : : "c" (msr_id), "A" (msr_value) );
}
inline uint32_t rdmsr(uint32_t msr_id)
{
    uint32_t msr_value;
    asm volatile ( "rdmsr" : "=A" (msr_value) : "c" (msr_id) );
    return msr_value;
}

void io_wait( void ) {

    // port 0x80 is used for 'checkpoints' during POST.
    // The Linux kernel seems to think it is free for use :-/
    asm volatile( "outb %%al, $0x80"
            : : "a"(0) );
}

struct RegDump dump_regs( void ) {

    struct RegDump dump;
    asm volatile("movq %%rax ,%0" : "=g"(dump.rax));
    asm volatile("movq %%rbx ,%0" : "=g"(dump.rbx));
    asm volatile("movq %%rcx ,%0" : "=g"(dump.rcx));
    asm volatile("movq %%rdx ,%0" : "=g"(dump.rdx));
    asm volatile("movq %%rsp ,%0" : "=g"(dump.rsp));
    asm volatile("movq %%rbp ,%0" : "=g"(dump.rbp));
    asm volatile("movq %%rdi ,%0" : "=g"(dump.rdi));
    asm volatile("movq %%rsi ,%0" : "=g"(dump.rsi));
    asm volatile("movq %%r8  ,%0" : "=g"(dump.r8));
    asm volatile("movq %%r9  ,%0" : "=g"(dump.r9));
    asm volatile("movq %%r10 ,%0" : "=g"(dump.r10));
    asm volatile("movq %%r11 ,%0" : "=g"(dump.r11));
    asm volatile("movq %%r12 ,%0" : "=g"(dump.r12));
    asm volatile("movq %%r13 ,%0" : "=g"(dump.r13));
    asm volatile("movq %%r14 ,%0" : "=g"(dump.r14));
    asm volatile("movq %%r15 ,%0" : "=g"(dump.r15));
    asm volatile("movq %%cr0 ,%0" : "=g"(dump.cr0));
    asm volatile("movq %%cr2 ,%0" : "=g"(dump.cr2));
    asm volatile("movq %%cr3 ,%0" : "=g"(dump.cr3));
    asm volatile("movq %%cr4 ,%0" : "=g"(dump.cr4));

    dump.flags = get_flags_reg();
return dump;
}
