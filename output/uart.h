#ifndef UART_H
#define UART_H
#include <include/types.h>
void serial_init();
void serial_kprintf(char* str);
void serial_write(const void *buffer, uint32_t count);

#endif
