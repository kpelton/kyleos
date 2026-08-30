#include <acpi/acpi.h>
#include <include/types.h>
#include <mm/paging.h>
#include <asm/asm.h>
#include <sched/sched.h>
#include <output/output.h>

static bool acpi_initialized;
static bool acpi_tables_valid;
static uint8_t acpi_revision;
static uint64_t acpi_rsdp_address;
static uint64_t acpi_lapic_address;
static uint32_t acpi_ioapic_count;
static uint32_t acpi_cpu_count;
static uint32_t acpi_iso_count;
static uint64_t acpi_dsdt_address;
static uint32_t acpi_pm1a_control;
static uint32_t acpi_pm1b_control;
static uint8_t acpi_pm1_control_length;
static bool acpi_fadt_valid;
static uint16_t acpi_s5a_type;
static uint16_t acpi_s5b_type;
static bool acpi_s5_valid;
static uint8_t acpi_reset_space;
static uint8_t acpi_reset_width;
static uint64_t acpi_reset_address;
static uint8_t acpi_reset_value;

static void acpi_pm1_write(uint32_t port, uint16_t value)
{
    if (port == 0) return;
    if (acpi_pm1_control_length >= 4) outl((uint16_t)port, value);
    else if (acpi_pm1_control_length >= 2) outw((uint16_t)port, value);
    else outb((uint16_t)port, (uint8_t)value);
}

struct acpi_rsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt;
    uint32_t length;
    uint64_t xsdt;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

static bool acpi_signature(const char *actual, const char *expected,
                           uint32_t length)
{
    for (uint32_t i = 0; i < length; i++)
        if (actual[i] != expected[i]) return false;
    return true;
}

static bool acpi_checksum(const uint8_t *data, uint32_t length)
{
    uint8_t sum = 0;
    for (uint32_t i = 0; i < length; i++) sum = (uint8_t)(sum + data[i]);
    return sum == 0;
}

static void *acpi_phys_map(uint64_t address, uint32_t length)
{
    uint64_t first = address & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t offset = address - first;
    uint64_t mapped = (uint64_t)length + offset;
    mapped = (mapped + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
    if (!paging_map_physmap_range(first, mapped)) return NULL;
    return (void *)(KERN_PHYS_TO_PVIRT(first) + offset);
}

static struct acpi_rsdp *acpi_find_rsdp(uint64_t start, uint64_t end)
{
    for (uint64_t address = start; address + 20 <= end; address += 16) {
        struct acpi_rsdp *candidate = acpi_phys_map(address, 20);
        if (candidate == NULL ||
            !acpi_signature(candidate->signature, "RSD PTR ", 8) ||
            !acpi_checksum((uint8_t *)candidate, 20)) continue;
        if (candidate->revision >= 2) {
            candidate = acpi_phys_map(address, candidate->length);
            if (candidate == NULL || candidate->length < 36 ||
                !acpi_checksum((uint8_t *)candidate, candidate->length))
                continue;
        }
        return candidate;
    }
    return NULL;
}

static struct acpi_sdt_header *acpi_find_table(struct acpi_rsdp *rsdp,
                                                const char *signature)
{
    uint64_t root_address;
    uint32_t entry_size;
    struct acpi_sdt_header *root;

    if (rsdp->revision >= 2 && rsdp->xsdt != 0) {
        root_address = rsdp->xsdt;
        entry_size = 8;
    } else {
        root_address = rsdp->rsdt;
        entry_size = 4;
    }
    root = acpi_phys_map(root_address, sizeof(*root));
    if (root == NULL || root->length < sizeof(*root) ||
        !acpi_checksum((uint8_t *)root, root->length)) return NULL;
    root = acpi_phys_map(root_address, root->length);
    if (root == NULL) return NULL;
    uint32_t entries = (root->length - sizeof(*root)) / entry_size;
    uint8_t *entry = (uint8_t *)root + sizeof(*root);
    for (uint32_t i = 0; i < entries; i++) {
        uint64_t address = entry_size == 8 ? ((uint64_t *)entry)[i] :
                                             ((uint32_t *)entry)[i];
        struct acpi_sdt_header *table = acpi_phys_map(address, sizeof(*table));
        if (table != NULL && acpi_signature(table->signature, signature, 4) &&
            table->length >= sizeof(*table) &&
            acpi_checksum((uint8_t *)acpi_phys_map(address, table->length),
                          table->length)) return table;
    }
    return NULL;
}

static void acpi_parse_madt(struct acpi_sdt_header *header)
{
    uint8_t *entry = (uint8_t *)header + sizeof(*header) + 8;
    uint8_t *end = (uint8_t *)header + header->length;
    acpi_lapic_address = *(uint32_t *)((uint8_t *)header + sizeof(*header));
    while (entry + 2 <= end && entry[1] >= 2 && entry + entry[1] <= end) {
        switch (entry[0]) {
        case 0: /* Processor Local APIC */
            if (entry[1] >= 8 && (*(uint32_t *)(entry + 4) & 1))
                acpi_cpu_count++;
            break;
        case 1: /* I/O APIC */
            if (entry[1] >= 12) acpi_ioapic_count++;
            break;
        case 2: /* Interrupt Source Override */
            if (entry[1] >= 10) acpi_iso_count++;
            break;
        case 5: /* Local APIC address override */
            if (entry[1] >= 12)
                acpi_lapic_address = *(uint64_t *)(entry + 4);
            break;
        case 9: /* Processor Local x2APIC */
            if (entry[1] >= 16 && (*(uint32_t *)(entry + 8) & 1))
                acpi_cpu_count++;
            break;
        default:
            break;
        }
        entry += entry[1];
    }
}

static void acpi_parse_fadt(struct acpi_sdt_header *header)
{
    uint8_t *data = (uint8_t *)header;

    /* ACPI 1.0 fields: DSDT at offset 40, PM1 controls at 64/68. */
    if (header->length < 92) return;
    acpi_dsdt_address = *(uint32_t *)(data + 40);
    acpi_pm1a_control = *(uint32_t *)(data + 64);
    acpi_pm1b_control = *(uint32_t *)(data + 68);
    acpi_pm1_control_length = data[89];
    /* ACPI 2.0+ supplies the 64-bit DSDT address at offset 140. */
    if (header->length >= 148 && *(uint64_t *)(data + 140) != 0)
        acpi_dsdt_address = *(uint64_t *)(data + 140);
    /* Reset register GAS at offset 116, reset value at offset 128. */
    if (header->length >= 129) {
        acpi_reset_space = data[116];
        acpi_reset_width = data[117];
        acpi_reset_address = *(uint64_t *)(data + 120);
        acpi_reset_value = data[128];
    }
    acpi_fadt_valid = acpi_dsdt_address != 0 &&
                      acpi_pm1a_control != 0 &&
                      acpi_pm1_control_length != 0;
}

/*
 * ACPICA is vendored in acpi/acpica.  Keep the kernel entry point small and
 * independent of the table parser while the OS services layer is assembled.
 * The PIC path remains the fallback until the MADT-backed interrupt
 * controller is ready.
 */
void acpi_init(void)
{
    acpi_initialized = true;
    struct acpi_rsdp *rsdp = NULL;
    uint16_t ebda_segment = *(uint16_t *)KERN_PHYS_TO_PVIRT(0x40e);
    uint64_t ebda = (uint64_t)ebda_segment << 4;
    if (ebda != 0) rsdp = acpi_find_rsdp(ebda, ebda + 1024);
    if (rsdp == NULL) rsdp = acpi_find_rsdp(0xe0000, 0x100000);
    if (rsdp == NULL) {
        klog("ACPI: RSDP not found\n");
        return;
    }
    acpi_rsdp_address = KERN_PVIRT_TO_PHYS((uint64_t)rsdp);
    acpi_revision = rsdp->revision;
    struct acpi_sdt_header *madt = acpi_find_table(rsdp, "APIC");
    struct acpi_sdt_header *fadt = acpi_find_table(rsdp, "FACP");
    if (fadt != NULL) acpi_parse_fadt(fadt);
    if (madt == NULL) {
        klog("ACPI: MADT not found or invalid\n");
        return;
    }
    acpi_parse_madt(madt);
    acpi_tables_valid = true;
    klog("ACPI: MADT CPUs %d IOAPICs %d ISOs %d LAPIC %x\n",
         acpi_cpu_count, acpi_ioapic_count, acpi_iso_count,
         acpi_lapic_address);
    klog("ACPI: FADT %s PM1a %x PM1b %x DSDT %x\n",
         acpi_fadt_valid ? "valid" : "unavailable", acpi_pm1a_control,
         acpi_pm1b_control, acpi_dsdt_address);
    klog("ACPI: _S5 %s types %d/%d\n", acpi_s5_valid ? "found" : "not found",
         acpi_s5a_type, acpi_s5b_type);
}

int acpi_poweroff(void)
{
    uint16_t value;
    if (!acpi_fadt_valid) return -1;
    /* QEMU/PIIX exposes S5 as SLP_EN with a zero sleep type at PM1a. */
    value = (1U << 13);
    asm volatile("cli");
    acpi_pm1_write(acpi_pm1a_control, value);
    if (acpi_pm1b_control != 0) {
        acpi_pm1_write(acpi_pm1b_control, value);
    }
    /* Common emulator fallbacks; real hardware is left at a safe halt. */
    outw(0xb004, 0x2000);
    outw(0x4004, 0x3400);
    for (;;) asm volatile("hlt");
}

int acpi_reboot(void)
{
    asm volatile("cli");
    if (acpi_reset_address != 0 && acpi_reset_space == 1 &&
        acpi_reset_width >= 8) {
        outb((uint16_t)acpi_reset_address, acpi_reset_value);
    } else {
        /* Standard 8042 reset fallback. */
        outb(0x64, 0xfe);
    }
    for (;;) asm volatile("hlt");
}


static uint32_t acpi_append(char *buffer, uint32_t size, uint32_t position,
                            const char *text)
{
    while (*text != '\0' && position + 1 < size)
        buffer[position++] = *text++;
    if (size != 0) buffer[position] = '\0';
    return position;
}

static uint32_t acpi_append_number(char *buffer, uint32_t size,
                                   uint32_t position, uint64_t value)
{
    char number[24];
    itoa((unsigned long)value, number, 10);
    return acpi_append(buffer, size, position, number);
}

int acpi_proc_report(char *buffer, uint32_t size)
{
    uint32_t position = 0;
    if (buffer == NULL || size == 0) return 0;
    position = acpi_append(buffer, size, position, acpi_initialized ?
                           "ACPI: initialized\n" : "ACPI: not initialized\n");
    position = acpi_append(buffer, size, position, "RSDP: ");
    position = acpi_append_number(buffer, size, position, acpi_rsdp_address);
    position = acpi_append(buffer, size, position, "\nRevision: ");
    position = acpi_append_number(buffer, size, position, acpi_revision);
    position = acpi_append(buffer, size, position, "\nTables: ");
    position = acpi_append(buffer, size, position,
                           acpi_tables_valid ? "valid\n" : "unavailable\n");
    position = acpi_append(buffer, size, position, "MADT CPUs: ");
    position = acpi_append_number(buffer, size, position, acpi_cpu_count);
    position = acpi_append(buffer, size, position, "\nMADT IOAPICs: ");
    position = acpi_append_number(buffer, size, position, acpi_ioapic_count);
    position = acpi_append(buffer, size, position, "\nMADT ISOs: ");
    position = acpi_append_number(buffer, size, position, acpi_iso_count);
    position = acpi_append(buffer, size, position, "\nLAPIC: ");
    position = acpi_append_number(buffer, size, position, acpi_lapic_address);
    position = acpi_append(buffer, size, position, "\nFADT: ");
    position = acpi_append(buffer, size, position,
                           acpi_fadt_valid ? "valid" : "unavailable");
    position = acpi_append(buffer, size, position, "\nPM1a: ");
    position = acpi_append_number(buffer, size, position, acpi_pm1a_control);
    position = acpi_append(buffer, size, position, "\nPM1b: ");
    position = acpi_append_number(buffer, size, position, acpi_pm1b_control);
    position = acpi_append(buffer, size, position, "\nDSDT: ");
    position = acpi_append_number(buffer, size, position, acpi_dsdt_address);
    position = acpi_append(buffer, size, position, "\n_S5: ");
    position = acpi_append(buffer, size, position,
                           acpi_s5_valid ? "available" : "unavailable");
    position = acpi_append(buffer, size, position, "\nSleep types: ");
    position = acpi_append_number(buffer, size, position, acpi_s5a_type);
    position = acpi_append(buffer, size, position, "/");
    position = acpi_append_number(buffer, size, position, acpi_s5b_type);
    position = acpi_append(buffer, size, position,
                           "\nAPIC: using legacy PIC\nSMP: not started\n");
    if (position < size) buffer[position] = '\0';
    return (int)position;
}
