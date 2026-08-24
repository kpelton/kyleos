#include <asm/asm.h>
#include <output/output.h>
#include <mm/mm.h>
#include <block/ata.h>
#include <drivers/pci.h>
#include <fs/fat.h>
#include <irq/irq.h>
#include <locks/mutex.h>
#include <mm/paging.h>
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
#define PCI_IDE_BUS_MASTER_CAPABLE 0x80
#define ATA_PRIMARY_COMPAT_PORT 0x1f0
#define ATA_SECONDARY_COMPAT_PORT 0x170
#define ATA_PRIMARY_COMPAT_CONTROL 0x3f6
#define ATA_SECONDARY_COMPAT_CONTROL 0x376
#define ATA_PRIMARY_IRQ 14
#define ATA_SECONDARY_IRQ 15

struct ata_channel {
    uint16_t command_base;
    uint16_t control_port;
    uint16_t bus_master_base;
    uint8_t irq;
    uint8_t index;
    bool compatibility_mode;
};

/* Active channel selected by MBR probing. */
static struct ata_channel ata_channel;

#define CMD_IDENTIFY 0xec
#define CMD_READ_SECTORS 0x20
#define CMD_WRITE_SECTORS 0x30
#define CMD_READ_DMA 0xc8
#define CMD_WRITE_DMA 0xca
#define CMD_CACHE_FLUSH 0xe7
#define STAT_ERROR 0x01
#define STAT_DEVICE_FAULT 0x20
#define STAT_DRIVE_BUSY 0x80
#define STAT_PIO_READY 0x8
#define ATA_MAX_LBA 0x0fffffffU
#define ATA_POLL_LIMIT 10000000U
#define ATA_YIELD_INTERVAL 65536U
#define ATA_DMA_MAX_SECTORS 64U
#define ATA_DMA_BUFFER_SIZE (ATA_DMA_MAX_SECTORS * ATA_SECTOR_SIZE)
#define ATA_DMA_TIMEOUT_MS 2000U
#define ATA_DMA_FAST_POLL_LIMIT 4096U

#define BM_COMMAND_REG 0
#define BM_STATUS_REG 2
#define BM_PRDT_REG 4
#define BM_COMMAND_START 0x01
#define BM_COMMAND_READ 0x08
#define BM_STATUS_ACTIVE 0x01
#define BM_STATUS_ERROR 0x02
#define BM_STATUS_INTERRUPT 0x04
#define ATA_DEVICE_CONTROL_NIEN 0x02

#define ATA_PRD_END 0x8000
//#define ATA_DEBUG

struct ata_prd {
    uint32_t buffer_phys;
    uint16_t byte_count;
    uint16_t flags;
} __attribute__((packed));

/* Legacy PCI IDE DMA accepts 32-bit physical addresses and a PRD may not
 * cross a 64 KiB boundary.  A fixed, aligned bounce buffer satisfies both
 * constraints and lets callers continue passing arbitrary kernel buffers. */
static uint8_t ata_dma_buffer[ATA_DMA_BUFFER_SIZE]
    __attribute__((aligned(65536)));
static struct ata_prd ata_dma_prdt __attribute__((aligned(16)));
static volatile int ata_dma_inflight;
static volatile int ata_dma_result;
static bool ata_dma_enabled;

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
static bool ata_configure_channel(const struct pci_device *controller,
                                  uint8_t channel);
static bool ata_dma_init(const struct pci_device *controller);
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

static int ata_program_taskfile(uint32_t sector, uint32_t count)
{
    if (count == 0 || count > ATA_MAX_SECTORS_PER_COMMAND ||
        sector > ATA_MAX_LBA || count - 1 > ATA_MAX_LBA - sector)
        return -1;

    outb(ata_channel.command_base + DRIVE_HEAD_REG,
         0xe0 | ((sector >> 24) & 0x0f));
    outb(ata_channel.command_base + FEATURES_REG, 0);
    outb(ata_channel.command_base + SEC_COUNT_REG, count);
    outb(ata_channel.command_base + SEC_NUM_REG, sector & 0xff);
    outb(ata_channel.command_base + CYL_LOW_REG, (sector >> 8) & 0xff);
    outb(ata_channel.command_base + CYL_HI_REG, (sector >> 16) & 0xff);
    return 0;
}

static int ata_begin_command(uint32_t sector, uint32_t count, uint8_t command)
{
    if (ata_program_taskfile(sector, count) < 0)
        return -1;

    outb(ata_channel.command_base + COMMAND_REG, command);
    ata_400ns_delay();
    return 0;
}

static int ata_pio_read_locked(uint32_t sector, uint32_t count, void *buffer)
{
    uint16_t *data = buffer;

    if (ata_begin_command(sector, count, CMD_READ_SECTORS) < 0)
        return -1;

    for (uint32_t current = 0; current < count; current++) {
        if (ata_wait(true) < 0)
            return -1;
        for (uint32_t word = 0; word < ATA_SECTOR_SIZE / 2; word++)
            *data++ = inw(ata_channel.command_base + DATA_REG);
    }

    return ata_wait(false);
}

static int ata_pio_write_locked(uint32_t sector, uint32_t count,
                                const void *buffer)
{
    const uint16_t *data = buffer;

    if (ata_begin_command(sector, count, CMD_WRITE_SECTORS) < 0)
        return -1;

    for (uint32_t current = 0; current < count; current++) {
        if (ata_wait(true) < 0)
            return -1;
        for (uint32_t word = 0; word < ATA_SECTOR_SIZE / 2; word++)
            outw(ata_channel.command_base + DATA_REG, *data++);
    }

    if (ata_wait(false) < 0)
        return -1;
    outb(ata_channel.command_base + COMMAND_REG, CMD_CACHE_FLUSH);
    ata_400ns_delay();
    return ata_wait(false);
}

static void ata_dma_stop(void)
{
    uint8_t command = inb(ata_channel.bus_master_base + BM_COMMAND_REG);

    outb(ata_channel.bus_master_base + BM_COMMAND_REG,
         command & ~BM_COMMAND_START);
}

static void ata_dma_clear_status(void)
{
    uint8_t status = inb(ata_channel.bus_master_base + BM_STATUS_REG);

    outb(ata_channel.bus_master_base + BM_STATUS_REG,
         status | BM_STATUS_ERROR | BM_STATUS_INTERRUPT);
}

static void ata_dma_disable(const char *reason)
{
    if (!ata_dma_enabled)
        return;

    ata_dma_stop();
    read_status();
    ata_dma_clear_status();
    outb(ata_channel.control_port, ATA_DEVICE_CONTROL_NIEN);
    IRQ_set_mask(ata_channel.irq);
    ata_dma_enabled = false;
    klog("ATA: DMA disabled: %s\n", reason);
}

static bool ata_dma_poll_completion(void)
{
    uint8_t bus_master_status;
    uint8_t device_status;

    if (ata_dma_inflight == 0)
        return true;
    bus_master_status =
        inb(ata_channel.bus_master_base + BM_STATUS_REG);
    if ((bus_master_status & BM_STATUS_INTERRUPT) == 0)
        return false;

    /* Reading the device status acknowledges the IDE interrupt.  This path
     * is especially important while servicing a page fault, where the CPU
     * entered with interrupts masked and cannot run the normal IRQ wrapper. */
    device_status = read_status();
    ata_dma_stop();
    outb(ata_channel.bus_master_base + BM_STATUS_REG,
         bus_master_status | BM_STATUS_ERROR | BM_STATUS_INTERRUPT);
    ata_dma_result =
        ((bus_master_status & (BM_STATUS_ACTIVE | BM_STATUS_ERROR)) != 0 ||
         (device_status & (STAT_ERROR | STAT_DEVICE_FAULT)) != 0) ?
        -1 : 0;
    __sync_synchronize();
    ata_dma_inflight = 0;
    return true;
}

static int ata_dma_wait(void)
{
    bool timed_out = false;
    bool restore_interrupts;

    /* Most legacy IDE DMA commands finish in a few microseconds.  Sleeping
     * immediately makes every small FAT or demand-page read pay for a full
     * scheduler round trip, which is much more expensive than the transfer
     * itself.  Poll the controller's completion bit briefly before sleeping.
     * Unlike waiting only for the IRQ wrapper, this also works during demand
     * paging, whose exception gate enters with CPU interrupts masked. */
    for (uint32_t poll = 0; poll < ATA_DMA_FAST_POLL_LIMIT; poll++) {
        if (ata_dma_poll_completion())
            return ata_dma_result;
        asm volatile("pause" ::: "memory");
    }

    sched_sleep_on_if_timeout((void *)&ata_dma_inflight,
                              &ata_dma_inflight,
                              ATA_DMA_TIMEOUT_MS);

    if (ata_dma_inflight != 0) {
        restore_interrupts =
            (get_flags_reg() & INTERRUPT_ENABLE_FLAG) != 0;
        asm volatile("cli" ::: "memory");
        if (ata_dma_inflight != 0) {
            ata_dma_stop();
            read_status();
            ata_dma_clear_status();
            ata_dma_result = -1;
            __sync_synchronize();
            ata_dma_inflight = 0;
            timed_out = true;
        }
        if (restore_interrupts)
            asm volatile("sti" ::: "memory");
    }

    if (timed_out)
        klog("ATA: DMA request timed out\n");
    return ata_dma_result;
}

static int ata_dma_chunk_locked(uint32_t sector, uint32_t count,
                                void *buffer, bool write)
{
    uint32_t byte_count;
    uint8_t bus_master_command;

    if (!ata_dma_enabled || count == 0 ||
        count > ATA_DMA_MAX_SECTORS)
        return -1;

    byte_count = count * ATA_SECTOR_SIZE;
    if (write)
        memcpy(ata_dma_buffer, buffer, byte_count);

    if (ata_wait(false) < 0 ||
        ata_program_taskfile(sector, count) < 0)
        return -1;

    ata_dma_stop();
    ata_dma_clear_status();

    ata_dma_prdt.buffer_phys =
        (uint32_t)KERN_VIRT_TO_PHYS(ata_dma_buffer);
    ata_dma_prdt.byte_count = (uint16_t)byte_count;
    ata_dma_prdt.flags = ATA_PRD_END;
    __sync_synchronize();

    outl(ata_channel.bus_master_base + BM_PRDT_REG,
         (uint32_t)KERN_VIRT_TO_PHYS(&ata_dma_prdt));
    bus_master_command = write ? 0 : BM_COMMAND_READ;
    outb(ata_channel.bus_master_base + BM_COMMAND_REG,
         bus_master_command);

    ata_dma_result = -1;
    ata_dma_inflight = 1;
    __sync_synchronize();

    outb(ata_channel.command_base + COMMAND_REG,
         write ? CMD_WRITE_DMA : CMD_READ_DMA);
    outb(ata_channel.bus_master_base + BM_COMMAND_REG,
         bus_master_command | BM_COMMAND_START);

    if (ata_dma_wait() < 0)
        return -1;

    __sync_synchronize();
    if (!write)
        memcpy(buffer, ata_dma_buffer, byte_count);
    return 0;
}

static int ata_dma_transfer_locked(uint32_t sector, uint32_t count,
                                   void *buffer, bool write)
{
    uint8_t *data = buffer;

    while (count != 0) {
        uint32_t chunk = count > ATA_DMA_MAX_SECTORS ?
                         ATA_DMA_MAX_SECTORS : count;

        if (ata_dma_chunk_locked(sector, chunk, data, write) < 0)
            return -1;
        sector += chunk;
        count -= chunk;
        data += chunk * ATA_SECTOR_SIZE;
    }

    if (write) {
        if (ata_wait(false) < 0)
            return -1;
        outb(ata_channel.command_base + COMMAND_REG, CMD_CACHE_FLUSH);
        ata_400ns_delay();
        if (ata_wait(false) < 0)
            return -1;
    }
    return 0;
}

static int ata_read_locked(uint32_t sector, uint32_t count, void *buffer)
{
    if (ata_dma_enabled && sched_is_running()) {
        if (ata_dma_transfer_locked(sector, count, buffer, false) == 0)
            return 0;
        ata_dma_disable("read failure; using PIO fallback");
    }
    return ata_pio_read_locked(sector, count, buffer);
}

static int ata_write_locked(uint32_t sector, uint32_t count,
                            const void *buffer)
{
    if (ata_dma_enabled && sched_is_running()) {
        if (ata_dma_transfer_locked(sector, count, (void *)buffer, true) == 0)
            return 0;
        /* Retrying an uncertain multi-sector write could duplicate a partial
         * transfer, so disable DMA and report the failure to the caller. */
        ata_dma_disable("write failure");
        return -1;
    }
    return ata_pio_write_locked(sector, count, buffer);
}

static void ata_dma_irq(uint8_t irq)
{
    uint8_t bus_master_status;
    uint8_t device_status;

    if (!ata_dma_enabled || ata_channel.irq != irq) {
        PIC_sendEOI(irq);
        return;
    }

    bus_master_status =
        inb(ata_channel.bus_master_base + BM_STATUS_REG);
    device_status = read_status();
    ata_dma_stop();
    outb(ata_channel.bus_master_base + BM_STATUS_REG,
         bus_master_status | BM_STATUS_ERROR | BM_STATUS_INTERRUPT);

    if (ata_dma_inflight != 0) {
        ata_dma_result =
            ((bus_master_status & (BM_STATUS_ACTIVE | BM_STATUS_ERROR)) != 0 ||
             (device_status & (STAT_ERROR | STAT_DEVICE_FAULT)) != 0) ?
            -1 : 0;
        __sync_synchronize();
        ata_dma_inflight = 0;
        sched_wakeup_one((void *)&ata_dma_inflight);
    }

    PIC_sendEOI(irq);
}

void ata_primary_irq(void)
{
    ata_dma_irq(ATA_PRIMARY_IRQ);
}

void ata_secondary_irq(void)
{
    ata_dma_irq(ATA_SECONDARY_IRQ);
}

uint8_t read_status(void)
{
    uint8_t status;
    status = inb(ata_channel.command_base + STATUS_REG);
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
    ata_dma_enabled = false;
    ata_dma_inflight = 0;

    while ((controller = pci_find_class(PCI_CLASS_MASS_STORAGE,
                                         PCI_SUBCLASS_IDE,
                                         controller_index++)) != NULL) {
        pci_enable_io(controller);
        kprintf("ATA: PCI IDE controller %d:%d.%d prog 0x%x\n",
                controller->bus, controller->slot, controller->function,
                controller->prog_if);

        for (uint8_t channel = 0; channel < 2; channel++) {
            if (!ata_configure_channel(controller, channel))
                continue;
            if (ata_init_port()) {
                passed = true;
                ata_dma_init(controller);
                break;
            }
        }
        if (passed)
            break;
    }

    if (passed == false)
        panic("PANIC: no MBR on a supported PCI IDE controller");
}

static bool ata_configure_channel(const struct pci_device *controller,
                                  uint8_t channel)
{
    uint8_t native_mask = channel == 0 ? PCI_IDE_PRIMARY_NATIVE :
                                         PCI_IDE_SECONDARY_NATIVE;
    uint8_t command_bar = channel == 0 ? 0 : 2;
    uint8_t control_bar = channel == 0 ? 1 : 3;
    bool native_mode = (controller->prog_if & native_mask) != 0;

    ata_channel.index = channel;
    ata_channel.compatibility_mode = !native_mode;
    ata_channel.bus_master_base = 0;

    if (native_mode) {
        uint16_t command_base =
            pci_bar_io_base(controller, command_bar);
        uint16_t control_base =
            pci_bar_io_base(controller, control_bar);

        if (command_base == 0 || control_base == 0) {
            klog("ATA: native channel %d has incomplete I/O BARs\n",
                 channel);
            return false;
        }
        ata_channel.command_base = command_base;
        ata_channel.control_port = control_base + 2;
        ata_channel.irq = controller->interrupt_line;
    } else if (channel == 0) {
        ata_channel.command_base = ATA_PRIMARY_COMPAT_PORT;
        ata_channel.control_port = ATA_PRIMARY_COMPAT_CONTROL;
        ata_channel.irq = ATA_PRIMARY_IRQ;
    } else {
        ata_channel.command_base = ATA_SECONDARY_COMPAT_PORT;
        ata_channel.control_port = ATA_SECONDARY_COMPAT_CONTROL;
        ata_channel.irq = ATA_SECONDARY_IRQ;
    }

    if (pci_bar_is_io(controller, 4))
        ata_channel.bus_master_base =
            pci_bar_io_base(controller, 4) + channel * 8;
    return true;
}

static bool ata_dma_init(const struct pci_device *controller)
{
    uint64_t buffer_phys = KERN_VIRT_TO_PHYS(ata_dma_buffer);
    uint64_t prdt_phys = KERN_VIRT_TO_PHYS(&ata_dma_prdt);

    if ((controller->prog_if & PCI_IDE_BUS_MASTER_CAPABLE) == 0 ||
        ata_channel.bus_master_base == 0) {
        klog("ATA: controller has no PCI bus-master interface; using PIO\n");
        return false;
    }
    /* Native PCI interrupt routing is not yet represented by the legacy PIC
     * IDT setup.  Continue to support native task-file BARs through PIO. */
    if (!ata_channel.compatibility_mode ||
        (ata_channel.irq != ATA_PRIMARY_IRQ &&
         ata_channel.irq != ATA_SECONDARY_IRQ)) {
        klog("ATA: native IRQ routing unsupported for DMA; using PIO\n");
        return false;
    }
    if (buffer_phys > 0xffffffffULL - (ATA_DMA_BUFFER_SIZE - 1) ||
        prdt_phys > 0xffffffffULL ||
        (buffer_phys & 0xffff) + ATA_DMA_BUFFER_SIZE > 0x10000) {
        klog("ATA: DMA memory does not satisfy legacy PCI limits; using PIO\n");
        return false;
    }
    if (!pci_enable_bus_master(controller)) {
        klog("ATA: PCI bus mastering could not be enabled; using PIO\n");
        return false;
    }

    ata_dma_prdt.buffer_phys = (uint32_t)buffer_phys;
    ata_dma_prdt.byte_count = ATA_DMA_BUFFER_SIZE;
    ata_dma_prdt.flags = ATA_PRD_END;
    ata_dma_stop();
    ata_dma_clear_status();
    outl(ata_channel.bus_master_base + BM_PRDT_REG, (uint32_t)prdt_phys);
    outb(ata_channel.control_port, 0);
    ata_dma_enabled = true;
    IRQ_clear_mask(ata_channel.irq);
    klog("ATA: PCI bus-master DMA enabled at 0x%x IRQ %d\n",
         ata_channel.bus_master_base, ata_channel.irq);
    return true;
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
    kprintf("ATA: trying command port 0x%x\n",
            ata_channel.command_base);
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
