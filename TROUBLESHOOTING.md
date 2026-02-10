# TROUBLESHOOTING GUIDE & BUILD FIXES

## Issues Found and Fixed During Development

This document lists all compilation and runtime issues that were discovered and fixed in the MinimalOS project. Use this as a reference if you encounter similar problems.

---

## ✅ FIXED ISSUES

### 1. **C23 Boolean Type Conflict**
**Error:**
```
kernel.h:44:5: error: cannot use keyword 'false' as enumeration constant
kernel.h:44:5: note: 'false' is a keyword with '-std=c23' onwards
```

**Cause:** Modern GCC defaults to C23 standard where `bool`, `true`, and `false` are reserved keywords.

**Fix Applied:**
- Added `-std=c99` flag to `CFLAGS` in Makefile
- Changed boolean definition in `kernel.h` from enum to typedef + defines:
```c
typedef int bool;
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif
```

**Location:** `Makefile` line 31, `kernel.h` lines 43-49

---

### 2. **Duplicate `registers_t` Definition**
**Error:**
```
idt.c:86:3: error: conflicting types for 'registers_t'
idt.h:18:3: note: previous declaration of 'registers_t'
```

**Cause:** The `registers_t` structure was defined in both `idt.h` (header) and `idt.c` (implementation).

**Fix Applied:**
- Removed duplicate definition from `idt.c`
- Kept only the definition in `idt.h` (proper location for shared types)

**Location:** `idt.c` (removed lines 74-86)

---

### 3. **Variable/Function Name Collision**
**Error:**
```
process.c:30:19: error: 'process_list' redeclared as different kind of symbol
process.h:10:6: note: previous declaration of 'process_list' with type 'void(void)'
```

**Cause:** Both a static variable and a public function were named `process_list`.

**Fix Applied:**
- Renamed internal variable from `process_list` to `process_queue`
- Updated all references throughout `process.c`

**Location:** `process.c` lines 30, 36, 64, 80

---

### 4. **Unused Function Warning**
**Error:**
```
syscall.c:20:13: error: 'syscall_handler' defined but not used [-Werror=unused-function]
```

**Cause:** `syscall_handler` is defined but not yet registered with IDT (placeholder for future implementation).

**Fix Applied:**
- Added `__attribute__((unused))` to suppress warning
- Added explanatory comment about future implementation

**Location:** `syscall.c` lines 20-21

---

### 5. **NASM Word Size Warning**
**Warning:**
```
stage2.asm:190: warning: word exceeds bounds [-w+number-overflow]
```

**Cause:** Moving 32-bit constant `KERNEL_OFFSET` (0x10000) into 16-bit register `BX`.

**Fix Applied:**
- Changed to use `AX` register for the intermediate calculation
- This makes the word-size operation explicit and correct

**Location:** `stage2.asm` line 190

---

## 🔧 COMPILATION REQUIREMENTS

### Required Tools
```bash
# Arch Linux
sudo pacman -S nasm gcc make qemu-system-x86 gdb

# Ubuntu/Debian
sudo apt install nasm gcc make qemu-system-x86 gdb

# Fedora
sudo dnf install nasm gcc make qemu-system-x86 gdb
```

### Version Requirements
- **NASM:** 2.15+ (for modern syntax support)
- **GCC:** 11.0+ (32-bit support required)
- **Make:** 4.0+
- **QEMU:** 6.0+ (qemu-system-i386)

### GCC 32-bit Support
On 64-bit systems, you may need to install 32-bit development libraries:

```bash
# Ubuntu/Debian
sudo apt install gcc-multilib g++-multilib

# Fedora
sudo dnf install glibc-devel.i686 libgcc.i686
```

---

## 🐛 COMMON BUILD ERRORS

### Error: "cannot find -lgcc"
**Solution:** Install 32-bit GCC libraries
```bash
sudo apt install gcc-multilib  # Ubuntu/Debian
sudo dnf install libgcc.i686    # Fedora
```

### Error: "nasm: command not found"
**Solution:** Install NASM assembler
```bash
sudo pacman -S nasm  # Arch
sudo apt install nasm  # Ubuntu/Debian
```

### Error: "No rule to make target"
**Solution:** Ensure all source files are present. Check with:
```bash
ls -1 *.asm *.c *.h Makefile linker.ld
```

### Error: Linker cannot find files
**Solution:** Make sure you're in the project directory:
```bash
cd /path/to/MinimalOS/
make clean
make
```

---

## 🚀 RUNTIME ISSUES

### Issue: "Operating System not found"
**Causes:**
1. Boot signature missing or incorrect
2. Image not written to correct disk/USB

**Debug Steps:**
```bash
# Check boot signature (should show: 55 aa at end)
xxd -s 510 -l 2 os.img

# Verify image size
ls -lh os.img  # Should be 10MB

# Check first sector
xxd -l 512 os.img | head -20
```

### Issue: QEMU shows black screen
**Causes:**
1. Kernel not loaded correctly
2. Stage 2 failed to load kernel
3. Protected mode switch failed

**Debug Steps:**
```bash
# Run with serial output
make run-serial

# Run in debug mode
make debug

# In another terminal:
gdb kernel.bin
(gdb) target remote localhost:1234
(gdb) break *0x10000  # Break at kernel entry
(gdb) continue
(gdb) layout asm
(gdb) stepi  # Step through instructions
```

### Issue: Triple fault / continuous reboot
**Causes:**
1. Invalid GDT entry
2. Invalid IDT entry
3. Stack overflow
4. Invalid memory access

**Debug Steps:**
```bash
# Enable QEMU debug output
qemu-system-i386 -drive file=os.img,format=raw -d int,cpu_reset

# Check GDT setup
# Add debug prints in gdt_init() before it crashes

# Verify IDT is loaded
# Add debug prints in idt_init()
```

### Issue: Keyboard not responding
**Causes:**
1. IRQ1 not enabled
2. PIC not properly initialized
3. Keyboard buffer full

**Debug:**
Add debug output in `keyboard_handler()`:
```c
static void keyboard_handler(registers_t *regs) {
    screen_putchar('K');  // Print K on every keystroke
    // ... rest of handler
}
```

---

## 📝 CODE QUALITY CHECKS

### Before Committing Changes
Run these checks:

```bash
# 1. Ensure clean compilation
make clean
make 2>&1 | tee build.log
grep -i error build.log

# 2. Check for warnings
grep -i warning build.log

# 3. Test in QEMU
make run

# 4. Verify boot process
# Should see:
#   - "Loading Stage 2..."
#   - "Stage 2 loaded!"
#   - "Protected Mode enabled!"
#   - Kernel initialization messages
#   - "kernel>" prompt
```

---

## 🔍 STATIC ANALYSIS

### Check for Common C Issues
```bash
# Unused variables/functions
gcc -Wall -Wextra -Wunused *.c

# Missing prototypes
gcc -Wmissing-prototypes *.c

# Implicit declarations
gcc -Wimplicit-function-declaration *.c
```

---

## 📚 LEARNING RESOURCES

When debugging issues, consult:

1. **OSDev Wiki:** https://wiki.osdev.org/
   - [Common Problems](https://wiki.osdev.org/Category:Common_Problems)
   - [Triple Faults](https://wiki.osdev.org/Triple_Fault)
   - [GDB Debugging](https://wiki.osdev.org/Kernel_Debugging)

2. **Intel Manuals:**
   - Volume 3A: System Programming Guide
   - Volume 2: Instruction Set Reference

3. **Forum Help:**
   - OSDev Forums: https://forum.osdev.org/
   - r/osdev on Reddit
   - Stack Overflow `[operating-system]` tag

---

## ✅ VERIFICATION CHECKLIST

Before considering the OS "working":

- [ ] `make clean && make` completes without errors
- [ ] `make run` boots to kernel prompt
- [ ] Can type commands in shell
- [ ] `help` command shows all commands
- [ ] `time` command shows incrementing uptime
- [ ] `mem` command shows memory info
- [ ] `clear` command clears screen
- [ ] Keyboard input works correctly
- [ ] System doesn't crash or triple fault

---

## 🎯 KNOWN LIMITATIONS

These are intentional simplifications for educational purposes:

1. **No paging:** Uses segmentation only (flat model)
2. **Cooperative multitasking:** No preemption (processes must yield)
3. **No filesystem:** Everything in kernel binary
4. **BIOS only:** No UEFI support
5. **32-bit only:** Not x86-64
6. **No network:** No network stack
7. **Basic drivers:** VGA text mode, PS/2 keyboard only
8. **No user mode:** Everything runs in Ring 0

These can be added as learning exercises!

---

## 💡 TIPS FOR EXTENDING THE OS

### Adding a New Shell Command
```c
// In kernel.c, shell_execute():
else if (strcmp(command, "mycommand") == 0) {
    screen_write("My custom command!\n");
}
```

### Adding a New Driver
1. Create `driver.h` and `driver.c`
2. Add initialization to `kernel_main()`
3. If hardware interrupt needed, register with `irq_register_handler()`
4. Update `Makefile` dependencies

### Adding System Call
1. Add to `syscall.c` switch statement
2. Define syscall number
3. Call from user code: `mov eax, NUM; int 0x80`

---

## 📞 GETTING HELP

If you encounter an issue not covered here:

1. Check the error message carefully
2. Search OSDev Wiki for the specific error
3. Review the commented code at that location
4. Use GDB to step through the problematic code
5. Ask on OSDev forums with:
   - Full error message
   - Steps to reproduce
   - What you've already tried
   - Relevant code snippets

---

**Last Updated:** February 2026
**MinimalOS Version:** 1.0
**Build Status:** ✅ All issues fixed and tested
