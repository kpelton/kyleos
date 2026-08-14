/* KyleOS keyboard backend. Mouse and joystick support are intentionally absent. */
#include "quakedef.h"
#include "kyleos_syscalls.h"

static int translate_scancode(unsigned char code, unsigned char extended)
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
    case 0x3b: return K_F1;
    case 0x3c: return K_F2;
    case 0x3d: return K_F3;
    case 0x3e: return K_F4;
    case 0x3f: return K_F5;
    case 0x40: return K_F6;
    case 0x41: return K_F7;
    case 0x42: return K_F8;
    case 0x43: return K_F9;
    case 0x44: return K_F10;
    case 0x57: return K_F11;
    case 0x58: return K_F12;
    case 0x47: return K_HOME;
    case 0x48: return K_UPARROW;
    case 0x49: return K_PGUP;
    case 0x4b: return K_LEFTARROW;
    case 0x4d: return K_RIGHTARROW;
    case 0x4f: return K_END;
    case 0x50: return K_DOWNARROW;
    case 0x51: return K_PGDN;
    case 0x52: return K_INS;
    case 0x53: return K_DEL;
    default:
        if (code < sizeof(ascii))
            return ascii[code];
        return 0;
    }
}

void Sys_SendKeyEvents(void)
{
    int pressed;
    unsigned char code;
    unsigned char extended;

    while (kyleos_key_poll(&pressed, &code, &extended) > 0) {
        int key = translate_scancode(code, extended);
        if (key != 0)
            Key_Event(key, pressed != 0);
    }
}

void IN_Init(void)
{
    kyleos_tty_raw(1);
}

void IN_Shutdown(void)
{
    kyleos_tty_raw(0);
}

void IN_Commands(void)
{
}

void IN_Move(usercmd_t *cmd)
{
    (void)cmd;
}

void IN_ClearStates(void)
{
    Key_ClearStates();
}
