
typedef __builtin_va_list va_list;
#define va_start(v, l)  __builtin_va_start(v, l)
#define va_end(v)   __builtin_va_end(v)
#define va_arg(v, T)    __builtin_va_arg(v, T)
#define va_copy(d, s)   __builtin_va_copy(d, s)

static int sleep(unsigned int msec) {
     long val = 0;
    asm volatile("mov $0, %%rax; int $0x80\n movq %%rax ,%0" : "=g"(val));
    return val;

}
static int print(char *msg) {
    long val = 0;
    asm volatile("mov $1, %%rax; int $0x80\n movq %%rax ,%0" : "=g"(val));
    return val;
}

static int open(char *path, int flags) {
    long val = 0;
    asm volatile("mov $2, %%rax; int $0x80\n movq %%rax ,%0" : "=g"(val));
    return val;
}

static int close(int fd) {
    long val = 0;
    asm volatile("mov $3, %%rax; int $0x80\n movq %%rax ,%0" : "=g"(val));
    return val;
}

static int read(int fd,void *buf, int count) {
    long val = 0;
    asm volatile("mov $4, %%rax; int $0x80\n movq %%rax ,%0" : "=g"(val));
    return val;
}

static char * itoa( unsigned long value, char * str, int base ) {
    char * rc;
    char * ptr;
    char * low;
    // Check for supported base.
    if ( base < 2 || base > 36 )
    {
        *str = '\0';
        return str;
    }
    rc = ptr = str;
    // Set '-' for negative decimals.
   // Remember where the numbers start.
    low = ptr;
    // The actual conversion.
    do
    {
        // Modulo is negative for negative value. This trick makes abs() unnecessary.
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + value % base];
        value /= base;
    } while ( value );
    // Terminating the string.
    *ptr-- = '\0';
    // Invert the numbers.
    while ( low < ptr )
    {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }
    return rc;
}


static void printf(char *format, ...)
{
    char *ptr = format;
    unsigned long x;
    char * str_ptr;
    char buffer[20];
    char cbuf[2];
    va_list arguments;
    va_start(arguments, format);
    while(*ptr != '\0') {
        if (*ptr == '%') {
            ptr++;
            switch (*ptr) {
                case 'x':
                    x = va_arg(arguments,unsigned long);
                    itoa(x,buffer,16);
                    print(buffer);
                break;
                case 'd':
                    x = va_arg(arguments,unsigned long);
                    itoa(x,buffer,10);
                    print(buffer);
                break;
                case 's':
                    str_ptr = va_arg(arguments,char *);
                    print(str_ptr);
                break;
                default:
                break;
            }
        } else {
            cbuf[0] = *ptr;
            cbuf[1] = '\0';
            print(cbuf);
        }
        ptr++;
    }
    va_end(arguments);
}

void read_test() 
{
    int retval = 0;
    int fd = 0;
    char buffer[1025];
    fd = open("/bible.txt",123);
    if (fd >= 0 ) {
        retval = read(fd,buffer,1024);
        printf("read returned %d\n",retval);
        buffer[1024]= '\0';
        printf(buffer);
        retval = close(fd);
        printf("close returned %d\n",retval);
    }else{
        printf("Error opening file");
    }

}

void read_fullpath_test() 
{
    int retval = 0;
    int fd = 0;
    char buffer[1025];
    fd = open("/cs/cs200/a03/a03.c",123);
    if (fd >= 0 ) {
        retval = read(fd,buffer,1024);
        printf("read returned %d\n",retval);
        buffer[1024]= '\0';
        printf(buffer);
        retval = close(fd);
        printf("close returned %d\n",retval);
    }else{
        printf("Error opening file");
    }

}

void test1() {
    int retval = 0;

    for(;;){
        retval = sleep(100);
        
        printf("sleep returned %d\n",retval);
        read_test();

        retval = open("/bible.txt",123);
        printf("open returned %d\n",retval);
        retval = close(retval);
        printf("close returned %d\n",retval);
        retval = close(-1);
        printf("invalid close check %d\n",retval);
}
}

int _start() 
{

    for(;;){
    read_fullpath_test();
    read_test();
    sleep(500);
    }
    
}
