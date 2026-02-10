# 🚀 GETTING STARTED - MinimalOS Development Guide

## Quick Start (For the Impatient)

```bash
# Build and run
make run

# That's it! Your OS should boot in QEMU
```

## Complete Setup Guide

### 1. Verify Your Environment

Make sure you have all required tools:

```bash
# Check NASM (assembler)
nasm -version
# Expected: NASM version 2.15.x or higher

# Check GCC (C compiler)
gcc --version
# Expected: gcc (GCC) 11.x or higher

# Check Make
make --version
# Expected: GNU Make 4.x or higher

# Check QEMU
qemu-system-i386 --version
# Expected: QEMU emulator version 6.x or higher
```

If any are missing:
```bash
# Arch Linux
sudo pacman -S nasm gcc make qemu-system-x86 gdb

# Ubuntu/Debian
sudo apt install nasm gcc make qemu-system-x86 gdb

# Fedora
sudo dnf install nasm gcc make qemu-system-x86 gdb
```

### 2. Understand the Project Structure

```
MinimalOS/
├── boot.asm          ← Stage 1 bootloader (START HERE)
├── stage2.asm        ← Stage 2 bootloader
├── kernel_entry.asm  ← Kernel entry (assembly→C bridge)
├── kernel.c/h        ← Main kernel code
├── gdt.c/h          ← Global Descriptor Table
├── idt.c/h          ← Interrupt Descriptor Table  
├── screen.c/h       ← VGA text driver
├── keyboard.c/h     ← Keyboard driver
├── timer.c/h        ← Timer driver
├── memory.c/h       ← Memory manager
├── process.c/h      ← Process manager
├── syscall.c/h      ← System calls
├── linker.ld        ← Linker script
├── Makefile         ← Build system
└── README.md        ← This file
```

### 3. Build the OS

```bash
# Clean any previous builds
make clean

# Build everything
make

# This creates:
# - boot.bin (512 bytes, Stage 1 bootloader)
# - stage2.bin (7.5 KB, Stage 2 bootloader)  
# - kernel.bin (kernel code)
# - os.img (10 MB bootable disk image)
```

### 4. Run in QEMU

```bash
# Run the OS
make run

# You should see:
# 1. BIOS boot messages
# 2. "Loading Stage 2..." 
# 3. "Stage 2 loaded!"
# 4. System initialization messages
# 5. Interactive shell prompt: "kernel>"
```

### 5. Explore the Shell

Try these commands:
```
kernel> help          # Show all commands
kernel> time          # System uptime
kernel> mem           # Memory info
kernel> clear         # Clear screen
kernel> test          # Create test process
```

## Learning Path: How to Study This Code

### Phase 1: The Boot Process (Days 1-2)

**Goal**: Understand how the computer starts and loads our OS

1. **Read boot.asm (Stage 1)**
   - Every line is commented
   - Understand: BIOS, MBR, boot signature
   - Key concept: Loading from disk using INT 0x13

2. **Read stage2.asm (Stage 2)**  
   - A20 line enabling
   - GDT setup
   - Protected Mode switch
   - Key concept: Real Mode → Protected Mode transition

3. **Experiment**:
   ```bash
   # Change the loading message in boot.asm
   msg_loading:    db "YOUR MESSAGE HERE", 0x0D, 0x0A, 0
   
   # Rebuild and test
   make clean && make run
   ```

### Phase 2: Assembly to C Bridge (Days 3-4)

**Goal**: Understand how assembly interfaces with C

1. **Read kernel_entry.asm**
   - ISR/IRQ stubs (interrupt handlers)
   - Stack setup for C
   - Port I/O functions
   - Key concept: Calling conventions, stack frames

2. **Read kernel.h**
   - Type definitions
   - Function prototypes
   - Inline assembly helpers

3. **Experiment**:
   ```c
   // In kernel.c, add to kernel_main():
   screen_write("Hello from my custom OS!\n");
   ```

### Phase 3: Core Subsystems (Days 5-7)

**Goal**: Understand the kernel components

1. **GDT (Global Descriptor Table)**
   - Read gdt.c with Intel manual open
   - Understand segment descriptors
   - Why we use flat memory model

2. **IDT (Interrupt Descriptor Table)**
   - Read idt.c
   - Understand CPU exceptions vs hardware interrupts
   - PIC remapping (why IRQs go to 32-47)

3. **Screen Driver**
   - Read screen.c
   - VGA text mode (0xB8000)
   - Memory-mapped I/O

### Phase 4: Hardware Drivers (Days 8-10)

1. **Timer (timer.c)**
   - PIT (Programmable Interval Timer)
   - IRQ0 handler
   - System ticks

2. **Keyboard (keyboard.c)**
   - PS/2 keyboard protocol
   - Scancode translation
   - IRQ1 handler
   - Key buffer implementation

### Phase 5: Memory & Processes (Days 11-14)

1. **Memory Manager (memory.c)**
   - Simple heap allocator
   - First-fit algorithm
   - Block coalescing

2. **Process Manager (process.c)**
   - Process structure
   - Simple cooperative scheduling
   - Context switching basics

3. **System Calls (syscall.c)**
   - INT 0x80 interface
   - System call numbers
   - Parameter passing

## Common Learning Activities

### Activity 1: Add a New Shell Command

```c
// In kernel.c, add to shell_execute():
else if (strcmp(command, "hello") == 0) {
    screen_write("Hello, OS Developer!\n");
}
```

### Activity 2: Modify VGA Colors

```c
// In screen.c, change DEFAULT_COLOR:
#define DEFAULT_COLOR   (LIGHT_GREEN | (BLUE << 4))
// This gives green text on blue background
```

### Activity 3: Change Timer Frequency

```c
// In kernel.c, kernel_main():
timer_init(100);  // Change from 50 Hz to 100 Hz
// System will tick twice as fast!
```

### Activity 4: Add Custom Exception Handler

```c
// In idt.c, modify isr_handler():
if (regs->int_no == 0) {  // Divide by zero
    screen_write("\n!!! DIVIDE BY ZERO !!!\n");
    screen_write("Don't divide by zero!\n");
    // Don't halt - just warn
    return;  // Return instead of halting
}
```

## Debugging Tips

### Problem: OS doesn't boot

**Check boot sector:**
```bash
# View first 512 bytes of os.img
xxd -l 512 os.img | less

# Last two bytes should be 55 AA (boot signature)
xxd -s 510 -l 2 os.img
# Should show: 55 aa
```

### Problem: Triple fault / reboot loop

**Use GDB to find the exact fault:**
```bash
# Terminal 1:
make debug

# Terminal 2:
gdb kernel.bin
(gdb) target remote localhost:1234
(gdb) layout asm        # Show assembly
(gdb) break *0x10000    # Break at kernel entry
(gdb) continue
(gdb) stepi             # Step one instruction at a time
```

### Problem: Keyboard not responding

**Check interrupt setup:**
```c
// Add debug output in keyboard.c:
static void keyboard_handler(registers_t *regs) {
    screen_write("K");  // Print K on every key press
    // ... rest of handler
}
```

### Problem: Understanding memory layout

**Add memory map display:**
```c
// Add to shell commands:
else if (strcmp(command, "memmap") == 0) {
    screen_write("Memory Map:\n");
    screen_write("0x00007C00: Stage 1\n");
    screen_write("0x00001000: Stage 2\n");
    screen_write("0x00010000: Kernel\n");
    screen_write("0x000B8000: VGA buffer\n");
}
```

## Advanced Experiments

### 1. Implement Paging

Add virtual memory support. See OSDev Wiki on Paging.

### 2. Add File System

Implement FAT12 to read files from disk.

### 3. User Mode Programs

Load and execute Ring 3 programs.

### 4. Preemptive Multitasking

Modify process scheduler to switch on timer tick.

### 5. Port to x86-64

Transition to Long Mode for 64-bit operation.

## Resources for Deep Dives

### Books
- "Operating Systems: Design and Implementation" by Tanenbaum
- "Operating System Concepts" by Silberschatz
- "Modern Operating Systems" by Tanenbaum

### Online
- OSDev Wiki: https://wiki.osdev.org/
- Intel Manuals: https://www.intel.com/sdm
- Linux source code: https://kernel.org/
- Minix source code: https://minix3.org/

### Communities
- OSDev Forums: https://forum.osdev.org/
- OSDev Discord
- r/osdev on Reddit

## Final Tips

1. **Read the comments!** Every line is explained.
2. **Experiment!** Break things and fix them.
3. **Use GDB!** It's your best debugging friend.
4. **Take notes!** Document what you learn.
5. **Be patient!** OS dev is complex but rewarding.

---

**Ready to start?**

```bash
make run
```

**Welcome to OS development! 🎉**
