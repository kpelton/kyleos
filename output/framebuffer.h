#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <include/types.h>

/* Called after paging is fully initialized. */
void framebuffer_init(void);
int framebuffer_present(const void *pixels, uint32_t bytes);
uint32_t framebuffer_width(void);
uint32_t framebuffer_height(void);
uint32_t framebuffer_pitch(void);
bool framebuffer_is_ready(void);
void framebuffer_clear(uint32_t color);
void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t width,
                           uint32_t height, uint32_t color);

#endif
