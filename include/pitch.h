/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef PITCH_H
#define PITCH_H

#include "game_types.h"

void _LoadPitchMap(uint16_t mapIndex);
void _FixupAndColorPitchFile(void);
void _AdjustFanColor(uint8_t *destPattern, const uint16_t *convTable);

#endif
