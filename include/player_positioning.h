/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef PLAYER_POSITIONING_H
#define PLAYER_POSITIONING_H

#include "game_types.h"

void SetPlayerWithNoBallDestination(Sprite *player, TeamGeneralInfo *team, int16_t ballX, int16_t ballY);

#endif /* PLAYER_POSITIONING_H */
