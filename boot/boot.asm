; ==============================================================================
; boot.asm - Safe Bootloader with Error Handling
; ==============================================================================
; Location: boot/boot.asm
; 
; This bootloader:
; 1. Initializes system safely
; 2. Loads kernel from disk with error checking
; 3. If ANY error occurs, halts safely (no loops)
; 4. Jumps to kernel only if everything succeeds
; ==============================================================================

[BITS 16]
[ORG 0x7C00]

KERNEL_OFFSET equ 0x1000        ; Load kernel at 4KB

start:
    ; Save boot drive (BIOS gives us this in DL)
    mov [BOOT_DRIVE], dl
    
    ; Setup segments safely
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    
    ; Print boot message
    mov si, msg_boot
    call print
    
    ; Load kernel from disk
    mov si, msg_loading
    call print
    
    ; Setup disk read parameters
    mov ah, 0x02                ; BIOS read function
    mov al, 10                  ; Read 10 sectors (5KB for kernel)
    mov ch, 0                   ; Cylinder 0
    mov cl, 2                   ; Start at sector 2 (after bootloader)
    mov dh, 0                   ; Head 0
    mov dl, [BOOT_DRIVE]        ; Drive number saved earlier
    
    ; Destination: ES:BX
    mov bx, KERNEL_OFFSET
    
    ; Read from disk
    int 0x13
    
    ; Check for errors - if error, HALT SAFELY (no jump to kernel)
    jc disk_error
    
    ; Verify we read correct number of sectors
    cmp al, 10
    jne disk_error
    
    ; Success - print message and jump to kernel
    mov si, msg_success
    call print
    
    ; Jump to kernel - only reaches here if load succeeded
    jmp KERNEL_OFFSET

; Error handler - HALTS SAFELY, no loop
disk_error:
    mov si, msg_error
    call print
    
    ; SAFE HALT - will not loop or restart
    cli
    hlt
    jmp $                       ; If HLT fails, infinite loop here (safe)

; Print null-terminated string
; Input: SI = string pointer
print:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp .loop
.done:
    popa
    ret

; Data
BOOT_DRIVE:     db 0
msg_boot:       db "MinimalOS Bootloader", 0x0D, 0x0A, 0
msg_loading:    db "Loading kernel...", 0x0D, 0x0A, 0
msg_success:    db "Kernel loaded, starting...", 0x0D, 0x0A, 0
msg_error:      db "DISK ERROR - System halted safely", 0x0D, 0x0A, 0

; Pad to 510 bytes and add boot signature
times 510-($-$$) db 0
dw 0xAA55
