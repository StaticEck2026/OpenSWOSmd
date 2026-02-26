#ifndef HEADERS_H
#define HEADERS_H

#include "game_types.h"

void _SetJumpHeaderHitAnimTable(Sprite *player);
void _PlayerHittingJumpHeader(Sprite *player, TeamGeneralInfo *team);
void _SetStaticHeaderDirection(Sprite *player, TeamGeneralInfo *team);
void _PlayerHittingStaticHeader(Sprite *player, TeamGeneralInfo *team);
void _AttemptStaticHeader(Sprite *player, uint16_t dir);
void _PlayerAttemptingJumpHeader(uint16_t dir, Sprite *player);

#endif /* HEADERS_H */
