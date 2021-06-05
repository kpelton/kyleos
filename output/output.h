#ifndef OUTPUT_H
#define OUTPUT_H
void print_regs(unsigned long exception);
char * itoa( unsigned long value, char * str, int base );
char * itoa_16( unsigned short value, char * str, int base );
char * itoa_8( unsigned char value, char * str, int base );
int atoi(char *str);
int kpow(int base, int exp);
void kprintf(char *str);
void kprint_hex(char *desc, unsigned long val);
void output_init();


#endif
