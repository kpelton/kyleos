#define INT_GATE 0x8E
#include "types.h"
struct gdt_entry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct idt_entry{
    uint16_t  offset_1; // offset bits 0..15
    uint16_t  selector; // a code segment selector in GDT or LDT
    uint8_t   ist; // type and attributes, see below
    uint8_t   type_attr; // type and attributes, see below
    uint16_t   offset_2; // offset bits 16..31
    uint32_t  offset_3; //offset bits 32..64
    uint32_t zero;
} __attribute__((packed));


struct gdt_ptr
{
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

struct idt_ptr
{
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

struct gdt_entry gdt[3];
struct gdt_ptr gp;

//Interrupt descriptor list
struct idt_entry idt[256];
struct idt_ptr ip_t;

void gdt_flush();
void idt_flush();
void std_handler();
void panic_handler();
void kbd_handler();
void timer_handler();
void idt_set_gate(int num, uint64_t base, uint8_t type_attr)
{
	idt[num].offset_1 = (uint16_t) (base & 0x0000FFFF);
	idt[num].offset_2 = (uint16_t) ((base  & 0xFFFF0000)>>16);
	idt[num].offset_3 = (uint32_t) ((base  & 0xFFFFFFFF00000000)>>32);
    idt[num].type_attr = type_attr;
    idt[num].selector = 0x8;
    idt[num].zero = 0;
    idt[num].ist = 0;

}
void idt_install(void)
{
    int i;
    ip_t.base = (uint64_t)&idt;
    ip_t.limit = (sizeof(struct idt_entry) *256)-1;

    //setup panic handler
    for(i=0; i<32; i++) {
      idt_set_gate(i,(uint64_t)panic_handler,INT_GATE);
    }
    
    //setup other ints
      idt_set_gate(32,(uint64_t )timer_handler,INT_GATE);
      idt_set_gate(33,(uint64_t )kbd_handler,INT_GATE);
              
    //write to cpu
    idt_flush();

}
 
// Very simple: fills a GDT entry using the parameters
void gdt_set_gate(int num, uint64_t base, uint64_t limit, uint8_t access, uint8_t gran)
{
	gdt[num].base_low = (uint16_t) (base & 0xFFFF);
	gdt[num].base_middle = (uint8_t) (base >> 16) & 0xFF;
	gdt[num].base_high = (uint8_t) (base >> 24) & 0xFF;
 
	gdt[num].limit_low = (limit & 0xFFFF);
	gdt[num].granularity = ((limit >> 16) & 0x0F);
 
	gdt[num].granularity |= (gran & 0xF0);
	gdt[num].access = access;
}
 
// Sets our 3 gates and installs the real GDT through the assembler function
void gdt_install()
{
	gp.limit = (sizeof(struct gdt_entry) * 3) - 1;
	gp.base = (uint64_t)&gdt;
 
	gdt_set_gate(0, 0, 0, 0, 0);
	gdt_set_gate(1, 0, 0xFFFFFFFFffffffff, 0x9A, 0x20);
	gdt_set_gate(2, 0, 0xFFFFFFFFffffffff, 0x92, 0x0);
 
	gdt_flush();
}
void gdt64_install()
{
    gp.limit = (sizeof(struct gdt_entry) * 3) - 1;
    gp.base = (uint64_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0, 0x9A, 0x20);
    gdt_set_gate(2, 0, 0, 0x9A, 0x0);

    gdt_flush();
}


