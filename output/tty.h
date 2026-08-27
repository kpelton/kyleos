#ifndef OUTPUT_TTY_H
#define OUTPUT_TTY_H

#include <include/types.h>

#define TTY_CONSOLE_COUNT 4
#define TTY_SERIAL_CONSOLE TTY_CONSOLE_COUNT
#define TTY_ACTIVE_CONSOLE (-1)

void tty_init(void);
int tty_read(int console, void *buffer, uint32_t count);
int tty_write(int console, const void *buffer, uint32_t count);
void tty_receive_char(char character);
void tty_receive_serial_char(char character);
void tty_set_raw(int console, bool enabled);
bool tty_is_raw(int console);
int tty_active_console(void);
bool tty_switch_console(int console);

void tty_add_key(uint8_t scancode, bool extended, bool pressed);
int tty_poll_key(int *pressed, uint8_t *scancode, bool *extended);

#endif
