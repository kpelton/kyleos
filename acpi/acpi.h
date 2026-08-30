#ifndef KYLEOS_ACPI_H
#define KYLEOS_ACPI_H

#include <include/types.h>

/* Bring up the ACPI platform layer after paging and the heap are ready. */
void acpi_init(void);

/* Render the read-only /proc/acpi status report. */
int acpi_proc_report(char *buffer, uint32_t size);

/* Request an ACPI S5 poweroff. Returns -1 when firmware data is unavailable. */
int acpi_poweroff(void);
int acpi_reboot(void);

#endif
