#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <include/types.h>

/* Called after paging is fully initialized. */
void framebuffer_init(void);
int framebuffer_present(const void *pixels, uint32_t bytes);
uint32_t framebuffer_width(void);
uint32_t framebuffer_height(void);
uint32_t framebuffer_pitch(void);

#endif
