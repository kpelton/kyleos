#include "doomkeys.h"

#include "doomgeneric.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static unsigned char convert_to_doom_key(unsigned int key)
{
    switch (key) {
    case 0x48: return KEY_UPARROW;
    case 0x50: return KEY_DOWNARROW;
    case 0x4b: return KEY_LEFTARROW;
    case 0x4d: return KEY_RIGHTARROW;
    case 0x01: return KEY_ESCAPE;
    case 0x1c: return KEY_ENTER;
    case 0x0f: return KEY_TAB;
    case 0x0e: return KEY_BACKSPACE;
    case 0x39: return KEY_USE;
    case 0x1d: return KEY_FIRE;
    case 0x38: return KEY_STRAFE_L;
    case 0x2a: return KEY_RSHIFT;
    case 0x10: return 'q'; case 0x11: return 'w';
    case 0x12: return 'e'; case 0x13: return 'r';
    case 0x14: return 't'; case 0x15: return 'y';
    case 0x16: return 'u'; case 0x17: return 'i';
    case 0x18: return 'o'; case 0x19: return 'p';
    case 0x1e: return 'a'; case 0x1f: return 's';
    case 0x20: return 'd'; case 0x21: return 'f';
    case 0x22: return 'g'; case 0x23: return 'h';
    case 0x24: return 'j'; case 0x25: return 'k';
    case 0x26: return 'l'; case 0x2c: return 'z';
    case 0x2d: return 'x'; case 0x2e: return 'c';
    case 0x2f: return 'v'; case 0x30: return 'b';
    case 0x31: return 'n'; case 0x32: return 'm';
    default: return 0;
    }
}

static int syscall_present(void *pixels, uint32_t bytes)
{
    register long rax asm("rax") = 23;
    asm volatile("int $0x80" : "+a"(rax) : "D"(pixels), "S"((long)bytes) : "memory");
    return (int)rax;
}

static int syscall_key_poll(int *pressed, unsigned char *key,
                            unsigned char *extended)
{
    register long rax asm("rax") = 24;
    asm volatile("int $0x80" : "+a"(rax) : "D"(pressed), "S"(key),
                 "d"(extended) : "memory");
    return (int)rax;
}

static uint32_t syscall_ticks_ms(void)
{
    register long rax asm("rax") = 25;
    asm volatile("int $0x80" : "+a"(rax) : : "memory");
    return (uint32_t)rax;
}

static void syscall_sleep_ms(uint32_t ms)
{
    register long rax asm("rax") = 0;
    asm volatile("int $0x80" : "+a"(rax) : "D"((long)ms) : "memory");
}

void DG_Init(void) {}

void DG_DrawFrame(void)
{
    syscall_present(DG_ScreenBuffer, DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
}

void DG_SleepMs(uint32_t ms)
{
    syscall_sleep_ms(ms);
}

uint32_t DG_GetTicksMs(void)
{
    return syscall_ticks_ms();
}

int DG_GetKey(int *pressed, unsigned char *doom_key)
{
    unsigned char scancode;
    unsigned char extended;
    int down;

    if (syscall_key_poll(&down, &scancode, &extended) <= 0)
        return 0;
    (void)extended;
    *doom_key = convert_to_doom_key(scancode);
    if (*doom_key == 0)
        return 0;
    *pressed = down;
    return 1;
}

void DG_SetWindowTitle(const char *title)
{
    (void)title;
}

int main(int argc, char **argv)
{
    static char *default_argv[] = {
        "doom", "-iwad", "/usr/share/doom/doom.wad", NULL
    };

    if (argc == 1) {
        argc = 3;
        argv = default_argv;
    }
    doomgeneric_Create(argc, argv);
    for (;;)
        doomgeneric_Tick();
}
