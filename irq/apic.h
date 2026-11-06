#ifndef KYLEOS_APIC_H
#define KYLEOS_APIC_H

#include <include/types.h>

bool lapic_prepare(uint64_t physical_address);
void lapic_eoi(void);
bool lapic_is_ready(void);
bool apic_enable_routing(void);
bool apic_routing_enabled(void);
void apic_mask_irq(uint8_t irq);
void apic_unmask_irq(uint8_t irq);

#endif
