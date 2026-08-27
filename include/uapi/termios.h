#ifndef KYLEOS_UAPI_TERMIOS_H
#define KYLEOS_UAPI_TERMIOS_H

#include <stdint.h>

#define KYLEOS_NCCS 13

#define KYLEOS_ISIG   0000001
#define KYLEOS_ICANON 0000002
#define KYLEOS_ECHO   0000010
#define KYLEOS_ECHOE  0000020
#define KYLEOS_ECHOK  0000040
#define KYLEOS_NOFLSH 0000200
#define KYLEOS_TOSTOP 0001000

#define KYLEOS_ICRNL 0000400

#define KYLEOS_VINTR  0
#define KYLEOS_VQUIT  1
#define KYLEOS_VERASE 2
#define KYLEOS_VKILL  3
#define KYLEOS_VEOF   4
#define KYLEOS_VEOL   5
#define KYLEOS_VSUSP 10

#define KYLEOS_TCSANOW   1
#define KYLEOS_TCSADRAIN 2
#define KYLEOS_TCSAFLUSH 3

struct kyleos_termios {
    uint16_t input_flags;
    uint16_t output_flags;
    uint16_t control_flags;
    uint16_t local_flags;
    uint8_t line;
    uint8_t control_characters[KYLEOS_NCCS];
    int8_t input_speed;
    int8_t output_speed;
};

#endif
