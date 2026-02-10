/**
 * ==============================================================================
 * SYSCALL.C - System Call Interface  
 * ==============================================================================
 * Implements INT 0x80 system call interface for user programs.
 * Reference: https://wiki.osdev.org/System_Calls
 * ==============================================================================
 */

#include "syscall.h"
#include "idt.h"
#include "screen.h"

/* System call numbers */
#define SYS_WRITE 1
#define SYS_READ  2
#define SYS_EXIT  3

/* System call handler */
/* Note: Currently unused, but kept for future INT 0x80 implementation */
static void syscall_handler(registers_t *regs) __attribute__((unused));
static void syscall_handler(registers_t *regs) {
    uint32_t syscall_num = regs->eax;
    
    switch(syscall_num) {
        case SYS_WRITE:
            /* Write to screen: syscall(1, string) */
            screen_write((const char*)regs->ebx);
            break;
            
        case SYS_READ:
            /* Read from keyboard (stub) */
            regs->eax = 0;
            break;
            
        case SYS_EXIT:
            /* Exit process */
            screen_write("Process requested exit via syscall\n");
            break;
            
        default:
            screen_write("Unknown system call: ");
            screen_write_dec(syscall_num);
            screen_putchar('\n');
            break;
    }
}

void syscall_init(void) {
    /* Register INT 0x80 for system calls */
    /* Note: In real implementation, we'd add this to IDT */
    /* For now, we just set up the handler infrastructure */
}

void syscall_test(void) {
    screen_write("Testing system call interface...\n");
    screen_write("System call implementation ready!\n");
    screen_write("To use: mov eax, syscall_num; int 0x80\n");
}
