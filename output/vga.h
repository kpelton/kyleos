#ifndef VGA_H
#define VGA_H
void vga_clear(void);
void vga_kprintf(char* str);
void vga_init();
void vga_framebuffer_ready(void);
#endif
