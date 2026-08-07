#ifndef INPUT_H
#define INPUT_H
#include <output/keyboard.h>
void input_read(char* dest);
void input_add_char(char in);
void input_init();
void input_add_key(uint8_t scancode, bool extended, bool pressed);
int input_poll_key(int *pressed, uint8_t *scancode, bool *extended);
void input_set_raw(bool enabled);
bool input_is_raw(void);
#endif
