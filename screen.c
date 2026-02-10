/**
 * ==============================================================================
 * SCREEN.C - VGA Text Mode Driver
 * ==============================================================================
 * This driver handles text output to the VGA text mode buffer.
 * 
 * VGA Text Mode Memory Layout:
 * - Base address: 0xB8000
 * - 80 columns x 25 rows = 2000 characters
 * - Each character: 2 bytes [character][attribute]
 * - Attribute byte format: [Blink][BG Color][FG Color]
 *   - Bits 0-3: Foreground color
 *   - Bits 4-6: Background color
 *   - Bit 7: Blink enable
 * 
 * Reference: OSDev Wiki - Text Mode Cursor
 * https://wiki.osdev.org/Text_Mode_Cursor
 * https://wiki.osdev.org/Printing_To_Screen
 * ==============================================================================
 */

#include "screen.h"
#include "kernel.h"

/**
 * ==============================================================================
 * Constants
 * ==============================================================================
 */

#define VIDEO_MEMORY    0xB8000     /* Base address of VGA text buffer */
#define MAX_ROWS        25          /* Screen height */
#define MAX_COLS        80          /* Screen width */

/* VGA color codes */
#define BLACK           0
#define BLUE            1
#define GREEN           2
#define CYAN            3
#define RED             4
#define MAGENTA         5
#define BROWN           6
#define LIGHT_GREY      7
#define DARK_GREY       8
#define LIGHT_BLUE      9
#define LIGHT_GREEN     10
#define LIGHT_CYAN      11
#define LIGHT_RED       12
#define LIGHT_MAGENTA   13
#define YELLOW          14
#define WHITE           15

/* Default color scheme: White text on black background */
#define DEFAULT_COLOR   (WHITE | (BLACK << 4))

/* VGA CRT Controller ports */
/* Reference: http://www.osdever.net/FreeVGA/vga/crtcreg.htm */
#define VGA_CTRL_REGISTER   0x3D4   /* CRT Controller Index Register */
#define VGA_DATA_REGISTER   0x3D5   /* CRT Controller Data Register */

/* CRT Controller register indices */
#define VGA_CURSOR_LOC_HIGH 14      /* Cursor Location High byte */
#define VGA_CURSOR_LOC_LOW  15      /* Cursor Location Low byte */

/**
 * ==============================================================================
 * Global Variables
 * ==============================================================================
 */

static uint16_t *video_memory = (uint16_t *)VIDEO_MEMORY;
static int cursor_row = 0;      /* Current cursor row (0-24) */
static int cursor_col = 0;      /* Current cursor column (0-79) */
static uint8_t current_color = DEFAULT_COLOR;

/**
 * ==============================================================================
 * Private Helper Functions
 * ==============================================================================
 */

/**
 * Get linear offset from row and column
 * Formula: offset = row * MAX_COLS + col
 */
static int get_offset(int row, int col) {
    return row * MAX_COLS + col;
}

/**
 * Update hardware cursor position
 * The VGA hardware has a cursor that we need to update separately
 */
static void update_cursor(void) {
    /* Calculate linear position */
    uint16_t position = get_offset(cursor_row, cursor_col);
    
    /* Send high byte to VGA controller */
    /* We write the register index to port 0x3D4, then data to 0x3D5 */
    write_port(VGA_CTRL_REGISTER, VGA_CURSOR_LOC_HIGH);
    write_port(VGA_DATA_REGISTER, (position >> 8) & 0xFF);
    
    /* Send low byte to VGA controller */
    write_port(VGA_CTRL_REGISTER, VGA_CURSOR_LOC_LOW);
    write_port(VGA_DATA_REGISTER, position & 0xFF);
}

/**
 * Scroll screen up by one line
 * Called when text reaches the bottom of the screen
 */
static void scroll_up(void) {
    int i;
    
    /* Copy each row to the row above it */
    /* Start from row 1 (not row 0) and copy to row 0, etc. */
    for (i = 0; i < (MAX_ROWS - 1) * MAX_COLS; i++) {
        video_memory[i] = video_memory[i + MAX_COLS];
    }
    
    /* Clear the last row */
    for (i = (MAX_ROWS - 1) * MAX_COLS; i < MAX_ROWS * MAX_COLS; i++) {
        video_memory[i] = (current_color << 8) | ' ';
    }
    
    /* Move cursor to last row */
    cursor_row = MAX_ROWS - 1;
    cursor_col = 0;
}

/**
 * ==============================================================================
 * Public Interface Functions
 * ==============================================================================
 */

/**
 * Clear entire screen
 * Fills screen with spaces using current color attribute
 */
void screen_clear(void) {
    int i;
    
    /* Fill entire screen with spaces */
    for (i = 0; i < MAX_ROWS * MAX_COLS; i++) {
        /* Each uint16_t is [attribute][character] */
        /* We set character to space (0x20) and use current color */
        video_memory[i] = (current_color << 8) | ' ';
    }
    
    /* Reset cursor to top-left */
    cursor_row = 0;
    cursor_col = 0;
    update_cursor();
}

/**
 * Write a single character at current cursor position
 * Handles special characters: \n (newline), \r (carriage return), \b (backspace)
 */
void screen_putchar(char c) {
    int offset;
    
    /* Handle special characters */
    switch (c) {
        case '\n':  /* Newline */
            cursor_row++;
            cursor_col = 0;
            break;
            
        case '\r':  /* Carriage return */
            cursor_col = 0;
            break;
            
        case '\b':  /* Backspace */
            if (cursor_col > 0) {
                cursor_col--;
                /* Clear the character */
                offset = get_offset(cursor_row, cursor_col);
                video_memory[offset] = (current_color << 8) | ' ';
            }
            break;
            
        case '\t':  /* Tab (8 spaces) */
            cursor_col = (cursor_col + 8) & ~7;  /* Round up to next multiple of 8 */
            if (cursor_col >= MAX_COLS) {
                cursor_col = 0;
                cursor_row++;
            }
            break;
            
        default:    /* Printable character */
            offset = get_offset(cursor_row, cursor_col);
            video_memory[offset] = (current_color << 8) | c;
            cursor_col++;
            break;
    }
    
    /* Check if we need to wrap to next line */
    if (cursor_col >= MAX_COLS) {
        cursor_col = 0;
        cursor_row++;
    }
    
    /* Check if we need to scroll */
    if (cursor_row >= MAX_ROWS) {
        scroll_up();
    }
    
    /* Update hardware cursor */
    update_cursor();
}

/**
 * Write null-terminated string to screen
 */
void screen_write(const char *str) {
    while (*str) {
        screen_putchar(*str);
        str++;
    }
}

/**
 * Write string with specified length
 */
void screen_write_len(const char *str, int len) {
    int i;
    for (i = 0; i < len; i++) {
        screen_putchar(str[i]);
    }
}

/**
 * Write hexadecimal number
 * Example: 0x1234ABCD
 */
void screen_write_hex(uint32_t n) {
    char hex_chars[] = "0123456789ABCDEF";
    int i;
    
    screen_write("0x");
    
    /* Print 8 hex digits (32 bits / 4 bits per digit) */
    for (i = 28; i >= 0; i -= 4) {
        screen_putchar(hex_chars[(n >> i) & 0xF]);
    }
}

/**
 * Write decimal number
 * Example: 12345
 */
void screen_write_dec(uint32_t n) {
    char buffer[12];  /* Max uint32_t is 10 digits + sign + null */
    int i = 0;
    
    /* Handle zero specially */
    if (n == 0) {
        screen_putchar('0');
        return;
    }
    
    /* Convert to string in reverse */
    while (n > 0) {
        buffer[i++] = '0' + (n % 10);
        n /= 10;
    }
    
    /* Print in correct order */
    while (i > 0) {
        screen_putchar(buffer[--i]);
    }
}

/**
 * Backspace operation
 * Moves cursor back and clears character
 */
void screen_backspace(void) {
    screen_putchar('\b');
}

/**
 * Set text color
 * fg: Foreground color (0-15)
 * bg: Background color (0-15)
 */
void screen_set_color(uint8_t fg, uint8_t bg) {
    current_color = fg | (bg << 4);
}

/**
 * Get current cursor position
 */
void screen_get_cursor(int *row, int *col) {
    if (row) *row = cursor_row;
    if (col) *col = cursor_col;
}

/**
 * Set cursor position
 */
void screen_set_cursor(int row, int col) {
    if (row >= 0 && row < MAX_ROWS) {
        cursor_row = row;
    }
    if (col >= 0 && col < MAX_COLS) {
        cursor_col = col;
    }
    update_cursor();
}
