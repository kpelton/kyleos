#ifndef KYLEOS_ACPI_H
#define KYLEOS_ACPI_H

#include <include/types.h>

#define ACPI_MAX_CPUS 256
#define ACPI_MAX_IOAPICS 4
#define ACPI_MAX_ISOS 16
struct acpi_cpu_info {
    uint32_t processor_uid;
    uint32_t apic_id;
    uint32_t flags;
    bool x2apic;
};
struct acpi_ioapic_info { uint8_t id; uint32_t address; uint32_t gsi_base; };
struct acpi_iso_info { uint8_t source; uint32_t gsi; uint16_t flags; };

/* Bring up the ACPI platform layer after paging and the heap are ready. */
void acpi_init(void);

/* Render the read-only /proc/acpi status report. */
int acpi_proc_report(char *buffer, uint32_t size);

/* Request an ACPI S5 poweroff. Returns -1 when firmware data is unavailable. */
int acpi_poweroff(void);
int acpi_reboot(void);
uint64_t acpi_lapic_phys(void);
uint32_t acpi_get_cpus(const struct acpi_cpu_info **items);
uint32_t acpi_get_ioapics(const struct acpi_ioapic_info **items);
uint32_t acpi_get_isos(const struct acpi_iso_info **items);

#endif
