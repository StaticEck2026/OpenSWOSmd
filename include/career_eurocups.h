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
