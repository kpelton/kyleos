#include <output/input.h>
#include <output/tty.h>

void input_init()
{
    tty_init();
    kbd_init();
}

void input_read(char* dest)
{
    int count = tty_read(TTY_ACTIVE_CONSOLE, dest, 511);

    if (count < 0)
        count = 0;
    dest[count] = '\0';
}

void input_add_char(char in)
{
    tty_receive_char(in);
}

void input_set_raw(bool enabled)
{
    tty_set_raw(TTY_ACTIVE_CONSOLE, enabled);
}

bool input_is_raw(void)
{
    return tty_is_raw(TTY_ACTIVE_CONSOLE);
}

void input_add_key(uint8_t scancode, bool extended, bool pressed)
{
    tty_add_key(scancode, extended, pressed);
}

int input_poll_key(int *pressed, uint8_t *scancode, bool *extended)
{
    return tty_poll_key(pressed, scancode, extended);
}
