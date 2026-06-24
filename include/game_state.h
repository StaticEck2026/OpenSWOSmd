/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "game_types.h"

/* Statistics */
void ResetGoalAttempt(void);
void AllowStatistics(void);
void StatisticsOff(void);

/* Time */
void ResetTime(void);
void UpdateTime(void);

/* Player half-tracking */
void BumpPlayersLastPlayedHalfAtHalfStart(void);
void BumpPlayersLastPlayedHalfAtHalfEnd(void);

/* Team setup */
void CopyPlayerNameLengths(void);

/* Goal sprites */
void ClearSaveFlagForGoalSprites(void);

/* Result display */
void ResetResultSprites(void);
void ShowResult(void);

/* Goal highlight */
void InitCurrentGoalHighlight(void);

#endif
