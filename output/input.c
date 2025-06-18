#include <output/output.h>
#include <output/input.h>
#include <locks/spinlock.h>
#define MAX_CHARS 512
#define UART_BACKSPACE_CHAR 127
static char input_buffer[MAX_CHARS] = {'\0'};
static char internal_input_buffer[MAX_CHARS];
static int input_current_place =0;
static struct spinlock input_spinlock;

void input_init() 
{
    kbd_init();
    init_spinlock(&input_spinlock);
}

void input_read(char* dest)
{

    acquire_spinlock(&input_spinlock);
    kstrcpy(dest,input_buffer);
    for (int i=0; i<MAX_CHARS; i++) 
        input_buffer[i] = '\0';
    release_spinlock(&input_spinlock);
}

void input_add_char(char in)
{
    //skip tab until proper tty is implemented
    if (in == '\t' || in >UART_BACKSPACE_CHAR)
        return;
    acquire_spinlock(&input_spinlock);
    char output[2];
    int count=1;


    if (in == '\r')
        in = '\n';
    // handle backspace
    if (in == UART_BACKSPACE_CHAR  && input_current_place > 0 ) {
        //kprintf("%d\n",input_current_place);
        internal_input_buffer[input_current_place-1] = '\0';
        input_current_place--;
        // left one char and clear cursor
        kprintf("\033[%dD\033[J",count,output);
        //kprintf("%d\n",input_current_place);

        release_spinlock(&input_spinlock);
        return;
    }
    //empty line nothing in buffer
    if (in == UART_BACKSPACE_CHAR  && input_current_place == 0 ) {
        release_spinlock(&input_spinlock);
        return;
    }
    internal_input_buffer[input_current_place] = in;
    
    input_current_place +=1;
    if (in == '\n') {
        internal_input_buffer[input_current_place] = '\0';  
        kstrcpy(input_buffer,internal_input_buffer);
        for (int i=0; i<MAX_CHARS; i++) 
            internal_input_buffer[i] = '\0';
        input_current_place=0;
  }
    if (input_current_place == MAX_CHARS-1 )
        input_current_place = 0;
    release_spinlock(&input_spinlock);

    output[0] = in;
    output[1] = '\0';
    kprintf("%s",output);

}
