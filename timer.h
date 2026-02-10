/**
 * timer.h - PIT Timer Driver Header
 */
#ifndef TIMER_H
#define TIMER_H

#include "kernel.h"

void timer_init(uint32_t frequency);
uint32_t timer_get_ticks(void);
void timer_wait(uint32_t ticks);

#endif
