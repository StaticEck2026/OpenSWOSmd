/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef REFEREE_H
#define REFEREE_H

#include "game_types.h"

void _InitRefereeAnimationTable(uint8_t *animTable);
void _ActivateReferee(void);
void _SetRefereeToLeavingState(void);
void _UpdateReferee(void);
void _UpdateBookedPlayerNumberSprite(void);

#endif
