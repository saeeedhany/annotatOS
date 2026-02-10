/**
 * linker.ld - Linker Script
 * Location: kernel/linker.ld
 *
 * Tells the linker how to arrange our kernel in memory
 */

OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    . = 0x1000;                 /* Kernel loaded at 4KB */
    
    .text : {
        *(.text)
    }
    
    .data : {
        *(.data)
        *(.rodata)
    }
    
    .bss : {
        *(.bss)
        *(COMMON)
    }
}
