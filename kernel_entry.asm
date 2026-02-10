; ==============================================================================
; KERNEL_ENTRY.ASM - Kernel Entry Point
; ==============================================================================
; This is where the bootloader jumps after setting up Protected Mode.
; Our job here is to:
; 1. Set up a proper execution environment for C code
; 2. Call the C kernel main function
; 3. Handle the return from main (infinite loop)
;
; Reference: OSDev Wiki - Bare Bones
; https://wiki.osdev.org/Bare_Bones
; ==============================================================================

[BITS 32]                       ; We're in 32-bit Protected Mode

; External references
; These are functions/variables defined in our C kernel
extern kernel_main              ; Main C kernel function (defined in kernel.c)

; Global symbols that can be accessed from C
global _start                   ; Entry point (linker needs this)
global gdt_flush                ; Function to reload GDT
global idt_flush                ; Function to load IDT
global isr_common_stub          ; Common ISR handler stub
global irq_common_stub          ; Common IRQ handler stub
global read_port                ; Read from I/O port
global write_port               ; Write to I/O port

; External C handlers
extern isr_handler              ; C function to handle ISRs
extern irq_handler              ; C function to handle IRQs

; ------------------------------------------------------------------------------
; Entry Point
; ------------------------------------------------------------------------------
_start:
    ; At this point:
    ; - We're in 32-bit Protected Mode
    ; - GDT is loaded (from stage2)
    ; - Segments are set up
    ; - Stack is at 0x90000
    
    ; Initialize the stack (ensure it's properly aligned for C code)
    mov esp, 0x90000            ; ESP = Stack pointer
    mov ebp, esp                ; EBP = Base pointer (for stack frames)
    
    ; Clear the direction flag (DF)
    ; When DF=0, string operations increment (EDI++, ESI++)
    ; When DF=1, string operations decrement (EDI--, ESI--)
    ; C code expects DF=0
    cld                         ; CLD = Clear Direction Flag
    
    ; Call the C kernel main function
    ; Parameters are passed on the stack in C calling convention
    ; We're not passing any parameters to kernel_main()
    call kernel_main            ; Call C function
    
    ; If kernel_main returns (it shouldn't), halt the CPU
.hang:
    cli                         ; Disable interrupts
    hlt                         ; Halt CPU (wait for interrupt, but they're disabled)
    jmp .hang                   ; If we somehow continue, loop forever

; ------------------------------------------------------------------------------
; GDT Flush - Reload GDT and segment registers
; ------------------------------------------------------------------------------
; Called from C: gdt_flush(uint32_t gdt_ptr)
; Reference: OSDev Wiki - GDT Tutorial
; https://wiki.osdev.org/GDT_Tutorial
gdt_flush:
    ; Get GDT pointer from stack (parameter)
    mov eax, [esp + 4]          ; EAX = first parameter (pointer to GDT descriptor)
    
    ; Load GDT
    lgdt [eax]                  ; Load GDT from address in EAX
    
    ; Reload segment registers
    ; We need to do a far jump to reload CS
    mov ax, 0x10                ; 0x10 = Data segment selector
    mov ds, ax                  ; Reload all data segment registers
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Far jump to reload CS (code segment)
    jmp 0x08:.flush             ; 0x08 = Code segment selector
                                ; Far jump: sets CS to 0x08 and jumps to .flush
.flush:
    ret                         ; Return to caller

; ------------------------------------------------------------------------------
; IDT Flush - Load IDT
; ------------------------------------------------------------------------------
; Called from C: idt_flush(uint32_t idt_ptr)
; Reference: OSDev Wiki - IDT
; https://wiki.osdev.org/IDT
idt_flush:
    mov eax, [esp + 4]          ; EAX = first parameter (pointer to IDT descriptor)
    lidt [eax]                  ; LIDT = Load Interrupt Descriptor Table
    ret

; ------------------------------------------------------------------------------
; I/O Port Functions
; ------------------------------------------------------------------------------
; Reference: OSDev Wiki - Inline Assembly/Examples
; https://wiki.osdev.org/Inline_Assembly/Examples
;
; IN and OUT instructions:
; - IN: Read from I/O port to AL/AX/EAX
; - OUT: Write from AL/AX/EAX to I/O port
;
; I/O ports are a separate address space from memory
; Common ports:
; - 0x20-0x21: PIC Master
; - 0x60: Keyboard data
; - 0x64: Keyboard status/command
; - 0xA0-0xA1: PIC Slave
; - 0x3D4-0x3D5: VGA CRT Controller

; uint8_t read_port(uint16_t port)
read_port:
    mov edx, [esp + 4]          ; EDX = port number (first parameter)
    xor eax, eax                ; Clear EAX (we only want to use AL)
    in al, dx                   ; IN AL, DX: Read byte from port in DX to AL
                                ; DX holds port number
                                ; AL receives the data
    ret                         ; Return value in EAX (caller only uses AL)

; void write_port(uint16_t port, uint8_t data)
write_port:
    mov edx, [esp + 4]          ; EDX = port number (first parameter)
    mov al, [esp + 8]           ; AL = data byte (second parameter)
    out dx, al                  ; OUT DX, AL: Write byte from AL to port in DX
    ret

; ------------------------------------------------------------------------------
; ISR Stubs - Interrupt Service Routine Entry Points
; ------------------------------------------------------------------------------
; Reference: OSDev Wiki - Interrupt Service Routines
; https://wiki.osdev.org/Interrupt_Service_Routines
;
; CPU Exceptions (ISRs 0-31):
; When an exception occurs, the CPU:
; 1. Pushes SS, ESP, EFLAGS, CS, EIP on stack (if privilege change)
; 2. Pushes error code (for some exceptions)
; 3. Jumps to ISR handler in IDT
;
; We need to:
; 1. Save processor state
; 2. Call C handler
; 3. Restore processor state
; 4. Return from interrupt

; Macro to create ISR stub without error code
; Some exceptions don't push an error code, so we push a dummy 0
%macro ISR_NOERRCODE 1
global isr%1                    ; Make this ISR globally visible
isr%1:
    cli                         ; Disable interrupts
    push byte 0                 ; Push dummy error code (to keep stack consistent)
    push byte %1                ; Push interrupt number
    jmp isr_common_stub         ; Jump to common handler
%endmacro

; Macro to create ISR stub with error code
; Some exceptions push an error code automatically
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli                         ; Disable interrupts
    ; CPU already pushed error code
    push byte %1                ; Push interrupt number
    jmp isr_common_stub         ; Jump to common handler
%endmacro

; Define all 32 CPU exception handlers
; Reference: Intel Manual Vol 3A, Table 6-1
ISR_NOERRCODE 0                 ; 0: Divide By Zero Exception
ISR_NOERRCODE 1                 ; 1: Debug Exception
ISR_NOERRCODE 2                 ; 2: Non Maskable Interrupt Exception
ISR_NOERRCODE 3                 ; 3: Breakpoint Exception
ISR_NOERRCODE 4                 ; 4: Into Detected Overflow Exception
ISR_NOERRCODE 5                 ; 5: Out of Bounds Exception
ISR_NOERRCODE 6                 ; 6: Invalid Opcode Exception
ISR_NOERRCODE 7                 ; 7: No Coprocessor Exception
ISR_ERRCODE   8                 ; 8: Double Fault Exception (WITH ERROR CODE)
ISR_NOERRCODE 9                 ; 9: Coprocessor Segment Overrun Exception
ISR_ERRCODE   10                ; 10: Bad TSS Exception (WITH ERROR CODE)
ISR_ERRCODE   11                ; 11: Segment Not Present Exception (WITH ERROR CODE)
ISR_ERRCODE   12                ; 12: Stack Fault Exception (WITH ERROR CODE)
ISR_ERRCODE   13                ; 13: General Protection Fault Exception (WITH ERROR CODE)
ISR_ERRCODE   14                ; 14: Page Fault Exception (WITH ERROR CODE)
ISR_NOERRCODE 15                ; 15: Reserved Exception
ISR_NOERRCODE 16                ; 16: Floating Point Exception
ISR_NOERRCODE 17                ; 17: Alignment Check Exception
ISR_NOERRCODE 18                ; 18: Machine Check Exception
ISR_NOERRCODE 19                ; 19: SIMD Floating-Point Exception
ISR_NOERRCODE 20                ; 20: Virtualization Exception
ISR_NOERRCODE 21                ; 21: Reserved
ISR_NOERRCODE 22                ; 22: Reserved
ISR_NOERRCODE 23                ; 23: Reserved
ISR_NOERRCODE 24                ; 24: Reserved
ISR_NOERRCODE 25                ; 25: Reserved
ISR_NOERRCODE 26                ; 26: Reserved
ISR_NOERRCODE 27                ; 27: Reserved
ISR_NOERRCODE 28                ; 28: Reserved
ISR_NOERRCODE 29                ; 29: Reserved
ISR_ERRCODE   30                ; 30: Security Exception (WITH ERROR CODE)
ISR_NOERRCODE 31                ; 31: Reserved

; ------------------------------------------------------------------------------
; IRQ Stubs - Hardware Interrupt Request Entry Points
; ------------------------------------------------------------------------------
; Reference: OSDev Wiki - PIC
; https://wiki.osdev.org/PIC
;
; IRQs are remapped from their default (which conflicts with CPU exceptions)
; to interrupts 32-47 in our IDT
; Default mapping: IRQ 0-7 → INT 0x08-0x0F, IRQ 8-15 → INT 0x70-0x77
; Our mapping: IRQ 0-7 → INT 32-39, IRQ 8-15 → INT 40-47

%macro IRQ 2
global irq%1
irq%1:
    cli                         ; Disable interrupts
    push byte 0                 ; Push dummy error code
    push byte %2                ; Push IRQ number (32 + IRQ number)
    jmp irq_common_stub         ; Jump to common handler
%endmacro

IRQ 0, 32                       ; IRQ 0: PIT (Programmable Interval Timer)
IRQ 1, 33                       ; IRQ 1: Keyboard
IRQ 2, 34                       ; IRQ 2: Cascade (used internally by PICs)
IRQ 3, 35                       ; IRQ 3: COM2
IRQ 4, 36                       ; IRQ 4: COM1
IRQ 5, 37                       ; IRQ 5: LPT2
IRQ 6, 38                       ; IRQ 6: Floppy disk
IRQ 7, 39                       ; IRQ 7: LPT1
IRQ 8, 40                       ; IRQ 8: CMOS real-time clock
IRQ 9, 41                       ; IRQ 9: Free for peripherals
IRQ 10, 42                      ; IRQ 10: Free for peripherals
IRQ 11, 43                      ; IRQ 11: Free for peripherals
IRQ 12, 44                      ; IRQ 12: PS/2 Mouse
IRQ 13, 45                      ; IRQ 13: FPU / Coprocessor / Inter-processor
IRQ 14, 46                      ; IRQ 14: Primary ATA Hard Disk
IRQ 15, 47                      ; IRQ 15: Secondary ATA Hard Disk

; ------------------------------------------------------------------------------
; Common ISR Handler
; ------------------------------------------------------------------------------
; This stub saves the processor state, calls the C handler, and restores state
isr_common_stub:
    ; Save all general-purpose registers
    pusha                       ; Pushes: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    
    ; Save segment registers
    push ds                     ; Push data segment
    push es                     ; Push extra segment
    push fs                     ; Push FS segment
    push gs                     ; Push GS segment
    
    ; Load kernel data segment
    mov ax, 0x10                ; 0x10 = Kernel data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Push ESP (pointer to stack with all saved registers)
    ; This becomes the parameter to isr_handler()
    mov eax, esp
    push eax
    
    ; Call C handler: void isr_handler(registers_t *regs)
    call isr_handler
    
    ; Remove parameter from stack
    pop eax
    
    ; Restore segment registers
    pop gs
    pop fs
    pop es
    pop ds
    
    ; Restore general-purpose registers
    popa                        ; Pops: EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX
    
    ; Clean up error code and ISR number from stack
    add esp, 8                  ; ESP += 8 (skip error code and interrupt number)
    
    ; Enable interrupts and return
    sti                         ; STI = Set Interrupt Flag (re-enable interrupts)
    iret                        ; IRET = Interrupt Return
                                ; Pops: EIP, CS, EFLAGS (and ESP, SS if privilege change)

; ------------------------------------------------------------------------------
; Common IRQ Handler
; ------------------------------------------------------------------------------
irq_common_stub:
    ; Save processor state (same as ISR)
    pusha
    push ds
    push es
    push fs
    push gs
    
    ; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Call C handler
    mov eax, esp
    push eax
    call irq_handler
    pop eax
    
    ; Restore state
    pop gs
    pop fs
    pop es
    pop ds
    popa
    
    ; Clean up stack
    add esp, 8
    
    ; Return from interrupt
    sti
    iret
