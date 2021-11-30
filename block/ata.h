#ifndef ATA_H
#define ATA_H
#include <include/types.h>
void ata_init();
int read_sec(uint32_t sec,void *buffer);
int write_sec(uint32_t sec,void *buffer);
struct mbr_info {
    uint32_t fs_start;
    uint32_t fs_size;
    uint8_t part_type;
};
#endif
