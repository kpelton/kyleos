#include <block/fat.h>
#include <block/ata.h>
#include <block/vfs.h>
#include <output/output.h>
#include <mm/mm.h>
#define NULL 0
static void read_directory(unsigned int sec, struct dnode *dir, struct vfs_device *dev);

static void read_file(unsigned int cluster, unsigned int first_fat_sector, unsigned int first_data_sector);
static inline unsigned int clust2sec(unsigned int cluster, struct fatFS *fs);
void read_inode_file(struct inode *i_node);

struct dnode *fat_read_root_dir(struct vfs_device *dev);
struct dnode *read_inode_dir(struct inode *i_node);
#define FAT_UNUSED_DIR 0xe5
#define FAT_END_OF_CHAIN 0x0FFFFFF8
#define FAT_LONG_FILENAME 0xf
#define FAT_DIR 0x10
#define FAT_FILE 0x20
//#define DEBUG
int fat_init(struct mbr_info mbr_entry)
{
    char bs_buf[512];
    struct fatFS *fs;
    struct vfs_device vfs_dev;
    struct vfs_ops *vfs_ops;

    fs = kmalloc(sizeof(struct fatFS));
    vfs_ops = kmalloc(sizeof(struct vfs_ops));
    kprintf("FAT driver init\n");
    //Read sector and fill out fs info data struct
    read_sec(mbr_entry.fs_start, bs_buf);
    fs->fat_boot = *(struct fat_BS *)bs_buf;
    fs->fat_boot_ext_32 = *(struct fat_extBS_32 *)fs->fat_boot.extended_section;
    fs->fat_size = fs->fat_boot_ext_32.table_size_32;
    fs->first_data_sector = mbr_entry.fs_start + fs->fat_boot.reserved_sector_count + (fs->fat_boot.table_count * fs->fat_size);
    fs->first_fat_sector = mbr_entry.fs_start + fs->fat_boot.reserved_sector_count;
    fs->mbr_info = mbr_entry;

#ifdef DEBUG
    kprintf("==== Fat debug ====\n\n");
    kprint_hex("mbr_entry.fs_start 0x", mbr_entry.fs_start);
    kprint_hex("first_fat_sector 0x", fs->first_fat_sector);
    kprint_hex("reserved sectors 0x", fs->fat_boot.reserved_sector_count);
    kprint_hex("sectors per cluster", fs->fat_boot.sectors_per_cluster);
    kprint_hex("table count 0x", fs->fat_boot.table_count);
    kprint_hex("fat_size 0x", fs->fat_size);
#endif
    //setup vfs device struct to be registered
    vfs_dev.fstype = FAT_FS;
    vfs_ops->read_root_dir = &fat_read_root_dir;
    vfs_ops->read_inode_dir = &read_inode_dir;
    vfs_ops->read_inode_file = &read_inode_file;
    vfs_dev.ops = vfs_ops;
    vfs_dev.finfo.fat = fs;

    vfs_register_device(vfs_dev);
    return 0;
}
static inline unsigned int clust2sec(unsigned int cluster, struct fatFS *fs)
{
    if (cluster == 0)
    {
        return fs->first_data_sector;
    }
    return (cluster - 2) * fs->fat_boot.sectors_per_cluster + fs->first_data_sector;
}

struct dnode *fat_read_root_dir(struct vfs_device *dev)
{
    struct dnode *dir;
    dir = kmalloc(sizeof(struct dnode));
    dir->root_inode = kmalloc(sizeof(struct inode));
    dir->root_inode->i_ino= 0;
    dir->root_inode->dev = dev;
    dir->root_inode->i_type = I_DIR;
    kstrcpy(dir->root_inode->i_name, "/");
    
    read_directory(dev->finfo.fat->first_data_sector, dir, dev);
    return dir;
}

struct dnode *read_inode_dir(struct inode *i_node)
{
    struct dnode *dir;

    dir = kmalloc(sizeof(struct dnode));
    dir->root_inode = kmalloc(sizeof(struct inode));
    dir->root_inode->i_ino =  i_node->i_ino;
    dir->root_inode->dev = i_node->dev;
    dir->root_inode->i_type = I_DIR;
    kstrcpy(dir->root_inode->i_name, i_node->i_name);

    kstrcpy(dir->i_name, i_node->i_name);
    unsigned int sec = clust2sec(i_node->i_ino, i_node->dev->finfo.fat);
    read_directory(sec, dir, i_node->dev);
    return dir;
}

void read_inode_file(struct inode *i_node)
{
    read_file(i_node->i_ino, i_node->dev->finfo.fat->first_fat_sector,
              i_node->dev->finfo.fat->first_data_sector);
}
static void read_cluster(unsigned int cluster_start, unsigned char *dest)
{
    int i;
    for (i = 0; i < 8; i++)
    {
        read_sec(cluster_start + i, dest + (512 * i));
    }
}

static unsigned int read_fat_ptr(unsigned int cluster_num, unsigned int first_fat_sector)
{
    //from osdev
    unsigned char FAT_table[512];
    unsigned int fat_offset = cluster_num * 4;
    unsigned int fat_sector = first_fat_sector + (fat_offset / 512);
    unsigned int ent_offset = fat_offset % 512;

    read_sec(fat_sector, FAT_table);

    unsigned int table_value = (FAT_table[ent_offset + 3] << 24 |
                                FAT_table[ent_offset + 2] << 16 |
                                FAT_table[ent_offset + 1] << 8 |
                                FAT_table[ent_offset]) &
                               0x0FFFFFFF;

    return table_value;
}

static void read_file(unsigned int cluster, unsigned int first_fat_sector, unsigned int first_data_sector)
{
    unsigned char cluster_dest[4097];
    unsigned int clust = cluster;
    cluster_dest[4096] = '\0';
    while (clust < FAT_END_OF_CHAIN)
    {
        read_cluster(((clust - 2) * 8 + first_data_sector), cluster_dest);
        clust = read_fat_ptr(clust, first_fat_sector);
        kprintf((char *)cluster_dest);
    }
}

static void read_directory(unsigned int sec, struct dnode *dir, struct vfs_device *dev)
{
    unsigned char cluster[4096];
    std_fat_8_3_fmt *file;
    unsigned char *dir_ptr = cluster;
    unsigned int cnumber;
    struct inode_list *ilist;
    struct inode_list *prev_ilist;
    struct inode *cur_inode;
    dir->head = NULL;
    read_cluster(sec, cluster);
    //kprintf("===Directory contents===\n");
    //kprint_hex("dir_ptr ",*dir_ptr);
    while (*dir_ptr != 0) {
        if (*dir_ptr != FAT_UNUSED_DIR &&
            !(*dir_ptr == 0x41 && dir_ptr[11] == FAT_LONG_FILENAME)) {
            file = (std_fat_8_3_fmt *)dir_ptr;
            if (file->attribute != 0xf) {
                ilist = kmalloc(sizeof(struct inode_list));
                if (dir->head == NULL)
                    dir->head = ilist;
                else
                    prev_ilist->next = ilist;

                cur_inode = kmalloc(sizeof(struct inode));
                ilist->current = cur_inode;
                ilist->next = NULL;
                cur_inode->dev = dev;
                prev_ilist = ilist;
                kstrcpy(cur_inode->i_name, (const char *)file->fname);
                cnumber = file->high_cluster << 16 | file->low_cluster;
                cur_inode->i_ino = cnumber;

                if ((file->attribute & FAT_DIR) == FAT_DIR)
                    cur_inode->i_type = I_DIR;
                else if ((file->attribute & FAT_FILE) == FAT_FILE)
                    cur_inode->i_type = I_FILE;
            }
        }
        dir_ptr += 32;
    }
}
