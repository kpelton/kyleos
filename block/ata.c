#include <asm/asm.h>
#include <output/output.h>
#include <mm/mm.h>
#include <block/ata.h>
#include <fs/fat.h>
#include <locks/mutex.h>
#include <sched/sched.h>

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
//pink
#define PINK_PRIMARY 0x6eb0
//silver
#define SILVER_PRIMARY 0x30c8
//qemu
#define QEMU_PRIMARY 0x1f0
uint16_t ATA_PORTS[] = {
    0x9060,
    0x9068,
    0x9070,
    0x9078,
    0x1f0,
    0x170,
};
//Global to control hdd
static int PRIMARY = 0;

#define CMD_IDENTIFY 0xec
#define CMD_READ_SECTORS 0x20
#define CMD_WRITE_SECTORS 0x30
#define CMD_CACHE_FLUSH 0xe7
#define STAT_ERROR 0x01
#define STAT_DEVICE_FAULT 0x20
#define STAT_DRIVE_BUSY 0x80
#define STAT_PIO_READY 0x8
#define ATA_MAX_LBA 0x0fffffffU
#define ATA_MAX_SECTORS_PER_COMMAND 255U
#define ATA_POLL_LIMIT 10000000U
#define ATA_YIELD_INTERVAL 65536U
//#define ATA_DEBUG

/* Small read-through cache.  FAT metadata and directory sectors are read
 * repeatedly, so avoiding another PIO transaction is a substantial win. */
#define ATA_READ_CACHE_ENTRIES 512
struct ata_cache_entry {
    uint32_t sector;
    uint8_t valid;
    uint8_t data[ATA_SECTOR_SIZE];
};
static struct ata_cache_entry ata_read_cache[ATA_READ_CACHE_ENTRIES];
static uint32_t ata_cache_next;


struct mbr_info fs1;
static struct mutex ata_mutex;
static bool ata_init_port();
uint8_t read_status(void);

static void ata_cache_reset(void)
{
    for (uint32_t i = 0; i < ATA_READ_CACHE_ENTRIES; i++)
        ata_read_cache[i].valid = 0;
    ata_cache_next = 0;
}

static void ata_cache_invalidate(uint32_t sector)
{
    for (uint32_t i = 0; i < ATA_READ_CACHE_ENTRIES; i++) {
        if (ata_read_cache[i].valid && ata_read_cache[i].sector == sector)
            ata_read_cache[i].valid = 0;
    }
}

static void ata_poll_relax(uint32_t poll_count)
{
    asm volatile("pause");

    /* ATA is also used before the first task starts, when schedule() cannot
     * safely be entered.  Once scheduling is live, let unrelated tasks run
     * while a slow device is changing state. */
    if (poll_count != 0 &&
        poll_count % ATA_YIELD_INTERVAL == 0 &&
        sched_is_running())
        schedule();
}

static int ata_wait(bool require_drq)
{
    uint32_t poll_count;

    for (poll_count = 0; poll_count < ATA_POLL_LIMIT; poll_count++) {
        uint8_t status = read_status();

        if (status == 0 || status == 0xff)
            return -1;
        if ((status & STAT_DRIVE_BUSY) == 0) {
            if (status & (STAT_ERROR | STAT_DEVICE_FAULT))
                return -1;
            if (!require_drq || (status & STAT_PIO_READY))
                return 0;
        }
        ata_poll_relax(poll_count);
    }

    return -1;
}

static void ata_400ns_delay(void)
{
    read_status();
    read_status();
    read_status();
    read_status();
}

static int ata_begin_command(uint32_t sector, uint32_t count, uint8_t command)
{
    if (count == 0 || count > ATA_MAX_SECTORS_PER_COMMAND ||
        sector > ATA_MAX_LBA || count - 1 > ATA_MAX_LBA - sector)
        return -1;

    outb(PRIMARY + DRIVE_HEAD_REG, 0xe0 | ((sector >> 24) & 0x0f));
    outb(PRIMARY + FEATURES_REG, 0);
    outb(PRIMARY + SEC_COUNT_REG, count);
    outb(PRIMARY + SEC_NUM_REG, sector & 0xff);
    outb(PRIMARY + CYL_LOW_REG, (sector >> 8) & 0xff);
    outb(PRIMARY + CYL_HI_REG, (sector >> 16) & 0xff);
    outb(PRIMARY + COMMAND_REG, command);
    ata_400ns_delay();
    return 0;
}

static int ata_read_locked(uint32_t sector, uint32_t count, void *buffer)
{
    uint16_t *data = buffer;

    if (ata_begin_command(sector, count, CMD_READ_SECTORS) < 0)
        return -1;

    for (uint32_t current = 0; current < count; current++) {
        if (ata_wait(true) < 0)
            return -1;
        for (uint32_t word = 0; word < ATA_SECTOR_SIZE / 2; word++)
            *data++ = inw(PRIMARY + DATA_REG);
    }

    return ata_wait(false);
}

static int ata_write_locked(uint32_t sector, uint32_t count,
                            const void *buffer)
{
    const uint16_t *data = buffer;

    if (ata_begin_command(sector, count, CMD_WRITE_SECTORS) < 0)
        return -1;

    for (uint32_t current = 0; current < count; current++) {
        if (ata_wait(true) < 0)
            return -1;
        for (uint32_t word = 0; word < ATA_SECTOR_SIZE / 2; word++)
            outw(PRIMARY + DATA_REG, *data++);
    }

    if (ata_wait(false) < 0)
        return -1;
    outb(PRIMARY + COMMAND_REG, CMD_CACHE_FLUSH);
    ata_400ns_delay();
    return ata_wait(false);
}

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
int write_sectors(uint32_t sec, uint32_t count, const void *buffer)
{
    int result;

    if (buffer == NULL || count == 0 || count > ATA_MAX_SECTORS_PER_COMMAND)
        return -1;

    acquire_mutex(&ata_mutex);
    for (uint32_t i = 0; i < count; i++)
        ata_cache_invalidate(sec + i);
    result = ata_write_locked(sec, count, buffer);
    release_mutex(&ata_mutex);
    return result;
}

int write_sec(uint32_t sec, const void *buffer)
{
    return write_sectors(sec, 1, buffer);
}

int read_sec(uint32_t sec, void *buffer)
{
    int cache_entry;
    int result;

    if (buffer == NULL)
        return -1;
#ifdef ATA_DEBUG
    print_drive_status();
#endif
    acquire_mutex(&ata_mutex);

    for (cache_entry = 0; cache_entry < ATA_READ_CACHE_ENTRIES; cache_entry++) {
        if (ata_read_cache[cache_entry].valid &&
            ata_read_cache[cache_entry].sector == sec) {
            memcpy(buffer, ata_read_cache[cache_entry].data, ATA_SECTOR_SIZE);
            release_mutex(&ata_mutex);
            return 0;
        }
    }

    result = ata_read_locked(sec, 1, buffer);
    if (result < 0) {
        release_mutex(&ata_mutex);
        return result;
    }

    cache_entry = (int)ata_cache_next;
    ata_cache_next = (ata_cache_next + 1) % ATA_READ_CACHE_ENTRIES;
    ata_read_cache[cache_entry].sector = sec;
    memcpy(ata_read_cache[cache_entry].data, buffer, ATA_SECTOR_SIZE);
    ata_read_cache[cache_entry].valid = 1;
    release_mutex(&ata_mutex);

    return 0;
}
void ata_init(void) {
    bool passed = false;

    init_mutex(&ata_mutex);
    for(uint64_t i = 0; i < sizeof(ATA_PORTS) / sizeof(ATA_PORTS[0]); i++) {
        PRIMARY = ATA_PORTS[i];
        if(ata_init_port() == true) {
            passed = true;
            break;
        }
    }
    if (passed == false)
        panic("PANIC:ATA init failed.. could not find MBR");
}

static bool ata_init_port()
{
    uint32_t part_start;
    uint32_t part_size;
    uint16_t valid_mbr;
    uint16_t part_type;
    bool passed = false;
    uint8_t mbr[ATA_SECTOR_SIZE];
    ata_cache_reset();
    kprintf("ATA init\n");
    if (read_sec(0, mbr) < 0)
        return false;
    part_start = mbr[0x1c6] | mbr[0x1c7] << 8 | mbr[0x1c8] << 16 | mbr[0x1c9] << 24;
    part_size = mbr[0x1ca] | mbr[0x1cb] << 8 | mbr[0x1cc] << 16 | mbr[0x1cd] << 24;
    valid_mbr = mbr[0x1fe] | mbr[0x1ff] << 8;
    part_type = mbr[0x1c2];
    kprintf("Trying port 0x%x\n",PRIMARY);
    kprintf("mbr:0x%x\n",valid_mbr);
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


        passed = true;
    }
    return passed;
}
