#define INT_GATE 0x8E
#include <include/types.h>
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
void panic_handler_1();
void panic_handler_2();
void panic_handler_3();
void panic_handler_4();
void panic_handler_5();
void panic_handler_6();
void panic_handler_7();
void panic_handler_8();
void panic_handler_9();
void panic_handler_10();
void panic_handler_11();
void panic_handler_12();
void panic_handler_13();
void panic_handler_14();
void panic_handler_15();
void panic_handler_16();
void panic_handler_17();
void panic_handler_18();
void panic_handler_19();
void panic_handler_20();
void panic_handler_21();
void panic_handler_22();
void panic_handler_23();
void panic_handler_24();
void panic_handler_25();
void panic_handler_26();
void panic_handler_27();
void panic_handler_28();
void panic_handler_29();
void panic_handler_30();
void panic_handler_31();
void panic_handler_32();
void kbd_handler();
void timer_handler();
void serial_handler();
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
#define IDT_PANIC(i)  idt_set_gate(i, (uint64_t) panic_handler_##i ,INT_GATE)


void idt_install(void)
{
    int i;
    ip_t.base = (uint64_t)&idt;
    ip_t.limit = (sizeof(struct idt_entry) *256)-1;

    IDT_PANIC(1);
    IDT_PANIC(2);
    IDT_PANIC(3);
    IDT_PANIC(4);
    IDT_PANIC(5);
    IDT_PANIC(6);
    IDT_PANIC(7);
    IDT_PANIC(8);
    IDT_PANIC(9);
    IDT_PANIC(10);
    IDT_PANIC(11);
    IDT_PANIC(12);
    IDT_PANIC(13);
    IDT_PANIC(14);
    IDT_PANIC(15);
    IDT_PANIC(16);
    IDT_PANIC(17);
    IDT_PANIC(18);
    IDT_PANIC(19);
    IDT_PANIC(20);
    IDT_PANIC(21);
    IDT_PANIC(22);
    IDT_PANIC(23);
    IDT_PANIC(24);
    IDT_PANIC(25);
    IDT_PANIC(26);
    IDT_PANIC(27);
    IDT_PANIC(28);
    IDT_PANIC(29);
    IDT_PANIC(30);
    IDT_PANIC(31);
    IDT_PANIC(32);

    
    //setup other ints
      idt_set_gate(32,(uint64_t )timer_handler,INT_GATE);
      idt_set_gate(33,(uint64_t )kbd_handler,INT_GATE);
      idt_set_gate(36,(uint64_t )serial_handler,INT_GATE);
              
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


