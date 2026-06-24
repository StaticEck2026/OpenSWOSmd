/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef HEADERS_H
#define HEADERS_H

#include "game_types.h"

void SetJumpHeaderHitAnimTable(Sprite *player);
void PlayerHittingJumpHeader(Sprite *player, TeamGeneralInfo *team);
void SetStaticHeaderDirection(Sprite *player, TeamGeneralInfo *team);
void PlayerHittingStaticHeader(Sprite *player, TeamGeneralInfo *team);
void AttemptStaticHeader(Sprite *player, uint16_t dir);
void PlayerAttemptingJumpHeader(uint16_t dir, Sprite *player);

#endif /* HEADERS_H */
