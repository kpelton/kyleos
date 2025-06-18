#include <fs/fat.h>
#include <block/ata.h>
#include <fs/vfs.h>
#include <output/output.h>
#include <mm/mm.h>
#include <include/types.h>

static void read_directory(struct dnode *dir, struct vfs_device *dev);
static void read_file(uint32_t cluster, uint32_t first_fat_sector, uint32_t first_data_sector, uint32_t sectors_per_cluster);
static inline uint32_t clust2sec(uint32_t cluster, struct fatFS *fs);
static uint32_t read_fat_ptr(uint32_t cluster_num, uint32_t first_fat_sector);
static void write_directory(struct inode *parent, char *name);

struct dnode *fat_read_root_dir(struct vfs_device *dev);
struct dnode *read_inode_dir(struct inode *i_node);
void cat_inode_file(struct inode *i_node);
int fat_create_dir(struct inode *parent, char *name);
int read_inode_file(struct file *rfile, void *buf, uint32_t count);
int fat_stat_file (struct file * rfile,struct stat *st);
static int device_num;
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
#define FAT_MAX_STD_FNAME 8
#define FAT_MAX_LFNAME_RECORDS 0x3f
#define FAT_CLUSTER_SIZE 4096
#define FAT_LFNAME_LAST_ENTRY 0x40
#define FAT_MAX_STD_NAME 8

//#define DEBUG


static void read_fat_to_mem(struct fatFS *fs)
{
    kprintf("reading in 0x%x sectors\n", fs->fat_size);
    fs->fat_ptr = kmalloc(ATA_SECTOR_SIZE * fs->fat_size);
    uint32_t *FAT_table = kmalloc(ATA_SECTOR_SIZE / sizeof(uint32_t));
    int k = 0;

    for (uint32_t i = 0; i < fs->fat_size; i++)
    {
        read_sec(fs->first_fat_sector + i, FAT_table);
        for (uint32_t j = 0; j < ATA_SECTOR_SIZE / sizeof(uint32_t); j++)
        {
            fs->fat_ptr[k] = FAT_table[j] & 0x0fffffff;
            k++;
        }
    }
    fs->cluster_count = k;
    kfree(FAT_table);
}

int fat_init(struct mbr_info mbr_entry)
{
    char bs_buf[ATA_SECTOR_SIZE];
    struct fatFS *fs;
    struct vfs_device vfs_dev;
    struct vfs_ops *vfs_ops;

    fs = kmalloc(sizeof(struct fatFS));
    vfs_ops = kmalloc(sizeof(struct vfs_ops));
    kprintf("FAT driver init %x\n",vfs_ops);
    // Read sector and fill out fs info data st
    read_sec(mbr_entry.fs_start, bs_buf);
    fs->fat_boot = *(struct fat_BS *)bs_buf;
    fs->fat_boot_ext_32 = *(struct fat_extBS_32 *)fs->fat_boot.extended_section;
    fs->fat_size = fs->fat_boot_ext_32.table_size_32;
    kprintf("Root cluster %x\n", fs->fat_boot_ext_32.root_cluster);
    kprintf("Sectors per cluster %x\n", fs->fat_boot.sectors_per_cluster);
    fs->root_cluster = fs->fat_boot_ext_32.root_cluster;
    fs->first_data_sector = mbr_entry.fs_start + fs->fat_boot.reserved_sector_count + (fs->fat_boot.table_count * fs->fat_size);
    fs->first_fat_sector = mbr_entry.fs_start + fs->fat_boot.reserved_sector_count;
    fs->mbr_info = mbr_entry;
    kprintf("fat Size %d\n", fs->fat_size);
    read_fat_to_mem(fs);

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
    vfs_ops->stat_file = &fat_stat_file;
    vfs_dev.ops = vfs_ops;
    vfs_dev.finfo.fat = fs;
    vfs_dev.rootfs = true;
    kstrcpy(vfs_dev.mountpoint,"/");
    kstrcpy(vfs_dev.mountpoint_root,"/");
    kstrcpy(vfs_dev.mountpoint_parent,"");
    device_num = vfs_register_device(vfs_dev);
    return 0;
}

int fat_stat_file (struct file * rfile,struct stat *st) {
	//TODO: this is incorrect because the inodes are not cached
	st->st_ino = rfile->i_node.i_ino;
	st->st_dev = device_num;
	st->st_mode =  rfile->i_node.i_type;
	st->st_size = rfile->i_node.file_size;
	return 0;
}

static void write_cluster(uint32_t sector_start, uint32_t sectors_per_cluster, uint8_t *data)
{
    uint32_t i;
    for (i = 0; i < sectors_per_cluster; i++)
        write_sec(sector_start + i, data + (ATA_SECTOR_SIZE * i));
}

static void write_fat_ptr(uint32_t cluster_num, uint32_t new_value, uint32_t first_fat_sector)
{
    // from osdev
    uint8_t FAT_table[ATA_SECTOR_SIZE];
    uint32_t fat_offset = cluster_num * 4;
    uint32_t fat_sector = first_fat_sector + (fat_offset / ATA_SECTOR_SIZE);
    uint32_t ent_offset = fat_offset % ATA_SECTOR_SIZE;

    read_sec(fat_sector, FAT_table);

    FAT_table[ent_offset + 3] = (new_value >> 24) & 0x0f;
    FAT_table[ent_offset + 2] = (new_value >> 16) & 0xff;
    FAT_table[ent_offset + 1] = (new_value >> 8) & 0xff;
    FAT_table[ent_offset] = new_value & 0xff;
    write_sec(fat_sector, FAT_table);
}

static uint32_t find_free_cluster(struct fatFS *fs)
{
    uint32_t i = 0;
    for (i = 0; i < fs->cluster_count; i++)
        if (fs->fat_ptr[i] == 0)
            return i;

    kprintf("%i \n", i);
    panic("Out of space");
    return 0;
}

static uint32_t add_new_link_to_chain(uint32_t cluster_num, struct fatFS *fs)
{
    uint32_t new_cluster = find_free_cluster(fs);
    fs->fat_ptr[cluster_num] = new_cluster;
    fs->fat_ptr[new_cluster] = FAT_END_OF_CHAIN;
    write_fat_ptr(cluster_num, new_cluster, fs->first_fat_sector);
    write_fat_ptr(new_cluster, FAT_END_OF_CHAIN, fs->first_fat_sector);

    return new_cluster;
}

int fat_create_dir(struct inode *parent, char *name)
{
    char truncated_name[FAT_MAX_FNAME + 1];
    kstrncpy(truncated_name, name, FAT_MAX_FNAME + 1);
    write_directory(parent, truncated_name);
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
    //kprintf("read_root_dir\n");
    dir = kmalloc(sizeof(struct dnode));
    dir->root_inode = kmalloc(sizeof(struct inode));
    dir->root_inode->i_ino = dev->finfo.fat->root_cluster;
    dir->root_inode->dev = dev;
    dir->root_inode->i_type = I_DIR;
    kstrncpy(dir->root_inode->i_name, "/", 2);
    dir->parent = NULL;
    dir->head = kmalloc(sizeof(struct inode_list));
    dir->head->current = kmalloc(sizeof(struct inode));
    dir->head->current->file_size = 0;
    kstrcpy(dir->head->current->i_name,"..");
    //Cross mount point since we are on mount 
    // Assumes we are not /
    dir->head->current->i_ino = dev->finfo.fat->root_cluster;
    dir->head->current->i_type = I_DIR;
    dir->head->current->dev = dev;
    dir->head->next = kmalloc(sizeof(struct inode_list));
    
    dir->head->next->current = kmalloc(sizeof(struct inode));
    kstrcpy(dir->head->next->current->i_name,".");
    dir->head->next->current->i_ino = dir->root_inode->i_ino;
    dir->head->next->current->i_type = I_DIR;
    dir->head->next->current->dev = dir->root_inode->dev;
    dir->head->next->current->file_size = 0;
    dir->head->next->next=NULL;
    read_directory(dir, dev);

    return dir;
}

struct dnode *read_inode_dir(struct inode *i_node)
{
    struct dnode *dir;
    
    // Standard call for root directory to handle . and ..
    
    if (i_node->i_ino == i_node->dev->finfo.fat->root_cluster) {
        return fat_read_root_dir(i_node->dev);
    }

    dir = kmalloc(sizeof(struct dnode));
    dir->root_inode = kmalloc(sizeof(struct inode));
    dir->root_inode->i_ino = i_node->i_ino;
    dir->root_inode->dev = i_node->dev;
    dir->root_inode->i_type = I_DIR;
    dir->head = NULL; 

    kstrncpy(dir->root_inode->i_name, i_node->i_name, FAT_MAX_FNAME);
    kstrncpy(dir->i_name, i_node->i_name, FAT_MAX_FNAME);
    read_directory(dir, i_node->dev);
    return dir;
}

void cat_inode_file(struct inode *i_node)
{
    read_file(i_node->i_ino, i_node->dev->finfo.fat->first_fat_sector,
              i_node->dev->finfo.fat->first_data_sector,
              i_node->dev->finfo.fat->fat_boot.sectors_per_cluster);
}
static void read_cluster(uint32_t sector_start, uint32_t sectors_per_cluster, uint8_t *dest)
{
    uint32_t i;
    for (i = 0; i < sectors_per_cluster; i++)
        read_sec(sector_start + i, dest + (ATA_SECTOR_SIZE * i));
}

static uint32_t read_fat_ptr(uint32_t cluster_num, uint32_t first_fat_sector)
{
    // from osdev
    uint8_t FAT_table[ATA_SECTOR_SIZE];
    uint32_t fat_offset = cluster_num * 4;
    uint32_t fat_sector = first_fat_sector + (fat_offset / ATA_SECTOR_SIZE);
    uint32_t ent_offset = fat_offset % ATA_SECTOR_SIZE;

    read_sec(fat_sector, FAT_table);

    uint32_t table_value = (FAT_table[ent_offset + 3] << 24 |
                            FAT_table[ent_offset + 2] << 16 |
                            FAT_table[ent_offset + 1] << 8 |
                            FAT_table[ent_offset]) &
                           0x0FFFFFFF;

    kprintf("Reading fat:%x %x\n", cluster_num, table_value);

    return table_value;
}

int read_inode_file(struct file *rfile, void *buf, uint32_t count)
{
    uint32_t cluster = rfile->i_node.i_ino;
    uint32_t first_data_sector = rfile->dev->finfo.fat->first_data_sector;
    uint32_t sectors_per_cluster = rfile->dev->finfo.fat->fat_boot.sectors_per_cluster;
    uint8_t cluster_dest[sectors_per_cluster * ATA_SECTOR_SIZE];
    uint8_t *buffer = (uint8_t *)buf;
    uint32_t bytes_read = 0;
    uint32_t j = 0;
    uint32_t i = 0;
    uint64_t total_read = 0;

    if(rfile->pos >= rfile->i_node.file_size)
        return 0;

    memzero8((uint8_t *) buf,count);
    // Need to cache which cluster we are on for next call
    while (cluster < FAT_END_OF_CHAIN && bytes_read < count)
    {
        j = 0;
        //optimization to skip reading data cluster if we are below file pos
        if (total_read / (sectors_per_cluster * ATA_SECTOR_SIZE) < rfile->pos / (sectors_per_cluster * ATA_SECTOR_SIZE))
        {
            total_read += sectors_per_cluster * ATA_SECTOR_SIZE;
            //kprintf("%d\n",total_read);
            //read next cluster
            cluster = rfile->dev->finfo.fat->fat_ptr[cluster];

            continue;
        }
        else
        {
            read_cluster((cluster - 2) * sectors_per_cluster + first_data_sector, sectors_per_cluster, cluster_dest);
            cluster = rfile->dev->finfo.fat->fat_ptr[cluster];
        }

        while (j < sectors_per_cluster * ATA_SECTOR_SIZE && rfile->pos+bytes_read < rfile->i_node.file_size && bytes_read < count)
        {
            if (total_read >= rfile->pos)
            {
                bytes_read++;
                buffer[i] = cluster_dest[j];
                // kprintf("%d, %d %d\n",i,j, rfile->pos);
                i++;
            }
            j++;
            total_read++;
        }
    }
    //kfree(cluster_dest);
    //kprintf("read inode total bytes read %d file->size %d file->pos %d\n",bytes_read,rfile->i_node.file_size,rfile->pos);
    return bytes_read;
}

static void read_file(uint32_t cluster, uint32_t first_fat_sector, uint32_t first_data_sector, uint32_t sectors_per_cluster)
{
    uint8_t *cluster_dest = kmalloc(sectors_per_cluster * ATA_SECTOR_SIZE + 1);
    uint32_t clust = cluster;
    cluster_dest[sectors_per_cluster * ATA_SECTOR_SIZE] = '\0';
    while (clust < FAT_END_OF_CHAIN)
    {
        read_cluster(((clust - 2) * sectors_per_cluster + first_data_sector), sectors_per_cluster, cluster_dest);
        clust = read_fat_ptr(clust, first_fat_sector);
        // kprintf((char *)cluster_dest);
    }
    kfree(cluster_dest);
}

static int fat_read_lfname_entry(char *dest, char *src, uint64_t len, uint64_t *dest_offset)
{
    uint64_t j = 0;
    int retval = 0;
    for (j = 0; (*dest_offset < FAT_MAX_FNAME) &&j < len; (*dest_offset)++, j += 2) {
        dest[*dest_offset] = src[j];
        retval++;
    }
    return retval;
}

static int fat_read_lfilename(char longfname[], uint8_t *dir_ptr)
{
    uint64_t i = 0;
    // Calculate where in char array this long file name goes
    struct fat_long_fmt *fptr = (struct fat_long_fmt *)dir_ptr;

    i = (FAT_LFNAME_RECORD_SIZE * ((fptr->order & FAT_MAX_LFNAME_RECORDS) - 1));
    int retval = 0;
    retval += fat_read_lfname_entry(longfname, fptr->first_entry, sizeof(fptr->first_entry), &i);
    retval += fat_read_lfname_entry(longfname, fptr->second_entry, sizeof(fptr->second_entry), &i);
    retval += fat_read_lfname_entry(longfname, fptr->third_entry, sizeof(fptr->third_entry), &i);
    return retval;
}

static int fat_write_lfname_entry(char *dest, char *src, uint64_t len, uint32_t *bytes_read, uint64_t *src_offset)
{
    uint64_t i = 0;
    for (i = 0; i < len; i += 2, (*src_offset)++)
    {
        if (src[*src_offset] == '\0')
            return -1;
        dest[i] = src[*src_offset];
        (*bytes_read)++;
    }
    return 1;
}

static uint32_t fat_write_lfilename(char longfname[], uint8_t *dir_ptr, uint32_t fat_lfname_record_num)
{
    uint64_t j = 0;
    uint32_t bytes_read = 0;
    memzero8(dir_ptr, FAT_DIR_RECORD_SIZE);
    struct fat_long_fmt *fptr = (struct fat_long_fmt *)dir_ptr;

    fptr->order = fat_lfname_record_num & FAT_MAX_LFNAME_RECORDS;
    fptr->attribute = FAT_LONG_FILENAME;

    if (!fat_write_lfname_entry(fptr->first_entry, longfname, sizeof(fptr->first_entry), &bytes_read, &j) ||
        !fat_write_lfname_entry(fptr->second_entry, longfname, sizeof(fptr->second_entry), &bytes_read, &j) ||
        !fat_write_lfname_entry(fptr->third_entry, longfname, sizeof(fptr->third_entry), &bytes_read, &j))
    {
        // if we exit early mark it as the final record
        fptr->order |= FAT_LFNAME_LAST_ENTRY;
    }

    return bytes_read;
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
        kstrcpy(cur_inode->i_name, (const char *)longfname);
    }
    else
    {
        char *ptr = (char *)file->fname;
        int i = 0;
        // max 8 chars
        while (*ptr != ' ' && i < FAT_MAX_STD_NAME)
        {
            ptr++;
            i++;
        }
        *ptr = '\0';
        kstrncpy(cur_inode->i_name, (const char *)file->fname, FAT_MAX_STD_NAME);
        // copy over empty string past the 8 chars
        kstrncpy(cur_inode->i_name + FAT_MAX_STD_NAME, "", 1);
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
    uint32_t sectors_per_cluster = parent->dev->finfo.fat->fat_boot.sectors_per_cluster;
    uint8_t *cluster = kmalloc(sectors_per_cluster * ATA_SECTOR_SIZE);

    uint8_t *dir_ptr = cluster;
    struct std_fat_8_3_fmt *fmt;
    uint32_t sector = 0;
    sector = clust2sec(new_cluster, parent->dev->finfo.fat);

    read_cluster(sector, sectors_per_cluster, cluster);
    memzero8(cluster, FAT_CLUSTER_SIZE);

    fmt = (struct std_fat_8_3_fmt *)dir_ptr;
    fmt->attribute = FAT_DIR;
    fmt->file_size = 0;
    fmt->low_cluster = 0xffff & parent->i_ino;
    fmt->high_cluster = (0x0fff0000 & parent->i_ino) >> 16;
    kstrcpy((char *)fmt->fname, "..");
    dir_ptr += FAT_DIR_RECORD_SIZE;

    fmt = (struct std_fat_8_3_fmt *)dir_ptr;
    fmt->attribute = FAT_DIR;
    fmt->file_size = 0;
    fmt->low_cluster = 0xffff & new_cluster;
    fmt->high_cluster = (0x0fff0000 & new_cluster) >> 16;
    kstrcpy((char *)fmt->fname, ".");

    write_cluster(sector, sectors_per_cluster, cluster);
    kfree(cluster);
}

static void write_longfname(struct inode *parent, char *name)
{
    uint32_t sectors_per_cluster = parent->dev->finfo.fat->fat_boot.sectors_per_cluster;
    uint8_t *cluster = kmalloc(sectors_per_cluster * ATA_SECTOR_SIZE);
    uint8_t *dir_ptr = cluster;
    uint32_t k = 0;
    uint32_t clust = parent->i_ino;
    uint32_t sector = 0;
    uint32_t prev_clust = FAT_END_OF_CHAIN;
    uint32_t name_len = kstrlen(name);
    uint32_t bytes_read = 0;
    uint32_t done_bytes = 0;
    uint32_t lfnamesec = 1;
    uint32_t max_dir_records = (ATA_SECTOR_SIZE * sectors_per_cluster) / FAT_DIR_RECORD_SIZE;

    read_cluster(clust2sec(clust, parent->dev->finfo.fat), sectors_per_cluster, cluster);

    do
    {
        if (prev_clust == 0)
            panic("test");

        while (k < max_dir_records)
        {
            // kprintf("lattr %x %x %x\n", *dir_ptr, dir_ptr[FAT_ATTRIBUTE],clust);
            if (*dir_ptr == FAT_UNUSED_DIR || *dir_ptr == 0)
            {
                bytes_read = fat_write_lfilename(name, dir_ptr, lfnamesec);
                done_bytes += bytes_read;
                name += bytes_read;
                sector = clust2sec(clust, parent->dev->finfo.fat);
                write_cluster(sector, parent->dev->finfo.fat->fat_boot.sectors_per_cluster, cluster);
                // kprintf("Done bytes %d Total:%d\n", done_bytes, name_len);
                if (done_bytes == name_len)
                {
                    // kprintf("All done\n");
                    kfree(cluster);
                    return;
                }
                else
                {
                    lfnamesec++;
                }
            }
            dir_ptr += FAT_DIR_RECORD_SIZE;
            k += 1;
        }

        prev_clust = clust;
        clust = parent->dev->finfo.fat->fat_ptr[clust];

        k = 0;
        // kprintf("cluster:%x prev_cluster:%x \n", clust, prev_clust);

        if (clust >= FAT_END_OF_CHAIN)
        {

            // kprintf("NEw cluster chanin %d \n",prev_clust);
            clust = add_new_link_to_chain(prev_clust, parent->dev->finfo.fat);
            // kprintf("NEw cluster chanin %d %d \n",prev_clust,clust);
            memzero8(cluster, sectors_per_cluster * ATA_SECTOR_SIZE);
            // kprintf("DONE\n");
        }
        dir_ptr = cluster;
    } while (clust < FAT_END_OF_CHAIN);
    panic("Should never get here");
}

static void write_directory(struct inode *parent, char *name)
{
    uint32_t sectors_per_cluster = parent->dev->finfo.fat->fat_boot.sectors_per_cluster;
    uint8_t *cluster = kmalloc(sectors_per_cluster * ATA_SECTOR_SIZE);
    uint8_t *dir_ptr = cluster;
    uint32_t k = 0;
    uint32_t clust = parent->i_ino;
    uint32_t sector = 0;
    uint32_t new_cluster = 0;
    uint32_t prev_clust = FAT_END_OF_CHAIN;
    struct std_fat_8_3_fmt *fmt;
    uint32_t max_dir_records = (ATA_SECTOR_SIZE * sectors_per_cluster) / FAT_DIR_RECORD_SIZE;

    write_longfname(parent, name);
    // asm("kyle: jmp kyle");
    read_cluster(clust2sec(clust, parent->dev->finfo.fat), parent->dev->finfo.fat->fat_boot.sectors_per_cluster, cluster);
    // kprintf("REading sector %x\n", clust2sec(clust, parent->dev->finfo.fat));

    do
    {
        while (k < max_dir_records)
        {
            // kprintf("wattr %x %x cluster %x\n", *dir_ptr, dir_ptr[FAT_ATTRIBUTE], clust);

            if (*dir_ptr == FAT_UNUSED_DIR || *dir_ptr == 0)
            {
                // setup long filename
                new_cluster = find_free_cluster(parent->dev->finfo.fat);
                parent->dev->finfo.fat->fat_ptr[new_cluster] = FAT_END_OF_CHAIN;
                write_fat_ptr(new_cluster, FAT_END_OF_CHAIN, parent->dev->finfo.fat->first_fat_sector);
                fmt = (struct std_fat_8_3_fmt *)dir_ptr;
                // rework how name is copied over and use long filename if longer than 8 chars
                fmt->attribute = FAT_DIR;
                fmt->file_size = 0;
                fmt->low_cluster = 0xffff & new_cluster;
                fmt->high_cluster = (0x0fff0000 & new_cluster) >> 16;
                kstrncpy((char *)fmt->fname, name, FAT_MAX_STD_FNAME);

                sector = clust2sec(clust, parent->dev->finfo.fat);
                write_cluster(sector, parent->dev->finfo.fat->fat_boot.sectors_per_cluster, cluster);
                // kprintf("Writing sector %x %x\n", sector, *dir_ptr);

                prepare_new_dir(parent, new_cluster);
                /// kprintf("cluster:%x new_cluster:%x  parent_cluster:%x\n", clust, new_cluster, parent->i_ino);

                kfree(cluster);
                return;
            }
            dir_ptr += FAT_DIR_RECORD_SIZE;
            k += 1;
        }
        prev_clust = clust;
        clust = parent->dev->finfo.fat->fat_ptr[clust];

        k = 0;
        // kprintf("cluster:%x prev_cluster:%x \n", clust, prev_clust);

        if (clust >= FAT_END_OF_CHAIN)
        {

            // kprintf("NEw cluster chanin %d \n",prev_clust);
            clust = add_new_link_to_chain(prev_clust, parent->dev->finfo.fat);
            // kprintf("NEw cluster chanin %d %d \n",prev_clust,clust);
            memzero8(cluster, sectors_per_cluster * ATA_SECTOR_SIZE);
            // kprintf("DONE\n");
        }
        dir_ptr = cluster;

    } while (clust < FAT_END_OF_CHAIN);
    panic("Should never get here");
}

static void read_directory(struct dnode *dir, struct vfs_device *dev)
{
    uint32_t sectors_per_cluster = dir->root_inode->dev->finfo.fat->fat_boot.sectors_per_cluster;
    uint8_t cluster[sectors_per_cluster * ATA_SECTOR_SIZE];
    uint8_t* dir_ptr = cluster;
    uint32_t k = 0;
    int using_lfname = 0;
    uint32_t clust = dir->root_inode->i_ino;
    //+1 for '\0' char
    char longfname[FAT_MAX_FNAME+1];


    struct inode_list *tail = NULL;
    // Handle case of ./.. for root dir two directories
    if (dir->head != NULL) {
        tail = dir->head->next;
    }
    int lbytes_written = 0;

    uint32_t max_dir_records = (ATA_SECTOR_SIZE * sectors_per_cluster) / FAT_DIR_RECORD_SIZE;

    read_cluster(clust2sec(clust, dev->finfo.fat), sectors_per_cluster, cluster);
    do
    {
        // kprintf("Reading clust %d\n", clust);
        while (*dir_ptr != 0 && k < max_dir_records)
        {
            // printf("attr %x %x\n", *dir_ptr, dir_ptr[FAT_ATTRIBUTE]);
            //   IF this is a long file name entry handle it
            if (*dir_ptr != FAT_UNUSED_DIR && dir_ptr[FAT_ATTRIBUTE] == FAT_LONG_FILENAME)
            {
                using_lfname = 1;
                
                // keep track of how many bytes have been written to arary
                lbytes_written += fat_read_lfilename(longfname, dir_ptr);
            }
            else if (*dir_ptr == FAT_UNUSED_DIR)
            {
                using_lfname = 0;
                lbytes_written = 0;
            }
            else
            {
                longfname[lbytes_written] = '\0';
                tail = fat_read_std_fmt(tail, dir, dev, dir_ptr, using_lfname, longfname);
                using_lfname = 0;
                lbytes_written = 0;
            }
            dir_ptr += FAT_DIR_RECORD_SIZE;
            k += 1;
        }
        clust = dev->finfo.fat->fat_ptr[clust];
        // kprintf("kyle: %x %x %x\n", prev_clust, clust,read_fat_ptr(prev_clust,dev->finfo.fat->first_fat_sector));

        // using_lfname = 0;
        // lbytes_written = 0;
        if (clust < FAT_END_OF_CHAIN)
        {
            k = 0;
            read_cluster(clust2sec(clust, dev->finfo.fat), sectors_per_cluster, cluster);
            dir_ptr = cluster;
        }
    } while (clust < FAT_END_OF_CHAIN);
}
