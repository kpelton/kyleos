#include <block/fat.h>
#include <block/ata.h>
#include <output/output.h>
#include <mm/mm.h>

static void read_directory(unsigned int sec);

void fat_init() {
    char bs_buf[512];
    char cbuffer[10];
    unsigned int total_sectors;
    unsigned int fat_size;
    unsigned int root_dir_sectors;
    unsigned int first_data_sector;
    unsigned int first_root_dir_sector;
    unsigned int first_fat_sector;
    unsigned int data_sectors;
    unsigned int first_sector_of_cluster;
    unsigned int root_cluster_32;

    struct fat_BS *fat_boot;
    struct fat_extBS_32 *fat_boot_ext_32;
    kprintf("FAT driver init\n");

    read_sec(fs1.fs_start,bs_buf);
    fat_boot = (struct fat_BS *)bs_buf;
    fat_boot_ext_32 = (struct fat_extBS_32 *)fat_boot->extended_section;

    total_sectors = fat_boot->total_sectors_32;
    fat_size = fat_boot_ext_32->table_size_32;
    first_data_sector = fs1.fs_start + fat_boot->reserved_sector_count + (fat_boot->table_count * fat_size); 
    first_fat_sector = fat_boot->reserved_sector_count;
   
   kprintf("==== Fat debug ====\n\n");

    kprint_hex("fs1.fs_start 0x",fs1.fs_start);
    kprint_hex("first_fat_sector 0x",first_fat_sector);
    kprint_hex("reserved sectors 0x",fat_boot->reserved_sector_count);
    kprint_hex("fs_start + reserved sectors byte offset",(fs1.fs_start +fat_boot->reserved_sector_count+(fat_size*fat_boot->table_count))*512);
    kprint_hex("fs_start + reserved sectors sector offset",(fs1.fs_start +fat_boot->reserved_sector_count+(fat_size*fat_boot->table_count)));
    kprint_hex("cluster start",(fs1.fs_start +(fat_size*fat_boot->table_count)));
    kprint_hex("first_data_sector 0x",first_data_sector);
    kprint_hex("table count 0x",fat_boot->table_count);
    kprint_hex("fat_size 0x",fat_size);
    read_directory(first_data_sector);
    read_directory(0x3e48);
}

static void read_cluster(unsigned int cluster_start, unsigned char *dest) {
    int i;
    for (i=0; i<8; i++) {
        read_sec(cluster_start+i,dest+(512*i));
    }
}

static void read_directory(unsigned sec) {
    unsigned char cluster[4096];
    std_fat_8_3_fmt *file;
    unsigned char *dir_ptr = cluster;
    int i;
    kprint_hex("sector ",sec);
    read_cluster(sec,cluster);
    //while we are not 0 in this directory
    kprintf("===Directory contents===\n");
    while (*dir_ptr != 0) { 
        if (*dir_ptr != 0xe5 && *dir_ptr != 0x41)  {
            file = (std_fat_8_3_fmt *) dir_ptr;
            kprintf(file->fname);
            kprint_hex("Cluster ",file->low_cluster);
            kprint_hex("Attribute ",file->attribute);
            if(file->attribute == 0x10) {
                //read_directory((file->low_cluster-2)*8+ 0x1820);
                kprint_hex("test ",(file->low_cluster-2)*8+ 0x1820);
            }
            kprint_hex("Size ",file->file_size);
            kprintf("\n");
        }
        dir_ptr+=32;
    }
  
  //kprint_hex("test ",(0x4c6 -2) + 0x1820);
  //read_cluster((0x4c4*8+0x1820), cluster);
  //kprintf(cluster);
}

