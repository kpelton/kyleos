// crappy pio ATA driver
#include <asm/asm.h>
#include <output/output.h>
#include <mm/mm.h>
#include <block/ata.h>
#include <fs/fat.h>
#include <locks/mutex.h>

#define DATA_REG 0
#define ERROR_REG 1
#define FEATURES_REG 1
#define SEC_COUNT_REG 2
#define SEC_NUM_REG 3
#define CYL_LOW_REG 4
#define CYL_HI_REG 5
#define DRIVE_HEAD_REG 6
#define COMMAND_REG 7
#define STATUS_REG 7
#define PRIMARY 0x1f0

#define CMD_IDENTIFY 0xec
#define CMD_READ_SECTORS 0x20
#define CMD_WRITE_SECTORS 0x30
#define CMD_CACHE_FLUSH 0xe7
#define STAT_DRIVE_BUSY 0x80
#define STAT_PIO_READY 0x8
//#define ATA_DEBUG

struct mbr_info fs1;
static struct mutex ata_mutex;

uint8_t read_status(void)
{
    uint8_t status;
    status = inb(PRIMARY + STATUS_REG);
    return status;
}

void print_drive_status(void)
{
    kprintf("ATA STATUS:0x%x\n", read_status());
}
int write_sec(uint32_t sec, void *buffer)
{
    short *data = 0;
    uint8_t status;
    int i;
    data = buffer;

#ifdef ATA_DEBUG
    print_drive_status();
#endif
    outb(PRIMARY + DRIVE_HEAD_REG, 0xE0);
    outb(PRIMARY + ERROR_REG, 0x00);
    // read 1 sectors =0x200
    outb(PRIMARY + SEC_COUNT_REG, 1);
    outb(PRIMARY + SEC_NUM_REG, sec & 0xff);
    outb(PRIMARY + CYL_LOW_REG, (sec & 0xff00) >> 8);
    // outb(PRIMARY + CYL_LOW_REG,10);
    outb(PRIMARY + CYL_HI_REG, (sec & 0xff0000) >> 16);
    outb(PRIMARY + COMMAND_REG, CMD_WRITE_SECTORS);
    status = read_status();

    while ((status & STAT_PIO_READY) != STAT_PIO_READY && (status & STAT_DRIVE_BUSY) != 0)
    {
        status = read_status();
#ifdef ATA_DEBUG
        print_drive_status();
#endif
    }

    // Data is not quite ready, need to read status a few more times
    for (i = 0; i < 100; i++)
        status = read_status();

    // data is ready
    for (i = 0; i < 256; i++)
    {
        /// kprintf("writing\n");
        outw(PRIMARY, data[i]);
        status = read_status();
        status = read_status();
        status = read_status();
        while ((status & STAT_PIO_READY) != STAT_PIO_READY && (status & STAT_DRIVE_BUSY) != 0)
            status = read_status();
        // print_drive_status();
    }
    return 0;
}
int read_sec(uint32_t sec, void *buffer)
{
    short *data = 0;
    uint8_t status;
    int i;
    data = buffer;
#ifdef ATA_DEBUG
    print_drive_status();
#endif

    outb(PRIMARY + DRIVE_HEAD_REG, 0xE0);
    outb(PRIMARY + ERROR_REG, 0x00);
    // read 1 sectors =0x200
    outb(PRIMARY + SEC_COUNT_REG, 1);
    outb(PRIMARY + SEC_NUM_REG, sec & 0xff);
    outb(PRIMARY + CYL_LOW_REG, (sec & 0xff00) >> 8);
    // outb(PRIMARY + CYL_LOW_REG,10);
    outb(PRIMARY + CYL_HI_REG, (sec & 0xff0000) >> 16);
    outb(PRIMARY + COMMAND_REG, CMD_READ_SECTORS);
    outb(PRIMARY + COMMAND_REG, CMD_CACHE_FLUSH);

    status = read_status();

    while ((status & STAT_PIO_READY) != STAT_PIO_READY && (status & STAT_DRIVE_BUSY) != 0)
    {
        status = read_status();
#ifdef ATA_DEBUG
        print_drive_status();
#endif
    }
    acquire_mutex(&ata_mutex);

    // data is ready
    for (i = 0; i < 256; i++)
    {
        while ((status & STAT_PIO_READY) != STAT_PIO_READY && (status & STAT_DRIVE_BUSY) != 0)
            status = read_status();
        data[i] = inw(PRIMARY);
    }
    status = read_status();
    release_mutex(&ata_mutex);

    return 0;
}

void ata_init(void)
{
    uint32_t part_start;
    uint32_t part_size;
    uint16_t valid_mbr;
    uint16_t part_type;

    char mbr[512];
    kprintf("ATA init\n");
    read_sec(0, mbr);
    part_start = mbr[0x1c6] | mbr[0x1c7] << 8 | mbr[0x1c8] << 16 | mbr[0x1c9] << 24;
    part_size = mbr[0x1ca] | mbr[0x1cb] << 8 | mbr[0x1cc] << 16 | mbr[0x1cd] << 24;
    valid_mbr = mbr[0x1fe] | mbr[0x1ff] << 8;
    part_type = mbr[0x1c2];
    init_mutex(&ata_mutex);

    // Look for valid MBR 0x55aa
    if (valid_mbr == 0xaa55)
    {
        // set FS info
        fs1.fs_start = part_start;
        fs1.fs_size = part_size;
        fs1.part_type = part_type;
        kprintf("MBR INFO:\n");
        kprintf("fs start: 0x%x\n", fs1.fs_start);
        kprintf("fs size:  0x%x\n", fs1.fs_size);
        kprintf("part_type: 0x%x\n", fs1.part_type);

        fat_init(fs1);
    }
    else
    {
        kprintf("PANIC:ATA init failed.. could not find MBR");
        asm("cli; hlt");
    }
}
