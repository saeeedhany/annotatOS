/**
 * ==============================================================================
 * MEMORY.C - Simple Kernel Heap Allocator
 * ==============================================================================
 * Basic memory management using a bitmap allocator.
 * Reference: https://wiki.osdev.org/Memory_Management
 * ==============================================================================
 */

#include "memory.h"
#include "screen.h"

/* Memory block structure */
typedef struct mem_block {
    uint32_t size;
    int is_free;
    struct mem_block *next;
} mem_block_t;

static uint8_t *heap_start = (uint8_t*)KERNEL_HEAP_START;
static uint8_t *heap_end;
static mem_block_t *free_list = NULL;
static uint32_t total_allocated = 0;

void memory_init(void) {
    heap_end = heap_start + KERNEL_HEAP_SIZE;
    
    /* Initialize first free block */
    free_list = (mem_block_t*)heap_start;
    free_list->size = KERNEL_HEAP_SIZE - sizeof(mem_block_t);
    free_list->is_free = 1;
    free_list->next = NULL;
}

void *kmalloc(uint32_t size) {
    if (size == 0) return NULL;
    
    /* Align size to 4 bytes */
    size = (size + 3) & ~3;
    
    /* Find free block using first-fit algorithm */
    mem_block_t *current = free_list;
    while (current) {
        if (current->is_free && current->size >= size) {
            /* Found suitable block */
            current->is_free = 0;
            
            /* Split block if large enough */
            if (current->size > size + sizeof(mem_block_t) + 16) {
                mem_block_t *new_block = (mem_block_t*)((uint8_t*)current + 
                                          sizeof(mem_block_t) + size);
                new_block->size = current->size - size - sizeof(mem_block_t);
                new_block->is_free = 1;
                new_block->next = current->next;
                current->next = new_block;
                current->size = size;
            }
            
            total_allocated += current->size;
            return (void*)((uint8_t*)current + sizeof(mem_block_t));
        }
        current = current->next;
    }
    
    return NULL; /* Out of memory */
}

void kfree(void *ptr) {
    if (!ptr) return;
    
    mem_block_t *block = (mem_block_t*)((uint8_t*)ptr - sizeof(mem_block_t));
    block->is_free = 1;
    total_allocated -= block->size;
    
    /* Coalesce adjacent free blocks */
    mem_block_t *current = free_list;
    while (current && current->next) {
        if (current->is_free && current->next->is_free) {
            current->size += sizeof(mem_block_t) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

void memory_info(void) {
    screen_write("Memory Information:\n");
    screen_write("  Heap Start: "); screen_write_hex((uint32_t)heap_start); screen_putchar('\n');
    screen_write("  Heap Size: "); screen_write_dec(KERNEL_HEAP_SIZE); screen_write(" bytes\n");
    screen_write("  Allocated: "); screen_write_dec(total_allocated); screen_write(" bytes\n");
    screen_write("  Free: "); screen_write_dec(KERNEL_HEAP_SIZE - total_allocated); 
    screen_write(" bytes\n");
}
