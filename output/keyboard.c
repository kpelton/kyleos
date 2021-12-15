#include <asm/asm.h>
#include <include/types.h>
#include <irq/irq.h>
#include <output/input.h>

#define BACKSPACE 0x0E
#define ENTER 0x1C


#define SC_MAX 57
const char *sc_name[] = {"ERROR", "Esc", "1", "2", "3", "4", "5", "6",
                         "7", "8", "9", "0", "-", "=", "Backspace", "Tab", "Q", "W", "E",
                         "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter", "Lctrl",
                         "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "`",
                         "LShift", "\\", "Z", "X", "C", "V", "B", "N", "M", ",", ".",
                         "/", "RShift", "Keypad *", "LAlt", "Spacebar"};
const char sc_ascii[] = {'?', '?', '1', '2', '3', '4', '5', '6',
                         '7', '8', '9', '0', '-', '=', '?', '?', 'q', 'w', 'e', 'r', 't', 'y',
                         'u', 'i', 'o', 'p', '[', ']', '?', '?', 'a', 's', 'd', 'f', 'g',
                         'h', 'j', 'k', 'l', ';', '\'', '`', '?', '\\', 'z', 'x', 'c', 'v',
                         'b', 'n', 'm', ',', '.', '/', '?', '?', '?', ' '};

static void keyboard_callback()
{
    /* The PIC leaves us the scancode in port 0x60 */
    uint8_t scancode = inb(0x60);
    char letter;

    if (scancode > SC_MAX){
        return;
    }
    else if (scancode == ENTER) {
        letter = '\r';
    }
    else {
        letter = sc_ascii[(int)scancode];
    }
    input_add_char(letter);
}

void kbd_irq()
{
    PIC_sendEOI(1);
    keyboard_callback();
}
