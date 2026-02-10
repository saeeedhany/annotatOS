/**
 * ==============================================================================
 * GDT.H - Global Descriptor Table Header
 * ==============================================================================
 */

#ifndef GDT_H
#define GDT_H

#include "kernel.h"

/* Initialize GDT */
void gdt_init(void);

/* Segment selectors for loading into segment registers */
#define KERNEL_CODE_SELECTOR 0x08   /* GDT entry 1, Ring 0 */
#define KERNEL_DATA_SELECTOR 0x10   /* GDT entry 2, Ring 0 */
#define USER_CODE_SELECTOR   0x18   /* GDT entry 3, Ring 3 */
#define USER_DATA_SELECTOR   0x20   /* GDT entry 4, Ring 3 */

#endif /* GDT_H */
