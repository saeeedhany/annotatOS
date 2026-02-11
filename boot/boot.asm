; ==============================================================================
; SYSTEM-LEVEL OVERVIEW
; ==============================================================================
; This 512-byte boot sector is the first software the BIOS executes after the
; POST firmware sequence. BIOS copies this sector from disk LBA 0 to physical
; address 0x0000:0x7C00 and transfers control to `start` in 16-bit real mode.
;
; Boot-time behavior:
;   1) Establishes a deterministic 16-bit execution context (segments + stack).
;   2) Uses BIOS interrupt services to print status and read kernel sectors.
;   3) Verifies disk I/O success and jumps to the loaded kernel image at 0x1000.
;   4) If any stage fails, halts safely in-place.
;
; Runtime behavior:
;   - This file has no long-lived runtime role. Once control is transferred to
;     the kernel, this code is effectively dead unless a reset occurs.
;
; Memory model and layout:
;   - Boot sector image occupies 512 bytes at physical 0x7C00..0x7DFF.
;   - BOOT_DRIVE and string literals live inside that region.
;   - Kernel payload is loaded at physical 0x1000 (ES:BX = 0x0000:0x1000).
;   - Stack starts at SS:SP = 0x0000:0x7C00 and grows downward.
;
; CPU-level implications:
;   - Real mode: 20-bit segmented addressing, no paging/protection/isolation.
;   - `cli`/`sti` bracket stack and segment setup to avoid interrupt handlers
;     running against transiently inconsistent state.
;   - BIOS interrupts (`int 0x10`, `int 0x13`) serialize with firmware routines
;     and consume registers according to BIOS ABI conventions.
;
; Limitations and edge cases:
;   - CHS read parameters are hard-coded and assume a BIOS geometry-compatible
;     disk image where kernel occupies sectors 2..11 on cylinder/head 0.
;   - No A20 enablement, no protected-mode transition, no filesystem parsing.
;   - If kernel exceeds 10 sectors, load truncation occurs silently unless AL
;     mismatch or carry flag catches failure.
;   - `jmp 0x1000` assumes code at 0x1000 is valid 16-bit entry code.
;
; Reference notes:
;   - BIOS boot protocol: IBM PC/AT compatible convention (boot signature 0xAA55).
;   - INT 13h AH=02h sector read contract per BIOS disk services manuals.
; ==============================================================================

[BITS 16]
[ORG 0x7C00]

KERNEL_OFFSET equ 0x1000        ; Physical load destination for kernel image.

start:
    ; BIOS passes boot drive in DL. Persist it before any BIOS calls may clobber.
    mov [BOOT_DRIVE], dl

    ; Enter a known-good execution context.
    ; DS/ES/SS=0 means symbolic addresses resolve within low physical memory.
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; Progress telemetry through BIOS teletype output (INT 10h AH=0Eh).
    mov si, msg_boot
    call print

    mov si, msg_loading
    call print

    ; Configure BIOS CHS read:
    ;   AH=0x02 (read sectors)
    ;   AL=10 sectors
    ;   CH=0 cylinder, CL=2 sector index (1-based), DH=0 head
    ;   DL=boot drive
    ; Destination buffer is ES:BX = 0x0000:0x1000.
    mov ah, 0x02
    mov al, 10
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [BOOT_DRIVE]
    mov bx, KERNEL_OFFSET

    int 0x13

    ; Error path #1: BIOS indicates failure via carry flag.
    jc disk_error

    ; Error path #2: short read. BIOS returns sectors transferred in AL.
    cmp al, 10
    jne disk_error

    mov si, msg_success
    call print

    ; Transfer control into loaded kernel image. No return is expected.
    jmp KERNEL_OFFSET

disk_error:
    mov si, msg_error
    call print

    ; Hard stop semantics:
    ; - CLI prevents asynchronous IRQ re-entry.
    ; - HLT puts core into low-power stopped state until reset/NMI.
    cli
    hlt
    jmp $                       ; Defensive sink if HLT is ineffective in emulator.

; ------------------------------------------------------------------------------
; print: null-terminated ASCII output via BIOS teletype
; Input: SI -> string in current DS segment
; Clobbers: AX (internally), preserves caller GPRs via PUSHA/POPA
; ------------------------------------------------------------------------------
print:
    pusha
.loop:
    lodsb                       ; AL = [DS:SI], SI++
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp .loop
.done:
    popa
    ret

; Data region: packed directly into the 512-byte boot sector footprint.
BOOT_DRIVE:     db 0
msg_boot:       db "AnnotatOS Bootloader", 0x0D, 0x0A, 0
msg_loading:    db "Loading kernel...", 0x0D, 0x0A, 0
msg_success:    db "Kernel loaded, starting...", 0x0D, 0x0A, 0
msg_error:      db "DISK ERROR - System halted safely", 0x0D, 0x0A, 0

; BIOS requires boot signature at bytes 510..511.
times 510-($-$$) db 0
dw 0xAA55
