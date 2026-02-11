/**
 * SYSTEM-LEVEL OVERVIEW
 *
 * This translation unit implements the runtime core of a tiny monolithic
 * kernel executing in 16-bit x86 real mode. The code directly manipulates
 * hardware-visible interfaces (VGA text memory and legacy PS/2 controller
 * ports) without firmware mediation once control leaves BIOS boot services.
 *
 * Boot-time behavior (as seen from this file):
 * 1) `kernel_main` is entered from `kernel_entry.asm` with flat real-mode
 *    segments (base 0) and a pre-positioned stack.
 * 2) Screen memory is cleared, a banner is printed, and shell loop starts.
 *
 * Runtime behavior:
 * 1) Poll keyboard controller status/data ports for Set-1 make codes.
 * 2) Translate scancodes into ASCII subset.
 * 3) Mutate in-memory command buffer and VGA memory for TTY-like interaction.
 * 4) Dispatch built-in commands and return to prompt indefinitely.
 *
 * Memory behavior and data layout:
 * - `vga_buffer` maps physical 0xB8000 where each cell is 16 bits:
 *   [attribute byte | ASCII byte].
 * - `cursor_x`/`cursor_y` are global scalar state in `.data` or `.bss`.
 * - `command_buffer` is a fixed-size stack array in `shell_run`; lifetime is
 *   per-loop-iteration and capacity is bounded by COMMAND_BUFFER_SIZE.
 * - No allocator, paging, virtual memory, or process isolation exists.
 *
 * CPU-level implications:
 * - Port I/O uses IN/OUT instructions (`inb`, `outw`) and therefore requires
 *   ring0-like unrestricted execution (naturally true in real mode).
 * - Busy-wait keyboard polling consumes CPU cycles continuously; there is no
 *   interrupt-driven sleep path for input readiness.
 * - `hlt` is used only in terminal states; main shell loop is active polling.
 *
 * Data structures:
 * - VGA text buffer: conceptual 2D matrix [25 rows][80 cols], stored linearly
 *   as 2000 contiguous uint16_t entries in row-major order.
 * - Command parser: null-terminated byte string in a 64-byte local array.
 *
 * Limitations and edge cases:
 * - No Shift/Ctrl state tracking; keyboard mapping is lowercase subset only.
 * - Backspace is line-local and does not traverse to previous lines.
 * - String ops are minimal (`strcmp` only) and assume trusted in-kernel data.
 * - Poweroff ports are emulator-specific and may not work on all machines.
 * - Shell loop has no timeout or cooperative scheduling.
 *
 * Reference hints:
 * - VGA text memory map: IBM VGA-compatible adapters (mode 03h semantics).
 * - Keyboard controller ports 0x64/0x60: classic i8042-compatible interface.
 */

/* VGA text mode memory base address (physical memory). */
#define VGA_MEMORY 0xB8000

/* Classic text mode dimensions. */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* PS/2 keyboard controller I/O ports. */
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_DATA_PORT 0x60

/* Shell command buffer size (characters per input line). */
#define COMMAND_BUFFER_SIZE 64

/* Basic fixed-width integer types (no libc available in freestanding kernel). */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

/* VGA buffer pointer. Each cell = [color:8 bits][ASCII char:8 bits]. */
static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;

/* Cursor location in text mode coordinates. */
static int cursor_x = 0;
static int cursor_y = 0;

/* -------------------------------------------------------------------------- */
/* Low-level I/O helpers                                                      */
/* -------------------------------------------------------------------------- */

/**
 * Read one byte from an I/O port.
 */
static uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/**
 * Write one 16-bit word to an I/O port.
 */
static void outw(uint16_t port, uint16_t value) {
    __asm__ __volatile__("outw %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * Halt the CPU forever.
 * Used when we want to stop execution safely.
 */
static void halt_forever(void) {
    while (1) {
        __asm__ __volatile__("hlt");
    }
}

/**
 * Ask QEMU/Bochs-compatible power management ports to power off the VM.
 * If unsupported, execution falls back to halting forever.
 */
static void qemu_poweroff(void) {
    outw(0x604, 0x2000);  /* QEMU ACPI poweroff (common on i440fx machine). */
    outw(0xB004, 0x2000); /* Bochs/older compatibility port. */
    halt_forever();
}

/* -------------------------------------------------------------------------- */
/* Screen output                                                              */
/* -------------------------------------------------------------------------- */

/**
 * Scroll the screen up by one row when cursor reaches the bottom.
 */
static void scroll_if_needed(void) {
    if (cursor_y < VGA_HEIGHT) {
        return;
    }

    int row;
    int col;

    /* Move each row up by one. */
    for (row = 1; row < VGA_HEIGHT; row++) {
        for (col = 0; col < VGA_WIDTH; col++) {
            vga_buffer[(row - 1) * VGA_WIDTH + col] = vga_buffer[row * VGA_WIDTH + col];
        }
    }

    /* Clear last row after shifting content upward. */
    for (col = 0; col < VGA_WIDTH; col++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = (0x0F << 8) | ' ';
    }

    cursor_y = VGA_HEIGHT - 1;
}

/**
 * Move to a new line (column 0 of next row), then scroll if needed.
 */
static void newline(void) {
    cursor_x = 0;
    cursor_y++;
    scroll_if_needed();
}

/**
 * Print one character at the current cursor position.
 */
static void put_char(char c) {
    if (c == '\n') {
        newline();
        return;
    }

    vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = (0x0F << 8) | (uint8_t)c;
    cursor_x++;

    if (cursor_x >= VGA_WIDTH) {
        newline();
    }
}

/**
 * Erase one character from the current line (used for backspace handling).
 */
static void backspace_char(void) {
    if (cursor_x == 0) {
        return;
    }

    cursor_x--;
    vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = (0x0F << 8) | ' ';
}

/**
 * Print a null-terminated string to the VGA text console.
 */
void print(const char* str) {
    int i = 0;
    while (str[i]) {
        put_char(str[i]);
        i++;
    }
}

/**
 * Clear the entire text screen and reset cursor to top-left corner.
 */
void clear_screen(void) {
    int i;
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (0x0F << 8) | ' ';
    }
    cursor_x = 0;
    cursor_y = 0;
}

/* -------------------------------------------------------------------------- */
/* String helpers (self-contained replacements for libc).                     */
/* -------------------------------------------------------------------------- */

/**
 * Compare two strings; return 0 if equal.
 */
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)(*s1) - (int)(*s2);
}

/* -------------------------------------------------------------------------- */
/* Keyboard input                                                             */
/* -------------------------------------------------------------------------- */

/**
 * Translate a Set-1 keyboard scancode (press event) into a lowercase ASCII
 * character. Returns 0 for unsupported keys.
 *
 * This mapping intentionally includes only commonly used characters for this
 * educational shell to keep the code focused and readable.
 */
static char scancode_to_ascii(uint8_t scancode) {
    switch (scancode) {
        case 0x02: return '1'; case 0x03: return '2'; case 0x04: return '3';
        case 0x05: return '4'; case 0x06: return '5'; case 0x07: return '6';
        case 0x08: return '7'; case 0x09: return '8'; case 0x0A: return '9';
        case 0x0B: return '0';

        case 0x10: return 'q'; case 0x11: return 'w'; case 0x12: return 'e';
        case 0x13: return 'r'; case 0x14: return 't'; case 0x15: return 'y';
        case 0x16: return 'u'; case 0x17: return 'i'; case 0x18: return 'o';
        case 0x19: return 'p'; case 0x1E: return 'a'; case 0x1F: return 's';
        case 0x20: return 'd'; case 0x21: return 'f'; case 0x22: return 'g';
        case 0x23: return 'h'; case 0x24: return 'j'; case 0x25: return 'k';
        case 0x26: return 'l'; case 0x2C: return 'z'; case 0x2D: return 'x';
        case 0x2E: return 'c'; case 0x2F: return 'v'; case 0x30: return 'b';
        case 0x31: return 'n'; case 0x32: return 'm';

        case 0x39: return ' ';  /* Space bar */
        case 0x0C: return '-';
        case 0x0D: return '=';
        default: return 0;
    }
}

/**
 * Block until a key press event is available, then return its scancode.
 *
 * Notes:
 * - Status port bit 0 indicates output buffer full (data ready).
 * - We ignore key-release scancodes (high bit set) and wait for key press.
 */
static uint8_t keyboard_read_keypress_scancode(void) {
    while (1) {
        if ((inb(KEYBOARD_STATUS_PORT) & 0x01) == 0) {
            continue;
        }

        uint8_t scancode = inb(KEYBOARD_DATA_PORT);

        /* Ignore key release events (0x80..0xFF). */
        if (scancode & 0x80) {
            continue;
        }

        return scancode;
    }
}

/* -------------------------------------------------------------------------- */
/* Shell commands                                                             */
/* -------------------------------------------------------------------------- */

/**
 * Print available shell commands.
 */
static void command_help(void) {
    print("Available commands:\n");
    print("  help  - Show available commands\n");
    print("  about - Show OS description, features, and purpose\n");
    print("  clear - Clear the screen\n");
    print("  exit  - Exit QEMU\n");
}

/**
 * Print educational OS description.
 */
static void command_about(void) {
    print("AnnotatOS - Educational Operating System\n");
    print("Description:\n");
    print("  A tiny OS that boots from BIOS and runs a text shell.\n");
    print("Features:\n");
    print("  - BIOS bootloader that loads a freestanding C kernel\n");
    print("  - VGA text-mode output\n");
    print("  - PS/2 keyboard input polling\n");
    print("  - Interactive shell with basic commands\n");
    print("Purpose:\n");
    print("  Teach core OS-building ideas from scratch in readable code.\n");
}

/**
 * Execute one shell command line.
 */
static void shell_execute_command(const char* command) {
    if (strcmp(command, "help") == 0) {
        command_help();
        return;
    }

    if (strcmp(command, "about") == 0) {
        command_about();
        return;
    }

    if (strcmp(command, "clear") == 0) {
        clear_screen();
        return;
    }

    if (strcmp(command, "exit") == 0) {
        print("Exiting QEMU...\n");
        qemu_poweroff();
        return;
    }

    if (command[0] == '\0') {
        return; /* Empty command: do nothing. */
    }

    print("Unknown command: ");
    print(command);
    print("\nType 'help' to list commands.\n");
}

/**
 * Run the interactive keyboard shell forever.
 */
void shell_run(void) {
    /*
     * Stack-resident command line buffer. Layout is contiguous bytes and always
     * kept NUL-terminated after each edit to make strcmp dispatch safe.
     */
    char command_buffer[COMMAND_BUFFER_SIZE];

    while (1) {
        int index = 0;
        command_buffer[0] = '\0';

        print("kernel> ");

        while (1) {
            uint8_t scancode = keyboard_read_keypress_scancode();

            /* Enter key finalizes the command line. */
            if (scancode == 0x1C) {
                put_char('\n');
                command_buffer[index] = '\0';
                shell_execute_command(command_buffer);
                print("\n");
                break;
            }

            /* Backspace deletes one character from both buffer and screen. */
            if (scancode == 0x0E) {
                if (index > 0) {
                    index--;
                    command_buffer[index] = '\0';
                    backspace_char();
                }
                continue;
            }

            /* Translate printable keys. */
            char c = scancode_to_ascii(scancode);
            if (c == 0) {
                continue;
            }

            /* Append char if buffer still has room (reserve space for NUL). */
            if (index < COMMAND_BUFFER_SIZE - 1) {
                command_buffer[index++] = c;
                command_buffer[index] = '\0';
                put_char(c); /* Echo typed character. */
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Boot banner and kernel entry                                               */
/* -------------------------------------------------------------------------- */

/**
 * Print the project ASCII logo.
 */
void print_logo(void) {
    print("\n");
    print("    _                              _       ___  ____  \n");
    print("   / \\   _ __  _ __   ___  _ __ _| |_    / _ \\/ ___| \n");
    print("  / _ \\ | '_ \\| '_ \\ / _ \\| '__| __|  | | | \\___ \\ \n");
    print(" / ___ \\| | | | | | | (_) | |  | |_   | |_| |___) |\n");
    print("/_/   \\_\\_| |_|_| |_|\\___/|_|   \\__|   \\___/|____/ \n");
    print("                    AnnotatOS                      \n");
}

/**
 * Kernel entry point called from kernel_entry.asm.
 */
void kernel_main(void) {
    clear_screen();
    print_logo();
    print("\nAnnotatOS v1.1 - Interactive Educational Operating System\n");
    print("Type 'help' to see commands.\n\n");
    shell_run();

    /* Defensive fallback: shell_run is an infinite loop, but keep safe halt. */
    halt_forever();
}
