#ifndef KYLEOS_UAPI_FRAMEBUFFER_H
#define KYLEOS_UAPI_FRAMEBUFFER_H

#include <stdint.h>

struct kyleos_framebuffer_rect {
    uint32_t surface_width;
    uint32_t surface_height;
    uint32_t surface_stride;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

#endif
