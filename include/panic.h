#ifndef PANIC_H
#define PANIC_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

void _PanicHaltLoop(void) __attribute__((noreturn));

#endif
