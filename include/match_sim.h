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
void _CalculateViewResult(uint8_t *ctx, uint16_t modeFlag,
                          uint16_t team1GoalsIn, uint16_t team2GoalsIn,
                          uint16_t statColumnIndex,
                          uint8_t *team1, uint8_t *team2,
                          uint16_t *outTeam1Goals, uint16_t *outTeam2Goals);

/* Second phase scoring — called from GetCachedOrCalculateExtraTimeResult */
void _ExtraTimeSecondPhaseScoring(uint16_t *outD3, uint16_t *outD4);

/* Penalty shootout scorer pair — called from GetCachedOrCalculatePenaltyShootoutResult */
void _PenaltyShootoutScorerPair(int16_t *outD5, uint16_t *outD6);
