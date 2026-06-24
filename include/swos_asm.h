/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef SWOS_ASM_H
#define SWOS_ASM_H

typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

extern void startShim(void);
extern void _start(void);
extern volatile uint16_t panicReasonCode;

#endif