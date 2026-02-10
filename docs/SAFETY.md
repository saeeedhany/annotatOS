# Safety Information

## Is This Safe?

**YES**. This operating system is completely safe to build and run.

### Why It's Safe

1. **Only runs in QEMU (virtual machine)**
   - Does not touch your real hardware
   - Cannot modify your actual computer
   - Can be closed at any time

2. **Proper error handling**
   - All disk operations checked for errors
   - On any error: halts safely
   - No infinite boot loops

3. **No dangerous operations**
   - Does not write to your hard drive
   - Does not modify BIOS
   - Only runs in virtual environment

## About the Previous Boot Loop

### What Happened
The earlier version had a bug where:
1. Disk read failed
2. Code jumped to invalid memory anyway
3. CPU reset
4. BIOS tried to boot again
5. Loop repeated

### Why Your Computer Was Safe
- This only happened in QEMU (virtual machine)
- Your real hardware was never involved
- Closing QEMU stopped everything
- No permanent effects

### How This Version Prevents Loops

```assembly
; After disk read
int 0x13                ; Try to read

; Check for error
jc disk_error          ; If error, go to safe halt

; Only jump to kernel if successful
jmp KERNEL_OFFSET      ; Only reached if read succeeded

disk_error:
    ; Print error message
    ; Then HALT SAFELY
    cli
    hlt
    jmp $              ; Safe infinite loop here
```

## What Can Go Wrong?

### In QEMU (all safe)
- QEMU window might freeze - just close it
- Bootloader might show error - system halts safely
- Black screen - bootloader failed, close QEMU
- Garbage on screen - video mode issue, close QEMU

**None of these can damage anything**

### On Real Hardware (not recommended yet)
- Wait until you're very comfortable with QEMU
- Use old/spare computer first
- Keep backup boot USB ready
- Should work fine, but test in QEMU first

## How to Exit Safely

### From QEMU
1. Close window (X button), OR
2. Press Ctrl+Alt+2 (QEMU monitor)
3. Type "quit" and press Enter

### From Virtual Machine
- VM will halt automatically
- Just close QEMU window

## Common Questions

### Q: Can this damage my hardware?
A: No. It only runs in QEMU virtual machine.

### Q: Can this damage my files?
A: No. It does not access your file system.

### Q: What if QEMU freezes?
A: Close the window. No harm done.

### Q: Can I test on real hardware?
A: Technically yes, but test thoroughly in QEMU first. Use spare/old computer.

### Q: What if bootloader fails?
A: It prints error and halts. Close QEMU and check code.

## Safe Testing Procedure

### Every Time You Make Changes

1. Build the OS:
```bash
make clean
make
```

2. Check for errors in build output

3. Run in QEMU:
```bash
make run
```

4. Observe behavior:
   - Should show bootloader message
   - Should show kernel loading
   - Should show ASCII logo
   - Should show shell prompt

5. If anything goes wrong:
   - Close QEMU window
   - Check error messages
   - Fix code
   - Try again

## Error Messages Explained

### "DISK ERROR - System halted safely"
- Bootloader couldn't read kernel from disk
- Image might be corrupted
- Run: make clean && make

### Black screen in QEMU
- Bootloader didn't start
- Check boot.bin is exactly 512 bytes
- Check boot signature (0xAA55)

### QEMU immediately closes
- Image file might be missing
- Check build/os.img exists
- Run: make

### Garbage on screen
- Video mode issue
- Usually harmless
- Try rebuilding: make clean && make

## What Makes This Version Safe

1. **Verified disk operations**
```assembly
int 0x13        ; Read disk
jc error        ; Jump if error (Carry Flag set)
cmp al, 10      ; Check sectors read
jne error       ; Jump if not equal
```

2. **Safe halt on any error**
```assembly
error:
    cli         ; Disable interrupts
    hlt         ; Halt CPU
    jmp $       ; Loop if HLT fails
```

3. **No blind jumps**
```assembly
; Only jump if verified success
jmp KERNEL_OFFSET
```

4. **Stack properly set up**
```assembly
mov ss, ax      ; Stack segment
mov sp, 0x7C00  ; Stack pointer
```

## Comparing to Real OS Development

### This Project
- Educational
- Runs in virtual machine
- Safe to experiment
- Cannot damage anything
- Perfect for learning

### Professional OS Development
- Also tests in virtual machines first
- Uses same safety practices
- Takes months/years of work
- This gives you the foundation

## Bottom Line

**You are completely safe to:**
- Build this OS
- Run it in QEMU
- Modify the code
- Experiment freely
- Learn from mistakes

**The worst that can happen:**
- QEMU shows error message
- QEMU window freezes
- Close window and try again

**Cannot happen:**
- Damage to your computer
- Loss of files
- BIOS corruption
- Hardware damage

## Ready to Start?

1. Read docs/STRUCTURE.md to understand organization
2. Run: make
3. Run: make run
4. Observe the boot process
5. Read the source code
6. Make small changes
7. Rebuild and test

Have fun learning!
