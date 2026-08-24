#ifndef PCI_H
#define PCI_H

#include <include/types.h>

#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_IDE 0x01
#define PCI_SUBCLASS_SATA 0x06
#define PCI_CLASS_DISPLAY 0x03
#define PCI_SUBCLASS_VGA 0x00

#define PCI_BAR_IO 0x01

struct pci_device {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t revision_id;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t class_code;
    uint8_t header_type;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint32_t bar[6];
};

void pci_init(void);
uint32_t pci_device_count(void);
const struct pci_device *pci_get_device(uint32_t index);
const struct pci_device *pci_find_class(uint8_t class_code,
                                        uint8_t subclass,
                                        uint32_t index);

uint8_t pci_config_read8(const struct pci_device *device, uint8_t offset);
uint16_t pci_config_read16(const struct pci_device *device, uint8_t offset);
uint32_t pci_config_read32(const struct pci_device *device, uint8_t offset);
void pci_config_write8(const struct pci_device *device, uint8_t offset,
                       uint8_t value);
void pci_config_write16(const struct pci_device *device, uint8_t offset,
                        uint16_t value);
void pci_config_write32(const struct pci_device *device, uint8_t offset,
                        uint32_t value);

bool pci_bar_is_io(const struct pci_device *device, uint8_t bar_index);
uint16_t pci_bar_io_base(const struct pci_device *device, uint8_t bar_index);
bool pci_bar_is_memory(const struct pci_device *device, uint8_t bar_index);
bool pci_bar_is_64(const struct pci_device *device, uint8_t bar_index);
uint64_t pci_bar_memory_base(const struct pci_device *device,
                             uint8_t bar_index);
void pci_enable_io(const struct pci_device *device);
bool pci_enable_bus_master(const struct pci_device *device);

#endif
