#include <output/framebuffer.h>
#include <asm/asm.h>
#include <mm/paging.h>
#include <output/output.h>
#include <output/vga.h>
#include <include/multiboot.h>
#include <mm/mtrr.h>

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
void *f_memcpy(void *dest, const void *src, int n)
{
    void *ret = dest;

    __asm__ volatile (
        "rep movsb"
        : "+D"(dest),
          "+S"(src),
          "+c"(n)
        :
        : "memory"
    );

    return ret;
}

void framebuffer_init(uint64_t mb_info)
{
    multiboot_info_t *mb = (multiboot_info_t *)mb_info;

    if (mb->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) {
        uint64_t phys = mb->framebuffer_addr;

        fb_width  = mb->framebuffer_width;
        fb_height = mb->framebuffer_height;
        fb_pitch  = mb->framebuffer_pitch;
        fb_size   = fb_pitch * fb_height;

        kprintf("GRUB framebuffer\n");
        kprintf("addr=%x\n", phys);
        kprintf("%dx%d bpp=%d pitch=%d\n",
                fb_width,
                fb_height,
                mb->framebuffer_bpp,
                fb_pitch);

        if (mb->framebuffer_bpp != 32) {
            kprintf("unsupported framebuffer bpp\n");
            return;
        }
        // setup WC MTRR to speed up FB
        fb_size = fb_pitch * fb_height;

        if (!mtrr_set_write_combining(phys, fb_size))
            kprintf("Framebuffer: unable to enable MTRR WC\n");

        uint32_t pages =
            (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;

        paging_map_physmap_range(phys, pages);

        framebuffer =
            (uint8_t *)KERN_PHYS_TO_PVIRT(phys);
        framebuffer_clear(0x00000000);
        vga_framebuffer_ready();

        return;
    }

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
    framebuffer_clear(0x00000000);
    vga_framebuffer_ready();
    kprintf("Framebuffer %dx%dx32 at %x\n", fb_width, fb_height, phys_base);
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
    if (framebuffer == NULL || x >= fb_width || y >= fb_height)
        return;
    if (x + width > fb_width)
        width = fb_width - x;
    if (y + height > fb_height)
        height = fb_height - y;
    for (uint32_t row = 0; row < height; row++) {
        uint32_t *pixels = (uint32_t *)(framebuffer + (y + row) * fb_pitch + x * 4);
        for (uint32_t col = 0; col < width; col++)
            pixels[col] = color;
    }
}

int framebuffer_present(const void *pixels, uint32_t bytes)
{
    const uint32_t src_width  = 640;
    const uint32_t src_height = 400;
    const uint32_t src_pitch  = src_width * 4;
    const uint32_t src_size   = src_pitch * src_height;

    if (framebuffer == NULL || pixels == NULL)
        return -1;

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

    /*
     * Fast path:
     *
     * If the source and destination rows have the same pitch,
     * copy the entire image in one operation.
     */
    if (fb_pitch == src_pitch &&
        xoff == 0 &&
        copy_width == src_width &&
        copy_height == src_height) {

        f_memcpy(dst,
               src,
               src_pitch * src_height);

        return 0;
    }

    /*
     * General path for a framebuffer with a different pitch
     * or a centered Doom image.
     */
    for (uint32_t y = 0; y < copy_height; y++) {
        f_memcpy(dst,
               src,
               bytes_per_row);

        dst += fb_pitch;
        src += src_pitch;
    }

    return 0;
}
uint32_t framebuffer_width(void) { return fb_width; }
uint32_t framebuffer_height(void) { return fb_height; }
uint32_t framebuffer_pitch(void) { return fb_pitch; }
