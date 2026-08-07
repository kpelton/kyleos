#include <output/framebuffer.h>
#include <asm/asm.h>
#include <mm/paging.h>
#include <output/output.h>

/* QEMU's std VGA exposes the Bochs VBE extension on these ports. */
#define BGA_INDEX_PORT  0x1ce
#define BGA_DATA_PORT   0x1cf
#define BGA_ID          0
#define BGA_XRES        1
#define BGA_YRES        2
#define BGA_BPP         3
#define BGA_ENABLE      4
#define BGA_ENABLE_ON   0x01
#define BGA_ENABLE_LFB  0x40

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc
#define PCI_CLASS_VGA   0x0300

static uint8_t *framebuffer;
static uint32_t fb_width;
static uint32_t fb_height;
static uint32_t fb_pitch;
static uint32_t fb_size;

static uint16_t bga_read(uint16_t index)
{
    outw(BGA_INDEX_PORT, index);
    return inw(BGA_DATA_PORT);
}

static void bga_write(uint16_t index, uint16_t value)
{
    outw(BGA_INDEX_PORT, index);
    outw(BGA_DATA_PORT, value);
}

static uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t function,
                           uint8_t offset)
{
    uint32_t address = 0x80000000U | ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) | ((uint32_t)function << 8) |
                       (offset & 0xfc);
    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

static uint32_t find_vga_framebuffer(void)
{
    for (uint8_t slot = 0; slot < 32; slot++) {
        uint32_t id = pci_read32(0, slot, 0, 0);
        uint32_t class_code;
        uint32_t bar0;

        if ((id & 0xffff) == 0xffff)
            continue;
        class_code = pci_read32(0, slot, 0, 8) >> 16;
        if (class_code != PCI_CLASS_VGA)
            continue;
        bar0 = pci_read32(0, slot, 0, 0x10);
        if ((bar0 & 1) == 0 && (bar0 & 0xfffffff0) != 0)
            return bar0 & 0xfffffff0;
    }
    return 0;
}

void framebuffer_init(void)
{
    uint32_t phys_base;
    uint32_t pages;
    uint16_t bga_id = bga_read(BGA_ID);

    /* A compatible Bochs/QEMU adapter reports a B0C* identifier. */
    if ((bga_id & 0xfff0) != 0xb0c0)
        return;
    phys_base = find_vga_framebuffer();
    if (phys_base == 0)
        return;

    bga_write(BGA_ENABLE, 0);
    bga_write(BGA_XRES, 640);
    bga_write(BGA_YRES, 400);
    bga_write(BGA_BPP, 32);
    bga_write(BGA_ENABLE, BGA_ENABLE_ON | BGA_ENABLE_LFB);

    fb_width = 640;
    fb_height = 400;
    fb_pitch = fb_width * 4;
    fb_size = fb_pitch * fb_height;
    pages = (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;
    paging_map_physmap_range(phys_base, pages);
    framebuffer = (uint8_t *)KERN_PHYS_TO_PVIRT(phys_base);
    kprintf("Framebuffer %dx%dx32 at %x\n", fb_width, fb_height, phys_base);
}

int framebuffer_present(const void *pixels, uint32_t bytes)
{
    if (framebuffer == NULL || pixels == NULL || bytes < fb_size)
        return -1;
    memcpy(framebuffer, pixels, fb_size);
    return 0;
}

uint32_t framebuffer_width(void) { return fb_width; }
uint32_t framebuffer_height(void) { return fb_height; }
uint32_t framebuffer_pitch(void) { return fb_pitch; }
