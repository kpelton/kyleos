#ifndef FAT_H
#define FAT_H
#include <block/ata.h>

typedef struct fat_extBS_32
{
	//extended fat32 stuff
	unsigned int		table_size_32;
	unsigned short		extended_flags;
	unsigned short		fat_version;
	unsigned int		root_cluster;
	unsigned short		fat_info;
	unsigned short		backup_BS_sector;
	unsigned char 		reserved_0[12];
	unsigned char		drive_number;
	unsigned char 		reserved_1;
	unsigned char		boot_signature;
	unsigned int 		volume_id;
	unsigned char		volume_label[11];
	unsigned char		fat_type_label[8];
 
}__attribute__((packed)) fat_extBS_32_t;
 
typedef struct fat_extBS_16
{
	//extended fat12 and fat16 stuff
	unsigned char		bios_drive_num;
	unsigned char		reserved1;
	unsigned char		boot_signature;
	unsigned int		volume_id;
	unsigned char		volume_label[11];
	unsigned char		fat_type_label[8];
 
}__attribute__((packed)) fat_extBS_16_t;
 
typedef struct fat_BS
{
	unsigned char 		bootjmp[3];
	unsigned char 		oem_name[8];
	unsigned short 	        bytes_per_sector;
	unsigned char		sectors_per_cluster;
	unsigned short		reserved_sector_count;
	unsigned char		table_count;
	unsigned short		root_entry_count;
	unsigned short		total_sectors_16;
	unsigned char		media_type;
	unsigned short		table_size_16;
	unsigned short		sectors_per_track;
	unsigned short		head_side_count;
	unsigned int 		hidden_sector_count;
	unsigned int 		total_sectors_32;
 
	//this will be cast to it's specific type once the driver actually knows what type of FAT this is.
	unsigned char		extended_section[54];
 
}__attribute__((packed)) fat_BS_t;

typedef struct std_fat_8_3_fmt
{
	unsigned char 		fname[11];
	unsigned char 		attribute;
	unsigned char 	    nt_resv;
	unsigned char		create_time_secs;
	unsigned short		create_time;
	unsigned short      create_date;
	unsigned short		access_date;
	unsigned short		high_cluster;
	unsigned short      modification_time;
	unsigned short      modification_date;
	unsigned short		low_cluster;
	unsigned int		file_size;
 
}__attribute__((packed)) std_fat_8_3_fmt;

struct fatFS
{
    struct fat_extBS_32 fat_boot_ext_32;
    struct fat_BS fat_boot;
    struct mbr_info mbr_info;
    unsigned int fat_size;
    unsigned int first_data_sector;
    unsigned int first_fat_sector;
};

int fat_init(struct mbr_info mbr_entry);
#endif
