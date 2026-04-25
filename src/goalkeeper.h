/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef GOALKEEPER_H
#define GOALKEEPER_H

#include "game_types.h"

/* ---- Exported functions ---- */
int16_t __attribute__((used, externally_visible)) _ShouldGoalkeeperDive(Sprite *keeper, Sprite *ball, TeamGeneralInfo *team);
void    __attribute__((used, externally_visible)) _GoalkeeperDeflectedBall(Sprite *ball, TeamGeneralInfo *team);
void    __attribute__((used, externally_visible)) _NextPenalty(void);
void    __attribute__((used, externally_visible)) _GoalkeeperClaimedTheBall(Sprite *keeper, Sprite *ball, TeamGeneralInfo *team);
void    __attribute__((used, externally_visible)) _CheckIfGoalkeeperClaimedTheBall(void);
void    __attribute__((used, externally_visible)) _GoalkeeperJumping(int16_t diveDir, int16_t speedVariant, int16_t destDir, Sprite *keeper, Sprite *ball, TeamGeneralInfo *team);

#endif
