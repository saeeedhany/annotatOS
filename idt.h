/**
 * ==============================================================================
 * IDT.H - Interrupt Descriptor Table Header
 * ==============================================================================
 */

#ifndef IDT_H
#define IDT_H

#include "kernel.h"

/* Registers structure passed to interrupt handlers */
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

/* Initialize IDT */
void idt_init(void);

/* Initialize and remap PIC */
void pic_init(void);

/* Register custom IRQ handler */
void irq_register_handler(int irq, void (*handler)(registers_t *regs));

#endif /* IDT_H */
