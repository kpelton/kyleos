#include <mm/mtrr.h>
#include <asm/asm.h>
#include <output/output.h>

/*
 * Intel MTRR MSRs.
 */
#define IA32_MTRRCAP          0x000000fe
#define IA32_MTRR_PHYSBASE0   0x00000200
#define IA32_MTRR_PHYSMASK0   0x00000201
#define IA32_MTRR_DEF_TYPE    0x000002ff

/*
 * IA32_MTRRCAP
 */
#define MTRRCAP_VCNT_MASK     0xffULL
#define MTRRCAP_WC            (1ULL << 10)

/*
 * IA32_MTRR_DEF_TYPE
 */
#define MTRR_DEF_TYPE_E       (1ULL << 11)

/*
 * IA32_MTRR_PHYSMASKn
 */
#define MTRR_MASK_VALID       (1ULL << 11)

/*
 * MTRR memory types.
 */
#define MTRR_TYPE_UC          0x00
#define MTRR_TYPE_WC          0x01
#define MTRR_TYPE_WT          0x04
#define MTRR_TYPE_WP          0x05
#define MTRR_TYPE_WB          0x06

#define PAGE_SIZE             4096ULL

/*
 * CR0 cache control bits.
 */
#define CR0_NW                (1ULL << 29)
#define CR0_CD                (1ULL << 30)


static inline void cpuid(uint32_t leaf,
                         uint32_t subleaf,
                         uint32_t *a,
                         uint32_t *b,
                         uint32_t *c,
                         uint32_t *d)
{
    asm volatile(
        "cpuid"
        : "=a"(*a),
          "=b"(*b),
          "=c"(*c),
          "=d"(*d)
        : "a"(leaf),
          "c"(subleaf)
    );
}


static uint32_t cpu_max_physical_bits(void)
{
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;

    /*
     * See whether extended CPUID leaf 0x80000008 exists.
     */
    cpuid(0x80000000, 0, &a, &b, &c, &d);

    if (a >= 0x80000008) {
        cpuid(0x80000008, 0, &a, &b, &c, &d);

        /*
         * EAX bits 7:0 are the physical-address width.
         */
        uint32_t bits = a & 0xff;

        if (bits >= 32 && bits <= 52)
            return bits;
    }

    /*
     * Reasonable fallback for older x86-64 processors.
     */
    return 36;
}


static bool cpu_has_mtrr(void)
{
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;

    cpuid(1, 0, &a, &b, &c, &d);

    /*
     * CPUID.01H:EDX bit 12 = MTRR.
     */
    return (d & (1U << 12)) != 0;
}


static uint64_t read_cr0(void)
{
    uint64_t value;

    asm volatile(
        "mov %%cr0, %0"
        : "=r"(value)
    );

    return value;
}


static void write_cr0(uint64_t value)
{
    asm volatile(
        "mov %0, %%cr0"
        :
        : "r"(value)
        : "memory"
    );
}


static void wbinvd(void)
{
    asm volatile(
        "wbinvd"
        :
        :
        : "memory"
    );
}


static uint64_t irq_save_disable(void)
{
    uint64_t flags;

    asm volatile(
        "pushfq\n"
        "popq %0\n"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );

    return flags;
}


static void irq_restore(uint64_t flags)
{
    /*
     * Only restore IF.  We don't want to overwrite unrelated
     * RFLAGS state.
     */
    if (flags & (1ULL << 9))
        asm volatile("sti" ::: "memory");
}


static int mtrr_find_free(uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        uint64_t mask =
            rdmsr(IA32_MTRR_PHYSMASK0 + i * 2);

        if ((mask & MTRR_MASK_VALID) == 0)
            return (int)i;
    }

    return -1;
}


static uint64_t highest_power_of_two(uint64_t value)
{
    uint64_t power = 1;

    while ((power << 1) != 0 &&
           (power << 1) <= value) {
        power <<= 1;
    }

    return power;
}


/*
 * Find the largest power-of-two block which:
 *
 *   - starts at addr
 *   - does not exceed remaining
 *   - is naturally aligned
 */
static uint64_t choose_block_size(uint64_t addr,
                                  uint64_t remaining)
{
    uint64_t size = highest_power_of_two(remaining);

    while (size > PAGE_SIZE) {
        if ((addr & (size - 1)) == 0)
            break;

        size >>= 1;
    }

    return size;
}


static void mtrr_program_range(uint32_t slot,
                               uint64_t base,
                               uint64_t size,
                               uint32_t phys_bits)
{
    uint64_t phys_mask;

    /*
     * Variable MTRRs describe:
     *
     *   base = physical base | memory type
     *   mask = address mask | VALID
     *
     * Ignore the low 12 address bits.
     */
    phys_mask =
        ((1ULL << phys_bits) - 1ULL) &
        ~(size - 1ULL) &
        ~0xfffULL;

    wrmsr(
        IA32_MTRR_PHYSBASE0 + slot * 2,
        (base & ~0xfffULL) | MTRR_TYPE_WC
    );

    wrmsr(
        IA32_MTRR_PHYSMASK0 + slot * 2,
        phys_mask | MTRR_MASK_VALID
    );
}


bool mtrr_set_write_combining(uint64_t phys_addr, uint64_t length)
{
    uint64_t capability;
    uint64_t old_cr0;
    uint64_t old_def_type;
    uint64_t irq_flags;

    uint32_t phys_bits;
    uint32_t variable_count;

    uint64_t start;
    uint64_t end;
    uint64_t size;
    uint64_t base;

    int slot = -1;

    if (length == 0)
        return false;

    if (!cpu_has_mtrr()) {
        klog("MTRR: CPU does not support MTRRs\n");
        return false;
    }

    capability = rdmsr(IA32_MTRRCAP);

    if ((capability & MTRRCAP_WC) == 0) {
        klog("MTRR: write combining unsupported\n");
        return false;
    }

    variable_count =
        (uint32_t)(capability & MTRRCAP_VCNT_MASK);

    if (variable_count == 0) {
        klog("MTRR: no variable MTRRs\n");
        return false;
    }

    phys_bits = cpu_max_physical_bits();

    /*
     * Actual framebuffer range, rounded outward to pages.
     */
    start = phys_addr & ~(PAGE_SIZE - 1);

    end =
        (phys_addr + length + PAGE_SIZE - 1) &
        ~(PAGE_SIZE - 1);

    if (end <= start)
        return false;

    /*
     * Find a single naturally-aligned power-of-two MTRR
     * that completely contains [start, end).
     *
     * Start with the smallest power of two >= framebuffer size.
     */
    size = PAGE_SIZE;

    while (size < (end - start))
        size <<= 1;

    /*
     * Align the candidate base downward.
     *
     * If that aligned region doesn't contain the framebuffer end,
     * double the size and try again.
     */
    for (;;) {
        base = start & ~(size - 1);

        if (base + size >= end)
            break;

        size <<= 1;
    }

    /*
     * Find one unused variable MTRR.
     */
    for (uint32_t i = 0; i < variable_count; i++) {
        uint64_t mask =
            rdmsr(IA32_MTRR_PHYSMASK0 + i * 2);

        if ((mask & MTRR_MASK_VALID) == 0) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        klog("MTRR: no free variable MTRR\n");
        return false;
    }

    klog("MTRR: framebuffer actual %x-%x\n",
         start,
         end);

    klog("MTRR: using slot %d WC base=%x size=%x\n",
         slot,
         base,
         size);

    /*
     * Disable interrupts and caching while changing MTRRs.
     */
    irq_flags = irq_save_disable();

    old_cr0 = read_cr0();

    write_cr0(
        (old_cr0 | CR0_CD) &
        ~CR0_NW
    );

    wbinvd();

    old_def_type = rdmsr(IA32_MTRR_DEF_TYPE);

    /*
     * Temporarily disable MTRRs.
     */
    wrmsr(
        IA32_MTRR_DEF_TYPE,
        old_def_type & ~MTRR_DEF_TYPE_E
    );

    wbinvd();

    /*
     * Program the single WC range.
     */
    mtrr_program_range(
        (uint32_t)slot,
        base,
        size,
        phys_bits
    );

    wbinvd();

    /*
     * Restore MTRR configuration.
     */
    wrmsr(
        IA32_MTRR_DEF_TYPE,
        old_def_type
    );

    wbinvd();

    /*
     * Restore normal caching.
     */
    write_cr0(old_cr0);

    irq_restore(irq_flags);

    klog("MTRR: framebuffer WC enabled\n");

    return true;
}

void mtrr_dump(void)
{
    uint64_t cap = rdmsr(IA32_MTRRCAP);
    uint32_t count = cap & 0xff;

    klog("MTRR count=%d def=%x\n",
            count,
            rdmsr(IA32_MTRR_DEF_TYPE));

    for (uint32_t i = 0; i < count; i++) {
        uint64_t base =
            rdmsr(IA32_MTRR_PHYSBASE0 + i * 2);

        uint64_t mask =
            rdmsr(IA32_MTRR_PHYSMASK0 + i * 2);

        klog("MTRR %d base=%x mask=%x type=%d valid=%d\n",
                i,
                base & ~0xfffULL,
                mask,
                base & 0xff,
                !!(mask & MTRR_MASK_VALID));
    }
}


