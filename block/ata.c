#include <asm/asm.h>
#include <output/output.h>
#include <mm/mm.h>
#include <block/ata.h>
#include <drivers/pci.h>
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
#define PCI_IDE_PRIMARY_NATIVE 0x01
#define PCI_IDE_SECONDARY_NATIVE 0x04
#define ATA_PRIMARY_COMPAT_PORT 0x1f0
#define ATA_SECONDARY_COMPAT_PORT 0x170

/* Command block selected from the PCI IDE programming interface and BARs. */
static uint16_t PRIMARY;

#define CMD_IDENTIFY 0xec
#define CMD_READ_SECTORS 0x20
#define CMD_WRITE_SECTORS 0x30
#define CMD_CACHE_FLUSH 0xe7
#define STAT_ERROR 0x01
#define STAT_DEVICE_FAULT 0x20
#define STAT_DRIVE_BUSY 0x80
#define STAT_PIO_READY 0x8
#define ATA_MAX_LBA 0x0fffffffU
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


struct mbr_info fs1;
static struct mutex ata_mutex;
static bool ata_init_port();
static uint16_t ata_channel_port(const struct pci_device *controller,
                                 uint8_t channel);
uint8_t read_status(void);

static void ata_cache_reset(void)
{
    for (uint32_t i = 0; i < ATA_READ_CACHE_ENTRIES; i++)
        ata_read_cache[i].valid = 0;
}

static void ata_cache_invalidate(uint32_t sector)
{
    struct ata_cache_entry *entry =
        &ata_read_cache[sector % ATA_READ_CACHE_ENTRIES];

    if (entry->valid && entry->sector == sector)
        entry->valid = 0;
}

static struct ata_cache_entry *ata_cache_find(uint32_t sector)
{
    struct ata_cache_entry *entry =
        &ata_read_cache[sector % ATA_READ_CACHE_ENTRIES];

    if (!entry->valid || entry->sector != sector)
        return NULL;
    return entry;
}

static void ata_cache_store(uint32_t sector, const void *data)
{
    struct ata_cache_entry *entry =
        &ata_read_cache[sector % ATA_READ_CACHE_ENTRIES];

    entry->sector = sector;
    memcpy(entry->data, data, ATA_SECTOR_SIZE);
    entry->valid = 1;
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

int read_sectors(uint32_t sec, uint32_t count, void *buffer)
{
    int result;

    if (buffer == NULL || count == 0 ||
        count > ATA_MAX_SECTORS_PER_COMMAND)
        return -1;

    acquire_mutex(&ata_mutex);
    result = ata_read_locked(sec, count, buffer);
    if (result == 0) {
        for (uint32_t i = 0; i < count; i++)
            ata_cache_store(sec + i,
                            (const uint8_t *)buffer + i * ATA_SECTOR_SIZE);
    }
    release_mutex(&ata_mutex);
    return result;
}

int read_sec(uint32_t sec, void *buffer)
{
    struct ata_cache_entry *cache_entry;
    int result;

    if (buffer == NULL)
        return -1;
#ifdef ATA_DEBUG
    print_drive_status();
#endif
    acquire_mutex(&ata_mutex);

    cache_entry = ata_cache_find(sec);
    if (cache_entry != NULL) {
        memcpy(buffer, cache_entry->data, ATA_SECTOR_SIZE);
        release_mutex(&ata_mutex);
        return 0;
    }

    result = ata_read_locked(sec, 1, buffer);
    if (result < 0) {
        release_mutex(&ata_mutex);
        return result;
    }

    ata_cache_store(sec, buffer);
    release_mutex(&ata_mutex);

    return 0;
}
void ata_init(void) {
    bool passed = false;
    uint32_t controller_index = 0;
    const struct pci_device *controller;

    init_mutex(&ata_mutex);

    while ((controller = pci_find_class(PCI_CLASS_MASS_STORAGE,
                                         PCI_SUBCLASS_IDE,
                                         controller_index++)) != NULL) {
        pci_enable_io(controller);
        kprintf("ATA: PCI IDE controller %d:%d.%d prog 0x%x\n",
                controller->bus, controller->slot, controller->function,
                controller->prog_if);

        for (uint8_t channel = 0; channel < 2; channel++) {
            PRIMARY = ata_channel_port(controller, channel);
            if (PRIMARY == 0)
                continue;
            if (ata_init_port()) {
                passed = true;
                break;
            }
        }
        if (passed)
            break;
    }

    if (passed == false)
        panic("PANIC: no MBR on a supported PCI IDE controller");
}

static uint16_t ata_channel_port(const struct pci_device *controller,
                                 uint8_t channel)
{
    uint8_t native_mask = channel == 0 ? PCI_IDE_PRIMARY_NATIVE :
                                         PCI_IDE_SECONDARY_NATIVE;
    uint8_t bar_index = channel == 0 ? 0 : 2;

    if ((controller->prog_if & native_mask) != 0) {
        uint16_t bar_port = pci_bar_io_base(controller, bar_index);

        if (bar_port == 0) {
            kprintf("ATA: native channel %d has no I/O BAR\n", channel);
            return 0;
        }
        return bar_port;
    }

    /* PCI IDE compatibility mode defines these command block addresses. */
    return channel == 0 ? ATA_PRIMARY_COMPAT_PORT :
                          ATA_SECONDARY_COMPAT_PORT;
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
    kprintf("ATA: trying command port 0x%x\n", PRIMARY);
    if (read_sec(0, mbr) < 0)
        return false;
    part_start = mbr[0x1c6] | mbr[0x1c7] << 8 | mbr[0x1c8] << 16 | mbr[0x1c9] << 24;
    part_size = mbr[0x1ca] | mbr[0x1cb] << 8 | mbr[0x1cc] << 16 | mbr[0x1cd] << 24;
    valid_mbr = mbr[0x1fe] | mbr[0x1ff] << 8;
    part_type = mbr[0x1c2];
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
