; ==============================================================================
; BOOT.ASM - Stage 1 Bootloader (First 512 bytes)
; ==============================================================================
; Reference: OSDev Wiki - Rolling Your Own Bootloader
; https://wiki.osdev.org/Rolling_Your_Own_Bootloader
;
; This is the Master Boot Record (MBR) bootloader that BIOS loads at 0x7C00.
; It must be exactly 512 bytes and end with the boot signature 0xAA55.
;
; BIOS Boot Process:
; 1. BIOS performs POST (Power-On Self Test)
; 2. BIOS loads first 512 bytes from boot device to 0x7C00
; 3. BIOS checks for boot signature (0xAA55) at offset 510-511
; 4. If valid, BIOS jumps to 0x7C00, starting execution here
; 5. CPU is in 16-bit Real Mode, interrupts enabled
;
; Our bootloader's job:
; 1. Set up segments and stack
; 2. Load Stage 2 bootloader from disk
; 3. Jump to Stage 2
; ==============================================================================

[BITS 16]                       ; Tell NASM we're in 16-bit Real Mode
[ORG 0x7C00]                    ; BIOS loads us here, so all addresses are relative to 0x7C00

; ------------------------------------------------------------------------------
; Constants and Configuration
; ------------------------------------------------------------------------------
KERNEL_OFFSET equ 0x1000        ; Where we'll load stage 2 (and eventually kernel)
                                ; 0x1000 = 4KB, safe area above BIOS data area (0x0000-0x0FFF)

; ------------------------------------------------------------------------------
; Entry Point - Execution starts here
; ------------------------------------------------------------------------------
start:
    ; Save boot drive number (BIOS passes it in DL)
    ; We need to save this before we modify DL
    mov [BOOT_DRIVE], dl        ; Save boot drive for later use

    ; Clear interrupts while we set up segments to avoid crashes
    ; Reference: OSDev Wiki - Real Mode
    cli                         ; CLI = Clear Interrupt Flag (IF=0)

    ; Set up segment registers
    ; In Real Mode: Physical Address = Segment * 16 + Offset
    ; We want to use 0x0000 as base, so Physical = 0x0000 * 16 + Offset = Offset
    xor ax, ax                  ; XOR with itself = 0 (faster than MOV AX, 0)
    mov ds, ax                  ; DS = Data Segment = 0x0000
    mov es, ax                  ; ES = Extra Segment = 0x0000
    mov ss, ax                  ; SS = Stack Segment = 0x0000

    ; Set up stack (grows downward from 0x7C00)
    ; Stack grows DOWN in memory, so we set SP to point below our bootloader
    ; Stack: [0x7C00 - stack_size] <-- SP grows down <-- 0x7C00
    mov sp, 0x7C00              ; SP = Stack Pointer, start just below bootloader
                                ; This gives us ~31KB of stack (0x7C00 bytes)

    ; Re-enable interrupts now that segments are safe
    sti                         ; STI = Set Interrupt Flag (IF=1)

    ; Print loading message to screen
    mov si, msg_loading         ; SI = Source Index for string operations
    call print_string           ; Call our print function

    ; Load Stage 2 from disk
    call load_stage2

    ; Jump to Stage 2
    jmp KERNEL_OFFSET           ; Jump to loaded code

; ------------------------------------------------------------------------------
; Function: print_string
; Print null-terminated string using BIOS interrupt
; Input: SI = pointer to null-terminated string
; Reference: BIOS Interrupt 0x10 - Video Services
; http://www.ctyme.com/intr/rb-0106.htm
; ------------------------------------------------------------------------------
print_string:
    pusha                       ; Push all general-purpose registers (AX,CX,DX,BX,SP,BP,SI,DI)
                                ; We do this to preserve registers for caller
.loop:
    lodsb                       ; LODSB = Load String Byte
                                ; Loads byte from [DS:SI] into AL, then SI++
                                ; This gets next character from string

    or al, al                   ; OR AL with itself sets ZF if AL=0 (null terminator)
    jz .done                    ; Jump if Zero (if null terminator found)

    ; Use BIOS teletype output to print character
    ; INT 0x10, AH=0x0E = Write character in TTY mode
    mov ah, 0x0E                ; AH = Function number (0x0E = teletype output)
    mov bh, 0x00                ; BH = Page number (0 = first page)
    int 0x10                    ; Call BIOS interrupt 0x10 (video services)
                                ; AL already contains character to print

    jmp .loop                   ; Loop to next character

.done:
    popa                        ; Pop all general-purpose registers (restore state)
    ret                         ; Return to caller

; ------------------------------------------------------------------------------
; Function: load_stage2
; Load Stage 2 bootloader from disk using BIOS interrupt
; Reference: BIOS Interrupt 0x13 - Disk Services
; http://www.ctyme.com/intr/rb-0607.htm
;
; CHS Addressing (Cylinder-Head-Sector):
; - Cylinder (Track): Concentric circle on disk
; - Head: Read/write head (0 or 1 for two-sided disk)
; - Sector: Segment of track (1-indexed, typically 1-63)
;
; LBA to CHS conversion:
; Sector   = (LBA % sectors_per_track) + 1
; Head     = (LBA / sectors_per_track) % num_heads
; Cylinder = LBA / (sectors_per_track * num_heads)
; ------------------------------------------------------------------------------
load_stage2:
    pusha                       ; Save all registers

    ; Set up parameters for INT 0x13, AH=0x02 (Read Sectors)
    mov ah, 0x02                ; AH = 0x02 = Read sectors function
    mov al, 15                  ; AL = Number of sectors to read (15 sectors = 7.5KB)
                                ; This is enough for our stage 2 bootloader

    ; CHS addressing for sector 2 (stage 2 starts after MBR)
    ; LBA 1 (second sector) in CHS:
    mov ch, 0x00                ; CH = Cylinder number (lower 8 bits)
    mov cl, 0x02                ; CL = Sector number (bits 0-5), cylinder (bits 6-7)
                                ; Sector 2 (stage 2 immediately follows MBR)
    mov dh, 0x00                ; DH = Head number

    ; DL should already contain boot drive number (BIOS sets it)
    ; But we'll save it to be safe
    mov dl, [BOOT_DRIVE]        ; DL = Drive number (0x00=floppy, 0x80=HDD)

    ; Where to load sectors in memory
    mov bx, KERNEL_OFFSET       ; BX = Buffer address (ES:BX = 0x0000:0x1000)

    ; Call BIOS disk read
    int 0x13                    ; INT 0x13 = BIOS disk services

    ; Check for errors (CF = Carry Flag set on error)
    jc .disk_error              ; Jump if Carry (error occurred)

    ; Check if we read the right number of sectors
    cmp al, 15                  ; AL now contains number of sectors actually read
    jne .disk_error             ; Jump if not equal (didn't read all sectors)

    ; Success!
    mov si, msg_loaded
    call print_string
    popa                        ; Restore registers
    ret

.disk_error:
    ; Print error message and hang
    mov si, msg_disk_error
    call print_string
    jmp $                       ; Infinite loop ($ = current address)

; ------------------------------------------------------------------------------
; Data Section
; ------------------------------------------------------------------------------
BOOT_DRIVE: db 0                ; Storage for boot drive number (BIOS sets DL on entry)

msg_loading:    db "Loading Stage 2...", 0x0D, 0x0A, 0
                                ; 0x0D = Carriage Return, 0x0A = Line Feed, 0 = null terminator

msg_loaded:     db "Stage 2 loaded!", 0x0D, 0x0A, 0

msg_disk_error: db "Disk read error!", 0x0D, 0x0A, 0

; ------------------------------------------------------------------------------
; Boot Signature
; Pad to 510 bytes, then add boot signature
; ------------------------------------------------------------------------------
times 510-($-$$) db 0           ; TIMES = Repeat instruction
                                ; $ = Current address, $$ = Start address of section
                                ; ($-$$) = Size of code so far
                                ; This pads with zeros to byte 510

dw 0xAA55                       ; Boot signature (DW = Define Word = 2 bytes)
                                ; BIOS looks for 0xAA55 at offset 510-511
                                ; Little-endian: 0x55 at byte 510, 0xAA at byte 511
