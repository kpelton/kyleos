#include <asm/asm.h>

void pit_init(void) {
    asm("cli");
    outb(0x43,0x34);
    //set 100hz 11931 0x2e9b
    outb(0x40,0x9b);
    outb(0x40,0x2e);
    asm("sti");
}

