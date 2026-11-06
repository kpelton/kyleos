#include <asm/asm.h>
#include <include/types.h>
#include <irq/irq.h>
#include <output/input.h>
#include <output/tty.h>
#include <locks/spinlock.h>

#define ESCAPE 0x01
#define BACKSPACE 0x0E
#define TAB 0x0F
#define CTRL 0x1D
#define ENTER 0x1C
#define LEFT_SHIFT 0x2A
#define RIGHT_SHIFT 0x36
#define ALT 0x38
#define CAPS_LOCK 0x3A
#define F1 0x3B
#define F4 0x3E
#define F10 0x44
#define F11 0x57
#define F12 0x58
#define KEY_HOME 0x47
#define KEY_PAGE_UP 0x49
#define KEY_LEFT 0x4B
#define KEY_RIGHT 0x4D
#define KEY_END 0x4F
#define KEY_UP 0x48
#define KEY_DOWN 0x50
#define KEY_PAGE_DOWN 0x51
#define KEY_INSERT 0x52
#define KEY_DELETE 0x53
#define SC_MAX 57
#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64
#define PS2_OUTPUT_FULL 0x01
#define PS2_INPUT_FULL 0x02
#define PS2_READ_CONFIG 0x20
#define PS2_WRITE_CONFIG 0x60
#define PS2_ENABLE_KEYBOARD 0xae
#define PS2_KEYBOARD_DEFAULTS 0xf6
#define PS2_KEYBOARD_ENABLE_SCAN 0xf4
#define PS2_ACK 0xfa

static struct spinlock kbd_spinlock;
static bool extended_prefix;
static bool left_shift;
static bool right_shift;
static bool alt;
static bool ctrl;
static bool caps_lock;

static bool ps2_wait_write(void)
{
    for (int i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & PS2_INPUT_FULL) == 0) return true;
        io_wait();
    }
    return false;
}

static bool ps2_wait_read(void)
{
    for (int i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & PS2_OUTPUT_FULL) != 0) return true;
        io_wait();
    }
    return false;
}

static bool ps2_command(uint8_t command)
{
    if (!ps2_wait_write()) return false;
    outb(PS2_COMMAND, command);
    return true;
}

static bool ps2_data(uint8_t data)
{
    if (!ps2_wait_write()) return false;
    outb(PS2_DATA, data);
    return true;
}

static bool ps2_keyboard_command(uint8_t command)
{
    return ps2_data(command) && ps2_wait_read() && inb(PS2_DATA) == PS2_ACK;
}

static void input_add_sequence(const char *sequence)
{
    while (*sequence != '\0') input_add_char(*sequence++);
}

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
    bool extended;

    if (scancode == 0xe0) {
        extended_prefix = true;
        return;
    }
    pressed = (scancode & 0x80) == 0;
    scancode &= 0x7f;
    extended = extended_prefix;
    extended_prefix = false;
    if (scancode == ALT) {
        alt = pressed;
        input_add_key(scancode, extended, pressed);
        return;
    }
    if (scancode == CTRL) {
        ctrl = pressed;
        input_add_key(scancode, extended, pressed);
        return;
    }
    if (!extended && scancode == CAPS_LOCK) {
        if (pressed) caps_lock = !caps_lock;
        input_add_key(scancode, extended, pressed);
        return;
    }
    if (!extended && alt && scancode >= F1 && scancode <= F4) {
        if (pressed)
            tty_switch_console(scancode - F1);
        return;
    }
    input_add_key(scancode, extended, pressed);
    if (!extended && (scancode == LEFT_SHIFT || scancode == RIGHT_SHIFT)) {
        if (scancode == LEFT_SHIFT) left_shift = pressed;
        else right_shift = pressed;
        return;
    }
    if (pressed && extended && input_is_raw()) {
        if (scancode == KEY_LEFT) input_add_sequence("\033[D");
        else if (scancode == KEY_RIGHT) input_add_sequence("\033[C");
        else if (scancode == KEY_UP) input_add_sequence("\033[A");
        else if (scancode == KEY_DOWN) input_add_sequence("\033[B");
        else if (scancode == KEY_HOME) input_add_sequence("\033[H");
        else if (scancode == KEY_END) input_add_sequence("\033[F");
        else if (scancode == KEY_PAGE_UP) input_add_sequence("\033[5~");
        else if (scancode == KEY_PAGE_DOWN) input_add_sequence("\033[6~");
        else if (scancode == KEY_INSERT) input_add_sequence("\033[2~");
        else if (scancode == KEY_DELETE) input_add_sequence("\033[3~");
        return;
    }
    if (pressed && !extended && input_is_raw() &&
        ((scancode >= F1 && scancode <= F10) ||
         scancode == F11 || scancode == F12)) {
        static const char *function_sequences[] = {
            "\033OP", "\033OQ", "\033OR", "\033OS", "\033[15~",
            "\033[17~", "\033[18~", "\033[19~", "\033[20~",
            "\033[21~", "\033[23~", "\033[24~"
        };
        int index = scancode <= F10 ? scancode - F1 :
                    scancode == F11 ? 10 : 11;

        input_add_sequence(function_sequences[index]);
        return;
    }
    if (!pressed || scancode > SC_MAX){
        return;
    } else if (scancode == ESCAPE) {
        letter = 27;
    } else if (scancode == BACKSPACE) {
        /* Keep the hardware keyboard consistent with serial, whose
         * backspace byte is handled by input_add_char(). */
        letter = 127;
    } else if (scancode == TAB) {
        if ((left_shift || right_shift) && input_is_raw()) {
            input_add_sequence("\033[Z");
            return;
        }
        letter = '\t';
    } else if (scancode == ENTER) {
        letter = '\r';
    }
    else {
        letter = sc_ascii[(int)scancode];
        if (letter >= 'a' && letter <= 'z') {
            if (caps_lock != (left_shift || right_shift))
                letter = letter - 'a' + 'A';
        } else if (left_shift || right_shift) {
            letter = shifted_char(letter);
        }
        if (ctrl) {
            if (letter >= 'a' && letter <= 'z')
                letter = letter - 'a' + 1;
            else if (letter >= '@' && letter <= '_')
                letter &= 0x1f;
        }
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
    uint8_t config;

    init_spinlock(&kbd_spinlock);
    extended_prefix = left_shift = right_shift = alt = ctrl = caps_lock = false;

    /* Warm resets may leave stale controller output and both clocks/IRQs in
     * their previous state. Re-establish the keyboard port explicitly. */
    for (int i = 0; i < 32 && (inb(PS2_STATUS) & PS2_OUTPUT_FULL); i++)
        (void)inb(PS2_DATA);
    if (!ps2_command(PS2_ENABLE_KEYBOARD) ||
        !ps2_command(PS2_READ_CONFIG) || !ps2_wait_read())
        return;
    config = inb(PS2_DATA);
    config |= 0x01;  /* Enable IRQ1. */
    config &= ~0x10; /* Enable the first-port clock. */
    if (!ps2_command(PS2_WRITE_CONFIG) || !ps2_data(config))
        return;
    (void)ps2_keyboard_command(PS2_KEYBOARD_DEFAULTS);
    (void)ps2_keyboard_command(PS2_KEYBOARD_ENABLE_SCAN);
}
