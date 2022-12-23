#include <asm/asm.h>
#include <output/output.h>
#include <output/input.h>
#include <init/tables.h>
#include <block/ata.h>
#include <irq/irq.h>
#include <mm/mm.h>
#include <timer/pit.h>
#include <fs/vfs.h>
#include <sched/sched.h>
#include <timer/rtc.h>
#include <mm/pmem.h>
#include <fs/elf.h>
#include <sched/exec.h>
#include <sched/ps.h>


void test_user_function5();
static const char OS_PROMPT[] = "Kyle OS |";

static char *dir_stack[100][256];
static int top_dir_stack = -1;

static void push_dir_stack(char *dir)
{
    if (top_dir_stack == -1)
    {
        top_dir_stack = 0;
    }
    else if (top_dir_stack == 99)
    {
        kprintf("Dir stack full");
        return;
    }
    else
    {
        top_dir_stack++;
    }
    kstrcpy((char *)dir_stack[top_dir_stack], dir);
}

static char *pop_dir_stack()
{
    char *val;
    if (top_dir_stack == -1)
    {
        kprintf("dirstack empty");
        return 0;
    }
    val = (char *)dir_stack[top_dir_stack];
    top_dir_stack -= 1;
    return val;
}

static void print_dir(struct inode *pwd)
{
    struct inode_list *ptr;
    struct dnode *dptr;

    dptr = vfs_read_inode_dir(pwd);
    if (dptr == 0)
        goto error;
    ptr = dptr->head;
    while (ptr != 0)
    {
        kprintf(ptr->current->i_name);
        if (ptr->current->i_type == I_DIR)
        {
            kprintf(" DIR");
        }
        else
        {
            kprintf(" FILE ");
        }
        kprintf("  size=%d\n", ptr->current->file_size);
        ptr = ptr->next;
    }
    vfs_free_dnode(dptr);
error:
    return;
}

static void mkdir(struct inode *pwd,char *dir_name)
{
    char dir[256] = {0};

    for(int i=0; i<256; i++) {
        if(dir_name[i] == '\n')
            break;
        dir[i] = dir_name[i];
    }
    //kprintf("vfs-create-dir %x\n",pwd->i_ino);
    vfs_create_dir(pwd,dir);
}
struct inode *shell_cd(char cmd[], struct dnode *dptr)
{
    struct inode_list *ptr;
    char *cmdptr = cmd;
    char *nptr = cmd;
    //Find directory
    while (*cmdptr != ' ')
        cmdptr++;
    cmdptr++;
    nptr = cmdptr;
    //Take out newline
    while (*nptr != '\n')
        nptr++;
    *nptr = '\0';

    ptr = dptr->head;
    while (ptr != 0)
    {
        if (ptr->current->i_type == I_DIR)
        {
            if (kstrcmp(cmdptr, ptr->current->i_name) == 0)
                return ptr->current;
        }
        ptr = ptr->next;
    }
    kprintf("bad directoy\n");
    return 0;
}


void print_prompt()
{
    kprintf((char *)OS_PROMPT);
    for (int i = 0; i <= top_dir_stack; i++)
    {
        kprintf((char *)dir_stack[i]);
        if (i == top_dir_stack)
            kprintf(">");
        else if (i > 0)
            kprintf("/");
    }
}

void fs_test()
{
    struct dnode *dptr;
    struct dnode *dptr1;
    dptr = vfs_read_root_dir("/");
    for (;;)
    {
        asm("sti");
        struct dnode *dptr2;

        dptr1 = vfs_read_inode_dir(dptr->head->next->next->next->current);

        print_dir(dptr1->root_inode);
        dptr2 = vfs_read_inode_dir(dptr->head->next->next->next->next->current);
        print_dir(dptr2->root_inode);

        vfs_free_dnode(dptr1);
        vfs_free_dnode(dptr2);

        mm_print_stats();
        ksleepm(10);
        //ksleepm(1000);
    }
    return;
}
void print_time()
{
    struct sys_time current_time = get_time();
    //TODO: figure out why this is showing hex
    kprintf("%d:%d:%d %d/%d/%d\n", current_time.hour, current_time.min,
            current_time.sec, current_time.month, current_time.day, current_time.year);
}


struct inode * read_path(char *path, struct dnode *pwd,enum inode_type type)
{
    int end = 0;
    char *blah = path;
    char buffer[1024];
    struct inode_list *ptr;
    struct dnode *dptr = pwd;
    //if (*blah == '/')
    //    return;

    while (end != -1)
    {
        end = kstrstr(blah, "/");
        kprintf("%d %s\n", end, blah);
        if (end >= 0)
        {
            kstrncpy(buffer, blah, end);
            buffer[end] = '\0';
            blah += end + 1;
        }
        //printf("%s\n",blah);
        ptr = dptr->head;
        while (ptr)
        {
            //kprintf("%s %s\n",ptr->current->i_name,buffer);

            if (kstrcmp(ptr->current->i_name, buffer) == 0 && ptr->current->i_type == I_DIR)
            {
                if (dptr != pwd)
                    vfs_free_dnode(dptr);
                dptr = vfs_read_inode_dir(ptr->current);
                break;
            }
            ptr = ptr->next;
        }
    }

    ptr = dptr->head;
    while (ptr)
    {
        //kprintf("%s\n",ptr->current->i_name);
        if (kstrcmp(ptr->current->i_name, blah) == 0 && ptr->current->i_type == (int)type) {

            vfs_free_dnode(dptr);
            return ptr->current;
        }
        ptr = ptr->next;
    }
    vfs_free_dnode(dptr);
    return NULL;
}

void start_dshell()
{

    char buffer[512];
    char *cptr = NULL;
    int pid;
    //struct dnode *dptr;
    //struct dnode *dptr1;
//    dptr = vfs_read_root_dir("0:/"); 
    //struct dnode *olddptr=dptr;
    //struct inode *pwd = dptr->root_inode;
    //struct inode *oldpwd=pwd;
    push_dir_stack("/");
    print_prompt();
    //kprintf("root %x\n",pwd->i_ino);
    //write_sec(0,buffer2);
    // asm("test: jmp test");
    //debug
    asm("sti");
    /*
for(;;) {

    struct dnode *dptr2;

     dptr1 = vfs_read_inode_dir(dptr->head->next->next->next->current);



     print_dir(dptr1->root_inode);
     dptr2 = vfs_read_inode_dir(dptr->head->next->next->next->next->current);
     print_dir(dptr2->root_inode);

     vfs_free_dnode(dptr1);
     vfs_free_dnode(dptr2);

     mm_print_stats();
     ksleepm(10);
     //ksleepm(1000);
}
    return;
*/
    void kthread_test()
    {
        for (;;)
        {
           // kprintf("test frok kthread!\n");
           // ksleepm(1000);
        }
    }

    //
    while (1)
    {
        read_input(buffer);
        if (buffer[0] == '\0')
        {
            asm("hlt");
            continue;
        }
        if (buffer[0] == 'k' && buffer[1] == 'i' 
                && buffer[2] == 'l' && buffer[3] == 'l'
                && buffer[4] == ' ' && buffer[5] != '\n') {
                cptr = buffer+5;
                while(*cptr != '\n' && *cptr != '\0') {           
                    cptr++;
                }
                *cptr = '\0';
                pid = atoi(buffer+5);
                asm("cli");
                if (sched_process_kill(pid) == false)
                    kprintf("Kill failed\n");
                asm("sti");
        }

        else if (kstrcmp(buffer, "mem\n") == 0)
        {
            mm_print_stats();
            phys_mem_print_usage();
        }
        else if (kstrcmp(buffer, "panic\n") == 0)
        {
            print_regs(0xdeadbeef, 0xdeadbeef);
        }
        else if (kstrcmp(buffer, "fatalpanic\n") == 0)
        {
            panic("Kernel Halted");
        }
        else if (kstrcmp(buffer, "jiffies\n") == 0)
        {
            kprintf("Jiffies: %d\n", read_jiffies());
        }
        else if (kstrcmp(buffer, "sched\n") == 0)
        {
            sched_stats();
        }
        else if (kstrcmp(buffer, "time\n") == 0)
        {
            print_time();
        }
        else if (kstrcmp(buffer, "sleep 10\n") == 0)
        {
            ksleepm(10000);
        }
        else if (kstrcmp(buffer, "sleep 5\n") == 0)
        {
            ksleepm(5000);
        }
        else if (kstrcmp(buffer, "sleep 1\n") == 0)
        {
            ksleepm(1000);
        }
        else if (kstrcmp(buffer, "addkproc\n") == 0)
        {
            kthread_add(kthread_test, "kthread");
        }
        else
        {
            kprintf("Unknown command:");
            kprintf(buffer);
        }
        print_prompt();
    }
}
