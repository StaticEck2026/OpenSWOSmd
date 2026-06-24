/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* match_sim.h — Match simulation / "view result" calculation
 *
 * Ported from SWOS2.s: CalculateViewResult, CalcTeamViewOffset,
 * ScaleViewIndexByTeamPriceRank, AssignSimulatedGoalsToScorers,
 * ApplyRandomPlayerStatusAfterSim, InitPlayerRemapTableForTeam,
 * BuildAvailablePlayersRemapAndMask, ComputeOldPlayerSkillForViewResult,
 * ExtraTimeSecondPhaseScoring (loc_121E56), PenaltyShootoutScorerPair (loc_121FCE).
 */
#pragma once
#include "game_types.h"

/* Main entry point — called from asm bridge with register params.
 * Returns team1Goals in *outTeam1Goals, team2Goals in *outTeam2Goals. */
void CalculateViewResult(DIYCompetition *ctx, uint16_t modeFlag,
                          uint16_t team1GoalsIn, uint16_t team2GoalsIn,
                          uint16_t statColumnIndex,
                          TeamFileEx *team1, TeamFileEx *team2,
                          uint16_t *outTeam1Goals, uint16_t *outTeam2Goals);

/* Second phase scoring — called from GetCachedOrCalculateExtraTimeResult */
void ExtraTimeSecondPhaseScoring(uint16_t *outD3, uint16_t *outD4);

/* Penalty shootout scorer pair — called from GetCachedOrCalculatePenaltyShootoutResult */
void PenaltyShootoutScorerPair(int16_t *outD5, uint16_t *outD6);

/* --- Team init & play (Chunk 102) --- */
void InitTeams(void);
void PlayFriendly(void);
int16_t CheckContinueAbortPrompt(uint8_t *header);

/* --- Skill adjustment & team tactics (Chunk 105) --- */
uint16_t ScaleSkill(uint16_t skill, int16_t factor, TeamGame *teamGame);
int16_t GetPlayerPricePercentageChange(int16_t factor, TeamFile *teamFile,
                                        uint16_t callerD0);
void AdjustPlayerSkills(uint16_t playerIdx, TeamFile *teamFile,
                         TeamFile *oppTeamFile, TeamGame *teamGame,
                         uint8_t *playerBase, PlayerInfo *pi,
                         uint16_t callerD0);
void ApplyTeamTactics(TeamGame *teamGame);

/* --- CalcTeamPriceRankPercentOffset (Chunk 180) --- */
int16_t CalcTeamPriceRankPercentOffset(DIYCompetition *comp, TeamFile *team);

/* --- GetPlayerPrice + UpdatePlayerPositionAndReturnRole (Chunk 187) --- */
int16_t GetPlayerPrice(TeamFile *teamFile, PlayerFile *playerFile, uint16_t playerNumber);
uint32_t UpdatePlayerPositionAndReturnRole(TeamFileEx *team, uint16_t playerIdx);

/* --- BuildMatchResultString (Chunk 106) --- */
uint8_t *BuildMatchResultString(uint16_t control, uint16_t leg2, uint16_t leg1,
                                  uint16_t pens, uint8_t *teamA, uint8_t *teamB,
                                  uint16_t flags);
