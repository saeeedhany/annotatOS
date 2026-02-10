#ifndef PROCESS_H
#define PROCESS_H

#include "kernel.h"

void process_init(void);
int process_create(void (*entry)(void), const char *name);
void process_yield(void);
void process_exit(void);
void process_list(void);

#endif
