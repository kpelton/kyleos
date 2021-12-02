#ifndef OUTPUT_H
#define OUTPUT_H

void print_regs(unsigned long exception,unsigned long rip);
char * itoa( unsigned long value, char * str, int base );
char * kstrcpy( char *dest, const char *src);
char * kstrncpy(char *dest, const char *src,int bytes);
int atoi(char *str);
int kpow(int base, int exp);
void kprintf( char *format, ...);
void output_init();
int kstrcmp(char *dest, const char *src);
int kstrlen(char *str);
void read_input(char * dest);
void panic(char* msg);

#endif
