/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* career_eurocups.h — European Cup qualification, grid caching, title holder management */
#pragma once
#include "game_types.h"

/* --- Grid Caching --- */
void DIY_CachePrimaryQualifiedTeamsGrid(void);
void DIY_CacheSecondaryQualifiedTeamsGrid(void);
void DIY_CacheTertiaryQualifiedTeamGrid(void);

/* --- Title Holder Grid Copy --- */
void Career_CopyEuroCupTeamGridAndTitleHolder(void);
void Career_CopyCupWinnersCupTeamGridAndTitleHolder(void);
void Career_CopyUefaCupTeamGridAndTitleHolder(void);

/* --- European Cup Qualification --- */
void Career_ProcessCompetitionForEuropeanCupQualification(void);

/* --- Euro Cup Slot Updates (Chunk 181) --- */
void Career_UpdateEuroCupCompetitionSlot(void);
void Career_UpdateCupWinnersCupCompetitionSlot(void);
void Career_UpdateUefaCupCompetitionSlot(void);

/* --- European Cup Team Draw (Chunk 183) --- */
void Career_DrawEuropeanCupTeamsAndCopyGrids(void);

/* --- Championship/World Cup Updates (Chunk 184) --- */
void Career_UpdateEuropeanChampionshipCompetitions(void);
void Career_UpdateWorldCupCompetitions(void);
