#include <sched/exec.h>
#include <include/types.h>
#include <fs/vfs.h>
#include <fs/elf.h>
#include <output/output.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <mm/pmem.h>
#include <sched/sched.h>
//Add elf file to runqueue given an inode. Will return false if something bad happened. 
bool exec_from_inode(struct inode *ifile)
{
    int bytes = 0;
    int i;
    struct file *rfile = vfs_open_file(ifile);
    struct elfhdr hdr;
    struct proghdr phdr;
    struct p_memblock *head = NULL;
    struct p_memblock *track = NULL;
    bytes = vfs_read_file(rfile, &hdr, sizeof(struct elfhdr));
    bool retval = true;
    uint32_t size;
    void *block;
    struct pg_tbl *new_pg_tbl = NULL;
    uint64_t page_ops;


    if (hdr.magic == ELF_MAGIC)
    {
        kprintf("Elf ph offset: 0x%x\n", hdr.phoff);
        kprintf("Elf ph count: 0x%x\n", hdr.phnum);
        kprintf("Elf ph size: 0x%x\n", hdr.phentsize);
        kprintf("Elf entry offset: 0x%x\n", hdr.entry);
        for (i = 0; i < hdr.phnum; i++)
        {
            bytes = vfs_read_file_offset(rfile, &phdr, sizeof(struct proghdr), 
                                         hdr.phoff + (sizeof(struct proghdr) * i));

            if (phdr.type != ELF_PROG_LOAD)
                continue;
            kprintf("-- %d --\n", i);
            kprintf("  Elf phdr type: 0x%x\n", phdr.type);
            kprintf("  Elf phdr flags: 0x%x\n", phdr.flags);
            kprintf("  Elf phdr vaddr: 0x%x\n", phdr.vaddr);
            kprintf("  Elf phdr paddr: 0x%x\n", phdr.paddr);
            kprintf("  Elf phdr off: 0x%x\n", phdr.off);
            kprintf("  Elf phdr memsz: 0x%x\n", phdr.memsz);
            kprintf("  Elf phdr align: 0x%x\n", phdr.align);
            if(new_pg_tbl == NULL) {
                new_pg_tbl = (struct pg_tbl *)kmalloc(sizeof(struct pg_tbl));
                new_pg_tbl->pml4 = (uint64_t *)KERN_PHYS_TO_PVIRT(pmem_alloc_zero_page());
            }
            size = phdr.memsz/PAGE_SIZE;
            if (phdr.memsz % PAGE_SIZE != 0)
                size++;

            //track pages allocated for program image in stack linked list
            block = pmem_alloc_block(size);
            track = kmalloc(sizeof(struct p_memblock));

            track->block = block;
            track->count = size;
            track->next = NULL;

            if(head == NULL) {
                head = track;
            } else {
                track->next = head;
                head = track;
            }
            page_ops = USER_PAGE;
            kprintf("block: %x\n",block);
            bytes = vfs_read_file_offset(rfile, (void *)KERN_PHYS_TO_PVIRT(block),phdr.memsz,phdr.off);
            kprintf("%x\n", KERN_PHYS_TO_PVIRT(block));
            kprintf("%x\n", *(uint64_t *)KERN_PHYS_TO_PVIRT(block));
            kprintf("mapping %x\n",size);

            //if write is not set for this program section map section as RO
            if ((phdr.flags  & ELF_PROG_FLAG_WRITE) == 0) {
                kprintf("Setting phdr.vaddr:%x %d  RO\n",phdr.vaddr,i);
                page_ops = USER_PAGE_RO;
            }
            paging_map_user_range(new_pg_tbl,(uint64_t) block,phdr.vaddr,size,page_ops);
            kprintf("  Read in %d\n", bytes);

        }
        if (new_pg_tbl != NULL)
            user_process_add_exec(hdr.entry,ifile->i_name,new_pg_tbl,head);
    }
    else
    {
        kprintf("Not an ELF executable!\n");
        retval = false;
    }
    vfs_close_file(rfile);

    return retval;
}
