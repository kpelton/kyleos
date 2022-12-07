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
int exec_from_inode(struct inode *ifile,bool replace)
{
            acquire_spinlock(&exec_spinlock);
    //int bytes = 0;
    int i;
    struct file *rfile = vfs_open_file(ifile,O_RDONLY);
    struct elfhdr hdr;
    struct proghdr phdr;
    vfs_read_file(rfile, &hdr, sizeof(struct elfhdr));
    int retval = -1;
    uint32_t size = 0;
    uint64_t vaddr;
    struct vmm_block *block;
    uint32_t offset_delta = 0;
    struct pg_tbl *new_pg_tbl = NULL;
    uint64_t page_ops;
    struct vmm_map *map = NULL;
    char name[VFS_MAX_FNAME];
    bool zero;
    if (hdr.magic == ELF_MAGIC)
    {
#ifdef EXEC_DEBUG
        kprintf("Elf ph offset: 0x%x\n", hdr.phoff);
        kprintf("Elf ph count: 0x%x\n", hdr.phnum);
        kprintf("Elf ph size: 0x%x\n", hdr.phentsize);
        kprintf("Elf entry offset: 0x%x\n", hdr.entry);
#endif
    map = vmm_map_new();
        for (i = 0; i < hdr.phnum; i++)
        {
                //acquire_spinlock(&exec_spinlock);

            vfs_read_file_offset(rfile, &phdr, sizeof(struct proghdr), 
                                         hdr.phoff + (sizeof(struct proghdr) * i));

            if (phdr.type != ELF_PROG_LOAD) {

               // r
                continue;
            }
            offset_delta = 0;
            size = 0;
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

            if(new_pg_tbl == NULL) {
                new_pg_tbl = (struct pg_tbl *)kmalloc(sizeof(struct pg_tbl));
                new_pg_tbl->pml4 = (uint64_t *)KERN_PHYS_TO_PVIRT(pmem_alloc_zero_page());
            }

            if (((phdr.vaddr) & 0x0000000000000fff) != 0UL) {
                vaddr = phdr.vaddr & 0xfffffffffffff000;
                offset_delta += phdr.vaddr &0xfff;
                kprintf("not paged aligned\n");
                size++;
            }else{
                vaddr = phdr.vaddr;
            }
            size += phdr.memsz/PAGE_SIZE;
            if (phdr.memsz % PAGE_SIZE != 0)
                size++;
            
            //track pages allocated for program image in stack linked list
            page_ops = USER_PAGE;
            if ((phdr.flags  & ELF_PROG_FLAG_WRITE) == 0) {
                //kprintf("Setting phdr.vaddr:%x %d  RO\n",phdr.vaddr,i);
                page_ops = USER_PAGE_RO;
            }
            zero = phdr.memsz != phdr.filesz;
            block = vmm_add_new_mapping(map,VMM_TEXT,vaddr,size,page_ops,zero);

#ifdef EXEC_DEBUG
            kprintf("Reading to %x \n",vaddr+offset_delta);
#endif
            vfs_read_file_offset(rfile, (void *)KERN_PHYS_TO_PVIRT((uint8_t*)block->paddr+offset_delta),phdr.filesz,phdr.off);
#ifdef EXEC_DEBUG
            kprintf("%x\n", KERN_PHYS_TO_PVIRT(block));
            kprintf("%x\n", *(uint64_t *)KERN_PHYS_TO_PVIRT(block));
            kprintf("mapping %x\n",size);
#endif

        }
        if (new_pg_tbl != NULL){

            if (replace == false)
                retval = user_process_add_exec(hdr.entry,ifile->i_name,map);
            else{
                struct ktask *t = get_current_process();
                kstrcpy(name,ifile->i_name);
                vfs_free_inode(ifile);
                release_spinlock(&exec_spinlock);
                retval = user_process_replace_exec(t,hdr.entry,name,map);
            }
        }
        release_spinlock(&exec_spinlock);


    }
            
    else
    {
        kprintf("Not an ELF executable!\n");
    }
    vfs_close_file(rfile);

    return retval;
}
