/* Aqua keyboard and window-relative mouse backend for Quake. */
#include "quakedef.h"
#include "quake_aqua.h"

#define INPUT_QUEUE_CAPACITY 128

struct queued_key {
    int pressed;
    int key;
};

static struct queued_key key_queue[INPUT_QUEUE_CAPACITY];
static unsigned int key_head;
static unsigned int queued_key_count;
static int mouse_x;
static int mouse_y;
static int pointer_x;
static int pointer_y;
static int pointer_valid;
static int pointer_buttons;

static int translate_scancode(int code, int extended)
{
    static const unsigned char ascii[58] = {
        0, K_ESCAPE, '1', '2', '3', '4', '5', '6',
        '7', '8', '9', '0', '-', '=', K_BACKSPACE, K_TAB,
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
        'o', 'p', '[', ']', K_ENTER, K_CTRL, 'a', 's',
        'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
        '\'', '`', K_SHIFT, '\\', 'z', 'x', 'c', 'v',
        'b', 'n', 'm', ',', '.', '/', K_SHIFT, '*',
        K_ALT, K_SPACE
    };

    (void)extended;
    switch (code) {
    case 0x3b: return K_F1; case 0x3c: return K_F2;
    case 0x3d: return K_F3; case 0x3e: return K_F4;
    case 0x3f: return K_F5; case 0x40: return K_F6;
    case 0x41: return K_F7; case 0x42: return K_F8;
    case 0x43: return K_F9; case 0x44: return K_F10;
    case 0x57: return K_F11; case 0x58: return K_F12;
    case 0x47: return K_HOME; case 0x48: return K_UPARROW;
    case 0x49: return K_PGUP; case 0x4b: return K_LEFTARROW;
    case 0x4d: return K_RIGHTARROW; case 0x4f: return K_END;
    case 0x50: return K_DOWNARROW; case 0x51: return K_PGDN;
    case 0x52: return K_INS; case 0x53: return K_DEL;
    default:
        return code >= 0 && code < (int)sizeof(ascii) ? ascii[code] : 0;
    }
}

static void queue_key(int pressed, int key)
{
    unsigned int tail;

    if (key == 0) return;
    if (queued_key_count == INPUT_QUEUE_CAPACITY) {
        key_head = (key_head + 1U) % INPUT_QUEUE_CAPACITY;
        queued_key_count--;
    }
    tail = (key_head + queued_key_count) % INPUT_QUEUE_CAPACITY;
    key_queue[tail].pressed = pressed != 0;
    key_queue[tail].key = key;
    queued_key_count++;
}

void QuakeAqua_QueueKey(int pressed, int code, int extended)
{
    queue_key(pressed, translate_scancode(code, extended));
}

void QuakeAqua_QueuePointer(int x, int y, int buttons, int wheel)
{
    int changed = pointer_buttons ^ buttons;
    int index;

    if (pointer_valid) {
        mouse_x += x - pointer_x;
        mouse_y += y - pointer_y;
    }
    pointer_x = x;
    pointer_y = y;
    pointer_valid = 1;
    for (index = 0; index < 3; index++) {
        int mask = 1 << index;

        if (changed & mask)
            queue_key((buttons & mask) != 0, K_MOUSE1 + index);
    }
    pointer_buttons = buttons;
    if (wheel != 0) {
        int key = wheel > 0 ? K_MWHEELUP : K_MWHEELDOWN;
        queue_key(1, key);
        queue_key(0, key);
    }
}

int QuakeAqua_PopKey(int *pressed, int *key)
{
    if (queued_key_count == 0 || pressed == NULL || key == NULL) return 0;
    *pressed = key_queue[key_head].pressed;
    *key = key_queue[key_head].key;
    key_head = (key_head + 1U) % INPUT_QUEUE_CAPACITY;
    queued_key_count--;
    return 1;
}

void QuakeAqua_TakeMouse(int *dx, int *dy)
{
    if (dx != NULL) *dx = mouse_x;
    if (dy != NULL) *dy = mouse_y;
    mouse_x = 0;
    mouse_y = 0;
}

void Sys_SendKeyEvents(void)
{
    int pressed;
    int key;

    while (QuakeAqua_PopKey(&pressed, &key))
        Key_Event(key, pressed != 0);
}

void IN_Init(void)
{
}

void IN_Shutdown(void)
{
}

void IN_Commands(void)
{
}

void IN_Move(usercmd_t *cmd)
{
    int dx;
    int dy;

    QuakeAqua_TakeMouse(&dx, &dy);
    if (key_dest != key_game || (dx == 0 && dy == 0)) return;
    cl.viewangles[YAW] -= m_yaw.value * dx * sensitivity.value;
    cl.viewangles[PITCH] += m_pitch.value * dy * sensitivity.value;
    if (cl.viewangles[PITCH] > 80) cl.viewangles[PITCH] = 80;
    if (cl.viewangles[PITCH] < -70) cl.viewangles[PITCH] = -70;
    V_StopPitchDrift();
    (void)cmd;
}

void IN_ClearStates(void)
{
    Key_ClearStates();
    key_head = 0;
    queued_key_count = 0;
    mouse_x = 0;
    mouse_y = 0;
    pointer_valid = 0;
    pointer_buttons = 0;
}
