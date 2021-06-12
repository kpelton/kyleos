#ifndef ATA_H
#define ATA_H
void ata_init();
int read_sec(unsigned int sec,void *buffer);
int write_sec(unsigned int sec,void *buffer);
struct mbr_info {
    unsigned int fs_start;
    unsigned int fs_size;
    unsigned char part_type;
};
#endif
