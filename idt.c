/**
 * ==============================================================================
 * IDT.C - Interrupt Descriptor Table Implementation
 * ==============================================================================
 * The IDT defines handlers for interrupts and exceptions. Each entry contains:
 * - Handler address: Function to call when interrupt occurs
 * - Segment selector: Which code segment handler runs in (usually kernel code)
 * - Type and attributes: Gate type, privilege level
 * 
 * Types of interrupts:
 * 1. CPU Exceptions (0-31): Divide by zero, page fault, etc.
 * 2. Hardware Interrupts (32-47): Keyboard, timer, disk, etc. (via PIC)
 * 3. Software Interrupts (48+): System calls (INT 0x80)
 * 
 * Reference: OSDev Wiki - Interrupt Descriptor Table
 * https://wiki.osdev.org/Interrupt_Descriptor_Table
 * Intel Manual Vol 3A, Section 6.10
 * ==============================================================================
 */

#include "idt.h"
#include "kernel.h"
#include "screen.h"

/**
 * ==============================================================================
 * IDT Entry Structure
 * ==============================================================================
 * Each IDT entry is 8 bytes (64 bits):
 * 
 * 63        48 47  45 44 43    40 39     32 31           16 15            0
 * [Offset 31:16][P|DPL|0|Type   |Reserved][Segment Selector][Offset 15:0]
 * 
 * - Offset: Address of interrupt handler function (split into low and high)
 * - Selector: Code segment selector from GDT (usually 0x08 for kernel code)
 * - Type: Gate type (0x8E for 32-bit interrupt gate, 0xEE for 32-bit trap gate)
 * - DPL: Descriptor Privilege Level (0-3, usually 0 for kernel, 3 for user)
 * - P: Present bit (must be 1 for valid entry)
 */
struct idt_entry {
    uint16_t base_low;       /* Lower 16 bits of handler address */
    uint16_t selector;       /* Kernel segment selector (from GDT) */
    uint8_t  always0;        /* Always 0 */
    uint8_t  flags;          /* Type and attributes */
    uint16_t base_high;      /* Upper 16 bits of handler address */
} __attribute__((packed));

/**
 * Flags byte format:
 * 7: P (Present) - Must be 1 for valid entry
 * 6-5: DPL (Descriptor Privilege Level) - Ring 0-3
 * 4: Storage Segment - Should be 0 for interrupt gates
 * 3-0: Gate Type
 *   - 0x5 = 32-bit Task Gate
 *   - 0xE = 32-bit Interrupt Gate (disables interrupts)
 *   - 0xF = 32-bit Trap Gate (doesn't disable interrupts)
 * 
 * Common flag values:
 * - 0x8E = 10001110 = Present, Ring 0, 32-bit Interrupt Gate
 * - 0xEE = 11101110 = Present, Ring 3, 32-bit Interrupt Gate
 */

/**
 * ==============================================================================
 * IDT Pointer Structure
 * ==============================================================================
 * Loaded into IDTR register using LIDT instruction
 */
struct idt_ptr {
    uint16_t limit;      /* Size of IDT - 1 */
    uint32_t base;       /* Address of first IDT entry */
} __attribute__((packed));

/**
 * ==============================================================================
 * Global Variables
 * ==============================================================================
 */

#define IDT_ENTRIES 256
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idt_pointer;

/**
 * ==============================================================================
 * External ISR Handlers from kernel_entry.asm
 * ==============================================================================
 * These are the assembly stub functions that save state and call our C handlers
 */
extern void isr0();   extern void isr1();   extern void isr2();   extern void isr3();
extern void isr4();   extern void isr5();   extern void isr6();   extern void isr7();
extern void isr8();   extern void isr9();   extern void isr10();  extern void isr11();
extern void isr12();  extern void isr13();  extern void isr14();  extern void isr15();
extern void isr16();  extern void isr17();  extern void isr18();  extern void isr19();
extern void isr20();  extern void isr21();  extern void isr22();  extern void isr23();
extern void isr24();  extern void isr25();  extern void isr26();  extern void isr27();
extern void isr28();  extern void isr29();  extern void isr30();  extern void isr31();

/* IRQ handlers (hardware interrupts remapped to 32-47) */
extern void irq0();   extern void irq1();   extern void irq2();   extern void irq3();
extern void irq4();   extern void irq5();   extern void irq6();   extern void irq7();
extern void irq8();   extern void irq9();   extern void irq10();  extern void irq11();
extern void irq12();  extern void irq13();  extern void irq14();  extern void irq15();

/**
 * ==============================================================================
 * Exception Messages
 * ==============================================================================
 * Human-readable descriptions of CPU exceptions
 * Reference: Intel Manual Vol 3A, Table 6-1
 */
static const char *exception_messages[] = {
    "Division By Zero",                 /* 0 */
    "Debug",                            /* 1 */
    "Non Maskable Interrupt",           /* 2 */
    "Breakpoint",                       /* 3 */
    "Into Detected Overflow",           /* 4 */
    "Out of Bounds",                    /* 5 */
    "Invalid Opcode",                   /* 6 */
    "No Coprocessor",                   /* 7 */
    "Double Fault",                     /* 8 */
    "Coprocessor Segment Overrun",      /* 9 */
    "Bad TSS",                          /* 10 */
    "Segment Not Present",              /* 11 */
    "Stack Fault",                      /* 12 */
    "General Protection Fault",         /* 13 */
    "Page Fault",                       /* 14 */
    "Unknown Interrupt",                /* 15 */
    "Coprocessor Fault",                /* 16 */
    "Alignment Check",                  /* 17 */
    "Machine Check",                    /* 18 */
    "SIMD Floating-Point Exception",    /* 19 */
    "Virtualization Exception",         /* 20 */
    "Reserved",                         /* 21 */
    "Reserved",                         /* 22 */
    "Reserved",                         /* 23 */
    "Reserved",                         /* 24 */
    "Reserved",                         /* 25 */
    "Reserved",                         /* 26 */
    "Reserved",                         /* 27 */
    "Reserved",                         /* 28 */
    "Reserved",                         /* 29 */
    "Security Exception",               /* 30 */
    "Reserved"                          /* 31 */
};

/**
 * ==============================================================================
 * Helper Function: Set IDT Entry
 * ==============================================================================
 */
static void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector,
                         uint8_t flags) {
    /* Set handler address */
    idt[num].base_low = base & 0xFFFF;         /* Lower 16 bits */
    idt[num].base_high = (base >> 16) & 0xFFFF; /* Upper 16 bits */
    
    /* Set segment selector (usually 0x08 = kernel code segment) */
    idt[num].selector = selector;
    
    /* Always 0 */
    idt[num].always0 = 0;
    
    /* Set type and attributes */
    idt[num].flags = flags;
}

/**
 * ==============================================================================
 * Public Function: Initialize IDT
 * ==============================================================================
 */
void idt_init(void) {
    /* Set up IDT pointer */
    idt_pointer.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idt_pointer.base = (uint32_t)&idt;
    
    /* Clear entire IDT to zeros */
    memset(&idt, 0, sizeof(struct idt_entry) * IDT_ENTRIES);
    
    /* Install ISR handlers (CPU exceptions 0-31) */
    /* 0x08 = kernel code segment, 0x8E = present, ring 0, 32-bit interrupt gate */
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);
    
    /* Install IRQ handlers (hardware interrupts 32-47) */
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);
    
    /* Load IDT using assembly function */
    idt_flush((uint32_t)&idt_pointer);
}

/**
 * ==============================================================================
 * ISR Handler (called from assembly stub)
 * ==============================================================================
 * This function is called by isr_common_stub in kernel_entry.asm
 * It receives a pointer to the saved register state
 */
void isr_handler(registers_t *regs) {
    /* Print exception information */
    screen_write("\n!!! CPU Exception !!!\n");
    screen_write("Exception: ");
    
    if (regs->int_no < 32) {
        screen_write(exception_messages[regs->int_no]);
    } else {
        screen_write("Unknown");
    }
    
    screen_write("\nInterrupt Number: ");
    screen_write_dec(regs->int_no);
    screen_write("\nError Code: ");
    screen_write_hex(regs->err_code);
    
    /* Print register dump for debugging */
    screen_write("\n\nRegister Dump:\n");
    screen_write("EAX="); screen_write_hex(regs->eax);
    screen_write(" EBX="); screen_write_hex(regs->ebx);
    screen_write(" ECX="); screen_write_hex(regs->ecx);
    screen_write(" EDX="); screen_write_hex(regs->edx);
    screen_write("\nESI="); screen_write_hex(regs->esi);
    screen_write(" EDI="); screen_write_hex(regs->edi);
    screen_write(" EBP="); screen_write_hex(regs->ebp);
    screen_write(" ESP="); screen_write_hex(regs->esp);
    screen_write("\nEIP="); screen_write_hex(regs->eip);
    screen_write(" CS="); screen_write_hex(regs->cs);
    screen_write(" DS="); screen_write_hex(regs->ds);
    screen_write(" EFLAGS="); screen_write_hex(regs->eflags);
    screen_write("\n\nSystem Halted.\n");
    
    /* Halt the system */
    for(;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

/**
 * ==============================================================================
 * IRQ Handler (called from assembly stub)
 * ==============================================================================
 * This function is called by irq_common_stub in kernel_entry.asm
 */

/* Array of function pointers for IRQ handlers */
static void (*irq_handlers[16])(registers_t *regs) = {0};

void irq_handler(registers_t *regs) {
    /* Send EOI (End of Interrupt) to PIC */
    /* If IRQ came from slave PIC (IRQ 8-15), send EOI to slave too */
    if (regs->int_no >= 40) {
        write_port(0xA0, 0x20);  /* Send EOI to slave PIC */
    }
    write_port(0x20, 0x20);      /* Send EOI to master PIC */
    
    /* Call registered handler if exists */
    int irq_num = regs->int_no - 32;  /* Convert to IRQ number (0-15) */
    if (irq_num >= 0 && irq_num < 16 && irq_handlers[irq_num]) {
        irq_handlers[irq_num](regs);
    }
}

/**
 * ==============================================================================
 * Register IRQ Handler
 * ==============================================================================
 * Allows other parts of the kernel to register handlers for specific IRQs
 * Example: keyboard driver registers handler for IRQ1
 */
void irq_register_handler(int irq, void (*handler)(registers_t *regs)) {
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = handler;
    }
}

/**
 * ==============================================================================
 * PIC Initialization
 * ==============================================================================
 * The PIC (Programmable Interrupt Controller) needs to be remapped because
 * its default IRQ mappings (0-15) conflict with CPU exceptions.
 * 
 * We remap:
 * - Master PIC (IRQ 0-7) → Interrupts 32-39
 * - Slave PIC (IRQ 8-15) → Interrupts 40-47
 * 
 * Reference: OSDev Wiki - PIC
 * https://wiki.osdev.org/PIC
 */
void pic_init(void) {
    /* ICW1 - Start initialization sequence (in cascade mode) */
    write_port(0x20, 0x11);  /* Master PIC command */
    write_port(0xA0, 0x11);  /* Slave PIC command */
    
    /* ICW2 - Remap IRQ base vectors */
    write_port(0x21, 0x20);  /* Master PIC offset to 32 */
    write_port(0xA1, 0x28);  /* Slave PIC offset to 40 */
    
    /* ICW3 - Set up cascading (slave PIC on IRQ2) */
    write_port(0x21, 0x04);  /* Tell master PIC there's a slave at IRQ2 (0000 0100) */
    write_port(0xA1, 0x02);  /* Tell slave PIC its cascade identity (0000 0010) */
    
    /* ICW4 - Set mode (8086 mode) */
    write_port(0x21, 0x01);  /* Master PIC: 8086 mode */
    write_port(0xA1, 0x01);  /* Slave PIC: 8086 mode */
    
    /* Clear interrupt masks (enable all IRQs) */
    write_port(0x21, 0x00);  /* Master PIC: unmask all */
    write_port(0xA1, 0x00);  /* Slave PIC: unmask all */
}
