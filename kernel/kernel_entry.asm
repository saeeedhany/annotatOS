; ==============================================================================
; kernel_entry.asm - Kernel Entry Point
; ==============================================================================
; Location: kernel/kernel_entry.asm
;
; This is where the bootloader jumps after loading the kernel.
; It sets up the environment and calls the C kernel.
; ==============================================================================

[BITS 16]

; External C function
extern kernel_main

; Entry point
global _start

_start:
    ; We're still in 16-bit real mode
    ; Set up segments
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x9000              ; Set up stack
    sti
    
    ; Call C kernel
    call kernel_main
    
    ; If kernel returns, halt
    cli
    hlt
    jmp $
