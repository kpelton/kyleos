#ifndef UART_H
#define UART_H
void serial_init();
void serial_kprintf(char* str);
void serial_read_input(char* dest);

#endif

