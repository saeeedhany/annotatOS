/**
 * ==============================================================================
 * TIMER.C - PIT (Programmable Interval Timer) Driver
 * ==============================================================================
 * The PIT is a hardware timer that generates IRQ0 at a programmable frequency.
 * We use it as our system clock for time tracking and task scheduling.
 * 
 * PIT Hardware:
 * - Base frequency: 1.193182 MHz (1193182 Hz)
 * - Channel 0: Connected to IRQ0 (system timer)
 * - Channel 1: RAM refresh (legacy, not used)
 * - Channel 2: PC speaker
 * 
 * To set frequency: divisor = 1193182 / desired_frequency
 * Example: 100 Hz → divisor = 11931
 * 
 * Reference: OSDev Wiki - Programmable Interval Timer
 * https://wiki.osdev.org/Programmable_Interval_Timer
 * ==============================================================================
 */

#include "timer.h"
#include "idt.h"
#include "screen.h"

/* PIT I/O ports */
#define PIT_CHANNEL0  0x40  /* Channel 0 data port (R/W) */
#define PIT_CHANNEL1  0x41  /* Channel 1 data port (R/W) */
#define PIT_CHANNEL2  0x42  /* Channel 2 data port (R/W) */
#define PIT_COMMAND   0x43  /* Mode/Command register (W) */

/* PIT base frequency in Hz */
#define PIT_FREQUENCY 1193182

/* Global tick counter */
static volatile uint32_t tick_count = 0;

/**
 * Timer interrupt handler
 * Called on every timer tick (IRQ0)
 */
static void timer_callback(registers_t *regs) {
    (void)regs; /* Unused parameter */
    tick_count++;
}

/**
 * Initialize PIT timer
 * frequency: Desired timer frequency in Hz (e.g., 100 = 100 ticks/second)
 */
void timer_init(uint32_t frequency) {
    /* Register IRQ0 handler */
    irq_register_handler(0, timer_callback);
    
    /* Calculate divisor from desired frequency */
    uint32_t divisor = PIT_FREQUENCY / frequency;
    
    /* Send command byte: Channel 0, lobyte/hibyte, rate generator */
    /* Command format: [Channel][Access Mode][Operating Mode][BCD]
     * - Channel: 00 (Channel 0)
     * - Access: 11 (lobyte/hibyte)
     * - Mode: 011 (square wave generator)
     * - BCD: 0 (binary mode)
     * Result: 00110110 = 0x36
     */
    write_port(PIT_COMMAND, 0x36);
    
    /* Send divisor (low byte, then high byte) */
    write_port(PIT_CHANNEL0, divisor & 0xFF);         /* Low byte */
    write_port(PIT_CHANNEL0, (divisor >> 8) & 0xFF);  /* High byte */
}

/**
 * Get current tick count
 */
uint32_t timer_get_ticks(void) {
    return tick_count;
}

/**
 * Wait for specified number of ticks
 */
void timer_wait(uint32_t ticks) {
    uint32_t end_tick = tick_count + ticks;
    while(tick_count < end_tick) {
        /* Halt CPU until next interrupt to save power */
        __asm__ __volatile__("hlt");
    }
}
