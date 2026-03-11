/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "game_types.h"

/* Statistics */
void _ResetGoalAttempt(void);
void _AllowStatistics(void);
void _StatisticsOff(void);

/* Time */
void _ResetTime(void);
void _UpdateTime(void);

/* Player half-tracking */
void _BumpPlayersLastPlayedHalfAtHalfStart(void);
void _BumpPlayersLastPlayedHalfAtHalfEnd(void);

/* Team setup */
void _CopyPlayerNameLengths(void);

/* Goal sprites */
void _ClearSaveFlagForGoalSprites(void);

/* Result display */
void _ResetResultSprites(void);
void _ShowResult(void);

/* Goal highlight */
void _InitCurrentGoalHighlight(void);

#endif
