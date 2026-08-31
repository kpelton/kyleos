#include <irq/apic.h>
#include <asm/asm.h>
#include <mm/paging.h>
#include <acpi/acpi.h>
#include <output/output.h>

#define IA32_APIC_BASE 0x1b
#define IA32_APIC_BASE_ENABLE (1ULL << 11)
#define IA32_APIC_BASE_ADDRESS 0x000ffffffffff000ULL
#define LAPIC_ID 0x020
#define LAPIC_VERSION 0x030
#define LAPIC_EOI 0x0b0
#define LAPIC_SVR 0x0f0
#define LAPIC_LVT_TIMER 0x320
#define LAPIC_LVT_THERMAL 0x330
#define LAPIC_LVT_PERFORMANCE 0x340
#define LAPIC_LVT_LINT0 0x350
#define LAPIC_LVT_LINT1 0x360
#define LAPIC_LVT_ERROR 0x370
#define LAPIC_LVT_MASKED (1U << 16)
#define LAPIC_SVR_ENABLE (1U << 8)
#define LAPIC_SPURIOUS_VECTOR 0xff

static volatile uint32_t *lapic;
struct ioapic_runtime { volatile uint32_t *base; uint32_t gsi_base; uint32_t maximum; };
static struct ioapic_runtime ioapics[ACPI_MAX_IOAPICS];
static uint32_t ioapic_count;
static bool routing_enabled;

static uint32_t lapic_read(uint32_t offset)
{
    return lapic[offset / sizeof(uint32_t)];
}

static void lapic_write(uint32_t offset, uint32_t value)
{
    lapic[offset / sizeof(uint32_t)] = value;
    (void)lapic_read(LAPIC_SVR);
}

bool lapic_prepare(uint64_t physical_address)
{
    uint64_t base;
    uint32_t eax = 1, ebx, ecx, edx;
    uint32_t maximum_lvt;

    asm volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    if ((edx & (1U << 9)) == 0 || physical_address == 0 ||
        (physical_address & (PAGE_SIZE - 1)) != 0 ||
        !paging_map_physmap_range(physical_address, 1))
        return false;
    base = rdmsr(IA32_APIC_BASE);
    base &= ~IA32_APIC_BASE_ADDRESS;
    base |= (physical_address & IA32_APIC_BASE_ADDRESS) |
            IA32_APIC_BASE_ENABLE;
    wrmsr(IA32_APIC_BASE, base);
    lapic = (volatile uint32_t *)KERN_PHYS_TO_PVIRT(physical_address);
    maximum_lvt = (lapic_read(LAPIC_VERSION) >> 16) & 0xff;
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED);
    if (maximum_lvt >= 5) lapic_write(LAPIC_LVT_THERMAL, LAPIC_LVT_MASKED);
    if (maximum_lvt >= 4)
        lapic_write(LAPIC_LVT_PERFORMANCE, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT0, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT1, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_ERROR, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | LAPIC_SPURIOUS_VECTOR);
    return true;
}

void lapic_eoi(void)
{
    if (lapic != NULL) lapic_write(LAPIC_EOI, 0);
}

bool lapic_is_ready(void)
{
    return lapic != NULL;
}

static uint32_t ioapic_read(struct ioapic_runtime *io, uint8_t reg)
{
    io->base[0] = reg;
    return io->base[4];
}

static void ioapic_write(struct ioapic_runtime *io, uint8_t reg, uint32_t value)
{
    io->base[0] = reg;
    io->base[4] = value;
}

static uint32_t irq_gsi(uint8_t irq, uint16_t *flags)
{
    const struct acpi_iso_info *isos;
    uint32_t count = acpi_get_isos(&isos);
    *flags = 0;
    for (uint32_t i = 0; i < count; i++)
        if (isos[i].source == irq) { *flags = isos[i].flags; return isos[i].gsi; }
    return irq;
}

static struct ioapic_runtime *ioapic_for_gsi(uint32_t gsi)
{
    for (uint32_t i = 0; i < ioapic_count; i++)
        if (gsi >= ioapics[i].gsi_base &&
            gsi <= ioapics[i].gsi_base + ioapics[i].maximum) return &ioapics[i];
    return NULL;
}

static bool ioapic_route(uint8_t irq, uint8_t vector, bool masked)
{
    uint16_t flags;
    uint32_t gsi = irq_gsi(irq, &flags);
    struct ioapic_runtime *io = ioapic_for_gsi(gsi);
    uint32_t low = vector;
    uint32_t pin;
    uint8_t apic_id;
    if (io == NULL) return false;
    pin = gsi - io->gsi_base;
    apic_id = (uint8_t)(lapic_read(LAPIC_ID) >> 24);
    if ((flags & 3) == 3) low |= 1U << 13;
    if (((flags >> 2) & 3) == 3) low |= 1U << 15;
    if (masked) low |= 1U << 16;
    ioapic_write(io, 0x10 + pin * 2 + 1, (uint32_t)apic_id << 24);
    ioapic_write(io, 0x10 + pin * 2, low);
    return true;
}

bool apic_enable_routing(void)
{
    static const uint8_t irqs[] = {0, 1, 4, 8, 12, 14, 15};
    static const uint8_t vectors[] = {32, 33, 36, 64, 68, 70, 71};
    const struct acpi_ioapic_info *descriptors;
    uint8_t pic_masks[2] = {inb(0x21), inb(0xa1)};
    ioapic_count = acpi_get_ioapics(&descriptors);
    if (ioapic_count == 0) return false;
    for (uint32_t i = 0; i < ioapic_count; i++) {
        if ((descriptors[i].address & (PAGE_SIZE - 1)) != 0 ||
            !paging_map_physmap_range(descriptors[i].address, 1)) return false;
        ioapics[i].base = (volatile uint32_t *)KERN_PHYS_TO_PVIRT(descriptors[i].address);
        ioapics[i].gsi_base = descriptors[i].gsi_base;
        ioapics[i].maximum = (ioapic_read(&ioapics[i], 1) >> 16) & 0xff;
        for (uint32_t pin = 0; pin <= ioapics[i].maximum; pin++) {
            uint32_t low = ioapic_read(&ioapics[i], 0x10 + pin * 2);
            ioapic_write(&ioapics[i], 0x10 + pin * 2, low | (1U << 16));
        }
    }
    for (uint32_t i = 0; i < sizeof(irqs) / sizeof(irqs[0]); i++)
        if (ioapic_for_gsi(irq_gsi(irqs[i], &(uint16_t){0})) == NULL)
            return false;
    if (!lapic_prepare(acpi_lapic_phys())) return false;
    for (uint32_t i = 0; i < sizeof(irqs) / sizeof(irqs[0]); i++) {
        bool masked = (pic_masks[irqs[i] / 8] & (1U << (irqs[i] % 8))) != 0;
        if (!ioapic_route(irqs[i], vectors[i], masked)) return false;
    }
    routing_enabled = true;
    klog("APIC: LAPIC enabled, %d IOAPIC(s) routing legacy IRQs\n", ioapic_count);
    return true;
}

static uint8_t irq_vector(uint8_t irq)
{
    if (irq == 8) return 64;
    if (irq == 12) return 68;
    if (irq == 14) return 70;
    if (irq == 15) return 71;
    return 32 + irq;
}
void apic_mask_irq(uint8_t irq) { if (routing_enabled) ioapic_route(irq, irq_vector(irq), true); }
void apic_unmask_irq(uint8_t irq) { if (routing_enabled) ioapic_route(irq, irq_vector(irq), false); }
bool apic_routing_enabled(void) { return routing_enabled; }
