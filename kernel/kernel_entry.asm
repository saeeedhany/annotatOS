; ==============================================================================
; SYSTEM-LEVEL OVERVIEW
; ==============================================================================
; This file defines the first kernel instruction stream reached from boot.asm.
; The bootloader performs disk I/O and jumps to physical 0x1000, which the
; linker script maps to symbol `_start` in this module.
;
; Boot-time behavior:
;   1) Re-initializes segment registers and stack for kernel-owned execution.
;   2) Calls C entrypoint `kernel_main`.
;   3) Falls back to halt loop if `kernel_main` unexpectedly returns.
;
; Runtime behavior:
;   - This file is transient trampoline code. After entering C, normal runtime
;     behavior is implemented in kernel.c.
;
; Memory behavior and layout:
;   - Executes from low memory region loaded at 0x1000.
;   - Stack base set to 0x9000 (real-mode stack, downward growth).
;   - No dynamic memory, heap, or relocation exists at this stage.
;
; CPU-level implications:
;   - CPU remains in 16-bit real mode.
;   - Calling C from assembly relies on compiler flags (`-m16`, freestanding)
;     and cdecl-compatible call/return expectations.
;   - Interrupts are masked during stack/segment reconfiguration to avoid ISR
;     execution against partially initialized state.
;
; Limitations and edge cases:
;   - No protected mode, GDT/IDT, paging, privilege levels, or ISR framework.
;   - Stack address is fixed and can collide with future larger kernels if not
;     coordinated with linker/load placement.
; ==============================================================================

[BITS 16]

extern kernel_main
global _start

_start:
    ; Establish deterministic segment and stack state for C code.
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x9000
    sti

    ; Control passes to high-level kernel logic.
    call kernel_main

    ; Defensive terminal state: if C returns, halt instead of running garbage.
    cli
    hlt
    jmp $
