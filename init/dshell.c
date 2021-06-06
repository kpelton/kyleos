#include <asm/asm.h>
#include <output/output.h>
#include <init/tables.h>
#include <block/ata.h>
#include <irq/irq.h>
#include <mm/mm.h>
#include <timer/pit.h>
#include <block/vfs.h>


const char WHEELER_PROMPT[] = "Wheeler OS |0:/>";

static void ksleepm(unsigned int msec) {
    unsigned int expires = read_jiffies()+(msec);
    while(read_jiffies() < expires);
}

static void print_dir(struct inode* pwd) {
    struct inode_list *ptr;
    struct dnode *dptr;
    struct inode *iptr;
    dptr = vfs_read_inode_dir(pwd);
    if (dptr == 0)
        goto error;
    ptr = dptr->head;
    while (ptr != 0) {
        kprintf(ptr->current->i_name);
        if (ptr->current->i_type == I_DIR) {
            kprintf(" DIR");
        } else {
            iptr = ptr->current;
            kprintf(" FILE");
    }
        kprint_hex(" inode=",ptr->current->i_ino);
        ptr= ptr->next;
    }
    error:
    return;

}
struct inode *  shell_cd(char cmd[], struct dnode *dptr) {
    struct inode_list *ptr;
    struct inode *iptr;
    int i;
    char buffer[512];
    char *cmdptr = cmd;
    char *nptr = cmd;
    //Find directory
    while(*cmdptr != ' ')
        cmdptr++;
    cmdptr++;
    nptr = cmdptr;
    //Take out newline
    while(*nptr != '\n')
        *nptr++;
    *nptr = '\0';


    ptr = dptr->head;
    while (ptr != 0) {
        buffer[0] = '\0';
        if (ptr->current->i_type == I_DIR) {
            for(i=0; ptr->current->i_name[i] != ' '; i++){
                buffer[i] = ptr->current->i_name[i];
            }
            buffer[i] = '\0';
            /*
            kprintf("Comparing ");
            kprintf(buffer);
            kprintf(" ");
            kprintf(cmdptr);
            kprintf("\n");
            */
            if (kstrcmp(cmdptr,buffer) == 0)
                return ptr->current;
        } 
        ptr= ptr->next;
    }
    kprintf("bad directoy\n");
    return dptr->root_inode;
}

static void shell_cat(char cmd[], struct dnode *dptr) {
    struct inode_list *ptr;
    struct inode *iptr;
    int i;
    char buffer[512];
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
        buffer[0] = '\0';
        if (ptr->current->i_type == I_FILE) {
            for(i=0; ptr->current->i_name[i] != ' '; i++){
                buffer[i] = ptr->current->i_name[i];
            }
            buffer[i] = '\0';
            if (kstrcmp(cmdptr,buffer) == 0) {
                vfs_read_inode_file(ptr->current);
                return;
            }
        } 
        ptr= ptr->next;
    }
    kprintf("bad file\n");
    return dptr->root_inode;
}

void start_dshell() {

    char buffer[512];
    kprintf(WHEELER_PROMPT);
    struct dnode *dptr;
    dptr = vfs_read_root_dir("0:/");
    struct inode *pwd = dptr->root_inode;

    while (1) { 
        ksleepm(1);
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
        }
        else if (buffer[0] == 'c' && buffer[1] == 'a' 
                && buffer[2] == 't' && buffer[3] == ' '
                && buffer[4] != '\0') {
            dptr = vfs_read_inode_dir(pwd); 
            shell_cat(buffer,dptr);
        }
        else if (kstrcmp(buffer,"mem\n") == 0) {
            mm_print_stats();
        }else if (kstrcmp(buffer,"panic\n") == 0) {
            print_regs(0);
        }else if (kstrcmp(buffer,"jiffies\n") == 0) {
            kprint_hex("Jiffies 0x",read_jiffies());
        }else if (kstrcmp(buffer,"gas\n") == 0) {
            for (;;)
                kprintf("Fuck ted wheeler\n");
        } else {
            kprintf("Unknown command:");
            kprintf(buffer);
        }
        kprintf(WHEELER_PROMPT);

    }
}