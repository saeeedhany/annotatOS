/**
 * kernel.c - Minimal Kernel with Shell
 * Location: kernel/kernel.c
 */

/* VGA text mode buffer */
#define VGA_MEMORY 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* Types */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

/* Global variables */
static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;
static int cursor_x = 0;
static int cursor_y = 0;

/* Function declarations */
void kernel_main(void);
void clear_screen(void);
void print(const char* str);
void print_logo(void);
void shell_run(void);
int strcmp(const char* s1, const char* s2);

/**
 * Kernel entry point
 */
void kernel_main(void) {
    clear_screen();
    print_logo();
    print("\nMinimalOS v1.0 - Educational Operating System\n");
    print("Type 'help' for available commands\n\n");
    shell_run();

    /* Should never reach here */
    while(1) {
        __asm__ __volatile__("hlt");
    }
}

/**
 * Clear screen
 */
void clear_screen(void) {
    int i;
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (0x0F << 8) | ' ';  /* White on black */
    }
    cursor_x = 0;
    cursor_y = 0;
}

/**
 * Print string to screen
 */
void print(const char* str) {
    int i = 0;
    while (str[i]) {
        if (str[i] == '\n') {
            cursor_x = 0;
            cursor_y++;
        } else {
            int offset = cursor_y * VGA_WIDTH + cursor_x;
            vga_buffer[offset] = (0x0F << 8) | str[i];
            cursor_x++;
            if (cursor_x >= VGA_WIDTH) {
                cursor_x = 0;
                cursor_y++;
            }
        }
        if (cursor_y >= VGA_HEIGHT) {
            cursor_y = VGA_HEIGHT - 1;
        }
        i++;
    }
}

/**
 * Print ASCII logo
 */

void print_logo(void) {
    print("\n");
    print("  __  __ _       _                 _    ___  ____  \n");
    print(" |  \\/  (_)     (_)               | |  / _ \\/ ___| \n");
    print(" | |\\/| |_ _ __  _ _ __ ___   __ _| | | | | \\___ \\ \n");
    print(" | |  | | | '_ \\| | '_ ` _ \\ / _` | | | |_| |___) |\n");
    print(" |_|  |_|_|_| |_|_|_| |_| |_|\\__,_|_|  \\___/|____/ \n");
    print("                                                   \n");
}

/**
 * Print decimal number
 */
void print_number(int num) {
    if (num == 0) {
        print("0");
        return;
    }

    char buffer[12];
    int i = 0;
    int is_negative = 0;

    if (num < 0) {
        is_negative = 1;
        num = -num;
    }

    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }

    if (is_negative) {
        buffer[i++] = '-';
    }

    buffer[i] = '\0';

    /* Reverse the string */
    int j;
    for (j = 0; j < i / 2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i - 1 - j];
        buffer[i - 1 - j] = temp;
    }

    print(buffer);
}

/**
 * Simple string comparison
 */
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

/**
 * String length
 */
int strlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

/**
 * Simple shell - just displays prompt and waits
 * Note: No keyboard driver yet, so this is a demonstration
 */
void shell_run(void) {
    print("kernel> ");

    /* For now, execute a demo command since we don't have keyboard input yet */
    print("help\n");
    print("\nAvailable commands:\n");
    print("  help   - Show this help message\n");
    print("  clear  - Clear the screen\n");
    print("  about  - About MinimalOS\n");
    print("  halt   - Halt the system\n");
    print("\nNote: Keyboard input not implemented yet.\n");
    print("This is a minimal demonstration kernel.\n\n");

    print("System initialized successfully.\n");
    print("MinimalOS is running.\n\n");

    /* Halt peacefully */
    print("System halted. You can close QEMU now.\n");
}
