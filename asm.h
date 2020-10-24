#ifndef ASM_H
#define ASM_H
#include "types.h"
//Structures
struct RegDump{

  uint64_t  cr0;
  uint64_t rax;
  uint64_t rbx;
  uint64_t rdx;
  uint64_t rsp;
  uint64_t rcx;
  uint64_t  rdi;
  uint64_t  rsi;
  uint64_t rip;
  uint64_t  cr3;
  uint64_t  cr4;
  uint64_t   flags;
};

//Wrapped asm functions
unsigned char inb( unsigned short port );
void outb( unsigned short port, unsigned char val );
void io_wait( void );
struct RegDump dump_regs();
uint32_t rdmsr(uint32_t msr_id);
void wrmsr (uint32_t msr_id, uint32_t msr_value);
#endif
