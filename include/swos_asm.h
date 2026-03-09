#ifndef SWOS_ASM_H
#define SWOS_ASM_H

typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

extern void _startShim(void);
extern void _start(void);
extern volatile uint16_t panicReasonCode;

#endif