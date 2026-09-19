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

/* Whether THIS FILE books and sends off players -- 0 when it was compiled with
 * SWOS_TEST_NO_INJURIES_CARDS. The netplay handshake asks it (p2p_link.c), here and not
 * in the file doing the asking, so a binary compiled partly with the flag reports what
 * its fouls will actually do: two peers that differ in this desync at the first booking
 * (2026-09-19, a Windows build with a stale tackle_foul.o against a clean Mac build). */
int32_t TackleFoul_BookingsEnabled(void);
void    PlayerTackledTheBallStrong(Sprite *player, TeamGeneralInfo *team);
void    TestFoulForPenaltyAndFreeKick(Sprite *victim, TeamGeneralInfo *team);
void    PlayerBeginTackling(int16_t pressedDir, Sprite *player, TeamGeneralInfo *team);

#endif
