################################################################################
# Makefile - MinimalOS Build System
################################################################################
# Organized structure:
#   boot/     - Bootloader source
#   kernel/   - Kernel source
#   build/    - Build artifacts
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

# Create bootable disk image
$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	@echo "Creating disk image..."
	@mkdir -p $(BUILD_DIR)
	dd if=/dev/zero of=$(OS_IMAGE) bs=512 count=2880 2>/dev/null
	dd if=$(BOOT_BIN) of=$(OS_IMAGE) bs=512 count=1 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$(OS_IMAGE) bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "Done: $(OS_IMAGE)"

# Build bootloader
$(BOOT_BIN): $(BOOT_SRC)
	@echo "Building bootloader..."
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS_BIN) $(BOOT_SRC) -o $(BOOT_BIN)
	@echo "Bootloader: $(BOOT_BIN)"

# Build kernel
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
	@echo "Starting MinimalOS in QEMU..."
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
	@echo "MinimalOS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build OS image"
	@echo "  make run      - Build and run in QEMU"
	@echo "  make debug    - Run with GDB support"
	@echo "  make clean    - Remove build files"
	@echo "  make structure - Show project structure"
	@echo ""
