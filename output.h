#ifndef OUTPUT_H
#define OUTPUT_H
void print_regs(unsigned long exception);
char * itoa( unsigned long value, char * str, int base );
void kprintf(char *str);
void output_init();


#endif
