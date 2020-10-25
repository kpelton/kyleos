ENTRY (kernel_bootstrap)
SECTIONS
{
         . = 0x00400;
	/* The kernel will live at 3GB + 1MB in the virtual address space, */
	/* which will be mapped to 1MB in the physical address space. */
	/* Note that we page-align the sections. */

     .multiboot.data : {
            *(.multiboot.data)
        }

       .multiboot.text : {
           *(.multiboot.text)
       }
 
     . += 0xffffffff80000000;
    .text ALIGN (0x1000) : AT (ADDR (.text) - 0xffffffff80000000)
    {
        *(.text)
    }

    .rodata ALIGN (0x1000): AT (ADDR (.rodata) - 0xffffffff80000000)
    {
        *(.rodata*)
    }

    .data ALIGN (0x1000) :  AT (ADDR (.data) - 0xffffffff80000000)
    {
        *(.data)
    }

    .bss  ALIGN(0x1000) :AT (ADDR (.bss) - 0xffffffff80000000)
    {
        *(COMMON)
        *(.bss)
    }
	_kernel_end = .;
}
