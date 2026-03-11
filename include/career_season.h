/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
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

/* --- Chunk 161: Season opinion --- */
void _ShowSeasonOpinion(void);

/* --- Career_UpdateLastNationalityCall (Chunk 170) --- */
uint16_t _Career_UpdateLastNationalityCall(void);

/* --- Chunk 174: Season Review functions --- */
void _SeasonFinished_ComputeSeasonReview(void);
void _ShowChairmanOpinionAboutTheSeason(void);

/* --- Chunk 181: Contract Negotiation --- */
void _BeginContractNegotiationSequence(uint8_t *ctx, uint8_t *team);

/* --- Chunk 182: Competition Season Processing --- */
void _Career_ProcessSecondaryCompetitionForNextSeason(uint8_t *competition);
void _Career_ProcessTertiaryCompetitionForNextSeason(uint8_t *competition);
void _Career_ProcessCompetitionForNextSeason(uint16_t compId);

/* --- Chunk 185: Cup Pick List System --- */
void _Career_SetCupPickList_ForSelectedRegion(uint16_t *teamList);

/* --- Chunk 186: Season Update Functions --- */
void _Career_AdvanceToNextSeason_UpdateCompetitions(void);
