#include "vga.h"
#include "asm.h"
static int cursor_x = 0;
static int cursor_y = 0;

static unsigned char *vram = (unsigned char *)0xffffffff800B8000;
//to be moved to stack once it is adjusted
void update_cursor(int row, int col) {
    unsigned short position=(row*80) + col;
    // cursor LOW port to vga INDEX register
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(position&0xFF));
    // cursor HIGH port to vga INDEX register
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char )((position>>8)&0xFF));
}

void vga_clear(){
    int i;
    asm("cli");
    for(i=0; i<(80*25)*2; i++) {
        vram[i] = 0x00;
    }
    asm("cli");
}
void vga_scroll(){
    int i=0;
    int j=0;
    int offset = 0;
    int from_offset = 0;
    asm("cli");
        for(j=1; j<25; j+=1) {
            for(i=0; i<80;  i+=1) {
                offset = (i*2)+(((j-1)*80)*2);
                from_offset = (i*2)+(((j)*80)*2);
                vram[offset] = vram[from_offset];
                vram[offset+1] = vram[from_offset+1];
                //vram[offset+1] = vram[from_offset+1] ;
        }
    }
    for(i=0; i<80;  i+=1) {
        offset = (i*2)+(((24)*80)*2);
        vram[offset]=0;
        vram[offset+1]=0;
    }
 
    asm("sti");
}

void print_loc(const int x, const int y, 
        const int back, const int fore, 
        const char c, const int blink) {

    int i = (x*2)+((y*80)*2);
    vram[i] = c;
    vram[i+1] = (fore) | (back << 4) | ((blink &1)<<8) ;
}


void vga_kprintf(char *str) {
    for(;*str !='\0'; str++) {

        if (*str == '\n') {
            cursor_y++;
            cursor_x=0; 
            if (cursor_y >=25){
                vga_scroll();

                cursor_y=24;
            }


        } else {
            print_loc(cursor_x,cursor_y,0, 3,*str,0);
            cursor_x++;   
        }
        if(cursor_x >= 80){
            cursor_y++;
            cursor_x=0;
             if (cursor_y ==25){
                vga_scroll();
                cursor_y=24;
            }
        }
        update_cursor(cursor_y,cursor_x);
    }
}

