#!/bin/bash
# Quick compilation test - run this before building
# Tests that all files compile without errors

echo "╔════════════════════════════════════════╗"
echo "║   MinimalOS Compilation Test Suite    ║"
echo "╚════════════════════════════════════════╝"
echo ""

ERRORS=0

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

test_file() {
    local file=$1
    local compiler=$2
    local flags=$3
    local output=$4
    
    echo -n "Testing $file... "
    if $compiler $flags $file -o $output 2>/dev/null; then
        echo -e "${GREEN}✓ PASS${NC}"
        return 0
    else
        echo -e "${RED}✗ FAIL${NC}"
        $compiler $flags $file -o $output 2>&1 | head -5
        return 1
    fi
}

echo "1. Assembly Files:"
test_file "boot.asm" "nasm" "-f bin" "boot.bin" || ERRORS=$((ERRORS+1))
test_file "stage2.asm" "nasm" "-f bin" "stage2.bin" || ERRORS=$((ERRORS+1))
test_file "kernel_entry.asm" "nasm" "-f elf32" "kernel_entry.o" || ERRORS=$((ERRORS+1))

echo ""
echo "2. C Source Files:"
CFLAGS="-std=c99 -m32 -ffreestanding -fno-pie -Wall -Wextra -Werror -nostdlib -nostdinc -fno-builtin -fno-stack-protector -c"

for src in kernel.c screen.c gdt.c idt.c timer.c keyboard.c memory.c process.c syscall.c; do
    test_file "$src" "gcc" "$CFLAGS" "${src%.c}.o" || ERRORS=$((ERRORS+1))
done

echo ""
echo "3. Build System:"
if [ -f "Makefile" ]; then
    echo -e "Makefile: ${GREEN}✓ EXISTS${NC}"
else
    echo -e "Makefile: ${RED}✗ MISSING${NC}"
    ERRORS=$((ERRORS+1))
fi

if [ -f "linker.ld" ]; then
    echo -e "Linker script: ${GREEN}✓ EXISTS${NC}"
else
    echo -e "Linker script: ${RED}✗ MISSING${NC}"
    ERRORS=$((ERRORS+1))
fi

echo ""
echo "4. Documentation:"
for doc in README.md GETTING_STARTED.md TROUBLESHOOTING.md; do
    if [ -f "$doc" ]; then
        echo -e "$doc: ${GREEN}✓ EXISTS${NC}"
    else
        echo -e "$doc: ${YELLOW}⚠ MISSING${NC}"
    fi
done

echo ""
echo "════════════════════════════════════════"
if [ $ERRORS -eq 0 ]; then
    echo -e "${GREEN}✓ ALL TESTS PASSED!${NC}"
    echo "Ready to build: run 'make' to compile the OS"
    exit 0
else
    echo -e "${RED}✗ $ERRORS TEST(S) FAILED${NC}"
    echo "Fix the errors above before building"
    exit 1
fi
