#include <asm/asm.h>
#include <include/types.h>
#include <irq/irq.h>
#include <output/input.h>
#include <locks/spinlock.h>

#define BACKSPACE 0x0E
#define ENTER 0x1C
#define LEFT_SHIFT 0x2A
#define RIGHT_SHIFT 0x36
#define KEY_LEFT 0x4B
#define KEY_RIGHT 0x4D
#define KEY_UP 0x48
#define KEY_DOWN 0x50
#define KEY_DELETE 0x53
#define SC_MAX 57

static struct spinlock kbd_spinlock;
static bool extended_prefix;
static bool left_shift;
static bool right_shift;

static char shifted_char(char ch)
{
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 'A';
    if (ch >= '0' && ch <= '9') {
        static const char shifted_digits[] = ")!@#$%^&*(";
        return shifted_digits[ch - '0'];
    }
    if (ch == '-') return '_';
    if (ch == '=') return '+';
    if (ch == '[') return '{';
    if (ch == ']') return '}';
    if (ch == '\\') return '|';
    if (ch == ';') return ':';
    if (ch == '\'') return '"';
    if (ch == '`') return '~';
    if (ch == ',') return '<';
    if (ch == '.') return '>';
    if (ch == '/') return '?';
    return ch;
}

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
    bool pressed;

    if (scancode == 0xe0) {
        extended_prefix = true;
        return;
    }
    pressed = (scancode & 0x80) == 0;
    scancode &= 0x7f;
    input_add_key(scancode, extended_prefix, pressed);
    if (!extended_prefix && (scancode == LEFT_SHIFT || scancode == RIGHT_SHIFT)) {
        if (scancode == LEFT_SHIFT) left_shift = pressed;
        else right_shift = pressed;
        return;
    }
    if (pressed && extended_prefix && input_is_raw()) {
        if (scancode == KEY_LEFT || scancode == KEY_RIGHT || scancode == KEY_UP ||
            scancode == KEY_DOWN || scancode == KEY_DELETE) {
            input_add_char(27);
            input_add_char('[');
            if (scancode == KEY_LEFT) input_add_char('D');
            if (scancode == KEY_RIGHT) input_add_char('C');
            if (scancode == KEY_UP) input_add_char('A');
            if (scancode == KEY_DOWN) input_add_char('B');
            if (scancode == KEY_DELETE) { input_add_char('3'); input_add_char('~'); }
        }
        extended_prefix = false;
        return;
    }
    extended_prefix = false;
    if (!pressed || scancode > SC_MAX){
        return;
    } else if (scancode == BACKSPACE) {
        /* Keep the hardware keyboard consistent with serial, whose
         * backspace byte is handled by input_add_char(). */
        letter = 127;
    } else if (scancode == ENTER) {
        letter = '\r';
    }
    else {
        letter = sc_ascii[(int)scancode];
        if (left_shift || right_shift)
            letter = shifted_char(letter);
    }
    input_add_char(letter);
}

void kbd_irq()
{
    acquire_spinlock(&kbd_spinlock);
    PIC_sendEOI(1);
    keyboard_callback();
    release_spinlock(&kbd_spinlock);

}

void kbd_init() 
{
    PIC_sendEOI(1);
    keyboard_callback();
    init_spinlock(&kbd_spinlock);
}
