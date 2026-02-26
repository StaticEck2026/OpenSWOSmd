#ifndef REFEREE_H
#define REFEREE_H

#include "game_types.h"

void _InitRefereeAnimationTable(uint8_t *animTable);
void _ActivateReferee(void);
void _SetRefereeToLeavingState(void);
void _UpdateReferee(void);
void _UpdateBookedPlayerNumberSprite(void);

#endif
