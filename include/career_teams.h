/* career_teams.h — Team cache lookup, save/restore selected teams */
#pragma once
#include "game_types.h"

/* --- Team league lookup (Chunk 151) --- */
int16_t _GetTeamLeague(uint16_t teamNumber);

/* --- Team lookup --- */
uint8_t *_GetTeamPointer(uint16_t teamId);
uint8_t *_VerifyTeamLoaded(uint16_t teamNum);
void _CopySelectedTeamNumbers(uint16_t count, uint8_t *dest);

/* --- Save/Restore --- */
void _SaveSelectedTeams(void);
void _RestoreSelectedTeams(void);

/* --- Key cache (Chunk 76) --- */
uint8_t *_TeamSelectionKeyCache_InsertSpaceSorted(uint16_t key, uint16_t insertionBytes);

/* --- TMD team pool management (Chunk 77) --- */
int16_t _SearchTeamInTmdFile(uint16_t teamNo);
void _Career_RemoveTeamFromTmdTeamPool(uint16_t teamNo);
void _Career_AppendTeamToTmdTeamPool(uint8_t *team);

/* --- Career competition team selection (Chunk 90) --- */
void _Career_SortTeamFilePtrsByAvgPlayerPrice(uint16_t extNumber);
void _LoadSomeEuroCup(uint8_t *whichCup);
void _AppendTeamsToSelectionFromTmdOrTeamFile(uint16_t *out, uint16_t *teamIds, uint16_t count);
void _Career_TryAddRandomTeamByRegionToLists(void);
void _FatalError(void);
void _Career_InitCompetitionTeamSelectionFromTableEntrySpec(uint8_t *spec, uint16_t idx);
void _Career_InitCompetitionTeamSelectionFromTableHeaderSpec(uint8_t *spec);
void _Career_InitCompetitionTeamSelectionFromAltTableHeaderSpec(uint8_t *spec);
void _Career_InitCompetitionTeamSelection_Dispatch(uint8_t *spec, uint16_t idx);

/* --- Competition state buffer copy (Chunk 92) --- */
void _Career_LoadCompetitionSlotStateFromBuffer(uint8_t *src);
void _Career_StoreCompetitionSlotStateToBuffer(uint8_t *dst);
void _Career_CopyCompetitionFileBufferToOrFromActive(void);
void _Career_CopyCompetitionContextToOrFromActive(void);
void _Career_CopyTertiaryCompetitionContextToOrFromActive(void);
void _Career_CopyEuroCupsCompetitionContextToOrFromActive(void);

/* --- Team selection key cache navigation (Chunk 92) --- */
uint8_t *_TeamSelectionKeyCache_GetListPtr(uint16_t listIndex);
uint8_t *_TeamSelectionKeyCache_GetListElementPtr(uint16_t listIndex, uint16_t elementIndex);
uint8_t *_TeamSelectionKeyCache_GetCurrentListPtr(void);
uint8_t *_TeamSelectionKeyCache_GetCurrentSelectedElementPtr(void);

/* --- Career state management (Chunk 92) --- */
void _Career_UpdateCareerFinishedFlag(void);
void _Career_AdvanceToNextTeamSelectionKeyCacheEntry(void);

/* --- Career match flow (Chunk 93) --- */
void _CareerPlayMatch(void);
void _Career_PlayMatchMenuLoop(void);
void _Career_NextMatch(void);
void _Career_OpenCompetitionSlotMenuAndApplyChanges(uint16_t d2);
void _Career_ProcessCurrentCompetitionSlotForMatch(void);
void _Career_ProcessCurrentCompetitionSlotAfterMatch(void);

/* --- Career auto-simulate & competition slot advancement (Chunk 94) --- */
void _Career_AutoSimulateLeagueFixturesUntilHumanTeam(void);
void _Career_AutoSimulateCupMatchesUntilHumanTeam(void);
void _Career_AutoSimulatePendingMatchUntilHumanTeam(void);
void _Career_AutoPlayOrSimulateLeagueFixtureLoop(void);
void _Career_AutoPlayCupMatchAndAdvanceLoop(void);
void _Career_UpdateHybridCompetitionProgressAndMaybeAdvanceSlot(void);
void _DIY_AdvanceToNextCompetitionSlot(void);

/* --- NationalTeamAddPlayer callbacks (Chunk 108) --- */
void _NationalTeamAddPlayerHomeOnSelect(void);
void _NationalTeamAddPlayerAbroadOnSelect(void);
void _NationalTeamAddPlayerExit(void);

/* --- National team menu update helpers (Chunk 148) --- */
void _Career_UpdateNationalTeamMenuEntries(void);
void _Career_UpdateRemovePlayersMenuEntry(void);

/* --- Career_CopyUnavailablePlayerToCareerTeamSlot (Chunk 170) --- */
void _Career_CopyUnavailablePlayerToCareerTeamSlot(uint8_t *srcPlayer, uint8_t *dstTeam,
    uint8_t *srcName, uint16_t srcTeamNumber, uint16_t srcPlayerIndex, uint16_t dstSlotIndex);
