#include <sched/exec.h>
#include <include/types.h>
#include <fs/vfs.h>
#include <fs/elf.h>
#include <output/output.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <mm/pmem.h>
#include <mm/vmm.h>
#include <sched/sched.h>
#include <locks/spinlock.h>

static struct spinlock exec_spinlock;
//#define EXEC_DEBUG_LL
//#define EXEC_DEBUG
void exec_init(){
    init_spinlock(&exec_spinlock);
}
//Add elf file to runqueue given an inode. Will return false if something bad happened. 
int exec_from_inode(struct inode *ifile,bool replace,char **argv)
{
    acquire_spinlock(&exec_spinlock);
    int i;
    struct elfhdr hdr;
    struct proghdr phdr;
    int retval = -1;
    uint64_t page_ops;
    struct vmm_map *map = NULL;
    char name[VFS_MAX_FNAME];
    bool entry_valid = false;
    struct file *rfile = NULL;

    rfile = vfs_open_file(ifile,O_RDONLY);
    if (rfile == NULL ||
        vfs_read_file(rfile, &hdr, sizeof(struct elfhdr)) != sizeof(struct elfhdr) ||
        hdr.magic != ELF_MAGIC || hdr.phentsize != sizeof(struct proghdr) ||
        hdr.phoff > ifile->file_size ||
        hdr.phnum > (ifile->file_size - hdr.phoff) / sizeof(struct proghdr) ||
        hdr.phoff + (uint64_t)hdr.phnum * sizeof(struct proghdr) >
            0x100000000ULL) {
        kprintf("Not a valid ELF executable!\n");
        goto out;
    }
#ifdef EXEC_DEBUG
        kprintf("Elf ph offset: 0x%x\n", hdr.phoff);
        kprintf("Elf ph count: 0x%x\n", hdr.phnum);
        kprintf("Elf ph size: 0x%x\n", hdr.phentsize);
        kprintf("Elf entry offset: 0x%x\n", hdr.entry);
#endif
    map = vmm_map_new();
    if (map == NULL || !vmm_set_backing_file(map, rfile))
        goto fail;
    /* The address space now owns this reference and releases it in vmm_free. */
    rfile = NULL;
        for (i = 0; i < hdr.phnum; i++)
        {
            uint64_t segment_end;
            uint64_t map_start;
            uint64_t map_end;
            uint64_t file_end;
            uint64_t pages;

            if (vfs_read_file_offset(map->backing_file, &phdr,
                                     sizeof(struct proghdr),
                                     hdr.phoff + sizeof(struct proghdr) * i) !=
                sizeof(struct proghdr))
                goto fail;

            if (phdr.type != ELF_PROG_LOAD)
                continue;
#ifdef EXEC_DEBUG
            kprintf("-- %d --\n", i);
            kprintf("  Elf phdr type: 0x%x\n", phdr.type);
            kprintf("  Elf phdr flags: 0x%x\n", phdr.flags);
            kprintf("  Elf phdr vaddr: 0x%x\n", phdr.vaddr);
            kprintf("  Elf phdr paddr: 0x%x\n", phdr.paddr);
            kprintf("  Elf phdr off: 0x%x\n", phdr.off);
            kprintf("  Elf phdr align: 0x%x\n", phdr.align);
            kprintf("  Elf phdr memsz: 0x%x\n", phdr.memsz);
            kprintf("  Elf phdr filesz: 0x%x\n", phdr.filesz);
#endif
            if (phdr.filesz > phdr.memsz || phdr.memsz == 0 ||
                phdr.vaddr + phdr.memsz < phdr.vaddr ||
                phdr.off + phdr.filesz < phdr.off)
                goto fail;
            segment_end = phdr.vaddr + phdr.memsz;
            file_end = phdr.off + phdr.filesz;
            if (file_end > map->backing_file->i_node.file_size ||
                file_end > 0x100000000ULL ||
                phdr.vaddr >= KERN_SPACE_BOUNDRY ||
                segment_end > KERN_SPACE_BOUNDRY)
                goto fail;
            if (phdr.align > 1 &&
                ((phdr.align & (phdr.align - 1)) != 0 ||
                 ((phdr.vaddr - phdr.off) & (phdr.align - 1)) != 0))
                goto fail;

            map_start = phdr.vaddr & ~(PAGE_SIZE - 1);
            if (segment_end > ~0ULL - (PAGE_SIZE - 1))
                goto fail;
            map_end = (segment_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            pages = (map_end - map_start) / PAGE_SIZE;
            
            page_ops = USER_PAGE;
            if ((phdr.flags & ELF_PROG_FLAG_WRITE) == 0)
                page_ops = USER_PAGE_RO;
            if (vmm_reserve_file_mapping(map, VMM_TEXT,
                                         (uint64_t *)map_start, pages,
                                         page_ops, phdr.vaddr, phdr.off,
                                         phdr.filesz) == NULL)
                goto fail;
            if ((phdr.flags & ELF_PROG_FLAG_EXEC) != 0 &&
                hdr.entry >= phdr.vaddr && hdr.entry < segment_end)
                entry_valid = true;
        }
        if (!entry_valid)
            goto fail;
        if (replace == false){
               retval = user_process_add_exec(hdr.entry,ifile->i_name,map,true,argv,NULL,vfs_get_root_inode(),true);
        }else{
                struct ktask *t = get_current_process();
                kstrcpy(name,ifile->i_name);
                vfs_free_inode(ifile);
                release_spinlock(&exec_spinlock);
                retval = user_process_replace_exec(t,hdr.entry,name,map,argv,t->cwd);
                return retval;
        }
        map = NULL;
        goto out;

fail:
    if (map != NULL)
        vmm_free(map);
out:
    if (rfile != NULL)
        vfs_close_file(rfile);
    release_spinlock(&exec_spinlock);
    return retval;
}
