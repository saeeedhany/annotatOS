/*
 * SYSTEM-LEVEL OVERVIEW
 *
 * This linker script defines the physical image layout for the freestanding
 * kernel binary loaded by the boot sector. Because the bootloader copies the
 * kernel blob to address 0x1000 and performs a direct jump there, link-time
 * virtual addresses must match that runtime physical placement.
 *
 * Boot-time behavior:
 * - Bootloader reads sectors into memory at 0x1000.
 * - CPU jumps to `_start` resolved in `.text` at that base.
 *
 * Runtime behavior:
 * - Section placement remains static for entire machine lifetime; there is no
 *   loader relocation, no paging remap, and no runtime rebasing.
 *
 * Memory behavior and section data structures:
 * - `.text`: executable machine code, read-only by convention.
 * - `.data` + `.rodata`: initialized writable data and constants packed
 *   contiguously in file image.
 * - `.bss` + COMMON: zero-initialized storage; in a raw binary output this
 *   region contributes to image size and lands as explicit bytes.
 *
 * CPU-level implications:
 * - Flat low-memory placement keeps segment arithmetic simple in real mode.
 * - Entry symbol must be reachable with 16-bit code generation model.
 *
 * Limitations and edge cases:
 * - No alignment directives beyond defaults; larger projects should add page/
 *   paragraph alignment constraints explicitly.
 * - No symbol exports for debugging metadata due to OUTPUT_FORMAT(binary).
 */

OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    . = 0x1000;                 /* Physical load/execute base coordinated with boot.asm */

    .text : {
        *(.text)
    }

    .data : {
        *(.data)
        *(.rodata)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }
}
