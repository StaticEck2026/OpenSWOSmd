/* career_season.h — Career season management, team list building, qualification. */
#pragma once
#include "game_types.h"

/* --- Random Team List Filling --- */
void _Career_FillRandomTeamListExcludingSelected(uint16_t excludeCount,
                                                  uint8_t *table, uint8_t *compIds);

/* --- European Cup Qualification State --- */
void _Career_PrepareEuropeanCupQualificationState(void);

/* --- Competition abbreviation (Chunk 145) --- */
uint8_t *_GetCompetitionAbbrevStr(uint8_t *competition);
