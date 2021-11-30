#ifndef FAT_H
#define FAT_H
#include <block/ata.h>
#include <include/types.h>

typedef struct fat_extBS_32
{
    //extended fat32 stuff
    uint32_t        table_size_32;
    uint16_t        extended_flags;
    uint16_t        fat_version;
    uint32_t        root_cluster;
    uint16_t        fat_info;
    uint16_t        backup_BS_sector;
    uint8_t         reserved_0[12];
    uint8_t         drive_number;
    uint8_t         reserved_1;
    uint8_t         boot_signature;
    uint32_t        volume_id;
    uint8_t         volume_label[11];
    uint8_t         fat_type_label[8];
 
}__attribute__((packed)) fat_extBS_32_t;
 
typedef struct fat_extBS_16
{
    //extended fat12 and fat16 stuff
    uint8_t        bios_drive_num;
    uint8_t        reserved1;
    uint8_t        boot_signature;
    uint32_t        volume_id;
    uint8_t        volume_label[11];
    uint8_t        fat_type_label[8];
 
}__attribute__((packed)) fat_extBS_16_t;
 
typedef struct fat_BS
{
    uint8_t         bootjmp[3];
    uint8_t         oem_name[8];
    uint16_t             bytes_per_sector;
    uint8_t        sectors_per_cluster;
    uint16_t        reserved_sector_count;
    uint8_t        table_count;
    uint16_t        root_entry_count;
    uint16_t        total_sectors_16;
    uint8_t        media_type;
    uint16_t        table_size_16;
    uint16_t        sectors_per_track;
    uint16_t        head_side_count;
    uint32_t         hidden_sector_count;
    uint32_t         total_sectors_32;
 
    //this will be cast to it's specific type once the driver actually knows what type of FAT this is.
    uint8_t        extended_section[54];
 
}__attribute__((packed)) fat_BS_t;

typedef struct std_fat_8_3_fmt
{
    uint8_t         fname[11];
    uint8_t         attribute;
    uint8_t         nt_resv;
    uint8_t        create_time_secs;
    uint16_t        create_time;
    uint16_t      create_date;
    uint16_t        access_date;
    uint16_t        high_cluster;
    uint16_t      modification_time;
    uint16_t      modification_date;
    uint16_t        low_cluster;
    uint32_t        file_size;
 
}__attribute__((packed)) std_fat_8_3_fmt;

struct fatFS
{
    struct fat_extBS_32 fat_boot_ext_32;
    struct fat_BS fat_boot;
    struct mbr_info mbr_info;
    uint32_t fat_size;
    uint32_t first_data_sector;
    uint32_t first_fat_sector;
};

int fat_init(struct mbr_info mbr_entry);
#endif
