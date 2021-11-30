#ifndef TABLES_H
#define TABLES_H
#include <include/types.h>

void gdt_install();
void idt_install();
void gdt64_install();
void set_tss_rsp(uint64_t *ptr);
void jump_usermode();

#endif
