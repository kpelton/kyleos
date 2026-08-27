#include <locks/spinlock.h>
#include <output/output.h>
#include <output/tty.h>
#include <output/uart.h>
#include <output/vga.h>

#define TTY_INPUT_SIZE 512
#define TTY_RAW_QUEUE_SIZE 128
#define TTY_KEY_QUEUE_SIZE 64
#define TTY_BACKSPACE 127

struct tty_console {
    char ready[TTY_INPUT_SIZE];
    uint32_t ready_length;
    char editing[TTY_INPUT_SIZE];
    uint32_t editing_length;
    char raw_queue[TTY_RAW_QUEUE_SIZE];
    uint32_t raw_head;
    uint32_t raw_tail;
    bool raw;
};

struct tty_key_event {
    uint8_t scancode;
    bool extended;
    bool pressed;
};

static struct tty_console consoles[TTY_CONSOLE_COUNT + 1];
static struct tty_key_event key_queue[TTY_KEY_QUEUE_SIZE];
static uint32_t key_head;
static uint32_t key_tail;
static int active_console;
static struct spinlock tty_lock;

static int resolve_console(int console)
{
    if (console == TTY_ACTIVE_CONSOLE)
        return active_console;
    if (console < 0 || console > TTY_SERIAL_CONSOLE)
        return -1;
    return console;
}

static void clear_bytes(char *buffer, uint32_t count)
{
    for (uint32_t index = 0; index < count; index++)
        buffer[index] = '\0';
}

static void tty_echo(int console, const char *text, uint32_t count)
{
    if (console == TTY_SERIAL_CONSOLE) {
        serial_write(text, count);
        return;
    }
    vga_console_write(console, text, count);
}

void tty_init(void)
{
    init_spinlock(&tty_lock);
    active_console = 0;
    key_head = 0;
    key_tail = 0;
    for (int index = 0; index <= TTY_SERIAL_CONSOLE; index++) {
        clear_bytes(consoles[index].ready, sizeof(consoles[index].ready));
        clear_bytes(consoles[index].editing,
                    sizeof(consoles[index].editing));
        clear_bytes(consoles[index].raw_queue,
                    sizeof(consoles[index].raw_queue));
        consoles[index].ready_length = 0;
        consoles[index].editing_length = 0;
        consoles[index].raw_head = 0;
        consoles[index].raw_tail = 0;
        consoles[index].raw = false;
    }
}

int tty_read(int console, void *buffer, uint32_t count)
{
    struct tty_console *tty;
    char *destination = buffer;
    uint32_t copied;

    if (buffer == NULL || count == 0)
        return 0;
    acquire_spinlock(&tty_lock);
    console = resolve_console(console);
    if (console < 0) {
        release_spinlock(&tty_lock);
        return -1;
    }
    tty = &consoles[console];
    if (tty->raw) {
        if (tty->raw_head == tty->raw_tail) {
            release_spinlock(&tty_lock);
            return 0;
        }
        destination[0] = tty->raw_queue[tty->raw_tail];
        tty->raw_tail = (tty->raw_tail + 1) % TTY_RAW_QUEUE_SIZE;
        release_spinlock(&tty_lock);
        return 1;
    }
    copied = tty->ready_length < count ? tty->ready_length : count;
    for (uint32_t index = 0; index < copied; index++)
        destination[index] = tty->ready[index];
    for (uint32_t index = copied; index < tty->ready_length; index++)
        tty->ready[index - copied] = tty->ready[index];
    tty->ready_length -= copied;
    release_spinlock(&tty_lock);
    return (int)copied;
}

int tty_write(int console, const void *buffer, uint32_t count)
{
    if (buffer == NULL)
        return -1;
    acquire_spinlock(&tty_lock);
    console = resolve_console(console);
    release_spinlock(&tty_lock);
    if (console < 0)
        return -1;
    if (console == TTY_SERIAL_CONSOLE) {
        serial_write(buffer, count);
        return (int)count;
    }
    vga_console_write(console, buffer, count);
    return (int)count;
}

static void tty_receive_char_on(int console, char character, bool echo_input)
{
    struct tty_console *tty;
    char echo[3];
    uint32_t echo_length = 0;

    acquire_spinlock(&tty_lock);
    if (console == TTY_ACTIVE_CONSOLE)
        console = active_console;
    if (console < 0 || console > TTY_SERIAL_CONSOLE) {
        release_spinlock(&tty_lock);
        return;
    }
    tty = &consoles[console];
    if (tty->raw) {
        uint32_t next = (tty->raw_head + 1) % TTY_RAW_QUEUE_SIZE;

        if (next != tty->raw_tail) {
            tty->raw_queue[tty->raw_head] = character;
            tty->raw_head = next;
        }
        release_spinlock(&tty_lock);
        return;
    }
    if (character == '\t') {
        release_spinlock(&tty_lock);
        return;
    }
    if (character == '\r')
        character = '\n';
    if (character == TTY_BACKSPACE) {
        if (tty->editing_length != 0) {
            tty->editing_length--;
            tty->editing[tty->editing_length] = '\0';
            echo[0] = '\b';
            echo[1] = ' ';
            echo[2] = '\b';
            echo_length = 3;
        }
        release_spinlock(&tty_lock);
        if (echo_input && echo_length != 0)
            tty_echo(console, echo, echo_length);
        return;
    }
    if (tty->editing_length >= TTY_INPUT_SIZE - 1) {
        release_spinlock(&tty_lock);
        return;
    }
    tty->editing[tty->editing_length++] = character;
    if (character == '\n' && tty->ready_length == 0) {
        for (uint32_t index = 0; index < tty->editing_length; index++)
            tty->ready[index] = tty->editing[index];
        tty->ready_length = tty->editing_length;
        clear_bytes(tty->editing, tty->editing_length);
        tty->editing_length = 0;
    }
    echo[0] = character;
    echo_length = 1;
    release_spinlock(&tty_lock);
    tty_echo(console, echo, echo_length);
}

void tty_receive_char(char character)
{
    tty_receive_char_on(TTY_ACTIVE_CONSOLE, character, true);
}

void tty_receive_serial_char(char character)
{
    tty_receive_char_on(TTY_SERIAL_CONSOLE, character, true);
}

void tty_set_raw(int console, bool enabled)
{
    struct tty_console *tty;

    acquire_spinlock(&tty_lock);
    console = resolve_console(console);
    if (console < 0) {
        release_spinlock(&tty_lock);
        return;
    }
    tty = &consoles[console];
    tty->raw = enabled;
    if (enabled) {
        clear_bytes(tty->ready, sizeof(tty->ready));
        clear_bytes(tty->editing, sizeof(tty->editing));
        tty->ready_length = 0;
        tty->editing_length = 0;
        tty->raw_head = 0;
        tty->raw_tail = 0;
        key_head = 0;
        key_tail = 0;
    }
    release_spinlock(&tty_lock);
}

bool tty_is_raw(int console)
{
    bool raw = false;

    acquire_spinlock(&tty_lock);
    console = resolve_console(console);
    if (console >= 0)
        raw = consoles[console].raw;
    release_spinlock(&tty_lock);
    return raw;
}

int tty_active_console(void)
{
    return active_console;
}

bool tty_switch_console(int console)
{
    if (console < 0 || console >= TTY_CONSOLE_COUNT)
        return false;
    acquire_spinlock(&tty_lock);
    if (console == active_console) {
        release_spinlock(&tty_lock);
        return true;
    }
    active_console = console;
    key_head = 0;
    key_tail = 0;
    release_spinlock(&tty_lock);
    vga_switch_console(console);
    return true;
}

void tty_add_key(uint8_t scancode, bool extended, bool pressed)
{
    uint32_t next;

    acquire_spinlock(&tty_lock);
    next = (key_head + 1) % TTY_KEY_QUEUE_SIZE;
    if (next != key_tail) {
        key_queue[key_head].scancode = scancode;
        key_queue[key_head].extended = extended;
        key_queue[key_head].pressed = pressed;
        key_head = next;
    }
    release_spinlock(&tty_lock);
}

int tty_poll_key(int *pressed, uint8_t *scancode, bool *extended)
{
    if (pressed == NULL || scancode == NULL || extended == NULL)
        return 0;
    acquire_spinlock(&tty_lock);
    if (key_head == key_tail) {
        release_spinlock(&tty_lock);
        return 0;
    }
    *scancode = key_queue[key_tail].scancode;
    *extended = key_queue[key_tail].extended;
    *pressed = key_queue[key_tail].pressed;
    key_tail = (key_tail + 1) % TTY_KEY_QUEUE_SIZE;
    release_spinlock(&tty_lock);
    return 1;
}
