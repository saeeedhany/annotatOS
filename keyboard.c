/**
 * ==============================================================================
 * KEYBOARD.C - PS/2 Keyboard Driver  
 * ==============================================================================
 * Handles keyboard input via IRQ1. Uses scancode set 1.
 * Reference: https://wiki.osdev.org/PS/2_Keyboard
 * ==============================================================================
 */

#include "keyboard.h"
#include "idt.h"
#include "screen.h"

#define KB_DATA_PORT 0x60
#define KB_STATUS_PORT 0x64

/* Scancode to ASCII conversion table (US layout, lowercase) */
static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, /* Ctrl */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, /* Left Shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 
    0, /* Right Shift */
    '*',
    0, /* Alt */
    ' ' /* Space */
};

/* Shifted scancodes */
static const char scancode_to_ascii_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' '
};

static volatile char key_buffer[256];
static volatile int buffer_start = 0;
static volatile int buffer_end = 0;
static int shift_pressed = 0;

/* Keyboard IRQ handler */
static void keyboard_handler(registers_t *regs) {
    (void)regs;
    uint8_t scancode = read_port(KB_DATA_PORT);
    
    /* Handle shift keys */
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return;
    }
    
    /* Only handle key press (not release) */
    if (scancode & 0x80) return;
    
    /* Convert scancode to ASCII */
    char ascii = 0;
    if (scancode < sizeof(scancode_to_ascii)) {
        ascii = shift_pressed ? scancode_to_ascii_shift[scancode] : 
                                scancode_to_ascii[scancode];
    }
    
    /* Add to buffer if valid */
    if (ascii) {
        key_buffer[buffer_end] = ascii;
        buffer_end = (buffer_end + 1) % 256;
    }
}

void keyboard_init(void) {
    irq_register_handler(1, keyboard_handler);
}

char keyboard_getchar(void) {
    /* Wait for character in buffer */
    while (buffer_start == buffer_end) {
        __asm__ __volatile__("hlt");
    }
    
    char c = key_buffer[buffer_start];
    buffer_start = (buffer_start + 1) % 256;
    return c;
}
