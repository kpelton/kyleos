#include <include/types.h>
#include <mm/pmem.h>
#include <output/output.h>

#define BLOCK_SIZE 4096
#define BIT_SIZE 64
#define MAX_PHYS_ZONES 10

static struct phys_mem_zone phys_mem_zones[MAX_PHYS_ZONES];
static int phys_max_found_zone = -1;

static uint64_t get_bit_in_map(uint64_t addr) 
{
    uint64_t bit = (addr/BLOCK_SIZE);
    return bit;
}

static uint64_t get_block(uint64_t bit_num) 
{
    uint64_t block = (bit_num/BIT_SIZE);
    return block;
}

static uint64_t get_bit_in_block(uint64_t bit) 
{
    uint64_t bit_in_block = bit%BIT_SIZE;
    return bit_in_block;
}

static uint64_t get_block_count(uint64_t size) 
{
    return (size / BLOCK_SIZE/BIT_SIZE)+1;
}

int pmem_addr_get_status(uint64_t addr, uint64_t *bitmap) 
{
    uint64_t bit = get_bit_in_map(addr);
    uint64_t block = get_block(bit);
    bit  = get_bit_in_block(bit);
  
    uint64_t val = bitmap[block] & (1UL << bit);

    return val > 1;
}

void pmem_addr_set_block(uint64_t addr, uint64_t *bitmap) 
{
    uint64_t bit = get_bit_in_map(addr);
    uint64_t block = get_block(bit);
    bit  = get_bit_in_block(bit);

    bitmap[block] |=  1UL <<bit;
}

void pmem_addr_free_block(uint64_t addr, uint64_t *bitmap) 
{
    uint64_t bit = get_bit_in_map(addr);
    uint64_t block = get_block(bit);
    bit  = get_bit_in_block(bit);
    bitmap[block] &=  ~(1UL <<bit);
}

//size: how 
uint64_t *pmem_alloc_bitmap(int size, int *size_allocated, void *memory_loc) {
    uint64_t count = get_block_count(size);
    uint64_t *bitmap = (uint64_t *) memory_loc;

    *size_allocated = (count*(BIT_SIZE/8));

    //initilize the bitmap to 0
    for(uint32_t i=0; i<count; i++) {
        bitmap[i] = 0;
    }

    return 0;
}

void phys_mem_early_init(uint64_t  mb_info) {

    int i;
    struct multiboot_info header;
    struct multiboot_mmap_entry entry;
    uint32_t offset = 0;
    uint64_t addr,max_addr;
    uint64_t len;



    for(i=0; i<MAX_PHYS_ZONES; i++)
        phys_mem_zones[i].in_use = 0;
    
    i = 0;

    //Read multiboot header and initialize zones
    header = *((struct multiboot_info *) mb_info);
    while (offset < header.mmap_length)  {
        entry = *(struct multiboot_mmap_entry *)( (uint64_t)header.mmap_addr + offset);
        addr = ((uint64_t)(entry.addr_high))<<32 | entry.addr_low;
        len = ((uint64_t)(entry.len_high))<<32  | entry.len_low;
        max_addr = (addr+len)-1;
        phys_mem_zones[i].in_use = 1;
        phys_mem_zones[i].base_ptr = (uint64_t *)addr;
        phys_mem_zones[i].len = len;
        if (entry.type == MULTIBOOT_MEMORY_RESERVED)
            phys_mem_zones[i].type = PMEM_RESERVED;
        else
            phys_mem_zones[i].type = PMEM_AVALIABLE;

        if (max_addr< 0x10000) 
            phys_mem_zones[i].location = PMEM_LOC_BELOW_1MB;
        else if (max_addr < 0x100000000) 
            phys_mem_zones[i].location = PMEM_LOC_BELOW_4GB;
        else
            phys_mem_zones[i].location = PMEM_LOC_HI_MEM;

        kprintf("addr:%x-%x\n",addr,(addr+len)-1);
        kprintf("entry_type:%s\n",phys_mem_zones[i].type == PMEM_RESERVED ? "reserved":"avail");
        i++;
        if (i >= MAX_PHYS_ZONES)
            panic("No more physical zones avaliable");
        offset += sizeof(struct multiboot_mmap_entry);
    }
    phys_max_found_zone = i;
}
