#include <block/fat.h>
#include <block/ata.h>
#include <block/vfs.h>
#include <output/output.h>
#include <mm/mm.h>

#define NULL 0
static void read_directory(struct dnode *dir, struct vfs_device *dev);
static void read_file(uint32_t cluster, uint32_t first_fat_sector, uint32_t first_data_sector);
static inline uint32_t clust2sec(uint32_t cluster, struct fatFS *fs);
static uint32_t read_fat_ptr(uint32_t cluster_num, uint32_t first_fat_sector);
struct dnode *fat_read_root_dir(struct vfs_device *dev);
struct dnode *read_inode_dir(struct inode *i_node);
static void write_directory(struct inode *parent, char *name);

void cat_inode_file(struct inode *i_node);
int fat_create_dir(struct inode *parent, char *name);
int read_inode_file(struct file *rfile, void *buf, uint32_t count);

#define FAT_UNUSED_DIR 0xe5
#define FAT_END_OF_CHAIN 0x0FFFFFF8
#define FAT_LONG_FILENAME 0xf
// how many bytes per long file name record
#define FAT_LFNAME_RECORD_SIZE 13
#define FAT_DIR 0x10
#define FAT_ATTRIBUTE 11
#define FAT_FILE 0x20
#define FAT_DIR_RECORD_SIZE 32
#define FAT_MAX_FNAME 256
#define FAT_MAX_LFNAME_RECORDS 0x3f
#define FAT_CLUSTER_SIZE 4096

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
    // Read sector and fill out fs info data struct
    read_sec(mbr_entry.fs_start, bs_buf);
    fs->fat_boot = *(struct fat_BS *)bs_buf;
    fs->fat_boot_ext_32 = *(struct fat_extBS_32 *)fs->fat_boot.extended_section;
    fs->fat_size = fs->fat_boot_ext_32.table_size_32;
    kprintf("Root cluster %x\n", fs->fat_boot_ext_32.root_cluster);
    fs->root_cluster = fs->fat_boot_ext_32.root_cluster;
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
    // setup vfs device struct to be registered
    vfs_dev.fstype = FAT_FS;
    vfs_ops->read_root_dir = &fat_read_root_dir;
    vfs_ops->read_inode_dir = &read_inode_dir;
    vfs_ops->cat_inode_file = &cat_inode_file;
    vfs_ops->read_file = &read_inode_file;
    vfs_ops->create_dir = &fat_create_dir;
    vfs_dev.ops = vfs_ops;
    vfs_dev.finfo.fat = fs;

    vfs_register_device(vfs_dev);
    return 0;
}

static uint32_t find_free_cluster(struct fatFS *fs)
{
    uint32_t *FAT_table = kmalloc(512);
    uint32_t fat_sector = fs->first_fat_sector;
    uint32_t table_value = 0;
    uint32_t i = 0;
    // will only read first 64 entries of FAT table
    read_sec(fat_sector, FAT_table);
    // kprintf("Read sector %x\n", fat_sector);
    for (i = 40; i < 64; i += 1)
    {

        table_value = FAT_table[i] & 0x0FFFFFFF;

        kprintf("finding cluster:%d %x\n", i, table_value);

        if (table_value == 0)
        {
            FAT_table[i] = 0x0ffffff8;
            write_sec(fat_sector, FAT_table);
            kfree(FAT_table);
            return i;
        }
    }
    panic("unable to find cluster");
    return 0;
}

static uint32_t add_new_link_to_chain(uint32_t cluster_num, struct fatFS *fs)
{
    // from osdev
    uint8_t FAT_table[512];
    uint32_t fat_offset = cluster_num * 4;
    uint32_t fat_sector = fs->first_fat_sector + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;
    uint32_t new_cluster;

    new_cluster = find_free_cluster(fs);

    read_sec(fat_sector, FAT_table);

    uint32_t table_value = (FAT_table[ent_offset + 3] << 24 |
                            FAT_table[ent_offset + 2] << 16 |
                            FAT_table[ent_offset + 1] << 8 |
                            FAT_table[ent_offset]) &
                           0x0FFFFFFF;
    kprintf("Reading fat:%d %x\n", fat_sector, table_value);

    FAT_table[ent_offset + 3] = (new_cluster >> 24) & 0xff;
    FAT_table[ent_offset + 2] = (new_cluster >> 16) & 0xff;
    FAT_table[ent_offset + 1] = (new_cluster >> 8) & 0xff;
    FAT_table[ent_offset] = (new_cluster)&0xff;
    write_sec(fat_sector, FAT_table);

    return new_cluster;
}

int fat_create_dir(struct inode *parent, char *name)
{

    write_directory(parent, name);
    return 1;
}

static inline uint32_t clust2sec(uint32_t cluster, struct fatFS *fs)
{
    if (cluster == 0)
        return fs->first_data_sector;

    return (cluster - 2) * fs->fat_boot.sectors_per_cluster + fs->first_data_sector;
}

struct dnode *fat_read_root_dir(struct vfs_device *dev)
{
    struct dnode *dir;
    kprintf("read_root_dir\n");

    dir = kmalloc(sizeof(struct dnode));
    dir->root_inode = kmalloc(sizeof(struct inode));
    dir->root_inode->i_ino = dev->finfo.fat->root_cluster;
    dir->root_inode->dev = dev;
    dir->root_inode->i_type = I_DIR;
    kstrncpy(dir->root_inode->i_name, "/", 2);

    read_directory(dir, dev);

    return dir;
}

struct dnode *read_inode_dir(struct inode *i_node)
{
    struct dnode *dir;
    kprintf("inode_dir_alloc\n");
    dir = kmalloc(sizeof(struct dnode));
    dir->root_inode = kmalloc(sizeof(struct inode));
    dir->root_inode->i_ino = i_node->i_ino;
    dir->root_inode->dev = i_node->dev;
    dir->root_inode->i_type = I_DIR;
    kstrncpy(dir->root_inode->i_name, i_node->i_name, 256);
    kstrncpy(dir->i_name, i_node->i_name, 256);
    read_directory(dir, i_node->dev);
    return dir;
}

void cat_inode_file(struct inode *i_node)
{
    read_file(i_node->i_ino, i_node->dev->finfo.fat->first_fat_sector,
              i_node->dev->finfo.fat->first_data_sector);
}
static void read_cluster(uint32_t sector_start, uint8_t *dest)
{
    int i;
    for (i = 0; i < 8; i++)
        read_sec(sector_start + i, dest + (512 * i));
}

static void write_cluster(uint32_t sector_start, uint8_t *data)
{
    int i;
    for (i = 0; i < 8; i++)
        write_sec(sector_start + i, data + (512 * i));
}

static uint32_t read_fat_ptr(uint32_t cluster_num, uint32_t first_fat_sector)
{
    // from osdev
    uint8_t FAT_table[512];
    uint32_t fat_offset = cluster_num * 4;
    uint32_t fat_sector = first_fat_sector + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    read_sec(fat_sector, FAT_table);

    uint32_t table_value = (FAT_table[ent_offset + 3] << 24 |
                            FAT_table[ent_offset + 2] << 16 |
                            FAT_table[ent_offset + 1] << 8 |
                            FAT_table[ent_offset]) &
                           0x0FFFFFFF;

    kprintf("Reading fat:%d %x\n", fat_sector, table_value);

    return table_value;
}

int read_inode_file(struct file *rfile, void *buf, uint32_t count)
{
    uint32_t cluster = rfile->i_node.i_ino;
    uint32_t first_fat_sector = rfile->dev->finfo.fat->first_fat_sector;
    uint32_t first_data_sector = rfile->dev->finfo.fat->first_data_sector;
    uint8_t cluster_dest[FAT_CLUSTER_SIZE];
    uint8_t *buffer = (uint8_t *)buf;
    uint32_t bytes_read = 0;
    int j = 0;
    int i = 0;
    uint64_t total_read = 0;
    while (cluster < FAT_END_OF_CHAIN && bytes_read < count)
    {
        read_cluster((cluster - 2) * 8 + first_data_sector, cluster_dest);
        cluster = read_fat_ptr(cluster, first_fat_sector);
        j = 0;
        while (j < FAT_CLUSTER_SIZE && bytes_read < rfile->i_node.file_size)
        {
            if (total_read >= rfile->pos)
            {
                bytes_read++;
                buffer[i] = cluster_dest[j];
                i++;
                j++;
            }
            total_read++;
        }
    }
    rfile->pos += bytes_read;
    return bytes_read;
}

static void read_file(uint32_t cluster, uint32_t first_fat_sector, uint32_t first_data_sector)
{
    uint8_t *cluster_dest = kmalloc(FAT_CLUSTER_SIZE + 1);
    uint32_t clust = cluster;
    cluster_dest[FAT_CLUSTER_SIZE] = '\0';
    while (clust < FAT_END_OF_CHAIN)
    {
        read_cluster(((clust - 2) * 8 + first_data_sector), cluster_dest);
        clust = read_fat_ptr(clust, first_fat_sector);
        // kprintf((char *)cluster_dest);
    }
    kfree(cluster_dest);
}

static void fat_read_lfilename(char longfname[], uint8_t *dir_ptr)
{
    int j = 0;
    int i = 0;
    // Calculate where in char array this long file name goes
    j = (FAT_LFNAME_RECORD_SIZE * ((dir_ptr[0] & FAT_MAX_LFNAME_RECORDS) - 1));
    // kprint_hex("dir_ptr[0] ",dir_ptr[0]);
    for (i = 1; i < 11; i += 2)
    {
        longfname[j] = dir_ptr[i];
        j += 1;
    }
    for (i = 14; i < 26; i += 2)
    {
        longfname[j] = dir_ptr[i];
        j += 1;
    }
    for (i = 28; i < 32; i += 2)
    {
        longfname[j] = dir_ptr[i];
        j += 1;
    }
}

static struct inode_list *fat_read_std_fmt(struct inode_list *tail, struct dnode *dir,
                                           struct vfs_device *dev, uint8_t *dir_ptr,
                                           int using_lfname, char *longfname)
{
    std_fat_8_3_fmt *file;
    struct inode_list *ilist;
    struct inode_list *prev_ilist = tail;
    struct inode *cur_inode;

    file = (std_fat_8_3_fmt *)dir_ptr;
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
    if (using_lfname)
    {
        kstrncpy(cur_inode->i_name, (const char *)longfname, 0xff);
    }
    else
    {
        char *ptr = (char *)file->fname;
        int i = 0;
        // max 8 chars
        while (*ptr != ' ' && i < 8)
        {
            ptr++;
            i++;
        }
        *ptr = '\0';
        kstrncpy(cur_inode->i_name, (const char *)file->fname, 8);
        // copy over empty string past the 8 chars
        kstrncpy(cur_inode->i_name + 8, "", 8);
        // kprintf("blah 123 %s\n",file->fname);
    }
    cur_inode->i_ino = file->high_cluster << 16 | file->low_cluster;
    if (cur_inode->i_ino == 0)
    {
        cur_inode->i_ino = cur_inode->dev->finfo.fat->root_cluster;
    }
    if ((file->attribute & FAT_DIR) == FAT_DIR)
    {
        cur_inode->i_type = I_DIR;
        cur_inode->file_size = file->file_size;
    }
    else if ((file->attribute & FAT_FILE) == FAT_FILE)
    {
        cur_inode->i_type = I_FILE;
        cur_inode->file_size = file->file_size;
    }
    return prev_ilist;
}

static void prepare_new_dir(struct inode *parent, uint32_t new_cluster)
{
    uint8_t *cluster = kmalloc(FAT_CLUSTER_SIZE);
    uint8_t *dir_ptr = cluster;
    struct std_fat_8_3_fmt *fmt;
    uint32_t sector = 0;
    sector = clust2sec(new_cluster, parent->dev->finfo.fat);

    read_cluster(sector, cluster);
    memzero8(cluster, FAT_CLUSTER_SIZE);
    fmt = (struct std_fat_8_3_fmt *)dir_ptr;
    fmt->attribute = FAT_DIR;
    fmt->file_size = 0;
    fmt->low_cluster = 0xffff & new_cluster;
    fmt->high_cluster = (0x0fff0000 & new_cluster) >> 16;
    kstrcpy((char *)fmt->fname, ".");

    dir_ptr += FAT_DIR_RECORD_SIZE;
    fmt = (struct std_fat_8_3_fmt *)dir_ptr;
    fmt->attribute = FAT_DIR;
    fmt->file_size = 0;
    fmt->low_cluster = 0xffff & parent->i_ino;
    fmt->high_cluster = (0x0fff0000 & parent->i_ino) >> 16;
    kstrcpy((char *)fmt->fname, "..");

    write_cluster(sector, cluster);
    kfree(cluster);
}
static void write_directory(struct inode *parent, char *name)
{
    uint8_t cluster[FAT_CLUSTER_SIZE];
    uint8_t *dir_ptr = cluster;
    int k = 0;
    uint32_t clust = parent->i_ino;
    uint32_t sector = 0;
    uint32_t new_cluster = 0;
    uint32_t prev_clust = FAT_END_OF_CHAIN;
    struct std_fat_8_3_fmt *fmt;
    read_cluster(clust2sec(clust, parent->dev->finfo.fat), cluster);
    kprintf("REading sector %x\n", clust2sec(clust, parent->dev->finfo.fat));
    do
    {
        while (k < 0x80)
        {
            kprintf("wattr %x %x cluster %x\n", *dir_ptr, dir_ptr[FAT_ATTRIBUTE], clust);

            if (*dir_ptr == 0xe5 || *dir_ptr == 0)
            {
                new_cluster = find_free_cluster(parent->dev->finfo.fat);
                fmt = (struct std_fat_8_3_fmt *)dir_ptr;

                fmt->attribute = FAT_DIR;
                fmt->file_size = 0;
                fmt->low_cluster = 0xffff & new_cluster;
                fmt->high_cluster = (0x0fff0000 & new_cluster) >> 16;
                kstrcpy((char *)fmt->fname, name);

                sector = clust2sec(clust, parent->dev->finfo.fat);
                write_cluster(sector, cluster);
                kprintf("Writing sector %x %x\n", sector, *dir_ptr);

                prepare_new_dir(parent, new_cluster);
                kprintf("cluster:%x new_cluster:%x  parent_cluster:%x\n", clust, new_cluster, parent->i_ino);
                return;
            }
            dir_ptr += FAT_DIR_RECORD_SIZE;
            k += 1;
        }
        prev_clust = clust;
        clust = read_fat_ptr(clust, parent->dev->finfo.fat->first_fat_sector);

        k = 0;
        kprintf("cluster:%x prev_cluster:%x \n", clust, prev_clust);

        if (clust >= FAT_END_OF_CHAIN)
        {

            // kprintf("NEw cluster chanin %d \n",prev_clust);
            clust = add_new_link_to_chain(prev_clust, parent->dev->finfo.fat);
            read_cluster(clust2sec(clust, parent->dev->finfo.fat), cluster);
            memzero8(cluster, FAT_CLUSTER_SIZE);
            // kprintf("DONE\n");
        }
        dir_ptr = cluster;

    } while (clust < FAT_END_OF_CHAIN);
    panic("Should never get here");
}

static void read_directory(struct dnode *dir, struct vfs_device *dev)
{
    uint8_t cluster[FAT_CLUSTER_SIZE];
    uint8_t *dir_ptr = cluster;
    int k = 0;
    int using_lfname = 0;
    uint32_t clust = dir->root_inode->i_ino;

    char longfname[FAT_MAX_FNAME];
    struct inode_list *tail = NULL;
    dir->head = NULL;
    int lbytes_written = 0;
    read_cluster(clust2sec(clust, dev->finfo.fat), cluster);
    do
    {
        kprintf("Reading clust %d\n", clust);
        while (*dir_ptr != 0 && k < 0x80)
        {
            kprintf("attr %x %x\n", *dir_ptr, dir_ptr[FAT_ATTRIBUTE]);
            // IF this is a long file name entry handle it
            if (*dir_ptr != 0xe5 && dir_ptr[FAT_ATTRIBUTE] == FAT_LONG_FILENAME)
            {
                using_lfname = 1;
                fat_read_lfilename(longfname, dir_ptr);
                // keep track of how many bytes have been written to arary
                lbytes_written += FAT_LFNAME_RECORD_SIZE;
            }
            else if (*dir_ptr == 0xe5)
            {
                using_lfname = 0;
                lbytes_written = 0;
            }
            else
            {
                longfname[lbytes_written] = '\0';
                // kprintf("test kyle 123\n");
                tail = fat_read_std_fmt(tail, dir, dev, dir_ptr, using_lfname, longfname);
                using_lfname = 0;
                lbytes_written = 0;
            }
            dir_ptr += FAT_DIR_RECORD_SIZE;
            k += 1;
        }
        clust = read_fat_ptr(clust, dev->finfo.fat->first_fat_sector);
        //using_lfname = 0;
        //lbytes_written = 0;
        k = 0;
        read_cluster(clust2sec(clust, dev->finfo.fat), cluster);
        dir_ptr = cluster;
    } while (clust < FAT_END_OF_CHAIN);
}
