# Project Structure Map

## Directory Organization

```
AnnotatOS/
├── boot/                   # Bootloader code
│   └── boot.asm           # Main bootloader (512 bytes, loaded by BIOS)
│
├── kernel/                 # Kernel code
│   ├── kernel_entry.asm   # Kernel entry point (assembly)
│   ├── kernel.c           # Main kernel code (C)
│   └── linker.ld          # Linker script
│
├── build/                  # Build outputs (auto-created)
│   ├── boot.bin           # Compiled bootloader
│   ├── kernel.bin         # Compiled kernel
│   ├── kernel_entry.o     # Object file
│   ├── kernel.o           # Object file
│   └── os.img             # Final bootable image
│
├── docs/                   # Documentation
│   ├── STRUCTURE.md       # This file
│   ├── SAFETY.md          # Safety information
│   └── README.md          # Project overview
│
└── Makefile               # Build system
```

## Boot Flow

```
1. BIOS
   |
   | (Loads first 512 bytes from disk to 0x7C00)
   v
2. boot/boot.asm
   |
   | (Loads kernel from disk sectors 2-11)
   v
3. kernel/kernel_entry.asm
   |
   | (Sets up environment, calls C code)
   v
4. kernel/kernel.c
   |
   | (Prints logo, runs shell)
   v
5. System Running
```

## Memory Layout

```
0x0000 - 0x03FF   BIOS Interrupt Vector Table
0x0400 - 0x04FF   BIOS Data Area
0x0500 - 0x7BFF   Free memory
0x7C00 - 0x7DFF   Bootloader (boot.bin loaded here by BIOS)
0x7E00 - 0x0FFF   Free memory
0x1000 - 0x????   Kernel (kernel.bin loaded here by bootloader)
0x9000            Stack (grows downward)
0xB8000           VGA text mode buffer
```

## Build Process

```
Source Files          Compilation              Final Image
------------          -----------              -----------

boot/boot.asm
    |
    | nasm -f bin
    v
build/boot.bin -----> dd (sector 1)
                           |
                           v
kernel/kernel_entry.asm    |
    |                      |
    | nasm -f elf32        |
    v                      |
build/kernel_entry.o       |         build/os.img
    |                      |         (bootable disk)
    | ld                   |
    +--------------------> dd (sectors 2+)
    |                      ^
build/kernel.o             |
    ^                      |
    | gcc -m16             |
    |                      |
kernel/kernel.c -----------+
```

## How Components Work Together

### 1. Bootloader (boot/boot.asm)
- Loaded by BIOS at 0x7C00
- Sets up segments and stack
- Loads kernel from disk sectors 2-11
- If any error: halts safely (no boot loop)
- If success: jumps to kernel at 0x1000

### 2. Kernel Entry (kernel/kernel_entry.asm)
- First code executed in kernel
- Sets up stack at 0x9000
- Calls C function kernel_main()
- If kernel_main returns: halts

### 3. Kernel Main (kernel/kernel.c)
- Clears screen
- Prints ASCII logo
- Prints welcome message
- Reads keyboard scancodes from PS/2 controller
- Executes shell commands (help/about/clear/exit)
- Powers off QEMU when requested

## Safety Features

### Error Handling
- Bootloader checks disk read success
- On any error: prints message and halts
- No jumps to invalid memory
- No boot loops possible

### Safe Halting
All halt points use:
```assembly
cli          ; Disable interrupts
hlt          ; Halt CPU
jmp $        ; If HLT fails, loop here (safe)
```

## File Relationships

```
Makefile
  |
  +-- Uses --> boot/boot.asm
  |              |
  |              +-- Produces --> build/boot.bin
  |
  +-- Uses --> kernel/kernel_entry.asm
  |              |
  |              +-- Produces --> build/kernel_entry.o
  |
  +-- Uses --> kernel/kernel.c
  |              |
  |              +-- Produces --> build/kernel.o
  |
  +-- Uses --> kernel/linker.ld
                 |
                 +-- Links --> build/kernel.bin
                                 |
                                 v
                            build/os.img (final image)
```

## Making Changes

### To modify bootloader:
1. Edit: boot/boot.asm
2. Run: make clean && make
3. Test: make run

### To modify kernel:
1. Edit: kernel/kernel.c or kernel/kernel_entry.asm
2. Run: make clean && make
3. Test: make run

### To add new kernel file:
1. Create: kernel/newfile.c
2. Update Makefile KERNEL_C_SRC
3. Run: make clean && make

## Understanding the Build

### What happens when you run "make"?

1. Creates build/ directory if needed
2. Assembles boot.asm to boot.bin (512 bytes)
3. Assembles kernel_entry.asm to kernel_entry.o
4. Compiles kernel.c to kernel.o
5. Links kernel_entry.o + kernel.o = kernel.bin
6. Creates empty disk image (1.44MB)
7. Writes boot.bin to sector 1
8. Writes kernel.bin starting at sector 2
9. Result: build/os.img (bootable disk image)

### What happens when you run "make run"?

1. Runs "make" to ensure image is built
2. Starts QEMU with os.img
3. QEMU simulates BIOS loading boot sector
4. Your OS boots and runs

## Troubleshooting

### Build fails?
- Check that boot/, kernel/ directories exist
- Check all source files are present
- Run: make clean && make

### Boot fails in QEMU?
- Check build/boot.bin is 512 bytes
- Check build/os.img exists
- Check bootloader messages for errors

### Want to see what's in the image?
```bash
xxd build/os.img | less        # View hex dump
xxd -l 512 build/os.img        # View boot sector only
```

## Next Steps

After understanding this structure:
1. Read boot/boot.asm line by line
2. Read kernel/kernel_entry.asm
3. Read kernel/kernel.c
4. Modify the ASCII logo
5. Add more shell commands
6. Learn about Protected Mode
7. Add command history and tab completion
8. Add more shell built-ins

This structure is designed to be:
- Easy to understand
- Easy to modify
- Easy to extend
- Safe to experiment with
