#include <asm/asm.h>
#include <output/output.h>
#include <output/input.h>
#include <init/tables.h>
#include <block/ata.h>
#include <irq/irq.h>
#include <mm/mm.h>
#include <fs/vfs.h>
#include <sched/sched.h>
#include <timer/rtc.h>
#include <timer/pit.h>
#include <mm/pmem.h>
#include <fs/elf.h>
#include <sched/exec.h>
#include <sched/ps.h>


void test_user_function5();
static const char OS_PROMPT[] = "Kyle OS |";

static char *dir_stack[100][256];
static int top_dir_stack = -1;
extern uint64_t TICK_HZ;
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

static void print_dir(struct dnode *parent,struct inode *pwd)
{
    struct inode_list *ptr;
    struct dnode *dptr;

    dptr = vfs_read_inode_dir(parent,pwd);
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
    char dir[256];
    for(int i=0; i<256; i++)
        dir[i] = 0 ;

    for(int i=0; i<256; i++) {
        if(dir_name[i] == '\n')
            break;
        dir[i] = dir_name[i];
    }
    //kprintf("vfs-create-dir %x\n",pwd->i_ino);
    vfs_create_dir(pwd,dir);
    
}

static void touch(struct inode *pwd,char *dir_name)
{
    char dir[256];
     for(int i=0; i<256; i++)
        dir[i] = 0 ;

    for(int i=0; i<256; i++) {
        if(dir_name[i] == '\n')
            break;
        dir[i] = dir_name[i];
    }
    //kprintf("vfs-create-dir %x\n",pwd->i_ino);
    vfs_create_file(pwd,dir,0);
}

struct inode *shell_cd(char cmd[], struct dnode *dptr)
{
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
    kprintf("Cding to %s\n",cmdptr);
    struct inode * data = vfs_walk_path(cmdptr,dptr,I_DIR);
    if (data){
        if (data->i_ino == dptr->root_inode->i_ino && data->dev->devicenum == dptr->root_inode->dev->devicenum)
            return 0;
    }
    kprintf("returned  %x\n",data);
    return data;
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
                dptr = vfs_read_inode_dir((struct dnode *)ptr,ptr->current);
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
    struct dnode *dptr;
    dptr = vfs_read_root_dir("/");
    struct dnode *olddptr = dptr;
    struct inode *pwd = dptr->root_inode;
    struct inode *oldpwd = pwd;
    struct inode *itmp;
    push_dir_stack("/");
    print_prompt();
    //kprintf("root %x\n",pwd->i_ino);
    //write_sec(0,buffer2);
    // asm("test: jmp test");
    //debug
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
        //
    while (1)
    {
        read_input(buffer);
        if (buffer[0] == '\0')
        {
            asm("hlt");
            continue;
        }
        if (kstrcmp(buffer, "ls\n") == 0)
        {

            print_dir(dptr,pwd);
            //kprintf("root %x\n",pwd->i_ino);

        }
        else if (buffer[0] == 'm' && buffer[1] == 'k' && buffer[2] == 'd' && buffer[3] == 'i'&& buffer[4] == 'r'  && buffer[5] == ' ' && buffer[6] != '\n')
        {
            //for(int j =0; j<500; j++){
             mkdir(pwd,buffer+6);
             //  mkdir(pwd,"a");

           // }
        }
        else if (buffer[0] == 't' && buffer[1] == 'o' && buffer[2] == 'u' && buffer[3] == 'c'&& buffer[4] == 'h'  && buffer[5] == ' ' && buffer[6] != '\n')
        {
            //for(int j =0; j<500; j++){
             touch(pwd,buffer+6);
             //  mkdir(pwd,"a");

           // }
        }

        else if (buffer[0] == 's' && buffer[1] == 'k' && buffer[2] == 'd' && buffer[3] == 'i'&& buffer[4] == 'r'  && buffer[5] == ' ' && buffer[6] != '\n')
        {
            for(int j =0; j<500; j++){
             //mkdir(pwd,buffer+6);
               mkdir(pwd,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

            }
        }

        else if (buffer[0] == 'l'  && buffer[1] == 's'  && buffer[2] == ' ' && buffer[3] != '\n')
        {
            cptr = buffer + 3;
            while (*cptr != '\n' && *cptr != '\0')
            {
                cptr++;
            }
            *cptr = '\0';
            dptr = vfs_read_inode_dir(dptr,pwd);
            itmp = read_path(buffer + 3, dptr,I_DIR);
            if(itmp != NULL) {
                print_dir(dptr,itmp);

                //vfs_free_dnode(dptr);
            }else
            {
                kprintf("ls failed\n");
            }
        }
        else if (buffer[0] == 'c' && buffer[1] == 'd' && buffer[2] == ' ' && buffer[3] != '\n')
        {
            olddptr=dptr;
            dptr = vfs_read_inode_dir(dptr,pwd);
            pwd = shell_cd(buffer, dptr);
            //If failure
            if (pwd == 0)
            {
                dptr = olddptr;
                pwd = oldpwd;
            }
            else
            {
                if (buffer[3] == '.' && buffer[4] == '.' && kstrcmp(dptr->i_name, "/") != 0 && oldpwd != pwd)
                    pop_dir_stack();
                else if (oldpwd != pwd)
                    push_dir_stack(pwd->i_name);
                olddptr = dptr;
                oldpwd = pwd;
            }
        }
        else if (buffer[0] == 'k' && buffer[1] == 'i' && buffer[2] == 'l' && buffer[3] == 'l' && buffer[4] == ' ' && buffer[5] != '\n')
        {
            cptr = buffer + 5;
            while (*cptr != '\n' && *cptr != '\0')
            {
                cptr++;
            }
            *cptr = '\0';
            pid = atoi(buffer + 5);
            if (sched_process_kill(pid,true) == false)
                kprintf("Kill failed\n");
        }

        else if (buffer[0] == 'c' && buffer[1] == 'a' && buffer[2] == 't' && buffer[3] == ' ' && buffer[4] != '\n')
        {
            struct file *rfile;
            uint8_t *rbuffer;
            uint32_t bytes;
            cptr = buffer + 4;
            kprintf("test\n");
            while (*cptr != '\n' && *cptr != '\0')
            {
                cptr++;
            }
            *cptr = '\0';
            dptr = vfs_read_inode_dir(dptr,pwd);
            itmp = read_path(buffer + 4, dptr,I_FILE);
            if(itmp != NULL) {
                //vfs_cat_inode_file(itmp);
                rfile = vfs_open_file(itmp,O_RDONLY);
                rbuffer = kmalloc(itmp->file_size+1);
                bytes = vfs_read_file(rfile,rbuffer,itmp->file_size);
                rbuffer[itmp->file_size] = '\0';
                kprintf("Read %d\n",bytes);
                kprintf("%s",rbuffer);
                kfree(rbuffer);
                vfs_close_file(rfile);
            }else
            {
                kprintf("cat failed\n");
            }
        }
        else if (buffer[0] == 'e' && buffer[1] == 'x' && buffer[2] == 'e' && buffer[3] == 'c' && buffer[4] ==  ' ' && buffer[5] != '\n')
        {
            cptr = buffer + 5;
            while (*cptr != '\n' && *cptr != '\0')
            {
                cptr++;
            }
            *cptr = '\0';
            dptr = vfs_read_inode_dir(dptr,pwd);
            itmp = read_path(buffer + 5, dptr,I_FILE);
            if(itmp != NULL) {
                int pid = exec_from_inode(itmp,false,NULL);
                kprintf("new pid: %d",pid);
            }else
            {
                kprintf("cat failed\n");
            }
        }
        else if (buffer[0] == 'e' && buffer[1] == 'x' && buffer[2] == 'e' && buffer[3] == 'w' && buffer[4] ==  ' ' && buffer[5] != '\n')
        {
            cptr = buffer + 5;
            while (*cptr != '\n' && *cptr != '\0')
            {
                cptr++;
            }

            char *strings[] = {"aaa","HELLO CHANNO TEST ARGV","aaaaaaaaaaaaaa","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",0};

            *cptr = '\0';
            dptr = vfs_read_inode_dir(dptr,pwd);
            itmp = read_path(buffer + 5, dptr,I_FILE);
            if(itmp != NULL) {
                int pid = exec_from_inode(itmp,false,(char **) &strings);
                if(pid >= 0)
                    process_wait(pid);

            }else
            {
                kprintf("cat failed\n");
            }
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
        else if (kstrcmp(buffer, "addhz50\n") == 0)
        {
            TICK_HZ+=50;
            pit_init();
        }
         else if (kstrcmp(buffer, "addhz500\n") == 0)
        {
            TICK_HZ+=500;
            pit_init();
        }
 
         else if (kstrcmp(buffer, "addtickhz50\n") == 0)
        {
            HZ+=50;
            pit_init();
        }
         else if (kstrcmp(buffer, "addtickhz500\n") == 0)
        {
            HZ+=500;
            pit_init();
        }
        else
        {
            kprintf("Unknown command:");
            kprintf(buffer);
        }
        print_prompt();
    }
}
