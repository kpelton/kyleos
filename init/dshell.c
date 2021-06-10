#include <asm/asm.h>
#include <output/output.h>
#include <init/tables.h>
#include <block/ata.h>
#include <irq/irq.h>
#include <mm/mm.h>
#include <timer/pit.h>
#include <block/vfs.h>


char WHEELER_PROMPT[] = "Ted Wheeler OS |0:";

char * dir_stack[100][256];
int top_dir_stack = -1;

static void ksleepm(unsigned int msec) {
    unsigned int expires = read_jiffies()+(msec);
    while(read_jiffies() < expires);
}

static void push_dir_stack(char *dir) 
{
    if (top_dir_stack == -1) {
        top_dir_stack = 0;
    } else if (top_dir_stack == 99) {
        kprintf("Dir stack full");
        return;
    }else {
        top_dir_stack++;
    }
    kstrcpy( (char*)dir_stack[top_dir_stack],dir);
}

static char * pop_dir_stack() 
{
    char *val;
    if (top_dir_stack == -1){ 
        kprintf("dirstack empty");
        return 0;
    }
    val = (char *) dir_stack[top_dir_stack];
    top_dir_stack -=1;
    return val;
}

static void print_dir(struct inode* pwd) {
    struct inode_list *ptr;
    struct dnode *dptr;

    dptr = vfs_read_inode_dir(pwd);
    if (dptr == 0)
        goto error;
    ptr = dptr->head;
    while (ptr != 0) {
        kprintf(ptr->current->i_name);
        if (ptr->current->i_type == I_DIR) {
            kprintf(" DIR");
        } else {
            kprintf(" FILE");
    }
        kprint_hex(" inode=",ptr->current->i_ino);
        ptr= ptr->next;
    }
    vfs_free_dnode(dptr);
    error:
    return;

}
struct inode *  shell_cd(char cmd[], struct dnode *dptr) {
    struct inode_list *ptr;
    char *cmdptr = cmd;
    char *nptr = cmd;
    //Find directory
    while(*cmdptr != ' ')
        cmdptr++;
    cmdptr++;
    nptr = cmdptr;
    //Take out newline
    while(*nptr != '\n')
        nptr++;
    *nptr = '\0';


    ptr = dptr->head;
    while (ptr != 0) {
        if (ptr->current->i_type == I_DIR) {

            kprintf("Comparing ");
            kprintf(ptr->current->i_name);
            kprintf(" ");
            kprintf(cmdptr);
            kprintf("\n");

            if (kstrcmp(cmdptr,ptr->current->i_name) == 0)
                return ptr->current;
        } 
        ptr= ptr->next;
    }
    kprintf("bad directoy\n");
    return 0;
}

static void shell_cat(char cmd[], struct dnode *dptr) {
    struct inode_list *ptr;
    char *cmdptr = cmd;
    char *nptr = cmd;
    //Find directory
    while(*cmdptr != ' ')
        cmdptr++;
    cmdptr++;
    nptr = cmdptr;
    //Take out newline
    while(*nptr != '\n')
        nptr++;
    *nptr = '\0';


    ptr = dptr->head;
    while (ptr != 0) {
        if (ptr->current->i_type == I_FILE) {
            kprintf("Comparing ");
            kprintf(ptr->current->i_name);
            kprintf(" ");
            kprintf(cmdptr);
            kprintf("\n");
            if (kstrcmp(cmdptr,ptr->current->i_name) == 0) {
                vfs_read_inode_file(ptr->current);
                return;
            }
        } 
        ptr= ptr->next;
    }
    kprintf("bad file\n");
}

void print_prompt() {
    kprintf(WHEELER_PROMPT);
    for(int i=0; i<=top_dir_stack; i++)
    {
        kprintf((char *)dir_stack[i]);
        if (i==top_dir_stack)
                kprintf(">");
        else if (i > 0)
            kprintf("/");
    }
}
void start_dshell() {

    char buffer[512];
    struct dnode *dptr;
    struct dnode *dptr1;
    dptr = vfs_read_root_dir("0:/"); 
    struct dnode *olddptr=dptr;
    struct inode *pwd = dptr->root_inode;
    struct inode *oldpwd=pwd;
    push_dir_stack("/");
    print_prompt();
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


//
    while (1) { 
        asm("hlt");
        for(int i =0; i<512; i++)
            buffer[i]= '\0';
        read_input(buffer);
        if (buffer[0] == '\0')
            continue;
        if (kstrcmp(buffer,"ls\n") == 0) {

            print_dir(pwd);
        }
        else if (buffer[0] == 'c' && buffer[1] == 'd' 
                && buffer[2] == ' ' && buffer[3] != '\n') {
            dptr = vfs_read_inode_dir(pwd);
            pwd = shell_cd(buffer,dptr);
            //If failure
            if (pwd == 0) {
                pwd = oldpwd;
            } else {
                vfs_free_dnode(olddptr);
                if (buffer[3] == '.' && buffer[4] == '.' && kstrcmp(dptr->i_name,"/") !=0 && oldpwd != pwd)
                    pop_dir_stack();
                else if(oldpwd != pwd)
                    push_dir_stack(pwd->i_name);
                olddptr = dptr;
                oldpwd = pwd;
            }

        }
        else if (buffer[0] == 'c' && buffer[1] == 'a' 
                && buffer[2] == 't' && buffer[3] == ' '
                && buffer[4] != '\0') {
            dptr1 = vfs_read_inode_dir(pwd);

            shell_cat(buffer,dptr1);
            vfs_free_dnode(dptr1);
        }
        else if (kstrcmp(buffer,"mem\n") == 0) {
            mm_print_stats();
        }else if (kstrcmp(buffer,"panic\n") == 0) {
            print_regs(0,0);
        }else if (kstrcmp(buffer,"jiffies\n") == 0) {
            kprint_hex("Jiffies 0x",read_jiffies());
        }else if (kstrcmp(buffer,"gas\n") == 0) {
            for (;;)
                kprintf("Fuck ted wheeler\n");
        } else {
            kprintf("Unknown command:");
            kprintf(buffer);
        }
        print_prompt();
    }
}