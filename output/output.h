#ifndef OUTPUT_H
#define OUTPUT_H
#include <include/types.h>

void print_regs(unsigned long exception,unsigned long rip);
char * itoa( unsigned long value, char * str, int base );
char * kstrcpy( char *dest, const char *src);
char * kstrncpy(char *dest, const char *src,int bytes);
int kstrstr(char *base, char *delim);
//Doesn't belong here
void *memcpy(void *dest, const void *src, int n);
 uint8_t *memcpy8(uint8_t *dest, const uint8_t *src,uint64_t bytes);
uint64_t *memcpy64(uint64_t *dest, const uint64_t *src,uint64_t bytes);
uint8_t * memzero8(uint8_t *dest,uint64_t bytes);
uint64_t * memzero64(uint64_t *dest,uint64_t bytes);
int atoi(char *str);
int kpow(int base, int exp);
void kprintf( char *format, ...);
void output_init();
int kstrcmp(char *dest, const char *src);
int kstrlen(char *str);
void read_input(char * dest);
void panic(char* msg);

#endif
