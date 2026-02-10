; ==============================================================================
; STAGE2.ASM - Stage 2 Bootloader (Protected Mode Setup)
; ==============================================================================
; Reference: OSDev Wiki - Protected Mode
; https://wiki.osdev.org/Protected_Mode
;
; Stage 2 Responsibilities:
; 1. Enable A20 line (access memory above 1MB)
; 2. Load GDT (Global Descriptor Table)
; 3. Switch to 32-bit Protected Mode
; 4. Load kernel from disk
; 5. Jump to kernel
;
; Memory Map at this point:
; 0x0000-0x03FF: Real Mode IVT (Interrupt Vector Table)
; 0x0400-0x04FF: BIOS Data Area
; 0x0500-0x7BFF: Free (our stack grows down from 0x7C00)
; 0x7C00-0x7DFF: Stage 1 bootloader (512 bytes)
; 0x7E00-0x9FFF: Free
; 0xA000-0xBFFF: VGA memory
; 0xC000-0xFFFF: BIOS
; 0x1000-?????: Stage 2 (us) and Kernel will be loaded here
; ==============================================================================

[BITS 16]                       ; We start in 16-bit Real Mode
[ORG 0x1000]                    ; Stage 1 loaded us at 0x1000

; ------------------------------------------------------------------------------
; Constants
; ------------------------------------------------------------------------------
KERNEL_OFFSET equ 0x10000       ; Load kernel at 64KB (above stage 2)
KERNEL_SECTORS equ 50           ; Read 50 sectors (25KB) for kernel

; ------------------------------------------------------------------------------
; Entry Point
; ------------------------------------------------------------------------------
stage2_start:
    ; Print status message
    mov si, msg_stage2
    call print_string_16

    ; Enable A20 line
    call enable_a20

    ; Load kernel from disk
    call load_kernel

    ; Load GDT
    lgdt [gdt_descriptor]       ; LGDT = Load Global Descriptor Table
                                ; Loads GDT base address and limit into GDTR register

    ; Switch to Protected Mode
    ; Reference: Intel Manual Vol 3A, Section 9.9
    mov eax, cr0                ; CR0 = Control Register 0 (system control flags)
                                ; Bit 0 (PE) = Protection Enable
    or eax, 0x1                 ; Set PE bit (bit 0)
    mov cr0, eax                ; Write back to CR0 - NOW IN PROTECTED MODE!

    ; CPU is now in Protected Mode, but still executing 16-bit code!
    ; We need to do a far jump to load CS with our code segment selector
    ; and flush the CPU pipeline of any pre-fetched 16-bit instructions
    jmp CODE_SEG:protected_mode_start
                                ; Far jump: Sets CS=CODE_SEG and jumps to label
                                ; This loads the code segment descriptor from GDT

; ------------------------------------------------------------------------------
; From here on, we're in 32-bit Protected Mode!
; ------------------------------------------------------------------------------
[BITS 32]                       ; Tell NASM we're now in 32-bit mode

protected_mode_start:
    ; Set up segment registers with data segment selector
    ; In Protected Mode, segment registers hold selectors (indexes into GDT)
    ; not actual segment addresses
    mov ax, DATA_SEG            ; AX = Data segment selector (from GDT)
    mov ds, ax                  ; DS = Data segment
    mov es, ax                  ; ES = Extra segment
    mov fs, ax                  ; FS = Additional segment
    mov gs, ax                  ; GS = Additional segment
    mov ss, ax                  ; SS = Stack segment
    
    ; Set up new stack in protected mode
    ; We'll use a larger stack now that we're in 32-bit mode
    mov ebp, 0x90000            ; EBP = Base pointer (bottom of stack)
    mov esp, ebp                ; ESP = Stack pointer (starts at bottom, grows down)

    ; Print success message
    mov esi, msg_protected_mode
    call print_string_32

    ; Jump to kernel!
    jmp KERNEL_OFFSET

; ------------------------------------------------------------------------------
; 16-bit Functions (Real Mode)
; ------------------------------------------------------------------------------
[BITS 16]

; Function: enable_a20
; Enable A20 line to access memory above 1MB
; Reference: OSDev Wiki - A20 Line
; https://wiki.osdev.org/A20_Line
;
; Historical Context:
; - 8086 had 20 address lines (1MB addressable: 2^20 = 1,048,576 bytes)
; - With segment:offset, addresses could wrap around (0xFFFF:0x0010 = 0x100000)
; - IBM PC wrapped addresses above 1MB to 0 for compatibility
; - A20 line controls whether bit 20 of address is always 0
; - We need to enable it to access full 32-bit address space
enable_a20:
    pusha
    
    ; Try BIOS method first (INT 0x15, AX=0x2401)
    mov ax, 0x2401              ; Function: Enable A20
    int 0x15                    ; BIOS keyboard services
    jnc .a20_done               ; Jump if no carry (success)
    
    ; If BIOS method fails, use keyboard controller method
    ; Wait for keyboard controller to be ready
    call .wait_keyboard
    
    ; Send write command to keyboard controller
    mov al, 0xAD                ; Command: Disable keyboard
    out 0x64, al                ; Port 0x64 = Keyboard controller command
    
    call .wait_keyboard
    
    mov al, 0xD0                ; Command: Read output port
    out 0x64, al
    
    call .wait_keyboard_data
    
    in al, 0x60                 ; Port 0x60 = Keyboard controller data
    push ax                     ; Save current value
    
    call .wait_keyboard
    
    mov al, 0xD1                ; Command: Write output port
    out 0x64, al
    
    call .wait_keyboard
    
    pop ax                      ; Restore value
    or al, 2                    ; Set bit 1 (A20 enable bit)
    out 0x60, al                ; Write it back
    
    call .wait_keyboard
    
    mov al, 0xAE                ; Command: Enable keyboard
    out 0x64, al
    
    call .wait_keyboard

.a20_done:
    popa
    ret

.wait_keyboard:
    ; Wait until keyboard controller input buffer is empty
    in al, 0x64                 ; Read status register
    test al, 2                  ; Test bit 1 (input buffer full)
    jnz .wait_keyboard          ; Loop while bit is set
    ret

.wait_keyboard_data:
    ; Wait until keyboard controller output buffer is full
    in al, 0x64                 ; Read status register
    test al, 1                  ; Test bit 0 (output buffer full)
    jz .wait_keyboard_data      ; Loop while bit is clear
    ret

; Function: load_kernel
; Load kernel from disk to memory
load_kernel:
    pusha
    
    mov si, msg_loading_kernel
    call print_string_16
    
    ; Set up INT 0x13 parameters
    mov ah, 0x02                ; Function: Read sectors
    mov al, KERNEL_SECTORS      ; Number of sectors to read
    mov ch, 0x00                ; Cylinder 0
    mov cl, 0x11                ; Sector 17 (0x11)
                                ; Sectors 1-16 are stage 1 and stage 2
    mov dh, 0x00                ; Head 0
    mov dl, 0x80                ; Drive 0x80 (first hard disk)
    
    ; Load to temporary buffer (we'll move it in protected mode if needed)
    mov ax, KERNEL_OFFSET
    shr ax, 4                   ; Convert to segment (divide by 16)
    mov es, ax                  ; ES:BX = destination
    xor bx, bx                  ; BX = 0, so ES:BX = KERNEL_OFFSET
    
    int 0x13                    ; BIOS disk read
    
    jc .kernel_error            ; Jump if error
    
    mov si, msg_kernel_loaded
    call print_string_16
    
    popa
    ret

.kernel_error:
    mov si, msg_kernel_error
    call print_string_16
    jmp $

; Function: print_string_16
; Print null-terminated string in Real Mode
; Input: SI = pointer to string
print_string_16:
    pusha
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    int 0x10
    jmp .loop
.done:
    popa
    ret

; ------------------------------------------------------------------------------
; 32-bit Functions (Protected Mode)
; ------------------------------------------------------------------------------
[BITS 32]

; Function: print_string_32
; Print null-terminated string in Protected Mode
; Input: ESI = pointer to string
; Reference: OSDev Wiki - Printing to Screen
; https://wiki.osdev.org/Printing_To_Screen
;
; VGA Text Mode:
; - Memory-mapped at 0xB8000
; - 80 columns x 25 rows = 2000 characters
; - Each character = 2 bytes: [character][attribute]
; - Attribute byte: [blink][background][foreground]
;   - Bits 0-3: Foreground color
;   - Bits 4-6: Background color
;   - Bit 7: Blink
print_string_32:
    pusha                       ; In 32-bit mode, this saves all 32-bit registers
    mov ebx, 0xB8000            ; EBX = VGA text buffer address
    mov ah, 0x0F                ; AH = Attribute (white on black)

.loop:
    lodsb                       ; Load byte from [ESI] into AL, ESI++
    or al, al                   ; Check for null terminator
    jz .done
    
    mov [ebx], ax               ; Write character + attribute to video memory
    add ebx, 2                  ; Move to next character position (2 bytes per char)
    jmp .loop

.done:
    popa
    ret

; ------------------------------------------------------------------------------
; Global Descriptor Table (GDT)
; ------------------------------------------------------------------------------
; Reference: OSDev Wiki - GDT
; https://wiki.osdev.org/GDT
; Intel Manual Vol 3A, Section 3.4.5
;
; GDT Structure:
; Each descriptor is 8 bytes with the following structure:
; - Limit (20 bits): Size of segment - 1
; - Base (32 bits): Starting address of segment
; - Access Byte (8 bits): Privilege level, type, etc.
; - Flags (4 bits): Granularity, size, etc.
;
; Segment Descriptor Format:
; 63        56 55  52 51      48 47           40 39        32
; [Base 24:31][Flags][Limit 16:19][Access Byte][Base 16:23]
; 31                              16 15                    0
; [      Base Address 0:15        ][    Segment Limit 0:15 ]

gdt_start:

; Null Descriptor (required by CPU, must be first entry)
gdt_null:
    dd 0x0                      ; DD = Define Double-word (4 bytes)
    dd 0x0                      ; Total: 8 bytes of zeros

; Code Segment Descriptor
; Base = 0x0, Limit = 0xFFFFF (4GB with granularity)
; Access = 0x9A (Present, Ring 0, Code, Executable, Readable)
; Flags = 0xC (4KB granularity, 32-bit)
gdt_code:
    dw 0xFFFF                   ; Limit (bits 0-15)
    dw 0x0                      ; Base (bits 0-15)
    db 0x0                      ; Base (bits 16-23)
    db 0x9A                     ; Access byte
                                ; Bit 7: Present (1 = segment is present)
                                ; Bit 6-5: DPL (00 = ring 0)
                                ; Bit 4: Descriptor type (1 = code/data)
                                ; Bit 3: Executable (1 = code segment)
                                ; Bit 2: Direction/Conforming (0 = grows up)
                                ; Bit 1: Readable (1 = can read from code segment)
                                ; Bit 0: Accessed (0 = not accessed yet)
    db 0xCF                     ; Flags (4 bits) + Limit (bits 16-19)
                                ; Flags bits:
                                ; Bit 7: Granularity (1 = 4KB blocks)
                                ; Bit 6: Size (1 = 32-bit protected mode)
                                ; Bit 5: Long mode (0 = not 64-bit)
                                ; Bit 4: Reserved (0)
                                ; Lower 4 bits = Limit bits 16-19
    db 0x0                      ; Base (bits 24-31)

; Data Segment Descriptor
; Base = 0x0, Limit = 0xFFFFF (4GB with granularity)
; Access = 0x92 (Present, Ring 0, Data, Writable)
; Flags = 0xC (4KB granularity, 32-bit)
gdt_data:
    dw 0xFFFF                   ; Limit (bits 0-15)
    dw 0x0                      ; Base (bits 0-15)
    db 0x0                      ; Base (bits 16-23)
    db 0x92                     ; Access byte
                                ; Bit 7: Present (1)
                                ; Bit 6-5: DPL (00 = ring 0)
                                ; Bit 4: Descriptor type (1 = code/data)
                                ; Bit 3: Executable (0 = data segment)
                                ; Bit 2: Direction (0 = grows up)
                                ; Bit 1: Writable (1 = can write to data segment)
                                ; Bit 0: Accessed (0)
    db 0xCF                     ; Flags + Limit (bits 16-19)
    db 0x0                      ; Base (bits 24-31)

gdt_end:

; GDT Descriptor (loaded into GDTR register)
gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size of GDT (limit)
                                ; Subtract 1 because limit is inclusive
    dd gdt_start                ; Address of GDT (base)

; Segment Selectors (offsets into GDT)
; Selector format: [Index (13 bits)][TI (1 bit)][RPL (2 bits)]
; - Index: Which GDT entry (0, 1, 2, ...)
; - TI: Table Indicator (0 = GDT, 1 = LDT)
; - RPL: Requested Privilege Level (0-3)
CODE_SEG equ gdt_code - gdt_start   ; 0x08 (index 1, TI=0, RPL=0)
DATA_SEG equ gdt_data - gdt_start   ; 0x10 (index 2, TI=0, RPL=0)

; ------------------------------------------------------------------------------
; Data Section
; ------------------------------------------------------------------------------
[BITS 16]
msg_stage2:         db "Stage 2 bootloader started", 0x0D, 0x0A, 0
msg_loading_kernel: db "Loading kernel...", 0x0D, 0x0A, 0
msg_kernel_loaded:  db "Kernel loaded!", 0x0D, 0x0A, 0
msg_kernel_error:   db "Kernel load error!", 0x0D, 0x0A, 0

[BITS 32]
msg_protected_mode: db "Protected Mode enabled! Jumping to kernel...", 0

; Pad stage 2 to fill allocated space
times 15*512-($-$$) db 0        ; Pad to 15 sectors (7.5KB)
