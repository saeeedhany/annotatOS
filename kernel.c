/**
 * ==============================================================================
 * KERNEL.C - Main Kernel Entry Point and Initialization
 * ==============================================================================
 * This is the heart of our operating system. When control is transferred from
 * the bootloader to kernel_entry.asm, it eventually calls kernel_main() here.
 * 
 * Responsibilities:
 * 1. Initialize all subsystems (GDT, IDT, PIC, etc.)
 * 2. Set up memory management
 * 3. Enable multitasking
 * 4. Provide a simple shell/command interface
 * 
 * Reference: OSDev Wiki - Bare Bones
 * https://wiki.osdev.org/Bare_Bones
 * ==============================================================================
 */

#include "kernel.h"
#include "screen.h"
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "keyboard.h"
#include "memory.h"
#include "process.h"
#include "syscall.h"

/**
 * Main kernel entry point
 * Called from kernel_entry.asm after Protected Mode is set up
 */
void kernel_main(void) {
    /* Clear the screen and print welcome message */
    screen_clear();
    screen_write("MinimalOS v1.0 - Educational Operating System\n");
    screen_write("==============================================\n\n");
    
    /* Initialize GDT (Global Descriptor Table) */
    screen_write("[*] Initializing GDT... ");
    gdt_init();
    screen_write("OK\n");
    
    /* Initialize IDT (Interrupt Descriptor Table) */
    screen_write("[*] Initializing IDT... ");
    idt_init();
    screen_write("OK\n");
    
    /* Initialize PIC (Programmable Interrupt Controller) and remap IRQs */
    screen_write("[*] Initializing PIC... ");
    pic_init();
    screen_write("OK\n");
    
    /* Initialize PIT (Programmable Interval Timer) for system clock */
    screen_write("[*] Initializing Timer... ");
    timer_init(50);  /* 50 Hz = 20ms per tick */
    screen_write("OK\n");
    
    /* Initialize keyboard driver */
    screen_write("[*] Initializing Keyboard... ");
    keyboard_init();
    screen_write("OK\n");
    
    /* Initialize memory management */
    screen_write("[*] Initializing Memory Manager... ");
    memory_init();
    screen_write("OK\n");
    
    /* Initialize process/task management */
    screen_write("[*] Initializing Process Manager... ");
    process_init();
    screen_write("OK\n");
    
    /* Initialize system calls */
    screen_write("[*] Initializing System Calls... ");
    syscall_init();
    screen_write("OK\n");
    
    screen_write("\n[*] System initialized successfully!\n");
    screen_write("[*] Type 'help' for available commands\n\n");
    
    /* Enable interrupts - we're ready to handle them now */
    __asm__ __volatile__("sti");
    
    /* Enter the command shell */
    shell_run();
    
    /* If shell returns (shouldn't happen), hang */
    for(;;) {
        __asm__ __volatile__("hlt");
    }
}

/**
 * Simple command shell
 * Reads commands from keyboard and executes them
 */
void shell_run(void) {
    char command[256];
    int cmd_pos = 0;
    
    screen_write("kernel> ");
    
    while(1) {
        /* Wait for keyboard input */
        char c = keyboard_getchar();
        
        if (c == '\n') {
            /* Execute command */
            screen_putchar('\n');
            command[cmd_pos] = '\0';
            
            if (cmd_pos > 0) {
                shell_execute(command);
            }
            
            /* Reset for next command */
            cmd_pos = 0;
            screen_write("kernel> ");
        }
        else if (c == '\b') {
            /* Backspace */
            if (cmd_pos > 0) {
                cmd_pos--;
                screen_backspace();
            }
        }
        else if (c >= ' ' && c <= '~') {
            /* Printable character */
            if (cmd_pos < 255) {
                command[cmd_pos++] = c;
                screen_putchar(c);
            }
        }
    }
}

/**
 * Execute shell command
 */
void shell_execute(const char *command) {
    if (strcmp(command, "help") == 0) {
        screen_write("Available commands:\n");
        screen_write("  help       - Show this help message\n");
        screen_write("  clear      - Clear the screen\n");
        screen_write("  time       - Show system uptime\n");
        screen_write("  mem        - Show memory information\n");
        screen_write("  ps         - List running processes\n");
        screen_write("  test       - Run test process\n");
        screen_write("  syscall    - Test system call\n");
    }
    else if (strcmp(command, "clear") == 0) {
        screen_clear();
    }
    else if (strcmp(command, "time") == 0) {
        uint32_t ticks = timer_get_ticks();
        uint32_t seconds = ticks / 50;  /* 50 Hz timer */
        uint32_t minutes = seconds / 60;
        uint32_t hours = minutes / 60;
        
        screen_write("Uptime: ");
        screen_write_dec(hours);
        screen_putchar(':');
        screen_write_dec(minutes % 60);
        screen_putchar(':');
        screen_write_dec(seconds % 60);
        screen_putchar('\n');
    }
    else if (strcmp(command, "mem") == 0) {
        memory_info();
    }
    else if (strcmp(command, "ps") == 0) {
        process_list();
    }
    else if (strcmp(command, "test") == 0) {
        /* Create a test process */
        process_create(test_process, "test_process");
        screen_write("Test process created\n");
    }
    else if (strcmp(command, "syscall") == 0) {
        /* Test system call */
        syscall_test();
    }
    else {
        screen_write("Unknown command: ");
        screen_write(command);
        screen_write("\nType 'help' for available commands\n");
    }
}

/**
 * Test process for multitasking demonstration
 */
void test_process(void) {
    int count = 0;
    while(count < 10) {
        screen_write("Test process running... ");
        screen_write_dec(count++);
        screen_putchar('\n');
        
        /* Yield to scheduler */
        process_yield();
    }
    
    screen_write("Test process finished\n");
    process_exit();
}

/**
 * String comparison
 * Returns 0 if strings are equal
 */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/**
 * String length
 */
int strlen(const char *s) {
    int len = 0;
    while (s[len]) {
        len++;
    }
    return len;
}

/**
 * Memory copy
 */
void memcpy(void *dest, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    
    while (n--) {
        *d++ = *s++;
    }
}

/**
 * Memory set
 */
void memset(void *dest, uint8_t val, uint32_t n) {
    uint8_t *d = (uint8_t *)dest;
    
    while (n--) {
        *d++ = val;
    }
}
