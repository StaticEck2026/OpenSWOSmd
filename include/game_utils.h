/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef GAME_UTILS_H
#define GAME_UTILS_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef short int16_t;

void _AssetLoaderResetState(void);
void _WaitForVSync(void);
void _InitSkinAndDefaultPointers(void);
void _BuildPaletteFromColors(const uint8_t *src, uint16_t *dst);
uint16_t _ReadTimerDelta(void);
void _Randomize2(uint16_t val);

#endif
