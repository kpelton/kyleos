#ifndef ATA_H
#define ATA_H
#include <include/types.h>
#define ATA_SECTOR_SIZE 512
#define ATA_MAX_SECTORS_PER_COMMAND 255U


void ata_init();
void ata_primary_irq(void);
void ata_secondary_irq(void);
int read_sectors(uint32_t sec, uint32_t count, void *buffer);
int read_sec(uint32_t sec,void *buffer);
int write_sec(uint32_t sec,const void *buffer);
int write_sectors(uint32_t sec,uint32_t count,const void *buffer);
struct mbr_info {
    uint32_t fs_start;
    uint32_t fs_size;
    uint8_t part_type;
};
#endif
