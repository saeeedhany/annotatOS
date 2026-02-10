# MinimalOS - Educational 32-bit Operating System

A fully-commented, educational operating system for x86 architecture. This project serves as a comprehensive learning resource for OS development, with every instruction and concept explained in detail.

## 🎯 Project Goals

This OS demonstrates:
- **Bootloader**: Custom 2-stage bootloader (MBR + Protected Mode setup)
- **Protected Mode**: 32-bit protected mode with GDT
- **Interrupts**: IDT with CPU exception and hardware interrupt handling
- **Drivers**: VGA text mode, PS/2 keyboard, PIT timer
- **Memory Management**: Simple heap allocator
- **Multitasking**: Cooperative process scheduling
- **System Calls**: INT 0x80 system call interface
- **Shell**: Interactive command-line interface

## 📚 Learning Path

### Prerequisites
- Basic understanding of C and Assembly
- Familiarity with computer architecture concepts
- Linux development environment (Arch Linux recommended)

### Recommended Reading Order
1. `boot.asm` - Understand BIOS boot process
2. `stage2.asm` - Learn about Protected Mode transition
3. `kernel_entry.asm` - See how assembly interfaces with C
4. `kernel.c` - Main kernel initialization
5. `gdt.c` / `idt.c` - Understand descriptor tables
6. Individual drivers (screen, keyboard, timer)
7. Memory management and process scheduling

## 🛠️ Building the OS

### Requirements
```bash
# On Arch Linux
sudo pacman -S nasm gcc make qemu-system-x86 gdb

# On Ubuntu/Debian
sudo apt install nasm gcc make qemu-system-x86 gdb
```

### Build Commands
```bash
# Build everything
make

# Build and run in QEMU
make run

# Clean build artifacts
make clean

# Run with debugging support (GDB on port 1234)
make debug

# Show all available targets
make help
```

## 🏗️ Architecture Overview

### Memory Map
```
0x00000000 - 0x000003FF : Real Mode IVT (Interrupt Vector Table)
0x00000400 - 0x000004FF : BIOS Data Area
0x00000500 - 0x00007BFF : Free memory (stack grows down from 0x7C00)
0x00007C00 - 0x00007DFF : Stage 1 Bootloader (512 bytes)
0x00007E00 - 0x0000FFFF : Free
0x00010000 - 0x???????? : Stage 2 Bootloader
0x00010000 - 0x???????? : Kernel (loaded at 64KB)
0x000A0000 - 0x000BFFFF : VGA video memory
0x000B8000              : VGA text mode buffer (80x25)
0x00100000 - 0x001FFFFF : Kernel heap (1MB)
0xFFFE0000 - 0xFFFFFFFF : BIOS ROM
```

### Boot Process
1. **BIOS** loads first 512 bytes (MBR) from disk to 0x7C00
2. **Stage 1** (boot.asm) loads Stage 2 from disk
3. **Stage 2** (stage2.asm):
   - Enables A20 line
   - Sets up GDT
   - Switches to Protected Mode
   - Loads kernel
4. **Kernel** (kernel_entry.asm → kernel_main()):
   - Initializes all subsystems
   - Enters interactive shell

### File Structure
```
boot.asm          - Stage 1 bootloader (MBR, 512 bytes)
stage2.asm        - Stage 2 bootloader (Protected Mode setup)
kernel_entry.asm  - Kernel entry point (assembly → C bridge)
kernel.c/h        - Main kernel and shell
gdt.c/h          - Global Descriptor Table
idt.c/h          - Interrupt Descriptor Table
screen.c/h       - VGA text mode driver
keyboard.c/h     - PS/2 keyboard driver
timer.c/h        - PIT timer driver
memory.c/h       - Heap memory manager
process.c/h      - Process/task manager
syscall.c/h      - System call interface
linker.ld        - Linker script
Makefile         - Build automation
```

## 🔧 Key Concepts Explained

### GDT (Global Descriptor Table)
Defines memory segments in Protected Mode. We use a "flat memory model" where all segments span the entire 4GB address space. Required entries:
- Null descriptor (required by CPU)
- Kernel code segment (Ring 0)
- Kernel data segment (Ring 0)
- User code segment (Ring 3)
- User data segment (Ring 3)

### IDT (Interrupt Descriptor Table)
Maps interrupt numbers to handler functions:
- 0-31: CPU exceptions (divide by zero, page fault, etc.)
- 32-47: Hardware interrupts (keyboard, timer, etc.) - remapped from default
- 48+: Software interrupts (system calls via INT 0x80)

### Protected Mode
32-bit operating mode with:
- 4GB address space
- Memory protection via segment descriptors
- Privilege levels (Ring 0-3)
- Hardware task switching support

### A20 Line
Bit 20 of address bus. Must be enabled to access memory above 1MB. Legacy compatibility feature from IBM PC days.

## 🎮 Using the OS

### Available Shell Commands
```
help      - Show command list
clear     - Clear screen
time      - Show system uptime
mem       - Display memory information
ps        - List processes
test      - Create test process
syscall   - Test system call interface
```

### Example Session
```
kernel> help
Available commands:
  help       - Show this help message
  clear      - Clear the screen
  time       - Show system uptime
  mem        - Show memory information
  ps         - List running processes
  test       - Run test process
  syscall    - Test system call

kernel> time
Uptime: 0:0:45

kernel> mem
Memory Information:
  Heap Start: 0x00100000
  Heap Size: 1048576 bytes
  Allocated: 0 bytes
  Free: 1048576 bytes
```

## 🐛 Debugging

### Using GDB
```bash
# Terminal 1: Start QEMU with debugging
make debug

# Terminal 2: Connect GDB
gdb kernel.bin
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

### Common Issues

**"Operating System not found"**
- Boot signature missing or incorrect
- Check that boot.bin is exactly 512 bytes
- Verify boot signature (0xAA55) at bytes 510-511

**Triple fault / reboot loop**
- GDT or IDT not properly initialized
- Stack overflow
- Invalid instruction execution
- Use `make debug` to find exact fault location

**Black screen after bootloader**
- Kernel not loaded correctly
- Check disk image sectors (stage2 at sector 2-16, kernel at sector 17+)
- Verify kernel entry point address

**Keyboard not working**
- IRQ not enabled (check PIC initialization)
- IDT entry for IRQ1 not set up
- Keyboard controller not initialized

## 📖 References and Resources

### Essential Reading
- [OSDev Wiki](https://wiki.osdev.org/) - Comprehensive OS development resource
- [Intel® 64 and IA-32 Architectures Software Developer Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [NASM Documentation](https://www.nasm.us/docs.php)

### Specific Topics
- [Rolling Your Own Bootloader](https://wiki.osdev.org/Rolling_Your_Own_Bootloader)
- [Protected Mode](https://wiki.osdev.org/Protected_Mode)
- [GDT Tutorial](https://wiki.osdev.org/GDT_Tutorial)
- [Interrupt Descriptor Table](https://wiki.osdev.org/IDT)
- [PIC - Programmable Interrupt Controller](https://wiki.osdev.org/PIC)
- [PS/2 Keyboard](https://wiki.osdev.org/PS/2_Keyboard)
- [Memory Management](https://wiki.osdev.org/Memory_Management)

## 🚀 Next Steps for Learning

After understanding this minimal OS, consider:

1. **Paging** - Virtual memory management
2. **User Mode** - Ring 3 execution with privilege separation
3. **ELF Loading** - Execute programs from disk
4. **Filesystem** - FAT12/FAT32 or custom filesystem
5. **Preemptive Multitasking** - Timer-based task switching
6. **IPC** - Inter-process communication
7. **64-bit Mode** - Long mode transition
8. **UEFI Boot** - Modern boot process
9. **SMP** - Symmetric multiprocessing
10. **Networking** - Network card drivers and TCP/IP stack

## 📄 License

This is educational code. Feel free to use, modify, and learn from it!

## 🙏 Acknowledgments

Based on resources from:
- OSDev.org community
- Intel Architecture manuals
- Numerous OS development tutorials

---

**Happy OS Development! 🎉**

Remember: The best way to learn is by doing. Read the code, modify it, break it, fix it, and understand every line!
