/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* career_eurocups.h — European Cup qualification, grid caching, title holder management */
#pragma once
#include "game_types.h"

/* --- Grid Caching --- */
void _DIY_CachePrimaryQualifiedTeamsGrid(void);
void _DIY_CacheSecondaryQualifiedTeamsGrid(void);
void _DIY_CacheTertiaryQualifiedTeamGrid(void);

/* --- Title Holder Grid Copy --- */
void _Career_CopyEuroCupTeamGridAndTitleHolder(void);
void _Career_CopyCupWinnersCupTeamGridAndTitleHolder(void);
void _Career_CopyUefaCupTeamGridAndTitleHolder(void);

/* --- European Cup Qualification --- */
void _Career_ProcessCompetitionForEuropeanCupQualification(void);

/* --- Euro Cup Slot Updates (Chunk 181) --- */
void _Career_UpdateEuroCupCompetitionSlot(void);
void _Career_UpdateCupWinnersCupCompetitionSlot(void);
void _Career_UpdateUefaCupCompetitionSlot(void);

/* --- European Cup Team Draw (Chunk 183) --- */
void _Career_DrawEuropeanCupTeamsAndCopyGrids(void);

/* --- Championship/World Cup Updates (Chunk 184) --- */
void _Career_UpdateEuropeanChampionshipCompetitions(void);
void _Career_UpdateWorldCupCompetitions(void);
