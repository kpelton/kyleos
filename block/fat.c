#include <block/fat.h>
#include <block/ata.h>
#include <output/output.h>
#include <mm/mm.h>

static void read_directory(unsigned int sec,unsigned int first_fat_sector);
static void read_file(unsigned int cluster,unsigned int first_fat_sector,unsigned int first_data_sector);
extern struct mbr_info fs1;

void fat_init() {
    char bs_buf[512];
    unsigned int total_sectors;
    unsigned int fat_size;
    unsigned int first_data_sector;
    unsigned int first_fat_sector;
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
  
    read_directory(first_data_sector,fs1.fs_start + first_fat_sector);
    read_directory(0x802,fs1.fs_start + first_fat_sector);
    //read_file(0x16889+2,fs1.fs_start + first_fat_sector,first_data_sector);
    read_file(0x802,fs1.fs_start + first_fat_sector,first_data_sector);
    //read_file(0x16889+2,fs1.fs_start + first_fat_sector,first_data_sector);
    //read_file(0x16889+2,fs1.fs_start + first_fat_sector,first_data_sector);
    //read_file(0x16889+2,fs1.fs_start + first_fat_sector,first_data_sector);
    //read_file(0x16b4a,fs1.fs_start + first_fat_sector,first_data_sector);
}

static void read_cluster(unsigned int cluster_start, unsigned char *dest) {
    int i;
    for (i=0; i<8; i++) {
        read_sec(cluster_start+i,dest+(512*i));
    }
}

static unsigned int read_fat_ptr(unsigned int cluster_num,unsigned int first_fat_sector) {
    //from osdev
    unsigned char FAT_table[512];
    unsigned int fat_offset = cluster_num * 4;
    unsigned int fat_sector = first_fat_sector + (fat_offset / 512);
    unsigned int ent_offset = fat_offset % 512;

    read_sec(fat_sector,FAT_table);
    
    unsigned int table_value =  FAT_table[ent_offset+3] <<24 | FAT_table[ent_offset+2] <<16 | FAT_table[ent_offset+1] <<8 |FAT_table[ent_offset] & 0x0FFFFFFF;
    //kprint_hex("fat value ",table_value);
    kprint_hex("fat offset ",fat_sector*512 + ent_offset);
    return table_value;
}


static void read_file(unsigned int cluster,unsigned int first_fat_sector,unsigned int first_data_sector) {
    unsigned char cluster_dest[4097];
    unsigned int clust = cluster;
    cluster_dest[4096] = '\0';
    kprintf("in read file\n");
    while (clust < 0x0FFFFFF8) {
        read_cluster(clust*8+first_data_sector,cluster_dest);
        clust = read_fat_ptr(clust,first_fat_sector);

        kprint_hex("cluster val ",clust);
        kprintf(cluster_dest);
    }
}
static void read_directory(unsigned int sec,unsigned int first_fat_sector) {
    unsigned char cluster[4096];
    std_fat_8_3_fmt *file;
    unsigned char *dir_ptr = cluster;
    unsigned int cnumber;
    int i;
    kprint_hex("sector ",sec);
    read_cluster(sec,(char *)cluster);
    //while we are not 0 in this directory
    kprintf("===Directory contents===\n");
    while (*dir_ptr != 0) { 
                if (*dir_ptr != 0xe5 && !(*dir_ptr == 0x41 && dir_ptr[11] == 0xf))  {
                    file = (std_fat_8_3_fmt *) dir_ptr;
                    if (file->attribute != 0xf) {
                        kprintf(file->fname);
                        cnumber = file->high_cluster<<16|file->low_cluster;
                        kprint_hex("Cluster ",cnumber);
                        //kprint_hex("Attribute ",file->attribute);
                    }
             }
        dir_ptr+=32;
    }
}

