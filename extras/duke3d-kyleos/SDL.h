#ifndef KYLEOS_DUKE3D_SDL_STUB_H
#define KYLEOS_DUKE3D_SDL_STUB_H

typedef struct SDL_Surface SDL_Surface;
typedef int SDL_GrabMode;

#define SDL_GRAB_QUERY 0
#define SDL_GRAB_OFF 0
#define SDL_GRAB_ON 1
#define SDL_INIT_VIDEO 0x20

static inline SDL_GrabMode SDL_WM_GrabInput(SDL_GrabMode mode)
{
    (void)mode;
    return SDL_GRAB_ON;
}

static inline int SDL_ShowCursor(int toggle)
{
    (void)toggle;
    return 0;
}

static inline void SDL_Quit(void)
{
}

static inline void SDL_QuitSubSystem(unsigned int flags)
{
    (void)flags;
}

#endif
