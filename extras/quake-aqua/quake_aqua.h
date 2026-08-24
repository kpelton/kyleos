#ifndef KYLEOS_QUAKE_AQUA_H
#define KYLEOS_QUAKE_AQUA_H

#include <stdint.h>

struct kui_surface;

void QuakeAqua_QueueKey(int pressed, int code, int extended);
void QuakeAqua_QueuePointer(int x, int y, int buttons, int wheel);
int QuakeAqua_PopKey(int *pressed, int *key);
void QuakeAqua_TakeMouse(int *dx, int *dy);
void QuakeAqua_DrawFrame(struct kui_surface *surface);
int QuakeAqua_TakeFrameReady(void);

#endif
