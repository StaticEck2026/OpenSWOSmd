/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef RANDOM_H
#define RANDOM_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef short int16_t;
typedef long int32_t;

uint16_t _Rand(void);
uint16_t _Rand2(void);
uint16_t _GetRandomWord(void);
void _SaveRand2State(void);
void _RestoreRand2State(void);

#endif
