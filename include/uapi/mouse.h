#ifndef KYLEOS_UAPI_MOUSE_H
#define KYLEOS_UAPI_MOUSE_H

#define KYLEOS_MOUSE_DEVICE "/dev/mouse"

/* Binary records returned by read(KYLEOS_MOUSE_DEVICE, ...). */
struct kyleos_mouse_event {
    int dx;
    int dy;
    unsigned char buttons;
    unsigned char reserved[3];
};

#endif
