#include <output/output.h>
#include <output/input.h>
#include <locks/spinlock.h>
#define MAX_CHARS 512
#define UART_BACKSPACE_CHAR 127
static char input_buffer[MAX_CHARS] = {'\0'};
static char internal_input_buffer[MAX_CHARS];
static int input_current_place =0;
static struct spinlock input_spinlock;
#define KEY_QUEUE_SIZE 64
struct key_event { uint8_t scancode; bool extended; bool pressed; };
static struct key_event key_queue[KEY_QUEUE_SIZE];
static int key_head;
static int key_tail;
static bool raw_mode;
#define RAW_QUEUE_SIZE 128
static char raw_queue[RAW_QUEUE_SIZE];
static int raw_head;
static int raw_tail;

void input_init() 
{
    kbd_init();
    init_spinlock(&input_spinlock);
}

void input_read(char* dest)
{

    acquire_spinlock(&input_spinlock);
    if (raw_mode) {
        if (raw_head != raw_tail) {
            dest[0] = raw_queue[raw_tail];
            raw_tail = (raw_tail + 1) % RAW_QUEUE_SIZE;
            dest[1] = '\0';
        } else {
            dest[0] = '\0';
        }
        release_spinlock(&input_spinlock);
        return;
    }
    kstrcpy(dest,input_buffer);
    for (int i=0; i<MAX_CHARS; i++) 
        input_buffer[i] = '\0';
    release_spinlock(&input_spinlock);
}

void input_add_char(char in)
{
    if (raw_mode) {
        acquire_spinlock(&input_spinlock);
        int next = (raw_head + 1) % RAW_QUEUE_SIZE;
        if (next != raw_tail) {
            raw_queue[raw_head] = in;
            raw_head = next;
        }
        release_spinlock(&input_spinlock);
        return;
    }
    //skip tab until proper tty is implemented
    if (in == '\t' )
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

void input_set_raw(bool enabled)
{
    acquire_spinlock(&input_spinlock);
    raw_mode = enabled;
    if (enabled) {
        input_current_place = 0;
        for (int i = 0; i < MAX_CHARS; i++) {
            input_buffer[i] = '\0';
            internal_input_buffer[i] = '\0';
        }
        raw_head = 0;
        raw_tail = 0;
    }
    release_spinlock(&input_spinlock);
}

bool input_is_raw(void)
{
    return raw_mode;
}

void input_add_key(uint8_t scancode, bool extended, bool pressed)
{
    int next;
    acquire_spinlock(&input_spinlock);
    next = (key_head + 1) % KEY_QUEUE_SIZE;
    if (next != key_tail) {
        key_queue[key_head].scancode = scancode;
        key_queue[key_head].extended = extended;
        key_queue[key_head].pressed = pressed;
        key_head = next;
    }
    release_spinlock(&input_spinlock);
}

int input_poll_key(int *pressed, uint8_t *scancode, bool *extended)
{
    if (pressed == NULL || scancode == NULL || extended == NULL)
        return 0;
    acquire_spinlock(&input_spinlock);
    if (key_head == key_tail) {
        release_spinlock(&input_spinlock);
        return 0;
    }
    *scancode = key_queue[key_tail].scancode;
    *extended = key_queue[key_tail].extended;
    *pressed = key_queue[key_tail].pressed;
    key_tail = (key_tail + 1) % KEY_QUEUE_SIZE;
    release_spinlock(&input_spinlock);
    return 1;
}
