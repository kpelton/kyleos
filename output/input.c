#include <output/output.h>
#include <output/input.h>
#define MAX_CHARS 512
static char input_buffer[MAX_CHARS] = {'\0'};
static char internal_input_buffer[MAX_CHARS];
static int input_current_place =0;

void input_init() 
{
    kbd_init();
}

void input_read(char* dest)
{
    kstrcpy(dest,input_buffer);
    for (int i=0; i<MAX_CHARS; i++) 
        input_buffer[i] = '\0';
}

void input_add_char(char in)
{
    char output[2];
    if (in == '\r')
        in = '\n';

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


    output[0] = in;
    output[1] = '\0';
    kprintf("%s",output);

}
