/**
 * ==============================================================================
 * LINKER.LD - Linker Script
 * ==============================================================================
 * This script tells the linker how to arrange our kernel in memory.
 * 
 * Memory Layout:
 * - 0x10000: Kernel code starts here (64KB, above bootloader)
 * - Sections are arranged in order: .text, .rodata, .data, .bss
 * 
 * Reference: OSDev Wiki - Linker Scripts
 * https://wiki.osdev.org/Linker_Scripts
 * GNU LD Manual: https://sourceware.org/binutils/docs/ld/Scripts.html
 * ==============================================================================
 */

/* Tell linker where kernel should be loaded */
/* 0x10000 = 64KB, safe above bootloader and stage2 */
OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    /* Kernel loaded at 64KB */
    . = 0x10000;
    
    /* .text section: executable code */
    .text ALIGN(4096) : {
        *(.text)           /* All .text sections from all object files */
    }
    
    /* .rodata section: read-only data (constants, strings) */
    .rodata ALIGN(4096) : {
        *(.rodata*)        /* All .rodata sections */
    }
    
    /* .data section: initialized global/static variables */
    .data ALIGN(4096) : {
        *(.data)           /* All .data sections */
    }
    
    /* .bss section: uninitialized global/static variables */
    /* This section is automatically zeroed by the loader */
    .bss ALIGN(4096) : {
        *(COMMON)          /* Common symbols */
        *(.bss)            /* All .bss sections */
    }
    
    /* End of kernel marker */
    _kernel_end = .;
}
