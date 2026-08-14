#ifndef KYLEOS_UAPI_MOUSE_H
#define KYLEOS_UAPI_MOUSE_H

#define KYLEOS_MOUSE_DEVICE "/dev/mouse"

/* Binary records returned by read(KYLEOS_MOUSE_DEVICE, ...). */
struct kyleos_mouse_event {
    int dx;
    int dy;
    unsigned char buttons;
    /* Positive scrolls up and negative scrolls down. */
    signed char wheel;
    unsigned char reserved[2];
};

#endif
