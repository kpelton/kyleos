#ifndef VGA_H
#define VGA_H
void kprintf(char *str);
void vga_clear(void);
char * itoa( unsigned int value, char * str, int base );
void print_loc(const int x, const int y, const int back, const int fore, const char c, const int blink);
void pring_regs();
#endif
