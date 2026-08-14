#ifndef MOUSE_H
#define MOUSE_H

#include <include/types.h>
#include <include/uapi/mouse.h>

void mouse_init(void);
void mouse_irq(void);
int mouse_poll(int *dx, int *dy, uint8_t *buttons);
int mouse_read_events(void *buffer, uint32_t count);

#endif
