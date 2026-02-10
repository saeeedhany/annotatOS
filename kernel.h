/**
 * ==============================================================================
 * KERNEL.H - Main Kernel Header File
 * ==============================================================================
 * This header defines all the basic types, constants, and function prototypes
 * used throughout the kernel.
 * 
 * Reference: OSDev Wiki - C
 * https://wiki.osdev.org/C
 * ==============================================================================
 */

#ifndef KERNEL_H
#define KERNEL_H

/**
 * ==============================================================================
 * Standard Type Definitions
 * ==============================================================================
 * We define our own types because we're in a freestanding environment
 * (no standard library). We need exact-width integer types.
 * 
 * Reference: C99 Standard - 7.18 Integer types
 */

/* Unsigned integer types */
typedef unsigned char      uint8_t;   /* 8-bit unsigned (0 to 255) */
typedef unsigned short     uint16_t;  /* 16-bit unsigned (0 to 65,535) */
typedef unsigned int       uint32_t;  /* 32-bit unsigned (0 to 4,294,967,295) */
typedef unsigned long long uint64_t;  /* 64-bit unsigned */

/* Signed integer types */
typedef signed char        int8_t;    /* 8-bit signed (-128 to 127) */
typedef signed short       int16_t;   /* 16-bit signed */
typedef signed int         int32_t;   /* 32-bit signed */
typedef signed long long   int64_t;   /* 64-bit signed */

/* Other useful types */
typedef uint32_t           size_t;    /* Size type (for sizeof, memory operations) */
typedef uint32_t           uintptr_t; /* Pointer-sized unsigned integer */

/* Boolean type (C99 compatible) */
typedef int bool;
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

/* NULL pointer */
#define NULL ((void*)0)

/**
 * ==============================================================================
 * Kernel Constants
 * ==============================================================================
 */

/* Memory constants */
#define PAGE_SIZE           4096      /* 4KB pages (standard x86 page size) */
#define KERNEL_HEAP_START   0x100000  /* 1MB - Start of kernel heap */
#define KERNEL_HEAP_SIZE    0x100000  /* 1MB heap size */

/* Process constants */
#define MAX_PROCESSES       32        /* Maximum number of processes */
#define PROCESS_STACK_SIZE  4096      /* 4KB stack per process */

/**
 * ==============================================================================
 * Function Prototypes
 * ==============================================================================
 */

/* Main kernel functions (kernel.c) */
void kernel_main(void);
void shell_run(void);
void shell_execute(const char *command);
void test_process(void);

/* String functions (kernel.c) */
int strcmp(const char *s1, const char *s2);
int strlen(const char *s);
void memcpy(void *dest, const void *src, uint32_t n);
void memset(void *dest, uint8_t val, uint32_t n);

/**
 * ==============================================================================
 * Assembly Functions
 * ==============================================================================
 * These are implemented in kernel_entry.asm and called from C
 */

/* Port I/O functions */
extern uint8_t read_port(uint16_t port);
extern void write_port(uint16_t port, uint8_t data);

/* GDT/IDT functions */
extern void gdt_flush(uint32_t gdt_ptr);
extern void idt_flush(uint32_t idt_ptr);

/**
 * ==============================================================================
 * Inline Assembly Helpers
 * ==============================================================================
 */

/**
 * Halt the CPU
 * Stops execution until next interrupt
 */
static inline void cpu_halt(void) {
    __asm__ __volatile__("hlt");
}

/**
 * Enable interrupts
 * Sets the IF (Interrupt Flag) in EFLAGS
 */
static inline void enable_interrupts(void) {
    __asm__ __volatile__("sti");
}

/**
 * Disable interrupts
 * Clears the IF (Interrupt Flag) in EFLAGS
 */
static inline void disable_interrupts(void) {
    __asm__ __volatile__("cli");
}

/**
 * Read from I/O port (8-bit)
 * Alternative to calling assembly read_port function
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ __volatile__("in %%dx, %%al" : "=a"(result) : "d"(port));
    return result;
}

/**
 * Write to I/O port (8-bit)
 * Alternative to calling assembly write_port function
 */
static inline void outb(uint16_t port, uint8_t data) {
    __asm__ __volatile__("out %%al, %%dx" : : "a"(data), "d"(port));
}

/**
 * Short delay for I/O operations
 * Some hardware needs a small delay between operations
 */
static inline void io_wait(void) {
    /* Port 0x80 is used for 'checkpoints' during POST */
    /* Writing to this port takes long enough for a delay */
    __asm__ __volatile__("outb %%al, $0x80" : : "a"(0));
}

#endif /* KERNEL_H */
