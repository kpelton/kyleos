#include <asm/asm.h>
unsigned char inb( unsigned short port ) {

    unsigned char ret;
    asm volatile( "inb %1, %0"
            : "=a"(ret) : "Nd"(port) );
    return ret;
}
unsigned short inw( unsigned short port ) {

    unsigned short ret;
    asm volatile( "inw %1, %0"
            : "=a"(ret) : "Nd"(port) );
    return ret;
}


void outb( unsigned short port, unsigned char val ) {

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

    asm volatile("movq %%rax ,%0; movq %%rbx, %1; movq %%rsp, %2;movq %%rdi, %3;movq %%rcx, %4; movq %%rdx, %5"
            : "=r"(dump.rax), 
            "=r" (dump.rbx),
            "=r" (dump.rsp),
            "=r" (dump.rdi),
            "=r" (dump.rcx),
            "=r" (dump.rdx));




    asm volatile(" mov %%cr0, %0;mov %%cr3, %1;mov %%cr4, %2;"
            : "=r" (dump.cr0),
            "=r" (dump.cr3),
            "=r" (dump.cr4));


    return dump;
}
