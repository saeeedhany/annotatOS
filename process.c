/**
 * ==============================================================================
 * PROCESS.C - Cooperative Multitasking / Process Management
 * ==============================================================================
 * Simple round-robin scheduler with cooperative multitasking.
 * Reference: https://wiki.osdev.org/Multitasking
 * ==============================================================================
 */

#include "process.h"
#include "memory.h"
#include "screen.h"

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_TERMINATED
} process_state_t;

typedef struct process {
    uint32_t id;
    char name[32];
    process_state_t state;
    uint32_t esp;  /* Saved stack pointer */
    uint32_t ebp;  /* Saved base pointer */
    uint8_t *stack;
    struct process *next;
} process_t;

static process_t *process_queue = NULL;
static process_t *current_process = NULL;
static uint32_t next_pid = 1;

void process_init(void) {
    /* Create idle process (kernel) */
    process_queue = NULL;
    current_process = NULL;
}

int process_create(void (*entry)(void), const char *name) {
    /* Allocate process structure */
    process_t *proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) return -1;
    
    /* Allocate stack */
    proc->stack = (uint8_t*)kmalloc(PROCESS_STACK_SIZE);
    if (!proc->stack) {
        kfree(proc);
        return -1;
    }
    
    /* Initialize process */
    proc->id = next_pid++;
    int i;
    for (i = 0; i < 31 && name[i]; i++) {
        proc->name[i] = name[i];
    }
    proc->name[i] = '\0';
    
    proc->state = PROCESS_READY;
    
    /* Set up stack for first run */
    uint32_t *stack_top = (uint32_t*)(proc->stack + PROCESS_STACK_SIZE);
    stack_top--; /* Return address (entry point) */
    *stack_top = (uint32_t)entry;
    proc->esp = (uint32_t)stack_top;
    proc->ebp = proc->esp;
    
    /* Add to process list */
    proc->next = process_queue;
    process_queue = proc;
    
    return proc->id;
}

void process_yield(void) {
    /* Simple yield - in real OS would save context and switch */
    __asm__ __volatile__("hlt");
}

void process_exit(void) {
    if (current_process) {
        current_process->state = PROCESS_TERMINATED;
        screen_write("Process exited\n");
    }
}

void process_list_processes(void) {
    screen_write("PID  State    Name\n");
    screen_write("---  -------  ----\n");
    
    process_t *proc = process_queue;
    while (proc) {
        screen_write_dec(proc->id);
        screen_write("    ");
        
        switch(proc->state) {
            case PROCESS_READY: screen_write("READY   "); break;
            case PROCESS_RUNNING: screen_write("RUNNING "); break;
            case PROCESS_TERMINATED: screen_write("TERM    "); break;
        }
        
        screen_write(proc->name);
        screen_putchar('\n');
        proc = proc->next;
    }
}

void process_list(void) {
    process_list_processes();
}
