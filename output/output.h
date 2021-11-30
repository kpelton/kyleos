#ifndef OUTPUT_H
#define OUTPUT_H

//Std args To be moved to a standard include directory under stdarg.h
typedef __builtin_va_list va_list;
#define va_start(v, l)  __builtin_va_start(v, l)
#define va_end(v)   __builtin_va_end(v)
#define va_arg(v, T)    __builtin_va_arg(v, T)
#define va_copy(d, s)   __builtin_va_copy(d, s)

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


#endif
