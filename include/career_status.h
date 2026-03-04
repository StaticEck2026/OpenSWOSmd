/* career_status.h — Player status decay, availability, team init, career finances,
 * chairman warnings, board confidence, reputation bonuses. */
#pragma once
#include "game_types.h"

/* --- Team initialization for match (decay status + cycle flags) --- */
void _InitInGameTeams_PlayerControlled(uint8_t *team1, uint8_t *team2, uint16_t mode);
void _InitInGameTeams_WithMatchParam(uint8_t *team1, uint8_t *team2);

/* --- Career-specific utilities --- */
void _BuildCareerTeamUnavailablePlayerList(void);
void _ClearCareerMatchPlayerStatusBucket(void);

/* --- Status checks --- */
uint16_t _ShouldApplyRandomPlayerStatusAfterSim(void);
uint16_t _IsPlayerInjured(uint8_t *playerBase, int16_t mode);

/* --- Player count utilities --- */
uint16_t _GetNumberOfSubstitutes(uint8_t *team, uint16_t numPl);
uint16_t _GetNumberOfSubstitutesInSelectionList(uint8_t *team, uint16_t extraCount);
uint32_t _GetNumberOfAvailablePlayers(uint8_t *team, int16_t excludeCardsFlag, uint16_t numPl);
uint32_t _GetNumberOfAvailableSubstitutes(uint8_t *team, int16_t flag);
uint32_t _GetNumberOfAvailableStartingPlayers(uint8_t *team, int16_t flag);
int16_t _GetAgeAdjustedPlayerFormFactor(uint8_t *playerBase);

/* --- Financial management --- */
void _ClampNewBalanceAndUpdateClubStatus(void);
void _ClearOverdraftStateIfRecovered(void);
void _UpdateCareerFinancesAfterMatch(uint8_t *ctx, uint8_t *team);
void _ApplyWeeklyFinancialDeductions(void);
void _InitializeCareerSeasonFinancials(void);

/* --- Chairman warnings --- */
void _MaybeSackManagerAfterSeasonReview(void);
void _UpdateCareerChairmanWarningsAfterMatch(void);
void _UpdateCareerAfterMatch(void);

/* --- Tournament checks --- */
uint16_t _IsMajorInternationalTournament(uint8_t **outCtx);
uint16_t _IsMajorTournamentAllowingRandomStatus(void);
uint16_t _IsMajorTournamentAllowingSquadChanges(void);
uint16_t _IsTournamentAllowingSquadChangesIfNotCareer(void);
uint16_t _ShouldSetupPlayers(void);

/* --- Board confidence --- */
int16_t _GetBoardConfidenceScore(void);
int16_t _UpdateBoardConfidenceScore(void);
int16_t _PushBoardConfidenceHistory(void);
void _UpdateBoardConfidenceModifiers(void);

/* --- Reputation & starting balance --- */
void _UpdateCareerReputationBonuses(void);
void _InitializeStartingBalanceForNewSeason(void);
uint32_t _CalculateStartingBalance(uint16_t teamNumber);

/* --- Wage management (Chunk 76) --- */
uint16_t _GetAvgTeamValueBaseForCurrentPlayer(uint16_t teamNumber, uint16_t playerIndex);
void _Career_UpdatePlayerWeeklyWageCache(uint16_t playerIndex);
void _Career_RemovePlayerWeeklyWageAndRecalcWeeklyWages(uint16_t playerIndex);

/* --- Post-match stats & player movement (Chunk 95) --- */
void _PlayerJoinedClubOnTrial(uint8_t *name);
void _TrialPlayerLeftClubMessage(uint8_t *name);
void _ReservePlayerLeftClubMessage(uint8_t *name);
void _Career_UpdateSinglePlayerPostMatchStats(uint8_t *playerSlotPtr);
void _Career_ApplyPostMatchPlayerStatModsIfCareerTeam(uint8_t *team, uint8_t *mods);
uint8_t *_AddTrialPlayer(void);
uint8_t *_TrainingMatch_AddTrialPlayerAtPosition(uint16_t position);
void _CheckPlayersLeavingComing(void);

/* --- Chairman messages (Chunk 77) --- */
void _ShowLeavingMessage(void);
void _ShowGreetingMessage(void);
void _ShowLastSeasonMessage(void);
void _ShowWishYouSuccessMessage(void);
void _ShowEnjoyLastSeasonMessage(void);
void _ShowRetirementMessage(void);

/* --- Club Business & Season Finance Init (Chunk 101) --- */
void _ClubBusinessInit(void);
void _Career_InitializeNewSeasonFinances(void);

/* --- Career retirement (Chunk 119) --- */
void _Career_HandleRetirement(void);

/* --- Career post-match update (Chunk 175) --- */
void _Career_PostMatchUpdate(void);

/* --- Career auto-advance chain (Chunk 179) --- */
void _Career_AutoAdvanceCompetitionAndPersistSlotState(uint8_t *compPtr);
void _Career_AutoAdvanceCupCompetitionFromRecord(uint8_t *record);
void _Career_AutoAdvanceLeagueCompetitionFromRecord(uint8_t *record);
void _Career_AutoAdvanceInternationalCompetitionFixtures(uint8_t *record, uint16_t index);

/* --- Career player form update (Chunk 188) --- */
void _UpdateCareerPlayersFormAfterMatch(uint8_t *compCtx, uint8_t *team1, uint8_t *team2,
                                         uint16_t team1Goals, uint16_t team2Goals);
