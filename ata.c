#include "asm.h"
#include "output.h"
#define DATA_REG 0
#define ERROR_REG 1
#define FEATURES_REG 1
#define SEC_COUNT_REG 2
#define SEC_NUM_REG 3
#define CYL_LOW_REG 4
#define CYL_HI_REG 5
#define DRIVE_HEAD_REG 6
#define COMMAND_REG  7
#define STATUS_REG  7

#define PRIMARY 0x1f0

#define CMD_IDENTIFY 0xec
void ata_init(void)
{
    char buffer[10];
    unsigned short test[512];
    unsigned short test2[512];
    unsigned short val;
    unsigned short swap;
    int i;
    kprintf("ATA init\n");
    outb(PRIMARY + DRIVE_HEAD_REG,0xA0);
    outb(PRIMARY + SEC_COUNT_REG,0x0);
    outb(PRIMARY + SEC_NUM_REG,0x0);
    outb(PRIMARY + COMMAND_REG,CMD_IDENTIFY);

    for (i=0; i<512; i++) {
        val = inw(0x1f0);
        swap = val &0xff;
        swap <<=8;
        swap |= val>>8;
        test[i] = swap; 
        test2[i] = val; 
    }
    kprintf(((unsigned char *)test)+54);
    kprintf("Cylindars\n");
    itoa(test2[1],buffer,16);
    kprintf(buffer);
}



