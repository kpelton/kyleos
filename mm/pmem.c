#include <include/types.h>
#include <mm/pmem.h>
#include <mm/mm.h>
#include <output/output.h>

#define BIT_SIZE 64
#define MAX_PHYS_ZONES 256
#define PHYS_MEM_START 0x100000
extern unsigned long _kernel_end;

static struct phys_mem_zone phys_mem_zones[MAX_PHYS_ZONES];
static int phys_max_found_zone = -1;
static int phys_first_region = -1;

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

static int count_bits (uint64_t var) 
{
    int bits = 0;
    while (var != 0) {
        bits +=  var & 1;
        var>>=1;
    }
    return bits;
}

static uint64_t get_used_page_count(uint64_t size,uint64_t *bitmap)
{
    uint64_t i;
    uint64_t used_pages = 0;
    for (i=0; i<get_block_count(size); i++) {
        used_pages += count_bits(bitmap[i]);   

    }
    return used_pages;
}
int pmem_addr_get_status(uint64_t addr, uint64_t *bitmap) 
{
    uint64_t bit = get_bit_in_map(addr);
    uint64_t block = get_block(bit);
    bit  = get_bit_in_block(bit);

    kprintf("addr, BITMAP,%x %x\n",addr,bitmap);
    uint64_t val = bitmap[block] & (1UL << bit);

    return val >= 1;
}

void pmem_addr_set_block(uint64_t addr, uint64_t *bitmap) 
{
    uint64_t bit = get_bit_in_map(addr);
    uint64_t block = get_block(bit);
    bit  = get_bit_in_block(bit);
    bitmap[block] |=  1UL <<bit;
}

void pmem_addr_set_region(uint64_t addr, uint64_t size, uint64_t *bitmap) 
{
    while (size !=  0) {
        pmem_addr_set_block(addr,bitmap);
        //kprintf("setting %x %x at %x\n",addr, (addr+size)-1,bitmap);

        addr+=BLOCK_SIZE;
        //IF the address is unaligned
        if (size-BLOCK_SIZE > size)
            break;
        size-=BLOCK_SIZE;
    }   
}

uint64_t pmem_addr_find_first_free_block(uint64_t size,uint64_t *bitmap)
{
    uint64_t i;
    uint64_t addr=0;
    uint64_t val;
    for (i=0; i<get_block_count(size); i++) {
        if (bitmap[i] < 0xffffffffffffffff){
            val = bitmap[i];
            //find where the bit is set
            while (val & 1)  {
                addr+=BLOCK_SIZE;
                val>>=1;
            }
            //kprintf("found free page at %x\n",addr);
            return addr;
        }

        addr+=BLOCK_SIZE*BIT_SIZE;
    }
    panic("WARNING could not find enough phys memory");
    return 0;
}

uint64_t pmem_addr_find_first_chunk(uint64_t size,uint64_t chunk_size, uint64_t *bitmap)
{
    uint64_t i;
    uint64_t addr=0;
    uint64_t found_size;
    for (i=0; i<get_block_count(size); i++) {

        if (bitmap[i] == 0x0){
            found_size+=(BLOCK_SIZE*BIT_SIZE);
            if(found_size >= chunk_size ) {
                kprintf("chunk at P:0x%x of at least %d pages\n",addr,chunk_size);
                return addr;
            }
        }else{
            found_size = 0;
        }
        addr+=BLOCK_SIZE*BIT_SIZE;
    }
    panic("WARNING could not find enough phys memory");
    return 0;
}

void pmem_addr_free_block(uint64_t addr, uint64_t *bitmap) 
{
    uint64_t bit = get_bit_in_map(addr);
    uint64_t block = get_block(bit);
    bit  = get_bit_in_block(bit);
    bitmap[block] &=  ~(1UL <<bit);
}

//size: how 
uint64_t *pmem_alloc_bitmap(uint64_t size, uint64_t *size_allocated, void *memory_loc) {
    //TODO: Allocate bitmap on 64b boundry. This will be slow
    uint64_t count = get_block_count(size);
    uint64_t *bitmap = (uint64_t *) memory_loc;
    *size_allocated = (count*(BIT_SIZE/8));

    
    //initilize the bitmap to 0
    for(uint32_t i=0; i<count; i++) {
       bitmap[i] = 0;
   }
    return bitmap;
}

void phys_mem_reserve_inital_region(uint64_t kernel_bitmap_size) {
    
    uint64_t i;
    kprintf("Marking Kernel + bitmap_region as used 0x%x\n",kernel_bitmap_size);
    kprintf("kernel end %x\n",&_kernel_end);

    i = phys_first_region;
     pmem_addr_set_region(0,
                                kernel_bitmap_size,
                                phys_mem_zones[i].bitmap);
    
}

void *pmem_alloc_block(unsigned int size_in_pages) {
    uint64_t addr = pmem_addr_find_first_chunk(phys_mem_zones[phys_first_region].len,
                                            size_in_pages,phys_mem_zones[phys_first_region].bitmap);

    if (addr != 0) {
        pmem_addr_set_region(addr,size_in_pages,phys_mem_zones[phys_first_region].bitmap);
    }
    //We need to add the base address here because all bitmaps start at 0
    return (void *)addr +phys_mem_zones[phys_first_region].base_addr;

}

void *pmem_alloc_page() {
    uint64_t addr = pmem_addr_find_first_free_block(phys_mem_zones[phys_first_region].len,phys_mem_zones[phys_first_region].bitmap);

    if (addr != 0) {
        pmem_addr_set_block(addr,phys_mem_zones[phys_first_region].bitmap);
    }
    //We need to add the base address here because all bitmaps start at 0
    return (void *)addr +phys_mem_zones[phys_first_region].base_addr;

}


void phys_mem_init() {

    //fix allignment issues here;
    uint64_t *alloc_location =  &_kernel_end ;
    uint64_t *initial_alloc_location = alloc_location;
    uint64_t size;
    uint64_t addr;
    uint64_t alloc_size; 
    uint64_t total_size; 

    for (int i=0; i != phys_max_found_zone; i++) {   
        if (phys_mem_zones[i].type != PMEM_RESERVED) {
        phys_mem_zones[i].bitmap = pmem_alloc_bitmap(phys_mem_zones[i].len,
                                                    &size, (void *)alloc_location);
        addr = (uint64_t) phys_mem_zones[i].base_addr;                  
        kprintf("Alloc %d B for P:0x%x-P:0x%x bitmap is at 0x%x\n",size,
                                                                addr,
                                                                (addr+phys_mem_zones[i].len)-1,
                                                                phys_mem_zones[i].bitmap);    
        alloc_location += size;                
        }
    }
    kprintf("Using %d K to store bitmaps\n",(alloc_location - initial_alloc_location)/1024);

    //alloc regions
    //for (i=0; i<)
    kprintf("kernel end %x\n",&_kernel_end);
    kprintf("initial alloc %x\n",initial_alloc_location);
    kprintf("alloc_location %x\n",alloc_location);
    alloc_size  = alloc_location - initial_alloc_location;
    total_size =  ((uint64_t)&_kernel_end - 0xffffffff80000000)+alloc_size + 4096;
    phys_mem_reserve_inital_region(total_size);
    kprintf("Inital region is using %d B out of %d B\n",
                        get_used_page_count(phys_mem_zones[phys_first_region].len,phys_mem_zones[phys_first_region].bitmap)*4096,
                        (phys_mem_zones[phys_first_region].len/4096) * 4096);

}

void phys_mem_print_usage() {
    kprintf("Below 4G region is using %d B out of %d B\n",
                    get_used_page_count(phys_mem_zones[phys_first_region].len,phys_mem_zones[phys_first_region].bitmap)*4096,
                    (phys_mem_zones[phys_first_region].len/4096) * 4096);
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
        phys_mem_zones[i].base_addr = addr;
        phys_mem_zones[i].len = len;
        if (entry.type == MULTIBOOT_MEMORY_RESERVED)
            phys_mem_zones[i].type = PMEM_RESERVED;
        else
            phys_mem_zones[i].type = PMEM_AVALIABLE;

        if (max_addr< 0x100000)  {
            phys_mem_zones[i].location = PMEM_LOC_BELOW_1MB;
            phys_mem_zones[i].in_use = 0;
        }
        else if (max_addr < 0x100000000) {
            if (phys_first_region == -1 && phys_mem_zones[i].in_use == 1)
                phys_first_region = i;
            phys_mem_zones[i].location = PMEM_LOC_BELOW_4GB;
        }   
        else {
            phys_mem_zones[i].location = PMEM_LOC_HI_MEM;
        }

        kprintf("addr:%x-%x\n",addr,(addr+len)-1);
        kprintf("entry_type:%s\n",phys_mem_zones[i].type == PMEM_RESERVED ? "reserved":"avail");
        i++;
        if (i >= MAX_PHYS_ZONES)
            panic("No more physical zones avaliable");
        offset += sizeof(struct multiboot_mmap_entry);
    }
    phys_max_found_zone = i;
}
