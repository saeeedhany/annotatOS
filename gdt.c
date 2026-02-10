/**
 * ==============================================================================
 * GDT.C - Global Descriptor Table Implementation
 * ==============================================================================
 * The GDT defines memory segments in Protected Mode. Each entry describes:
 * - Base address: Where the segment starts in memory
 * - Limit: Size of the segment
 * - Access rights: Privilege level, read/write/execute permissions
 * - Flags: Granularity (byte/page), size (16/32-bit)
 * 
 * In modern "flat memory model", we create segments that span entire 4GB space.
 * Segmentation is mostly obsolete (replaced by paging), but we still need GDT.
 * 
 * Reference: OSDev Wiki - GDT Tutorial
 * https://wiki.osdev.org/GDT_Tutorial
 * Intel Manual Vol 3A, Section 3.4.5
 * ==============================================================================
 */

#include "gdt.h"
#include "kernel.h"

/**
 * ==============================================================================
 * GDT Entry Structure
 * ==============================================================================
 * Each GDT entry is 8 bytes with the following bit layout:
 * 
 * 63        56 55  52 51      48 47           40 39        32
 * [Base 24:31][Flags][Limit 16:19][Access Byte][Base 16:23]
 * 31                              16 15                    0
 * [      Base Address 0:15        ][    Segment Limit 0:15 ]
 */
struct gdt_entry {
    uint16_t limit_low;      /* Limit bits 0-15 */
    uint16_t base_low;       /* Base bits 0-15 */
    uint8_t  base_middle;    /* Base bits 16-23 */
    uint8_t  access;         /* Access byte (see below) */
    uint8_t  granularity;    /* Flags (4 bits) + Limit bits 16-19 (4 bits) */
    uint8_t  base_high;      /* Base bits 24-31 */
} __attribute__((packed));   /* Prevent compiler from padding this structure */

/**
 * Access Byte Format:
 * 7: Present (P) - Must be 1 for valid segment
 * 6-5: Descriptor Privilege Level (DPL) - Ring 0-3
 * 4: Descriptor Type (S) - 1 for code/data, 0 for system
 * 3: Executable (E) - 1 for code, 0 for data
 * 2: Direction/Conforming (DC)
 *    - Data: 0 = grows up, 1 = grows down
 *    - Code: 0 = only executable in ring = DPL, 1 = executable in ring >= DPL
 * 1: Readable/Writable (RW)
 *    - Code: 1 = readable
 *    - Data: 1 = writable
 * 0: Accessed (A) - Set by CPU when segment is accessed
 */

/**
 * Granularity Byte Format:
 * 7: Granularity (G) - 0 = 1 byte, 1 = 4KB (page)
 * 6: Size (DB) - 0 = 16-bit, 1 = 32-bit
 * 5: Long mode (L) - 1 for 64-bit code segment
 * 4: Reserved (should be 0)
 * 3-0: Limit bits 16-19
 */

/**
 * ==============================================================================
 * GDT Pointer Structure
 * ==============================================================================
 * This structure is loaded into GDTR register using LGDT instruction
 */
struct gdt_ptr {
    uint16_t limit;      /* Size of GDT - 1 (in bytes) */
    uint32_t base;       /* Address of first GDT entry */
} __attribute__((packed));

/**
 * ==============================================================================
 * Global Variables
 * ==============================================================================
 */

/* We define 5 GDT entries */
#define GDT_ENTRIES 5

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr gdt_pointer;

/**
 * ==============================================================================
 * Helper Function: Set GDT Entry
 * ==============================================================================
 */
static void gdt_set_gate(int num, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran) {
    /* Set base address */
    gdt[num].base_low = (base & 0xFFFF);         /* Bits 0-15 */
    gdt[num].base_middle = (base >> 16) & 0xFF;  /* Bits 16-23 */
    gdt[num].base_high = (base >> 24) & 0xFF;    /* Bits 24-31 */
    
    /* Set segment limit */
    gdt[num].limit_low = (limit & 0xFFFF);       /* Bits 0-15 */
    gdt[num].granularity = (limit >> 16) & 0x0F; /* Bits 16-19 */
    
    /* Set granularity and flags */
    gdt[num].granularity |= (gran & 0xF0);       /* Flags in upper 4 bits */
    
    /* Set access byte */
    gdt[num].access = access;
}

/**
 * ==============================================================================
 * Public Function: Initialize GDT
 * ==============================================================================
 */
void gdt_init(void) {
    /* Set up GDT pointer */
    gdt_pointer.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gdt_pointer.base = (uint32_t)&gdt;
    
    /*
     * Entry 0: Null Descriptor
     * Required by Intel specification - must be all zeros
     * Accessing this selector causes a General Protection Fault
     */
    gdt_set_gate(0, 0, 0, 0, 0);
    
    /*
     * Entry 1: Kernel Code Segment (Ring 0)
     * Base: 0x0
     * Limit: 0xFFFFFF (with 4KB granularity = 4GB)
     * Access: 0x9A
     *   - 10011010 binary
     *   - Present(1) DPL(00) Type(1) Exec(1) DC(0) RW(1) Accessed(0)
     *   - Present, Ring 0, Code segment, Executable, Readable
     * Granularity: 0xCF
     *   - 11001111 binary
     *   - G(1) DB(1) L(0) Reserved(0) Limit[19:16](1111)
     *   - 4KB granularity, 32-bit, Limit = 0xFFFFF
     */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    
    /*
     * Entry 2: Kernel Data Segment (Ring 0)
     * Base: 0x0
     * Limit: 0xFFFFFF (with 4KB granularity = 4GB)
     * Access: 0x92
     *   - 10010010 binary
     *   - Present(1) DPL(00) Type(1) Exec(0) DC(0) RW(1) Accessed(0)
     *   - Present, Ring 0, Data segment, Writable
     * Granularity: 0xCF (same as code segment)
     */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    
    /*
     * Entry 3: User Code Segment (Ring 3)
     * Base: 0x0
     * Limit: 0xFFFFFF (with 4KB granularity = 4GB)
     * Access: 0xFA
     *   - 11111010 binary
     *   - Present(1) DPL(11) Type(1) Exec(1) DC(0) RW(1) Accessed(0)
     *   - Present, Ring 3, Code segment, Executable, Readable
     * Granularity: 0xCF
     */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    
    /*
     * Entry 4: User Data Segment (Ring 3)
     * Base: 0x0
     * Limit: 0xFFFFFF (with 4KB granularity = 4GB)
     * Access: 0xF2
     *   - 11110010 binary
     *   - Present(1) DPL(11) Type(1) Exec(0) DC(0) RW(1) Accessed(0)
     *   - Present, Ring 3, Data segment, Writable
     * Granularity: 0xCF
     */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    
    /* Load the GDT using assembly function */
    /* This calls the gdt_flush function in kernel_entry.asm */
    gdt_flush((uint32_t)&gdt_pointer);
}

/**
 * ==============================================================================
 * Segment Selector Definitions
 * ==============================================================================
 * These are the values to load into segment registers.
 * 
 * Selector Format (16 bits):
 * - Bits 15-3: Index into GDT (which entry to use)
 * - Bit 2: TI (Table Indicator) - 0 = GDT, 1 = LDT
 * - Bits 1-0: RPL (Requested Privilege Level) - 0-3
 * 
 * Examples:
 * - 0x08 = 0000000000001|0|00 = Index 1, GDT, Ring 0 (Kernel Code)
 * - 0x10 = 0000000000010|0|00 = Index 2, GDT, Ring 0 (Kernel Data)
 * - 0x18 = 0000000000011|0|00 = Index 3, GDT, Ring 0 (User Code, but with Ring 0 RPL)
 * - 0x1B = 0000000000011|0|11 = Index 3, GDT, Ring 3 (User Code with Ring 3 RPL)
 * - 0x20 = 0000000000100|0|00 = Index 4, GDT, Ring 0 (User Data, but with Ring 0 RPL)
 * - 0x23 = 0000000000100|0|11 = Index 4, GDT, Ring 3 (User Data with Ring 3 RPL)
 */
