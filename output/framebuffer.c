#include <output/framebuffer.h>
#include <asm/asm.h>
#include <mm/paging.h>
#include <output/output.h>
#include <output/vga.h>
#include <include/multiboot.h>
#include <mm/mtrr.h>
#include <drivers/pci.h>

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

static const struct pci_device *framebuffer_pci_owner(uint64_t phys)
{
    uint32_t display_index = 0;
    const struct pci_device *display;

    while ((display = pci_find_class(PCI_CLASS_DISPLAY,
                                      PCI_SUBCLASS_VGA,
                                      display_index++)) != NULL) {
        for (uint8_t bar = 0; bar < 6; bar++) {
            if (!pci_bar_is_memory(display, bar))
                continue;
            if (pci_bar_memory_base(display, bar) == phys)
                return display;
            if (pci_bar_is_64(display, bar))
                bar++;
        }
    }
    return NULL;
}

static uint64_t find_bga_framebuffer(const struct pci_device **owner)
{
    uint32_t display_index = 0;
    const struct pci_device *display;

    while ((display = pci_find_class(PCI_CLASS_DISPLAY,
                                      PCI_SUBCLASS_VGA,
                                      display_index++)) != NULL) {
        uint64_t base = pci_bar_memory_base(display, 0);

        if (base == 0)
            continue;
        if (owner != NULL)
            *owner = display;
        return base;
    }
    return 0;
}
static void framebuffer_fill_pixels(uint32_t *pixels, uint32_t color,
                                    uint64_t count)
{
    if (count == 0)
        return;
    if (count == 1) {
        *pixels = color;
        return;
    }

    asm volatile(
        "cld\n\t"
        "rep stosl"
        : "+D"(pixels), "+c"(count)
        : "a"(color)
        : "memory", "cc");
}

static void framebuffer_copy_rows(uint8_t *dest, uint32_t dest_pitch,
                                  const uint8_t *src, uint32_t src_pitch,
                                  uint32_t row_bytes, uint32_t rows)
{
    if (row_bytes == 0 || rows == 0)
        return;

    if (dest_pitch == row_bytes && src_pitch == row_bytes) {
        memcpy8(dest, src, (uint64_t)row_bytes * rows);
        return;
    }

    for (uint32_t row = 0; row < rows; row++) {
        memcpy8(dest, src, row_bytes);
        dest += dest_pitch;
        src += src_pitch;
    }
}

void framebuffer_init(uint64_t mb_info)
{
    multiboot_info_t *mb = (multiboot_info_t *)mb_info;

    if (mb->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) {
        uint64_t phys = mb->framebuffer_addr;
        const struct pci_device *display = framebuffer_pci_owner(phys);

        fb_width  = mb->framebuffer_width;
        fb_height = mb->framebuffer_height;
        fb_pitch  = mb->framebuffer_pitch;
        fb_size   = fb_pitch * fb_height;

        klog("GRUB framebuffer\n");
        klog("addr=%x\n", phys);
        klog("%dx%d bpp=%d pitch=%d\n",
             fb_width,
             fb_height,
             mb->framebuffer_bpp,
             fb_pitch);

        if (display != NULL)
            klog("PCI display %d:%d.%d %x:%x owns framebuffer BAR\n",
                 display->bus, display->slot, display->function,
                 display->vendor_id, display->device_id);
        else
            klog("PCI: no display BAR matched framebuffer address\n");

        if (mb->framebuffer_bpp != 32) {
            klog("unsupported framebuffer bpp\n");
            return;
        }
        /* Write combining is important for the large streaming stores below. */
        if (!mtrr_set_write_combining(phys, fb_size))
            klog("Framebuffer: unable to enable MTRR WC\n");

        uint32_t pages =
            (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;

        paging_map_physmap_range(phys, pages);

        framebuffer =
            (uint8_t *)KERN_PHYS_TO_PVIRT(phys);
        framebuffer_clear(0x00000000);
        vga_framebuffer_ready();

        return;
    }

    uint64_t phys_base;
    uint32_t pages;
    uint16_t bga_id = bga_read(BGA_ID);
    const struct pci_device *display = NULL;

    /* A compatible Bochs/QEMU adapter reports a B0C* identifier. */
    if ((bga_id & 0xfff0) != 0xb0c0)
        return;
    phys_base = find_bga_framebuffer(&display);
    if (phys_base == 0)
        return;

    klog("BGA: using PCI display %d:%d.%d BAR0 at %x\n",
         display->bus, display->slot, display->function, phys_base);

    bga_write(BGA_ENABLE, 0);
    bga_write(BGA_XRES, 1280);
    bga_write(BGA_YRES, 720);
    bga_write(BGA_BPP, 32);
    bga_write(BGA_ENABLE, BGA_ENABLE_ON | BGA_ENABLE_LFB);

    fb_width = 1280;
    fb_height = 720;
    fb_pitch = fb_width * 4;
    fb_size = fb_pitch * fb_height;
    if (!mtrr_set_write_combining(phys_base, fb_size))
        klog("Framebuffer: unable to enable MTRR WC\n");
    pages = (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;
    paging_map_physmap_range(phys_base, pages);
    framebuffer = (uint8_t *)KERN_PHYS_TO_PVIRT(phys_base);
    framebuffer_clear(0x00000000);
    vga_framebuffer_ready();
    klog("Framebuffer %dx%dx32 at %x\n", fb_width, fb_height, phys_base);
}

bool framebuffer_is_ready(void)
{
    return framebuffer != NULL;
}

void framebuffer_clear(uint32_t color)
{
    framebuffer_fill_rect(0, 0, fb_width, fb_height, color);
}

void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t width,
                           uint32_t height, uint32_t color)
{
    if (framebuffer == NULL || width == 0 || height == 0 ||
        x >= fb_width || y >= fb_height)
        return;
    if (width > fb_width - x)
        width = fb_width - x;
    if (height > fb_height - y)
        height = fb_height - y;

    uint32_t *pixels =
        (uint32_t *)(framebuffer + y * fb_pitch + x * sizeof(uint32_t));

    if (x == 0 && width == fb_width && fb_pitch == fb_width * sizeof(uint32_t)) {
        framebuffer_fill_pixels(pixels, color, (uint64_t)width * height);
        return;
    }

    for (uint32_t row = 0; row < height; row++) {
        framebuffer_fill_pixels(pixels, color, width);
        pixels = (uint32_t *)((uint8_t *)pixels + fb_pitch);
    }
}

int framebuffer_present(const void *pixels, uint32_t bytes)
{
    uint32_t src_width = 640;
    uint32_t src_height = 400;
    uint32_t src_pitch;
    uint32_t src_size;

    if (framebuffer == NULL || pixels == NULL)
        return -1;

    /* New clients present a tightly packed native-size surface.  Preserve
     * centered 640x400 blits for existing programs built against the old
     * framebuffer ABI. */
    if ((uint64_t)bytes == (uint64_t)fb_width * fb_height * 4) {
        src_width = fb_width;
        src_height = fb_height;
    }
    src_pitch = src_width * 4;
    src_size = src_pitch * src_height;
    if (bytes < src_size)
        return -1;

    uint32_t copy_width =
        src_width < fb_width ? src_width : fb_width;

    uint32_t copy_height =
        src_height < fb_height ? src_height : fb_height;

    uint32_t xoff =
        fb_width > copy_width
            ? (fb_width - copy_width) / 2
            : 0;

    uint32_t yoff =
        fb_height > copy_height
            ? (fb_height - copy_height) / 2
            : 0;

    uint8_t *dst =
        framebuffer +
        yoff * fb_pitch +
        xoff * 4;

    const uint8_t *src =
        (const uint8_t *)pixels;

    uint32_t bytes_per_row = copy_width * 4;

    /* One helper handles both tightly packed and pitched/centered blits. */
    framebuffer_copy_rows(dst, fb_pitch, src, src_pitch,
                          bytes_per_row, copy_height);

    return 0;
}

int framebuffer_present_rect(const void *pixels, uint32_t surface_width,
                             uint32_t surface_height, uint32_t surface_stride,
                             uint32_t x, uint32_t y, uint32_t width,
                             uint32_t height)
{
    const uint8_t *src;
    uint8_t *dst;
    uint32_t src_pitch;

    if (framebuffer == NULL || pixels == NULL ||
        surface_width != fb_width || surface_height != fb_height ||
        surface_stride < surface_width || x >= surface_width ||
        y >= surface_height || width == 0 || height == 0)
        return -1;
    if (width > surface_width - x) width = surface_width - x;
    if (height > surface_height - y) height = surface_height - y;
    src_pitch = surface_stride * sizeof(uint32_t);
    src = (const uint8_t *)pixels + y * src_pitch + x * sizeof(uint32_t);
    dst = framebuffer + y * fb_pitch + x * sizeof(uint32_t);
    framebuffer_copy_rows(dst, fb_pitch, src, src_pitch,
                          width * sizeof(uint32_t), height);
    return 0;
}

uint32_t framebuffer_width(void) { return fb_width; }
uint32_t framebuffer_height(void) { return fb_height; }
uint32_t framebuffer_pitch(void) { return fb_pitch; }
