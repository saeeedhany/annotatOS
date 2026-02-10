/**
 * ==============================================================================
 * SCREEN.H - VGA Text Mode Driver Header
 * ==============================================================================
 */

#ifndef SCREEN_H
#define SCREEN_H

#include "kernel.h"

/* Screen functions */
void screen_clear(void);
void screen_putchar(char c);
void screen_write(const char *str);
void screen_write_len(const char *str, int len);
void screen_write_hex(uint32_t n);
void screen_write_dec(uint32_t n);
void screen_backspace(void);
void screen_set_color(uint8_t fg, uint8_t bg);
void screen_get_cursor(int *row, int *col);
void screen_set_cursor(int row, int col);

#endif /* SCREEN_H */
