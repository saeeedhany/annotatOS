################################################################################
# SYSTEM-LEVEL OVERVIEW
################################################################################
# This Makefile describes the host-side build pipeline that transforms source
# artifacts (16-bit assembly + freestanding C) into a BIOS-bootable floppy
# image. It is not executed by the target machine; it governs how host tools
# produce bytes that the machine will execute.
#
# Build-time flow:
#   1) Assemble boot sector as flat 512-byte binary.
#   2) Assemble kernel entry trampoline to ELF32 object.
#   3) Compile kernel C to ELF32 object using freestanding/no-libc flags.
#   4) Link objects with linker.ld into flat binary at load address 0x1000.
#   5) Compose final disk image: boot sector at LBA0, kernel at following LBAs.
#
# Memory model relevance:
#   - Build artifacts intentionally encode runtime memory expectations:
#       * boot.bin executes at 0x7C00 (BIOS convention)
#       * kernel.bin linked for 0x1000 (bootloader destination)
#   - Floppy image uses 2880 sectors (1.44MB) with raw sector addressing.
#
# CPU-level relevance:
#   - `-m16` drives generation of 16-bit compatible code paths.
#   - `-ffreestanding -nostdlib -nostdinc` avoids assumptions about user-space
#     runtime, startup CRT, or host-provided system libraries.
#
# Limitations and edge cases:
#   - Pipeline assumes required host tools are installed (nasm/gcc/ld/qemu).
#   - Kernel placement is static; no size guard beyond floppy capacity and the
#     bootloader's sector-read limit.
#   - `run` target depends on QEMU defaults that may vary by host environment.
################################################################################

# Tools
AS = nasm
CC = gcc
LD = ld
QEMU = qemu-system-i386

# Directories
BOOT_DIR = boot
KERNEL_DIR = kernel
BUILD_DIR = build

# Flags
ASFLAGS_BIN = -f bin
ASFLAGS_ELF = -f elf32
CFLAGS = -m16 -ffreestanding -fno-pie -nostdlib -nostdinc -fno-stack-protector -Wall -Werror
LDFLAGS = -m elf_i386 -T $(KERNEL_DIR)/linker.ld

# Output files
BOOT_BIN = $(BUILD_DIR)/boot.bin
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
OS_IMAGE = $(BUILD_DIR)/os.img

# Source files
BOOT_SRC = $(BOOT_DIR)/boot.asm
KERNEL_ENTRY_SRC = $(KERNEL_DIR)/kernel_entry.asm
KERNEL_C_SRC = $(KERNEL_DIR)/kernel.c

################################################################################
# Main Targets
################################################################################

.PHONY: all
all: $(OS_IMAGE)

# Compose a bootable floppy image with deterministic sector placement.
$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	@echo "Creating disk image..."
	@mkdir -p $(BUILD_DIR)
	dd if=/dev/zero of=$(OS_IMAGE) bs=512 count=2880 2>/dev/null
	dd if=$(BOOT_BIN) of=$(OS_IMAGE) bs=512 count=1 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$(OS_IMAGE) bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "Done: $(OS_IMAGE)"

# Build 512-byte BIOS boot sector.
$(BOOT_BIN): $(BOOT_SRC)
	@echo "Building bootloader..."
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS_BIN) $(BOOT_SRC) -o $(BOOT_BIN)
	@echo "Bootloader: $(BOOT_BIN)"

# Build kernel binary from assembly entry + C runtime.
$(KERNEL_BIN): $(KERNEL_ENTRY_SRC) $(KERNEL_C_SRC)
	@echo "Building kernel..."
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS_ELF) $(KERNEL_ENTRY_SRC) -o $(BUILD_DIR)/kernel_entry.o
	$(CC) $(CFLAGS) -c $(KERNEL_C_SRC) -o $(BUILD_DIR)/kernel.o
	$(LD) $(LDFLAGS) -o $(KERNEL_BIN) $(BUILD_DIR)/kernel_entry.o $(BUILD_DIR)/kernel.o
	@echo "Kernel: $(KERNEL_BIN)"

################################################################################
# Run Targets
################################################################################

.PHONY: run
run: $(OS_IMAGE)
	@echo "Starting AnnotatOS in QEMU..."
	@echo "Close window to exit"
	$(QEMU) -drive file=$(OS_IMAGE),format=raw

.PHONY: debug
debug: $(OS_IMAGE)
	@echo "Starting QEMU in debug mode..."
	@echo "Connect GDB to localhost:1234"
	$(QEMU) -drive file=$(OS_IMAGE),format=raw -s -S

################################################################################
# Utility Targets
################################################################################

.PHONY: clean
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)
	@echo "Clean complete"

.PHONY: structure
structure:
	@echo "Project Structure:"
	@echo "boot/         - Bootloader code"
	@echo "kernel/       - Kernel code"
	@echo "build/        - Build outputs (created by make)"
	@echo "docs/         - Documentation"

.PHONY: help
help:
	@echo "AnnotatOS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build OS image"
	@echo "  make run      - Build and run in QEMU"
	@echo "  make debug    - Run with GDB support"
	@echo "  make clean    - Remove build files"
	@echo "  make structure - Show project structure"
	@echo ""
