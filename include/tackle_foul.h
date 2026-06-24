/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef TACKLE_FOUL_H
#define TACKLE_FOUL_H

#include "game_types.h"

/* ---- Exported functions ---- */
void    PlayerTackledTheBallWeak(Sprite *player, TeamGeneralInfo *team);
void    SetPlayerDowntimeAfterTackle(Sprite *player, TeamGeneralInfo *team);
void    PlayerTackled(Sprite *player, TeamGeneralInfo *team);
int16_t TryBookingThePlayer(Sprite *player, TeamGeneralInfo *team);
int16_t TrySendingOffThePlayer(Sprite *player, TeamGeneralInfo *team);
void    SendPlayerAway(Sprite *player, TeamGeneralInfo *team);
void    PlayerTacklingTestFoul(Sprite *player, TeamGeneralInfo *team);
void    GoalkeeperCaughtTheBall(Sprite *keeper, TeamGeneralInfo *team);
void    PlayerTackledTheBallStrong(Sprite *player, TeamGeneralInfo *team);
void    TestFoulForPenaltyAndFreeKick(Sprite *victim, TeamGeneralInfo *team);
void    PlayerBeginTackling(int16_t pressedDir, Sprite *player, TeamGeneralInfo *team);

#endif
