#ifndef VGA_H
#define VGA_H
#include <include/types.h>
void vga_clear(void);
void vga_kprintf(char* str);
void vga_console_write(int console, const void *buffer, uint32_t count);
bool vga_switch_console(int console);
void vga_init();
void vga_framebuffer_ready(void);
#endif
