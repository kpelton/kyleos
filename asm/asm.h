#ifndef ASM_H
#define ASM_H
#include <include/types.h>
//Structures
struct RegDump{
  uint64_t rax;
  uint64_t rbx;
  uint64_t rcx;
  uint64_t rdx;
  uint64_t rsp;
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
};

//Wrapped asm functions
unsigned char inb( unsigned short port );
unsigned short inw( unsigned short port );
void outb( unsigned short port, unsigned char val );
void outw( unsigned short port,unsigned short val );
void io_wait( void );
struct RegDump dump_regs();
uint32_t rdmsr(uint32_t msr_id);
void wrmsr (uint32_t msr_id, uint32_t msr_value);
#endif
