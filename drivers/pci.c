#include <asm/asm.h>
#include <drivers/pci.h>
#include <output/output.h>

#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_CONFIG_DATA 0xcfc
#define PCI_CONFIG_ENABLE 0x80000000U

#define PCI_VENDOR_ID 0x00
#define PCI_DEVICE_ID 0x02
#define PCI_COMMAND 0x04
#define PCI_REVISION_ID 0x08
#define PCI_PROG_IF 0x09
#define PCI_SUBCLASS 0x0a
#define PCI_CLASS_CODE 0x0b
#define PCI_HEADER_TYPE 0x0e
#define PCI_BAR0 0x10
#define PCI_INTERRUPT_LINE 0x3c
#define PCI_INTERRUPT_PIN 0x3d

#define PCI_HEADER_MULTIFUNCTION 0x80
#define PCI_HEADER_LAYOUT_MASK 0x7f
#define PCI_HEADER_GENERAL_DEVICE 0x00
#define PCI_COMMAND_IO_SPACE 0x0001
#define PCI_COMMAND_BUS_MASTER 0x0004
#define PCI_BAR_MEMORY_TYPE_MASK 0x06
#define PCI_BAR_MEMORY_TYPE_64 0x04
#define PCI_MAX_DEVICES 256

static struct pci_device devices[PCI_MAX_DEVICES];
static uint32_t device_count;

static uint32_t pci_config_address(uint8_t bus, uint8_t slot,
                                   uint8_t function, uint8_t offset)
{
    return PCI_CONFIG_ENABLE |
           ((uint32_t)bus << 16) |
           ((uint32_t)slot << 11) |
           ((uint32_t)function << 8) |
           (offset & 0xfc);
}

static uint32_t pci_raw_read32(uint8_t bus, uint8_t slot, uint8_t function,
                               uint8_t offset)
{
    outl(PCI_CONFIG_ADDRESS,
         pci_config_address(bus, slot, function, offset));
    return inl(PCI_CONFIG_DATA);
}

static void pci_raw_write32(uint8_t bus, uint8_t slot, uint8_t function,
                            uint8_t offset, uint32_t value)
{
    outl(PCI_CONFIG_ADDRESS,
         pci_config_address(bus, slot, function, offset));
    outl(PCI_CONFIG_DATA, value);
}

static uint16_t pci_raw_read16(uint8_t bus, uint8_t slot, uint8_t function,
                               uint8_t offset)
{
    uint32_t value = pci_raw_read32(bus, slot, function, offset);
    return (value >> ((offset & 2) * 8)) & 0xffff;
}

static uint8_t pci_raw_read8(uint8_t bus, uint8_t slot, uint8_t function,
                             uint8_t offset)
{
    uint32_t value = pci_raw_read32(bus, slot, function, offset);
    return (value >> ((offset & 3) * 8)) & 0xff;
}

static void pci_record_function(uint8_t bus, uint8_t slot, uint8_t function)
{
    struct pci_device *device;
    uint8_t bar_count;

    if (device_count >= PCI_MAX_DEVICES) {
        klog("PCI: device table full\n");
        return;
    }

    device = &devices[device_count++];
    device->bus = bus;
    device->slot = slot;
    device->function = function;
    device->vendor_id = pci_raw_read16(bus, slot, function, PCI_VENDOR_ID);
    device->device_id = pci_raw_read16(bus, slot, function, PCI_DEVICE_ID);
    device->revision_id = pci_raw_read8(bus, slot, function, PCI_REVISION_ID);
    device->prog_if = pci_raw_read8(bus, slot, function, PCI_PROG_IF);
    device->subclass = pci_raw_read8(bus, slot, function, PCI_SUBCLASS);
    device->class_code = pci_raw_read8(bus, slot, function, PCI_CLASS_CODE);
    device->header_type = pci_raw_read8(bus, slot, function, PCI_HEADER_TYPE);
    device->interrupt_line =
        pci_raw_read8(bus, slot, function, PCI_INTERRUPT_LINE);
    device->interrupt_pin =
        pci_raw_read8(bus, slot, function, PCI_INTERRUPT_PIN);

    for (uint8_t i = 0; i < 6; i++)
        device->bar[i] = 0;

    bar_count = ((device->header_type & PCI_HEADER_LAYOUT_MASK) ==
                 PCI_HEADER_GENERAL_DEVICE) ? 6 : 2;
    for (uint8_t i = 0; i < bar_count; i++)
        device->bar[i] =
            pci_raw_read32(bus, slot, function, PCI_BAR0 + i * 4);

    klog("PCI %d:%d.%d %x:%x class %x:%x prog %x\n",
         device->bus, device->slot, device->function,
         device->vendor_id, device->device_id,
         device->class_code, device->subclass, device->prog_if);
}

void pci_init(void)
{
    device_count = 0;
    klog("PCI: scanning configuration space\n");

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_raw_read16(bus, slot, 0, PCI_VENDOR_ID);
            uint8_t function_count;

            if (vendor == 0xffff)
                continue;

            function_count =
                (pci_raw_read8(bus, slot, 0, PCI_HEADER_TYPE) &
                 PCI_HEADER_MULTIFUNCTION) ? 8 : 1;

            for (uint8_t function = 0; function < function_count; function++) {
                vendor = pci_raw_read16(bus, slot, function, PCI_VENDOR_ID);
                if (vendor != 0xffff)
                    pci_record_function(bus, slot, function);
            }
        }
    }

    klog("PCI: found %d device functions\n", device_count);
}

uint32_t pci_device_count(void)
{
    return device_count;
}

const struct pci_device *pci_get_device(uint32_t index)
{
    if (index >= device_count)
        return NULL;
    return &devices[index];
}

const struct pci_device *pci_find_class(uint8_t class_code,
                                        uint8_t subclass,
                                        uint32_t index)
{
    for (uint32_t i = 0; i < device_count; i++) {
        if (devices[i].class_code != class_code ||
            devices[i].subclass != subclass)
            continue;
        if (index == 0)
            return &devices[i];
        index--;
    }
    return NULL;
}

uint32_t pci_config_read32(const struct pci_device *device, uint8_t offset)
{
    if (device == NULL)
        return 0xffffffffU;
    return pci_raw_read32(device->bus, device->slot, device->function, offset);
}

uint16_t pci_config_read16(const struct pci_device *device, uint8_t offset)
{
    uint32_t value = pci_config_read32(device, offset);
    return (value >> ((offset & 2) * 8)) & 0xffff;
}

uint8_t pci_config_read8(const struct pci_device *device, uint8_t offset)
{
    uint32_t value = pci_config_read32(device, offset);
    return (value >> ((offset & 3) * 8)) & 0xff;
}

void pci_config_write32(const struct pci_device *device, uint8_t offset,
                        uint32_t value)
{
    if (device == NULL)
        return;
    pci_raw_write32(device->bus, device->slot, device->function, offset,
                    value);
}

void pci_config_write16(const struct pci_device *device, uint8_t offset,
                        uint16_t value)
{
    if (device == NULL)
        return;
    outl(PCI_CONFIG_ADDRESS,
         pci_config_address(device->bus, device->slot, device->function,
                            offset));
    outw(PCI_CONFIG_DATA + (offset & 2), value);
}

void pci_config_write8(const struct pci_device *device, uint8_t offset,
                       uint8_t value)
{
    if (device == NULL)
        return;
    outl(PCI_CONFIG_ADDRESS,
         pci_config_address(device->bus, device->slot, device->function,
                            offset));
    outb(PCI_CONFIG_DATA + (offset & 3), value);
}

bool pci_bar_is_io(const struct pci_device *device, uint8_t bar_index)
{
    if (device == NULL || bar_index >= 6 || device->bar[bar_index] == 0 ||
        device->bar[bar_index] == 0xffffffffU)
        return false;
    return (device->bar[bar_index] & PCI_BAR_IO) != 0;
}

uint16_t pci_bar_io_base(const struct pci_device *device, uint8_t bar_index)
{
    if (!pci_bar_is_io(device, bar_index))
        return 0;
    return device->bar[bar_index] & 0xfffc;
}

bool pci_bar_is_memory(const struct pci_device *device, uint8_t bar_index)
{
    if (device == NULL || bar_index >= 6 || device->bar[bar_index] == 0 ||
        device->bar[bar_index] == 0xffffffffU)
        return false;
    return (device->bar[bar_index] & PCI_BAR_IO) == 0;
}

bool pci_bar_is_64(const struct pci_device *device, uint8_t bar_index)
{
    if (!pci_bar_is_memory(device, bar_index))
        return false;
    return (device->bar[bar_index] & PCI_BAR_MEMORY_TYPE_MASK) ==
           PCI_BAR_MEMORY_TYPE_64;
}

uint64_t pci_bar_memory_base(const struct pci_device *device,
                             uint8_t bar_index)
{
    uint64_t base;

    if (!pci_bar_is_memory(device, bar_index))
        return 0;

    base = device->bar[bar_index] & 0xfffffff0U;
    if (pci_bar_is_64(device, bar_index)) {
        if (bar_index >= 5)
            return 0;
        base |= (uint64_t)device->bar[bar_index + 1] << 32;
    }
    return base;
}

void pci_enable_io(const struct pci_device *device)
{
    uint16_t command = pci_config_read16(device, PCI_COMMAND);

    if ((command & PCI_COMMAND_IO_SPACE) == 0)
        pci_config_write16(device, PCI_COMMAND,
                           command | PCI_COMMAND_IO_SPACE);
}

bool pci_enable_bus_master(const struct pci_device *device)
{
    uint16_t command;

    if (device == NULL)
        return false;

    command = pci_config_read16(device, PCI_COMMAND);
    if ((command & PCI_COMMAND_BUS_MASTER) == 0)
        pci_config_write16(device, PCI_COMMAND,
                           command | PCI_COMMAND_BUS_MASTER);

    return (pci_config_read16(device, PCI_COMMAND) &
            PCI_COMMAND_BUS_MASTER) != 0;
}
