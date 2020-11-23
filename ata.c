#include "asm.h"
#include "output.h"
#include "mm.h"

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
#define CMD_READ_SECTORS 0x20
#define STAT_DRIVE_BUSY  0x80
#define STAT_PIO_READY  0x8

unsigned char read_status(void) {
    unsigned char status;
    status = inb(PRIMARY + STATUS_REG);
    return status;
}

void print_drive_status(void) {

    char cbuffer[10];
    unsigned char status;

    status = read_status();
    itoa(status,cbuffer,16);
    kprintf("ATA STATUS:");
    kprintf(cbuffer);
    kprintf("\n");

}
int read_sec(unsigned int sec,void *buffer  ) {
    short *data =0;
    unsigned char status;
    int i;

    data = buffer;
    print_drive_status();
    outb(PRIMARY + DRIVE_HEAD_REG,0xE0);
    outb(PRIMARY + ERROR_REG,0x00);
    //read 1 sectors =0x200
    outb(PRIMARY + SEC_COUNT_REG,1);
    outb(PRIMARY + SEC_NUM_REG,sec & 0xff);
    outb(PRIMARY + CYL_LOW_REG, (sec &0xff00) >>8);
    //outb(PRIMARY + CYL_LOW_REG,10);
    outb(PRIMARY + CYL_HI_REG,(sec &0xff0000) >>16);
    outb(PRIMARY + COMMAND_REG,CMD_READ_SECTORS);
    status = read_status();

    while ((status & STAT_PIO_READY) != STAT_PIO_READY && (status & STAT_DRIVE_BUSY  ) != 0) {
        status = read_status();
        print_drive_status();
    }
    //data is ready
    for (i=0; i<256; i++) {
        data[i] = inw(0x1f0);
        //itoa_16(data[i],cbuffer,16);
        //kprintf(cbuffer);
        //kprintf(" ");
    }
    //kprintf("\n");
    return 0;
}
void ata_init(void)
{
    unsigned int part_start;
    unsigned int part_size;
    unsigned short valid_mbr;

    char mbr[512];
    char cbuffer[10];
    kprintf("ATA init\n");
    read_sec(0,mbr);
    part_start = mbr[0x1c6] | mbr[0x1c7] <<8 | mbr[0x1c8] <<16| mbr[0x1c9] <<24;
    part_size = mbr[0x1ca] | mbr[0x1cb] <<8 | mbr[0x1cc] <<16| mbr[0x1cd] <<24;
    valid_mbr = mbr[0x1fe] | mbr[0x1ff] <<8 ;


    //Look for valid MBR 0x55aa
    if (valid_mbr == 0xaa55) {
        kprintf("first part\n");
        itoa(part_start,cbuffer,16);
        kprintf(cbuffer);
        kprintf("\n");

        kprintf("part_size\n");
        itoa(part_size,cbuffer,16);
        kprintf(cbuffer);
        kprintf("\n");
    }
    else {
        kprintf("PANIC:ATA init failed.. could not find MBR");
        asm("cli; hlt");
    }

}



