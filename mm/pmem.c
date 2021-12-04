#include <include/types.h>

#define BLOCK_SIZE 4096
#define BIT_SIZE 64

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


