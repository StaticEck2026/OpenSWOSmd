/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef REFEREE_H
#define REFEREE_H

#include "game_types.h"

void InitRefereeAnimationTable(uint8_t *animTable);
void ActivateReferee(void);
void SetRefereeToLeavingState(void);
void UpdateReferee(void);
void UpdateBookedPlayerNumberSprite(void);

#endif
