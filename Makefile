################################################################################
# MAKEFILE - Build System for MinimalOS
################################################################################
# This Makefile automates the entire build process:
# 1. Assemble bootloader and stage2
# 2. Compile C kernel files
# 3. Assemble kernel entry
# 4. Link everything together
# 5. Create bootable disk image
# 6. Run in QEMU
#
# Usage:
#   make           - Build everything
#   make run       - Build and run in QEMU
#   make clean     - Remove all build artifacts
#   make debug     - Run in QEMU with GDB support
#
# Reference: OSDev Wiki - Bare Bones
# https://wiki.osdev.org/Bare_Bones
################################################################################

# Compiler and tool configuration
# ---------------------------------
AS = nasm                         # Assembler (NASM)
CC = gcc                          # C Compiler (GCC)
LD = ld                           # Linker (GNU LD)

# Assembly flags
# NASM flags:
# -f elf32: Output 32-bit ELF object files (for C linking)
ASFLAGS = -f elf32

# C compiler flags
# -std=c99: Use C99 standard (avoid C23 keyword issues)
# -m32: Generate 32-bit code
# -ffreestanding: No standard library (kernel environment)
# -fno-pie: Don't generate position-independent code
# -Wall: Enable all warnings
# -Wextra: Enable extra warnings
# -Werror: Treat warnings as errors
# -nostdlib: Don't link standard library
# -nostdinc: Don't use standard include paths
# -fno-builtin: Don't use built-in functions
# -fno-stack-protector: Disable stack protection (no runtime support)
CFLAGS = -std=c99 -m32 -ffreestanding -fno-pie -Wall -Wextra -Werror \
         -nostdlib -nostdinc -fno-builtin -fno-stack-protector

# Linker flags
# -m elf_i386: Target 32-bit x86 ELF
# -T linker.ld: Use our custom linker script
LDFLAGS = -m elf_i386 -T linker.ld

# QEMU configuration
# --------------------------
QEMU = qemu-system-i386           # QEMU for 32-bit x86
QEMU_FLAGS = -drive file=os.img,format=raw,index=0,media=disk \
             -m 128M              # 128MB RAM

# Source files
# ------------
# C source files (kernel implementation)
C_SOURCES = kernel.c screen.c gdt.c idt.c timer.c keyboard.c \
            memory.c process.c syscall.c

# Assembly source files
ASM_SOURCES = kernel_entry.asm

# Object files (generated from sources)
C_OBJECTS = $(C_SOURCES:.c=.o)
ASM_OBJECTS = $(ASM_SOURCES:.asm=.o)
ALL_OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

################################################################################
# Build Targets
################################################################################

# Default target: build everything
.PHONY: all
all: os.img

# Build bootable disk image
# This is the final output - a raw disk image with:
# - Sector 1: Stage 1 bootloader (boot.bin)
# - Sectors 2-16: Stage 2 bootloader (stage2.bin)
# - Sectors 17+: Kernel (kernel.bin)
os.img: boot.bin stage2.bin kernel.bin
	@echo "=== Creating bootable disk image ==="
	# Create 10MB disk image (10MB = 20480 sectors * 512 bytes)
	dd if=/dev/zero of=os.img bs=512 count=20480 2>/dev/null
	
	# Write stage 1 bootloader to sector 1 (first 512 bytes)
	# conv=notrunc: Don't truncate the output file
	dd if=boot.bin of=os.img bs=512 count=1 conv=notrunc 2>/dev/null
	
	# Write stage 2 to sectors 2-16 (15 sectors)
	# seek=1: Skip first sector (where stage 1 is)
	dd if=stage2.bin of=os.img bs=512 seek=1 conv=notrunc 2>/dev/null
	
	# Write kernel starting at sector 17 (after stage 2)
	# seek=16: Skip first 16 sectors (stage 1 + stage 2)
	dd if=kernel.bin of=os.img bs=512 seek=16 conv=notrunc 2>/dev/null
	
	@echo "=== Disk image created: os.img ==="
	@ls -lh os.img

# Assemble stage 1 bootloader
# This is the MBR (Master Boot Record) - exactly 512 bytes
# -f bin: Output raw binary (not ELF)
boot.bin: boot.asm
	@echo "=== Assembling stage 1 bootloader ==="
	$(AS) -f bin boot.asm -o boot.bin
	@echo "Stage 1 size: $$(stat -c%s boot.bin) bytes (must be 512)"

# Assemble stage 2 bootloader
# This sets up protected mode and loads the kernel
stage2.bin: stage2.asm
	@echo "=== Assembling stage 2 bootloader ==="
	$(AS) -f bin stage2.asm -o stage2.bin
	@echo "Stage 2 size: $$(stat -c%s stage2.bin) bytes"

# Link kernel binary
# Combines all kernel object files into single binary
kernel.bin: $(ALL_OBJECTS)
	@echo "=== Linking kernel ==="
	$(LD) $(LDFLAGS) -o kernel.bin $(ALL_OBJECTS)
	@echo "Kernel size: $$(stat -c%s kernel.bin) bytes"

# Compile C source files to object files
# Pattern rule: %.o depends on %.c
# $<: First prerequisite (the .c file)
# $@: Target name (the .o file)
%.o: %.c
	@echo "=== Compiling $< ==="
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble kernel entry assembly file
# This is the bridge between bootloader and C code
kernel_entry.o: kernel_entry.asm
	@echo "=== Assembling kernel entry ==="
	$(AS) $(ASFLAGS) kernel_entry.asm -o kernel_entry.o

################################################################################
# Run and Debug Targets
################################################################################

# Run OS in QEMU
# This builds everything first, then launches QEMU
.PHONY: run
run: os.img
	@echo "=== Starting QEMU ==="
	@echo "Press Ctrl+Alt+G to release mouse, Ctrl+Alt+2 for QEMU monitor"
	$(QEMU) $(QEMU_FLAGS)

# Run in QEMU with debugging support
# Waits for GDB to connect on port 1234
# In another terminal: gdb kernel.bin, then (gdb) target remote localhost:1234
.PHONY: debug
debug: os.img
	@echo "=== Starting QEMU in debug mode ==="
	@echo "Connect with: gdb kernel.bin"
	@echo "Then in GDB: target remote localhost:1234"
	$(QEMU) $(QEMU_FLAGS) -s -S
	# -s: Shorthand for -gdb tcp::1234
	# -S: Don't start CPU at startup (wait for GDB 'continue')

# Run with serial output (for debugging print statements)
.PHONY: run-serial
run-serial: os.img
	@echo "=== Starting QEMU with serial output ==="
	$(QEMU) $(QEMU_FLAGS) -serial stdio

# Run in curses mode (no SDL window, terminal only)
.PHONY: run-curses
run-curses: os.img
	@echo "=== Starting QEMU in curses mode ==="
	$(QEMU) $(QEMU_FLAGS) -curses

################################################################################
# Utility Targets
################################################################################

# Clean build artifacts
.PHONY: clean
clean:
	@echo "=== Cleaning build artifacts ==="
	rm -f *.o *.bin os.img
	@echo "Clean complete"

# Show disk image info
.PHONY: info
info: os.img
	@echo "=== Disk Image Information ==="
	@echo "File: os.img"
	@ls -lh os.img
	@echo ""
	@echo "First 512 bytes (MBR):"
	@xxd -l 512 os.img | head -n 10
	@echo "..."
	@echo ""
	@echo "Boot signature (should be 55 AA):"
	@xxd -s 510 -l 2 os.img

# Dump disassembly of kernel (for debugging)
.PHONY: disasm
disasm: kernel.bin
	@echo "=== Disassembling kernel ==="
	objdump -D -b binary -m i386 -M intel kernel.bin | less

# Create bootable USB (DANGEROUS - double check device!)
# Usage: make usb DEVICE=/dev/sdX
.PHONY: usb
usb: os.img
	@echo "=== Writing to USB device $(DEVICE) ==="
	@echo "WARNING: This will DESTROY all data on $(DEVICE)!"
	@echo "Press Ctrl+C to cancel, or Enter to continue"
	@read dummy
	sudo dd if=os.img of=$(DEVICE) bs=4M status=progress
	sync
	@echo "Done! You can now boot from $(DEVICE)"

################################################################################
# Help
################################################################################

.PHONY: help
help:
	@echo "MinimalOS Build System"
	@echo "======================"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build OS image"
	@echo "  make run      - Build and run in QEMU"
	@echo "  make debug    - Run with GDB debugging"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make info     - Show disk image info"
	@echo "  make disasm   - Disassemble kernel"
	@echo "  make help     - Show this help"
	@echo ""
	@echo "Advanced:"
	@echo "  make run-serial   - Run with serial console"
	@echo "  make run-curses   - Run in terminal mode"
	@echo "  make usb DEVICE=/dev/sdX - Write to USB (DANGEROUS)"
	@echo ""

################################################################################
# Dependencies
################################################################################
# Header file dependencies
# If any header changes, recompile relevant C files

kernel.o: kernel.h screen.h gdt.h idt.h timer.h keyboard.h memory.h process.h syscall.h
screen.o: screen.h kernel.h
gdt.o: gdt.h kernel.h
idt.o: idt.h kernel.h screen.h
timer.o: timer.h kernel.h idt.h screen.h
keyboard.o: keyboard.h kernel.h idt.h screen.h
memory.o: memory.h kernel.h screen.h
process.o: process.h kernel.h memory.h screen.h
syscall.o: syscall.h kernel.h idt.h screen.h
