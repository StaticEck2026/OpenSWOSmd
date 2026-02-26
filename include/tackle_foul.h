#ifndef TACKLE_FOUL_H
#define TACKLE_FOUL_H

#include "game_types.h"

/* ---- Exported functions ---- */
void    _PlayerTackledTheBallWeak(Sprite *player, TeamGeneralInfo *team);
void    _SetPlayerDowntimeAfterTackle(Sprite *player, TeamGeneralInfo *team);
void    _PlayerTackled(Sprite *player, TeamGeneralInfo *team);
int16_t _TryBookingThePlayer(Sprite *player, TeamGeneralInfo *team);
int16_t _TrySendingOffThePlayer(Sprite *player, TeamGeneralInfo *team);
void    _SendPlayerAway(Sprite *player, TeamGeneralInfo *team);
void    _PlayerTacklingTestFoul(Sprite *player, TeamGeneralInfo *team);
void    _GoalkeeperCaughtTheBall(Sprite *keeper, TeamGeneralInfo *team);
void    _PlayerTackledTheBallStrong(Sprite *player, TeamGeneralInfo *team);
void    _TestFoulForPenaltyAndFreeKick(Sprite *victim, TeamGeneralInfo *team);
void    _PlayerBeginTackling(int16_t pressedDir, Sprite *player, TeamGeneralInfo *team);

#endif
