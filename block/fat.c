#include <block/fat.h>
#include <block/ata.h>
#include <output/output.h>
#include <mm/mm.h>

static void read_directory(unsigned int sec);
static void read_file(unsigned int cluster,unsigned int first_fat_sector,unsigned int first_data_sector);
static inline unsigned int clust2sec(unsigned int cluster,struct fatFS * fs);
#define FAT_UNUSED_DIR 0xe5
#define FAT_END_OF_CHAIN 0x0FFFFFF8
#define FAT_LONG_FILENAME 0xf

struct fatFS * fat_init(struct mbr_info mbr_entry) {
    char bs_buf[512];
    struct  fatFS* fs;

    fs = kmalloc(sizeof(struct fatFS));
    kprintf("FAT driver init\n");
    //Read sector and fill out fs info data struct
    read_sec(mbr_entry.fs_start,bs_buf);
    fs->fat_boot = *(struct fat_BS *)bs_buf;
    fs->fat_boot_ext_32 = *(struct fat_extBS_32 *)fs->fat_boot.extended_section;
    fs->fat_size = fs->fat_boot_ext_32.table_size_32;
    fs->first_data_sector = mbr_entry.fs_start + fs->fat_boot.reserved_sector_count
                            + (fs->fat_boot.table_count * fs->fat_size);
    fs->first_fat_sector = mbr_entry.fs_start + fs->fat_boot.reserved_sector_count;
    fs->mbr_info = mbr_entry;

    kprintf("==== Fat debug ====\n\n");
    kprint_hex("mbr_entry.fs_start 0x",mbr_entry.fs_start);
    kprint_hex("first_fat_sector 0x",fs->first_fat_sector);
    kprint_hex("reserved sectors 0x",fs->fat_boot.reserved_sector_count);
    kprint_hex("sectors per cluster",fs->fat_boot.sectors_per_cluster);
    kprint_hex("table count 0x",fs->fat_boot.table_count);
    kprint_hex("fat_size 0x",fs->fat_size);
  
    read_directory(fs->first_data_sector);
    //read_file(0x804,fs->first_fat_sector,fs->first_data_sector);
    read_directory(clust2sec(0xbe1,fs));
    return fs;
}

static inline unsigned int clust2sec(unsigned int cluster,struct fatFS * fs) {
    if (cluster == 0 ) {
        return fs->first_data_sector;
    }
    return (cluster - 2) * fs->fat_boot.sectors_per_cluster + fs->first_data_sector;
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
    
    unsigned int table_value =  (FAT_table[ent_offset+3] << 24 | \
			    FAT_table[ent_offset+2] << 16 | \
				FAT_table[ent_offset+1] << 8  | \
				FAT_table[ent_offset]) & 0x0FFFFFFF;

    return table_value;
}

static void read_file(unsigned int cluster,unsigned int first_fat_sector,unsigned int first_data_sector) {
    unsigned char cluster_dest[4097];
    unsigned int clust = cluster;
    cluster_dest[4096] = '\0';

    kprintf("in read file\n");
    while (clust < FAT_END_OF_CHAIN) {
        read_cluster(((clust-2)*8+first_data_sector),cluster_dest);
        clust = read_fat_ptr(clust,first_fat_sector);
        kprintf((char *) cluster_dest);
    }
}

static void read_directory(unsigned int sec) {
    unsigned char cluster[4096];
    std_fat_8_3_fmt *file;
    unsigned char *dir_ptr = cluster;
    unsigned int cnumber;
    kprint_hex("sector ",sec);
    read_cluster(sec,cluster);
    //while we are not 0 in this directory
    kprintf("===Directory contents===\n");

    kprint_hex("dir_ptr ",*dir_ptr);
    while (*dir_ptr != 0) { 
                if (*dir_ptr != FAT_UNUSED_DIR && !(*dir_ptr == 0x41 && dir_ptr[11] == FAT_LONG_FILENAME))  {
                    file = (std_fat_8_3_fmt *) dir_ptr;
                    if (file->attribute != 0xf) {
                        kprintf((char *) file->fname);
                        cnumber = file->high_cluster<<16|file->low_cluster;
                        kprint_hex("Cluster ",cnumber);
                        //kprint_hex("Attribute ",file->attribute);
                    }
             }
        dir_ptr+=32;
    }
}

