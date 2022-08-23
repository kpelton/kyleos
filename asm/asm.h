#ifndef ASM_H
#define ASM_H
#include <include/types.h>
//Structures
#define INTERRUPT_ENABLE_FLAG 0x200
struct RegDump{
  uint64_t rax;
  uint64_t rbx;
  uint64_t rcx;
  uint64_t rdx;
  uint64_t rsp;
  uint64_t rbp;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t r8;
  uint64_t r9;
  uint64_t r10;
  uint64_t r11;
  uint64_t r12;
  uint64_t r13;
  uint64_t r14;
  uint64_t r15;
  uint64_t cr0;
  uint64_t cr2;
  uint64_t cr3;
  uint64_t cr4;
  uint64_t flags;
};

//Wrapped asm functions
uint8_t inb( uint16_t port );
uint16_t inw( uint16_t port );
void outb( uint16_t port, uint8_t val );
void outw( uint16_t port,uint16_t val );
void io_wait( void );
struct RegDump dump_regs();
uint32_t rdmsr(uint32_t msr_id);
void tss_flush();
void jump_usermode();
uint64_t get_flags_reg();

void wrmsr (uint32_t msr_id, uint32_t msr_value);
#endif
