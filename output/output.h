#ifndef OUTPUT_H
#define OUTPUT_H
void print_regs(unsigned long exception);
char * itoa( unsigned long value, char * str, int base );
char * itoa_16( unsigned short value, char * str, int base );
char * itoa_8( unsigned char value, char * str, int base );
char * kstrcpy( char *dest, const char *src);
char * kstrncpy(char *dest, const char *src,int bytes);
int atoi(char *str);
int kpow(int base, int exp);
void kprintf(char *str);
void kprint_hex(char *desc, unsigned long val);
void output_init();
int kstrcmp(char *dest, const char *src);
void read_input(char * dest);


#endif
