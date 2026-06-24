/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef RANDOM_H
#define RANDOM_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef short int16_t;
typedef int int32_t;

uint16_t Rand(void);
uint16_t Rand2(void);
uint16_t GetRandomWord(void);
void SaveRand2State(void);
void RestoreRand2State(void);

#endif
