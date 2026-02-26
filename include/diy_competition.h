/* diy_competition.h — DIY competition/tournament management, player pricing */
#pragma once
#include "game_types.h"

/* --- Player Price Utilities --- */
uint16_t _GetAveragePlayerPrice(uint8_t *team);
uint32_t _GetLowestAndHighestPlayerPrice(void);
uint16_t _CountNearnessFactor(uint16_t curBallQuadrant, uint16_t dstBallQuadrant,
                              uint16_t price, const uint8_t *curQuadDistFactor,
                              const uint8_t *playerPositions, const uint8_t *coordToQuad);

/* --- DIY Team Shuffling --- */
void _DIY_ShuffleTeams(uint8_t *diy, uint8_t *slot);
void _DIY_SortAndShuffleTeamsByValue(uint8_t *diy, uint8_t *slot);
void _DIY_ShuffleLeagueTeamOrder(void);

/* --- DIY Initialization --- */
void _InitDIYTournament(void);
void _DIY_InitCompetitionDefaults(void);
void _DIY_ResetCompetitionProgressAndShuffle(void);
void _DIYTournament_RecalculateMatchesPerTeam(void);

/* --- DIY Team Management --- */
void _DIY_CacheSelectedTeamPointers(void);
uint16_t _DIY_CountUnselectedTeams(void);

/* --- DIY League Table --- */
void _DIY_SortLeagueTableByStandings(void);

/* --- DIY Strength/Bracket --- */
uint32_t _DIY_ComputeAverageTeamStrengthTier(uint16_t count, uint8_t *table);
void _DIY_InitCupBracketProgressionTable(void);
void _DIYTournament_BuildMatchesPerTeamProgressionTable(void);

/* --- Cup Round String Utilities --- */
uint16_t _GetNextCupRoundStringIndex(uint16_t cur, uint16_t n, uint16_t add);
const char *_GetNextCupRoundString(uint16_t cur, uint16_t n, uint16_t add);
uint16_t _GetCupRoundStringIndex(uint16_t cur, uint16_t flag, uint16_t idx,
                                  uint16_t stage, uint16_t opt);
const char *_GetCupRoundString(uint16_t cur, uint16_t flag, uint16_t idx,
                                uint16_t stage, uint16_t opt);

/* --- DIY League Scratch Buffer --- */
void _DIY_LoadLeagueScratchFromTable(void);
void _DIY_SaveLeagueScratchToTable(void);

/* --- DIY Cup Init & Helpers --- */
void _InitDIYCup(void);
void _DIY_ShuffleTeamOrderList(void);
void _DIY_ShuffleTeamGridPairs(void);
void _DIY_SyncProgressToDiyFileBufferCopy(void);
void _DIY_ValidateLeagueScratchBuffer(void);

/* --- DIY Cup Bracket & Team Grid --- */
void _DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs(void);
void _DIY_ReorderTeamGridByOrderList(void);
void _DIY_RecountMissingTeamsInTeamGrid(void);
void _DIY_BuildTeamSelectionPointerCache(void);
void _DIY_InitCupBracketState(void);

/* --- DIY Buffer Copy Operations --- */
void _SaveDIYTournament(void);
void _RestoreDIYTournament(void);
void _DIY_CopyCompetitionFileBufferToDiyFileBufferCopy(void);
void _DIY_CopyDiyFileBufferCopyToCompetitionFileBuffer(void);
void _DIY_CopyCompetitionFileBufferToCompetitionStart(void);
void _DIY_CopyCompetitionStartToCompetitionFileBuffer(void);
void _DIY_CopyCompetitionFileBufferToCompetitionStart_Alt(void);
void _DIY_CopyCompetitionStartToCompetitionFileBuffer_Alt(void);
void _SaveDIYLeague(void);
void _RestoreDIYLeague(void);
void _DIY_SaveCompetitionFileBufferToDIYCompetition(void);
void _DIY_RestoreCompetitionFileBufferFromDIYCompetition(void);

/* --- Competition Derived Fields & Fixture Helpers --- */
void _DIY_RecomputeCompetitionDerivedFields(void);
void _DIY_GetCurrentFixtureTeamPtrs(void);
void _DIY_AdvanceLeagueProgressAfterMatch(uint8_t *team1, uint8_t *team2);
uint16_t _DIY_GetCurrentFixtureTeamClass(void);

/* --- Qualified Team List Building --- */
void _DIY_BuildQualifiedTeamListFromSortedTable(uint16_t count, uint8_t *out);
void _DIY_BuildQualifiedTeamListFromDiyFileBuffer(uint16_t count, uint8_t *out);
void _DIY_SyncCompetitionTeamGridOrBuildBracket(void);

/* --- Competition Team Index & Promotion/Relegation --- */
void _DIY_CopyCompetitionTeamIndexTable(uint8_t *dst);
void _DIY_ApplyPromotionRelegationWeightsToScratch(const uint8_t *weights);
void _DIY_UpdatePromotionRelegationScratchCounts(void);

/* --- Career Team Search (Chunk 89) --- */
uint32_t _Career_IsTeamIdNotInListOrReturnSentinel(uint16_t teamId, uint16_t *endPtr);
uint32_t _Career_IsTeamIdNotInListOrReturnSentinel2(uint16_t teamId, uint16_t *endPtr);

/* --- Chunk 96: DIY Competition Slot Editing & Progress --- */
void _DIY_EditCompetitionSlot(uint16_t paramA, uint16_t paramB, uint16_t paramC);
void _DIY_ReorderTeamsBySelectionFlags(void);
void _DIY_UpdateCompetitionProgress(void);
void _DIY_RecordCupBracketProgress(void);
void _DIY_UpdateCompetitionProgressOrAdvanceSlot(uint8_t *blk);
void _DIY_RecordLeagueFixtureResultAndAdvanceProgress(void);
void _DIY_RebuildCompetitionAndTeamSelectionCaches(void);
