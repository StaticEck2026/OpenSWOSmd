/* match_sim.c — Match simulation / "view result" calculation
 *
 * Ported from SWOS2.s: CalculateViewResult, CalcTeamViewOffset,
 * ScaleViewIndexByTeamPriceRank, AssignSimulatedGoalsToScorers,
 * ApplyRandomPlayerStatusAfterSim, InitPlayerRemapTableForTeam,
 * BuildAvailablePlayersRemapAndMask, ComputeOldPlayerSkillForViewResult.
 *
 * CalcTeamPriceRankPercentOffset and UpdatePlayerPositionAndReturnRole
 * remain in assembly with C-callable bridges.
 */

#include "match_sim.h"
#include "game_types.h"

/* ---- Assembly externs (functions still in asm) ---- */
extern uint16_t _Rand2(void);
extern void Career_ApplyPostMatchPlayerStatModsIfCareerTeam(void);
extern uint16_t _ShouldApplyRandomPlayerStatusAfterSim(void);

/* C-callable bridges to asm functions (defined in swos2_unit.s) */
extern int16_t _CCalcTeamPriceRankPercentOffset(uint8_t *ctx, uint8_t *team);
extern uint32_t _CUpdatePlayerPositionAndReturnRole(uint8_t *team, uint16_t playerIdx);
extern void _CCareer_ApplyPostMatchPlayerStatModsIfCareerTeam(uint8_t *team, uint8_t *remap);

/* GetNumberOfAvailable* — return packed: d6 in low word, d7 in upper bits */
extern uint32_t _GetNumberOfAvailablePlayers(uint8_t *team, int16_t excludeCardsFlag, uint16_t numPl);
extern uint32_t _GetNumberOfAvailableSubstitutes(uint8_t *team, int16_t flag);
extern uint16_t _IsPlayerInjured(uint8_t *playerBase, int16_t mode);

/* ---- Data externs (still in asm) ---- */
extern TeamFileEx careerTeam;
extern uint8_t    currentMatchPlayers[32];
extern uint16_t   careerNumPlayers;
extern uint16_t   trainingGame;
extern uint16_t   gameType;
extern uint16_t   currentSeasonShowing;
extern uint8_t    newRandomSeedCopy;
extern uint8_t    viewRandomPhase;
extern uint8_t    seasonPlaying;
extern uint8_t    kRandomTable[];
extern uint32_t   teamTacticsTable[];
extern uint32_t   positionsTable[];
extern uint16_t   oldPlayerSkill;
extern uint8_t    playerPositionsArray[];

/* Data tables in asm for this module */
extern uint8_t    simGoalsByStrengthDiffTable[];
extern uint8_t    simHighGoalFallbackValues[];
extern uint32_t   simGoalProbByTacticPtrs[];
extern uint32_t   simExtraGoalProbByTacticPtrs[];
extern uint8_t    simGoalsByIndexDiffTable[];
extern uint8_t    simExtraGoalThresholdsA[];
extern uint8_t    simExtraGoalThresholdsB[];
extern uint8_t    simGoalScorerPairTable[];
extern uint32_t   goalScorerDistributionByBallOutTactic[];
extern uint8_t    kUserTeamOffsetBiasLUT[];
extern uint8_t    kTeamViewOffsetBaseLUT[];
extern uint8_t    availablePickCountByGoalsInTable[];
extern uint8_t    preferredTacticsSlotsByRole[];
extern uint8_t    candidatePlayersByGoalsInTable[];
extern uint32_t   playersPositionsByTactics[];
extern uint8_t    playerStatusBucketThresholds[];

/* ---- Module-local state ---- */
static uint8_t  *viewResultCtx;
static uint16_t  viewResultModeFlag;
static uint16_t  viewResultStatColumnIndex;
static uint16_t  viewResultTeam1GoalsIn;
static uint16_t  viewResultTeam2GoalsIn;
static uint8_t  *viewResultTeam1Ptr;
static uint8_t  *viewResultTeam2Ptr;
static int16_t   team1ViewBaseIndex;
static int16_t   team2ViewBaseIndex;
static uint16_t  team1ViewIndex;
static uint16_t  team2ViewIndex;
static int16_t   viewIndexDelta;
static int16_t   team1ViewOffsetX;
static int16_t   team2ViewOffsetX;
static int16_t   team1ViewOffsetY;
static int16_t   team2ViewOffsetY;
static uint16_t  team1SimulatedGoalsOut;
static uint16_t  team2SimulatedGoalsOut;
static uint8_t   team1PlayerRemapTable[32];
static uint8_t   team2PlayerRemapTable[32];
static uint32_t  team1AvailableMask;
static uint32_t  team2AvailableMask;
static uint16_t  team1AvailableCount;
static uint16_t  team2AvailableCount;
static uint32_t  currentGoalScorerDistPtr;
static uint16_t  oldPlayerSkillDivisor;

/* Externally-visible state (defined in asm, referenced by other asm modules) */
extern uint16_t inEditTactics;
extern uint8_t  team1GoalCounterByPlayer[];
extern uint8_t  team2GoalCounterByPlayer[];
static int16_t  careerTeamViewOffsetX;


/* ================================================================
 * CalcTeamViewOffset
 * in:  team -> TeamFile pointer
 * Returns packed uint32_t: high word = X, low word = Y (both int16_t).
 * Called from asm bridge and internally.
 * ================================================================ */
__attribute__((used, externally_visible))
uint32_t _CalcTeamViewOffset(uint8_t *team) {
    uint8_t d0;
    int16_t d1, d2;
    uint16_t d3;

    /* d0 = low byte of globalTeamNumber */
    d0 = (uint8_t)((TeamFileEx *)team)->team.globalTeamNumber;
    /* XOR with currentSeasonShowing low byte */
    d0 ^= (uint8_t)currentSeasonShowing;
    /* XOR with newRandomSeedCopy */
    d0 ^= newRandomSeedCopy;

    /* Look up in kRandomTable */
    d0 = kRandomTable[d0];
    d3 = d0;

    /* Add viewRandomPhase, mask to 8 bits */
    d0 = (uint8_t)(d0 + viewRandomPhase);

    /* Look up base X offset from kTeamViewOffsetBaseLUT */
    d1 = (int8_t)kTeamViewOffsetBaseLUT[d0];
    /* Y offset: index+7, wrapped to 8 bits */
    d2 = (int8_t)kTeamViewOffsetBaseLUT[(uint8_t)(d0 + 7)];

    /* Double the offsets */
    d1 = d1 + d1;
    d2 = d2 + d2;

    /* Check if team is user-controlled (teamControls == 1) */
    if (((TeamFileEx *)team)->team.teamControls == 1) {
        /* Compute bias index: d3 += (viewRandomPhase << 2) >> 6; d3 &= 0x3F */
        uint16_t biasIdx = d3;
        uint8_t vrp = viewRandomPhase;
        biasIdx += (uint16_t)((vrp << 2) >> 6);
        biasIdx &= 0x3F;

        int8_t bias = (int8_t)kUserTeamOffsetBiasLUT[biasIdx];
        d1 += bias + bias;
        bias = (int8_t)kUserTeamOffsetBiasLUT[biasIdx];
        d2 += bias + bias;
    }

    /* If not user-controlled, subtract 1 from both */
    if (((TeamFileEx *)team)->team.teamControls != 1) {
        d1 -= 1;
        d2 -= 1;
    }

    /* Clamp X to [-12, +12] */
    if (d1 < -12) d1 = -12;
    else if (d1 > 12) d1 = 12;

    /* Clamp Y to [-12, +12] */
    if (d2 < -12) d2 = -12;
    else if (d2 > 12) d2 = 12;

    return ((uint32_t)(uint16_t)d1 << 16) | (uint16_t)d2;
}

/* ================================================================
 * ScaleViewIndexByTeamPriceRank
 * in:  value (d7), team (a1)
 * out: scaled value (0..31)
 * ================================================================ */
static uint16_t ScaleViewIndexByTeamPriceRank(uint16_t value, uint8_t *team) {
    int16_t pctOff = _CCalcTeamPriceRankPercentOffset(viewResultCtx, team);
    uint16_t factor = (uint16_t)(pctOff + 100);
    uint32_t result = (uint32_t)value * (uint32_t)factor;
    result /= 100;

    if ((int16_t)result < 0) return 0;
    if (result > 31) return 31;
    return (uint16_t)result;
}

/* ================================================================
 * AssignSimulatedGoalsToScorers
 * in:  team (a0), remapTable (a2), goalCounters (a3), numGoals (d5)
 * ================================================================ */
static void AssignSimulatedGoalsToScorers(uint8_t *team, uint8_t *remapTable,
                                          uint8_t *goalCounters, uint16_t numGoals) {
    int16_t d5 = (int16_t)numGoals - 1;
    if (d5 < 0) return;

    uint8_t *playerNums = ((TeamFileEx *)team)->team.playerNumbers;
    if (team == (uint8_t *)&careerTeam) {
        playerNums = currentMatchPlayers;
    }

    /* Get tactic index -> ball-out-of-play tactics -> position list */
    int16_t tacticIdx = (int8_t)((TeamFileEx *)team)->team.tactics;
    TeamTactics *tactics = (TeamTactics *)(uint32_t)teamTacticsTable[tacticIdx];
    int16_t ballOutTacticIdx = (int8_t)tactics->ballOutOfPlayTactics;
    uint8_t *positions = (uint8_t *)(uint32_t)positionsTable[ballOutTacticIdx];

    /* Get goal scorer distribution pointer */
    currentGoalScorerDistPtr = goalScorerDistributionByBallOutTactic[ballOutTacticIdx];

    for (; d5 >= 0; d5--) {
        uint8_t *distTable = (uint8_t *)(uint32_t)currentGoalScorerDistPtr;
        uint16_t d0 = _Rand2();

        /* Special case: if d0 >= 0xF8, it's an own goal */
        if (d0 >= 0xF8) {
            team[0x16]++;  /* increment own goals counter at offset 0x16 */
            continue;
        }

        d0 &= 0x7F;
        int16_t d6 = 10;  /* start from position 10 (forward) */

        /* Walk the cumulative probability distribution (11 entries) */
        uint8_t *dp = distTable;
        int16_t i;
        for (i = 0; i < 11; i++) {
            if (d0 < dp[i]) goto found_position;
            d0 -= dp[i];
            d6--;
        }

        /* If we exhausted all 11 entries, do a second random for the last 2 buckets */
        d0 = _Rand2();
        d0 &= 0x7F;
        d6 = 1;
        dp = distTable + 11;  /* point to the 12th entry */
        if (d0 < dp[0]) goto found_position;
        d0 -= dp[0];
        d6--;

    found_position:
        /* Map through position table: d6 = positions[d6] - 1 */
        d6 = (uint8_t)positions[d6];
        d6 = (uint8_t)(d6 - 1);

        /* Map through player numbers: d6 = playerNums[d6] */
        d6 = playerNums[(uint8_t)d6];

        /* Check remap table; if negative (0x80+), use remapped player */
        if (!((int8_t)remapTable[(uint8_t)d6] < 0)) {
            /* Not remapped - 50% chance to use the remap */
            uint16_t r = _Rand2();
            if (!(r & 1)) {
                d6 = remapTable[(uint8_t)d6];
            }
        }

        /* Increment goal counter for this player */
        goalCounters[(uint8_t)d6]++;

        /* Update player's per-stat-column goal count */
        PlayerFile *playerRec = &((TeamFileEx *)team)->players[(uint8_t)d6];
        uint16_t statCol = viewResultStatColumnIndex;
        playerRec->goalStats[statCol]++;
    }
}

/* ================================================================
 * ApplyRandomPlayerStatusAfterSim
 * Applies random injury/card/status changes to players after simulation.
 * ================================================================ */
static void ApplyRandomPlayerStatusAfterSim(uint8_t *team, uint8_t *remapTable) {
    if (trainingGame) return;
    if (((TeamFileEx *)team)->team.teamControls == 1) return;

    /* Get number of available players */
    uint32_t result = _GetNumberOfAvailablePlayers(team, -1, 0);
    uint16_t availCount = (uint16_t)(result >> 16); /* d6 from asm */

    int16_t d5;
    if (gameType == 4) {
        d5 = 4;
    } else {
        d5 = 2;
    }
    d5 -= (int16_t)availCount;
    if (d5 == 0) return;

    uint8_t *playerNums = ((TeamFileEx *)team)->team.playerNumbers;
    if (team == (uint8_t *)&careerTeam) {
        playerNums = currentMatchPlayers;
    }

    int16_t d7;
    for (d7 = 10; d7 >= 0; d7--) {
        int16_t d6 = (int8_t)playerNums[d7];
        uint16_t d0 = _Rand2();

        /* Check remap: if negative byte in remapTable, use direct; else 50% chance */
        if (!((int8_t)remapTable[d6] < 0)) {
            if (!(d0 & 1)) {
                d6 = (int8_t)remapTable[d6];
            }
        }

        uint16_t d2 = (uint16_t)d6;
        PlayerFile *playerRec = &((TeamFileEx *)team)->players[d2];

        uint8_t cardsInj = playerRec->cardsInjuries;
        uint16_t d1 = cardsInj & 0x1C;  /* injury bits */
        uint16_t cardBits = cardsInj & 0x03;

        /* Goalkeeper (d7==0) has much higher skip threshold */
        if (d7 == 0) {
            if (d0 >= 2) goto next_player;
        } else {
            if (d0 >= 16) goto next_player;
        }

        /* Check player's position (role) */
        uint8_t posAndFace = playerRec->positionAndFace;
        uint16_t role = (posAndFace & 0xE0) >> 5;

        if (role == 0) {
            /* Goalkeeper role - check available substitutes */
            uint32_t subResult = _GetNumberOfAvailableSubstitutes(team, -1);
            uint16_t subAvail = (uint16_t)(subResult >> 16); /* d6 from asm */
            if (subAvail != 0) goto next_player;
        }

        /* Second random for status type determination */
        d0 = _Rand2();
        d0 &= 0x1F;

        if (d0 < 0x1D) {
            /* Most common case: minor status */
            if (!inEditTactics) {
                if (d1 == 0) {
                    /* No current injury - check ShouldApply */
                    if (_ShouldApplyRandomPlayerStatusAfterSim() == 0) {
                        /* skip this player entirely (loc_1223B2) */
                        d5--;
                        if (d5 == 0) return;
                        continue;
                    }
                    d1 = 4;
                } else {
                    d1 = 8;
                }
            } else {
                goto next_player;
            }
        } else if (d0 == 0x1D) {
            /* Moderate status change */
            if (inEditTactics) {
                cardBits = 2;
            } else {
                if (d1 == 0) {
                    /* loc_122376: ShouldApply -> if yes d1=0xC, else d1=8 */
                    if (_ShouldApplyRandomPlayerStatusAfterSim() != 0) {
                        d1 = 0x0C;
                    } else {
                        d1 = 8;
                    }
                } else {
                    /* loc_122384 */
                    if (_ShouldApplyRandomPlayerStatusAfterSim() != 0) {
                        d1 = 0x10;
                    } else {
                        d1 = 8;
                    }
                }
            }
        } else if (d0 == 0x1E) {
            /* Serious status change */
            if (inEditTactics) {
                cardBits = 2;
            } else {
                /* loc_122384: ShouldApply -> if yes d1=0x10, else d1=8 */
                if (_ShouldApplyRandomPlayerStatusAfterSim() != 0) {
                    d1 = 0x10;
                } else {
                    d1 = 8;
                }
            }
        } else {
            /* d0 == 0x1F: most severe */
            if (inEditTactics) {
                cardBits = 2;
            } else {
                if (d1 == 0) {
                    /* loc_122384 */
                    if (_ShouldApplyRandomPlayerStatusAfterSim() != 0) {
                        d1 = 0x10;
                    } else {
                        d1 = 8;
                    }
                } else {
                    /* same path: loc_122384 */
                    if (_ShouldApplyRandomPlayerStatusAfterSim() != 0) {
                        d1 = 0x10;
                    } else {
                        d1 = 8;
                    }
                }
            }
        }

        /* Apply new injury bits (loc_122396) */
        {
            uint8_t val = playerRec->cardsInjuries;
            val = (val & ~0x1C) | (uint8_t)(d1 & 0x1C);
            playerRec->cardsInjuries = val;

            val = playerRec->cardsInjuries;
            val = (val & ~0x03) | (uint8_t)(cardBits & 0x03);
            playerRec->cardsInjuries = val;
        }

        /* loc_1223B2: decrement budget */
        d5--;
        if (d5 == 0) return;
        continue;

    next_player:
        ; /* dbf d7 continues the loop */
    }
}

/* ================================================================
 * InitPlayerRemapTableForTeam
 * Assigns random status buckets to players based on match factors.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitPlayerRemapTableForTeam(uint8_t *team, uint8_t *remapTable) {
    if (((TeamFileEx *)team)->team.teamControls == 1) return;

    uint32_t result = _GetNumberOfAvailablePlayers(team, -1, 0);
    uint16_t availCount = (uint16_t)(result >> 16); /* d6 */

    int16_t d5;
    if (gameType == 4) {
        d5 = 4;
    } else {
        d5 = 2;
    }
    d5 -= (int16_t)availCount;
    if (d5 <= 0) return;

    uint8_t *playerNums = ((TeamFileEx *)team)->team.playerNumbers;
    int16_t d7;

    if (team == (uint8_t *)&careerTeam) {
        playerNums = currentMatchPlayers;
        if (remapTable == (uint8_t *)0) {
            d7 = (int16_t)(careerNumPlayers - 1);
            goto loop_start;
        }
    }
    d7 = 10;

loop_start:
    for (; d7 >= 0; d7--) {
        int16_t d6 = (int8_t)playerNums[d7];
        uint16_t d0 = _Rand2();

        /* Check remap table if non-null */
        if (remapTable != (uint8_t *)0) {
            if (!((int8_t)remapTable[d6] < 0)) {
                if (!(d0 & 1)) {
                    d6 = (int8_t)remapTable[d6];
                }
            }
        }

        uint16_t d2 = (uint16_t)d6;
        PlayerFile *playerRec = &((TeamFileEx *)team)->players[d2];

        /* Goalkeeper skip check */
        if (d7 == 0) {
            if (d0 >= 2) continue;
        }

        /* Check current position/role */
        uint8_t posAndFace = playerRec->positionAndFace;
        uint16_t role = (posAndFace & 0xE0) >> 5;

        if (role == 0) {
            /* Goalkeeper: check available substitutes */
            uint32_t subResult = _GetNumberOfAvailableSubstitutes(team, -1);
            uint16_t subAvail = (uint16_t)(subResult >> 16);
            if (subAvail != 0) continue;
        }

        /* Check current status bucket (bits 5-7 of cardsInjuries) */
        uint8_t cardsInj = playerRec->cardsInjuries;
        uint16_t statusBucket = cardsInj & 0xE0;

        if (statusBucket == 0) {
            /* No status - need very low random to trigger */
            if (d0 >= 5) continue;
        } else if (statusBucket == 0x20) {
            /* Status 1 - moderate threshold */
            if (d0 >= 0x40) continue;
        } else {
            /* Higher status - skip */
            continue;
        }

        /* Assign new status bucket from probability thresholds */
        uint8_t *thresholds = playerStatusBucketThresholds;
        d0 = _Rand2();
        d0 &= 0x3F;
        uint16_t newBucket = 0x20;

        int16_t j;
        for (j = 0; j < 8; j++) {
            if (d0 < thresholds[j]) goto apply_bucket;
            d0 -= thresholds[j];
            newBucket += 0x20;
        }

    apply_bucket:
        {
            uint8_t val = playerRec->cardsInjuries;
            val = (val & 0x1F) | (uint8_t)(newBucket & 0xE0);
            playerRec->cardsInjuries = val;
        }

        d5--;
        if (d5 == 0) return;
    }
}

/* ================================================================
 * BuildAvailablePlayersRemapAndMask
 * Builds the player substitution remap table and availability mask.
 * out: *outCount = count, *outMask = bitmask
 * ================================================================ */
static void BuildAvailablePlayersRemapAndMask(uint8_t *team, uint8_t *remapTable,
                                              uint16_t *outCount, uint32_t *outMask) {
    uint32_t mask = 0;

    /* Fill remap table with 0xFF (identity/no-remap) — use longword
     * stores matching original asm (move.l d0,(a1)+ x8) to avoid
     * GCC generating a memset call (no libc in this project). */
    {
        uint32_t *p32 = (uint32_t *)remapTable;
        p32[0] = 0xFFFFFFFF; p32[1] = 0xFFFFFFFF;
        p32[2] = 0xFFFFFFFF; p32[3] = 0xFFFFFFFF;
        p32[4] = 0xFFFFFFFF; p32[5] = 0xFFFFFFFF;
        p32[6] = 0xFFFFFFFF; p32[7] = 0xFFFFFFFF;
    }

    uint8_t *playerNums = ((TeamFileEx *)team)->team.playerNumbers;
    if (team == (uint8_t *)&careerTeam) {
        playerNums = currentMatchPlayers;
    }

    /* Get pick count based on goals conceded */
    uint8_t *pickCounts = availablePickCountByGoalsInTable + (viewResultTeam1GoalsIn << 3);
    uint16_t d0 = _Rand2();
    uint8_t d4 = pickCounts[d0 & 7];
    if (d4 == 0) {
        *outCount = 0;
        *outMask = 0;
        return;
    }
    int16_t d4s = (int8_t)d4;

    /* Get candidate players based on goals against */
    uint8_t *candidates = candidatePlayersByGoalsInTable + ((viewResultTeam2GoalsIn - 1) << 5);
    d0 = _Rand2();
    candidates += (d0 & 0x18);  /* offset by 0, 8, 16, or 24 */

    uint16_t d6 = 0;
    d4s--;

    for (; d4s >= 0; d4s--) {
        uint8_t candidateIdx = *candidates++;
        if (candidateIdx == 0) break;

        int16_t d2 = (int8_t)candidateIdx;
        d2 = playerNums[d2];

        /* Check player record */
        PlayerFile *playerRec = &((TeamFileEx *)team)->players[(uint16_t)d2];

        /* Check status bucket (bits 5-7 of cardsInjuries) - must be 0 */
        if (playerRec->cardsInjuries & 0xE0) continue;

        /* Check injury/card status based on inEditTactics */
        if (!inEditTactics) {
            uint8_t ci = playerRec->cardsInjuries;
            if ((ci & 0x1C) >= 8) continue;
        } else {
            uint8_t ci = playerRec->cardsInjuries;
            if ((ci & 0x03) >= 1) continue;
        }

        /* Check position/role for goalkeeper */
        uint8_t posAndFace = playerRec->positionAndFace;
        uint16_t role = (posAndFace & 0xE0) >> 5;

        if (role == 0) {
            /* Goalkeeper: random chance to skip */
            d0 = _Rand2();
            if (d0 >= 4) {
                d4s++;  /* retry: don't count this against the budget (go back to loop start) */
                continue;
            }
        }

        /* Pick a preferred position slot for this player's role */
        uint8_t *roleSlots = preferredTacticsSlotsByRole + ((int16_t)role << 3);
        d0 = _Rand2();
        uint8_t slotIdx = roleSlots[d0 & 7];

        /* Map through tactics to get position index */
        uint8_t tacticIdx = ((TeamFileEx *)team)->team.tactics;
        uint8_t *tacticPositions = (uint8_t *)(uint32_t)playersPositionsByTactics[(int8_t)tacticIdx];
        uint8_t posIdx = tacticPositions[slotIdx];
        uint8_t targetPlayer = playerNums[posIdx];

        /* Check if target player's remap slot is available (0xFF = available) */
        if (!((int8_t)remapTable[targetPlayer] < 0)) {
            /* Slot taken, try once more with different random */
            d0 = _Rand2();
            slotIdx = roleSlots[d0 & 7];

            /* Re-read tactics mapping */
            tacticIdx = ((TeamFileEx *)team)->team.tactics;
            tacticPositions = (uint8_t *)(uint32_t)playersPositionsByTactics[(int8_t)tacticIdx];
            posIdx = tacticPositions[slotIdx];
            targetPlayer = playerNums[posIdx];

            if (!((int8_t)remapTable[targetPlayer] < 0)) {
                continue;  /* Still taken, skip this candidate */
            }
        }

        /* Assign remap: remapTable[targetPlayer] = candidatePlayerNum */
        remapTable[targetPlayer] = (uint8_t)d2;
        mask |= (1u << (uint8_t)d2);
        d6++;
    }

    *outCount = d6;
    *outMask = mask;
}

/* ================================================================
 * ComputeOldPlayerSkillForViewResult
 * Calculates average skill index for team, accounting for goalkeeper.
 * in:  team, goalsCount
 * out: skill index (0..31 range)
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _ComputeOldPlayerSkillForViewResult(uint8_t *team, uint16_t goalsCount) {
    oldPlayerSkillDivisor = 1;
    uint16_t d5 = 0;
    uint16_t d6 = goalsCount;

    if (d6 != 0) {
        int16_t d3 = (int16_t)d6 - 1;
        uint16_t d2 = 0;
        uint16_t d7 = 11;  /* start from player index 11 */

        for (; d3 >= 0; d3--) {
            /* Call UpdatePlayerPositionAndReturnRole */
            uint32_t res = _CUpdatePlayerPositionAndReturnRole(team, d7);
            uint16_t posValue = (uint16_t)(res & 0xFFFF);       /* d0 = position value */
            uint16_t roleValue = (uint16_t)((res >> 16) & 0xFFFF); /* d1 = role */

            uint16_t scaled = posValue << 2;  /* lsl.w #2,d0 */
            d2 += 4;  /* addq.w #4,d2 */

            if (roleValue == 0) {
                /* Goalkeeper: undo the scaling */
                scaled = posValue;  /* lsr.w #2,d0 which undoes lsl.w #2 */
                d2 -= 3;  /* subq.w #3,d2 */
            }

            d5 += scaled;
            d7++;
        }

        /* Average: (d2/2 + d5) / d2 */
        uint16_t half = d2 >> 1;
        uint32_t sum = (uint32_t)(half + d5);
        d5 = (uint16_t)(sum / d2);
    }

    /* Second pass: iterate all 11 outfield players (indices 0..10) */
    uint16_t d7;
    for (d7 = 0; d7 < 11; d7++) {
        uint32_t res = _CUpdatePlayerPositionAndReturnRole(team, d7);
        uint16_t posValue = (uint16_t)(res & 0xFFFF);
        uint16_t roleValue = (uint16_t)((res >> 16) & 0xFFFF);

        if (posValue == 0 && roleValue != 0) {
            oldPlayerSkillDivisor++;
        }
        d5 += posValue;
    }

    /* Divide by 15 */
    uint32_t d0_ext = (uint32_t)d5;
    uint16_t result = (uint16_t)(d0_ext / 15);

    /* Check goalkeeper: if position is 0 (goalkeeper role), halve result.
     * Asm used move.b -$20(a0),d1 for career team — that works because
     * currentMatchPlayers was exactly 0x20 bytes before careerTeam in the
     * asm memory layout.  In C they are separate variables, so use the
     * symbol directly. */
    uint8_t *goalkeepPlayerNums;
    if (team == (uint8_t *)&careerTeam) {
        goalkeepPlayerNums = currentMatchPlayers;
    } else {
        goalkeepPlayerNums = ((TeamFileEx *)team)->team.playerNumbers;
    }
    int16_t gkIdx = (int8_t)goalkeepPlayerNums[0];
    PlayerFile *gkRec = &((TeamFileEx *)team)->players[(uint16_t)gkIdx];
    uint8_t gkPosAndFace = gkRec->positionAndFace;
    uint16_t gkRole = (gkPosAndFace & 0xE0) >> 5;

    if (gkRole == 0) {
        result >>= 1;
    }

    /* Final division by oldPlayerSkillDivisor and store */
    result = (uint16_t)((uint32_t)result / oldPlayerSkillDivisor);
    oldPlayerSkill = result;

    /* Clear flag at offset 0x3B (someFlag in TeamFile) */
    ((TeamFileEx *)team)->team.someFlag = 0;

    return result;
}

/* ================================================================
 * CalculateViewResult — main entry point
 * ================================================================ */
__attribute__((used, externally_visible))
void _CalculateViewResult(uint8_t *ctx, uint16_t modeFlag,
                          uint16_t team1GoalsIn, uint16_t team2GoalsIn,
                          uint16_t statColumnIndex,
                          uint8_t *team1, uint8_t *team2,
                          uint16_t *outTeam1Goals, uint16_t *outTeam2Goals) {
    /* Store parameters in module state */
    viewResultCtx = ctx;
    inEditTactics = *(uint16_t *)(ctx + 0x3C);
    viewResultModeFlag = modeFlag;
    viewResultTeam1GoalsIn = team1GoalsIn;
    viewResultTeam2GoalsIn = team2GoalsIn;
    viewResultStatColumnIndex = statColumnIndex;
    viewResultTeam1Ptr = team1;
    viewResultTeam2Ptr = team2;

    /* Clear goal counters (32 bytes each) — longword stores to avoid
     * GCC generating memset calls (no libc). */
    {
        uint32_t *p1 = (uint32_t *)team1GoalCounterByPlayer;
        uint32_t *p2 = (uint32_t *)team2GoalCounterByPlayer;
        p1[0] = 0; p1[1] = 0; p1[2] = 0; p1[3] = 0;
        p1[4] = 0; p1[5] = 0; p1[6] = 0; p1[7] = 0;
        p2[0] = 0; p2[1] = 0; p2[2] = 0; p2[3] = 0;
        p2[4] = 0; p2[5] = 0; p2[6] = 0; p2[7] = 0;
    }

    /* Build player remap tables and availability */
    BuildAvailablePlayersRemapAndMask(team1, team1PlayerRemapTable,
                                      &team1AvailableCount, &team1AvailableMask);
    BuildAvailablePlayersRemapAndMask(team2, team2PlayerRemapTable,
                                      &team2AvailableCount, &team2AvailableMask);

    /* Init player remap tables */
    _InitPlayerRemapTableForTeam(team1, team1PlayerRemapTable);
    _InitPlayerRemapTableForTeam(team2, team2PlayerRemapTable);

    /* Apply post-match career stat mods */
    _CCareer_ApplyPostMatchPlayerStatModsIfCareerTeam(team1, team1PlayerRemapTable);
    _CCareer_ApplyPostMatchPlayerStatModsIfCareerTeam(team2, team2PlayerRemapTable);

    /* Apply random player status */
    ApplyRandomPlayerStatusAfterSim(team1, team1PlayerRemapTable);
    ApplyRandomPlayerStatusAfterSim(team2, team2PlayerRemapTable);

    /* Compute old player skill (view base index) */
    /* Note: for team1, the goals-in parameter is team2's goals (opponent's goals) */
    team1ViewBaseIndex = (int16_t)_ComputeOldPlayerSkillForViewResult(team1, team2GoalsIn);
    team2ViewBaseIndex = (int16_t)_ComputeOldPlayerSkillForViewResult(team2, team2GoalsIn);

    /* Scale by price rank unless both teams are user-controlled */
    if (!(((TeamFileEx *)team1)->team.teamControls == 1 && ((TeamFileEx *)team2)->team.teamControls == 1)) {
        team1ViewBaseIndex = (int16_t)ScaleViewIndexByTeamPriceRank(
            (uint16_t)team1ViewBaseIndex, team1);
        team2ViewBaseIndex = (int16_t)ScaleViewIndexByTeamPriceRank(
            (uint16_t)team2ViewBaseIndex, team2);
    }

    /* Calculate view offsets */
    {
        uint32_t packed = _CalcTeamViewOffset(team1);
        team1ViewOffsetX = (int16_t)(packed >> 16);
        team1ViewOffsetY = (int16_t)(packed & 0xFFFF);
    }
    if (viewResultTeam1Ptr == (uint8_t *)&careerTeam) {
        careerTeamViewOffsetX = team1ViewOffsetX;
    }

    {
        uint32_t packed = _CalcTeamViewOffset(team2);
        team2ViewOffsetX = (int16_t)(packed >> 16);
        team2ViewOffsetY = (int16_t)(packed & 0xFFFF);
    }
    if (viewResultTeam2Ptr == (uint8_t *)&careerTeam) {
        careerTeamViewOffsetX = team2ViewOffsetX;
    }

    /* ---- First scoring phase: based on X offsets ---- */
    int16_t d3 = team1ViewBaseIndex + team1ViewOffsetX;
    {
        uint16_t idx = (d3 < 0) ? 0 : (uint16_t)d3;
        if (idx > 31) idx = 31;
        team1ViewIndex = idx;
    }

    int16_t d4 = team2ViewBaseIndex + team2ViewOffsetX;
    {
        uint16_t idx = (d4 < 0) ? 0 : (uint16_t)d4;
        if (idx > 31) idx = 31;
        team2ViewIndex = idx;
    }

    /* Compute strength difference */
    d3 = d3 - d4;
    if (viewResultModeFlag == 0) {
        d3 += 2;
    }
    if (d3 > 23) d3 = 23;
    else if (d3 < -23) d3 = -23;
    viewIndexDelta = d3;

    /* Look up goals from strength difference table */
    int16_t absDelta = d3;
    if (absDelta < 0) absDelta = -absDelta;

    uint8_t *goalLookup = simGoalsByStrengthDiffTable + ((uint16_t)absDelta << 2);
    uint16_t r = _Rand2();
    int16_t d1 = (int8_t)goalLookup[r & 3];

    /* Sentinel check: if negative, use high goal fallback */
    if (d1 < 0) {
        uint8_t *fallback = simHighGoalFallbackValues;
        r = _Rand2();
        uint16_t fbIdx = (uint16_t)((r * 6) >> 8);
        d1 = (int8_t)fallback[fbIdx];
    }
    /* d1 is now extended to int16_t */

    /* Look up additional goals from tactic-based probability */
    uint8_t *tacticGoalProbs = (uint8_t *)(uint32_t)simGoalProbByTacticPtrs[d1];
    r = _Rand2();
    int16_t d2 = 0;

    /* Walk up to 4 cumulative thresholds */
    uint8_t *tp = tacticGoalProbs;
    if (r >= tp[0]) {
        r -= tp[0]; d2++;
        if (r >= tp[1]) {
            r -= tp[1]; d2++;
            if (r >= tp[2]) {
                r -= tp[2]; d2++;
                if (r >= tp[3]) {
                    r -= tp[3]; d2++;

                    /* Exceeded 4 thresholds: use extra table */
                    uint8_t *extraProbs = (uint8_t *)(uint32_t)simExtraGoalProbByTacticPtrs[d1];
                    r = _Rand2();
                    d2 = 3;
                    if (r >= extraProbs[0]) {
                        r -= extraProbs[0]; d2++;
                        if (r >= extraProbs[1]) {
                            d2++;
                        }
                    }
                }
            }
        }
    }

    d1 += d2;  /* total = base + additional */

    /* If delta was negative, swap d1 and d2 */
    if (d3 < 0) {
        int16_t tmp = d1;
        d1 = d2;
        d2 = tmp;
    }

    team1SimulatedGoalsOut = (uint16_t)d1;
    team2SimulatedGoalsOut = (uint16_t)d2;

    /* Assign goals to scorers for first phase */
    AssignSimulatedGoalsToScorers(team1, team1PlayerRemapTable,
                                  team1GoalCounterByPlayer, (uint16_t)d1);
    AssignSimulatedGoalsToScorers(team2, team2PlayerRemapTable,
                                  team2GoalCounterByPlayer, (uint16_t)d2);

    /* Return the simulated goals from first phase */
    *outTeam1Goals = team1SimulatedGoalsOut;
    *outTeam2Goals = team2SimulatedGoalsOut;
}

/* ================================================================
 * ExtraTimeSecondPhaseScoring (loc_121E56)
 * Separate entry point called from GetCachedOrCalculateExtraTimeResult.
 * Performs second Y-offset-based scoring phase, adding goals to
 * team1/2SimulatedGoalsOut and assigning to scorers.
 * Returns d3 (team1 extra goals) and d4 (team2 extra goals).
 * ================================================================ */
__attribute__((used, externally_visible))
void _ExtraTimeSecondPhaseScoring(uint16_t *outD3, uint16_t *outD4) {
    int16_t d1 = team1ViewBaseIndex + team1ViewOffsetY;
    if (d1 < 0) d1 = 0;
    if (d1 > 31) d1 = 31;
    team1ViewIndex = (uint16_t)d1;

    int16_t d2 = team2ViewBaseIndex + team2ViewOffsetY;
    if (d2 < 0) d2 = 0;
    if (d2 > 31) d2 = 31;
    team2ViewIndex = (uint16_t)d2;

    /* Absolute difference of Y-based indices */
    int16_t yDiff = d1 - d2;
    if (yDiff < 0) yDiff = -yDiff;

    uint8_t *yGoalLookup = simGoalsByIndexDiffTable + ((uint16_t)yDiff << 2);
    uint16_t r = _Rand2();
    int16_t d3 = (int8_t)yGoalLookup[r & 3];

    /* Extra goals from Y-phase thresholds */
    uint8_t *extraA = simExtraGoalThresholdsA;
    r = _Rand2();
    int16_t d4 = 0;

    if (r >= extraA[0]) {
        r -= extraA[0]; d4++;
        if (r >= extraA[1]) {
            r -= extraA[1]; d4++;

            /* Use second threshold table */
            uint8_t *extraB = simExtraGoalThresholdsB;
            r = _Rand2();
            d4 = 1;
            if (r >= extraB[0]) {
                r -= extraB[0]; d4++;
            }
        }
    }

    d3 += d4;

    /* Assign extra goals: higher-indexed team gets d3, other gets d4 */
    if (!((uint16_t)d1 > (uint16_t)d2)) {
        int16_t tmp = d3;
        d3 = d4;
        d4 = tmp;
    }

    team1SimulatedGoalsOut += (uint16_t)d3;
    team2SimulatedGoalsOut += (uint16_t)d4;

    /* Assign second-phase goals to scorers */
    AssignSimulatedGoalsToScorers(viewResultTeam1Ptr, team1PlayerRemapTable,
                                  team1GoalCounterByPlayer, (uint16_t)d3);
    AssignSimulatedGoalsToScorers(viewResultTeam2Ptr, team2PlayerRemapTable,
                                  team2GoalCounterByPlayer, (uint16_t)d4);

    *outD3 = (uint16_t)d3;
    *outD4 = (uint16_t)d4;
}

/* ================================================================
 * PenaltyShootoutScorerPair (loc_121FCE)
 * Separate entry point called from GetCachedOrCalculatePenaltyShootoutResult.
 * Returns d5 and d6 via output params.
 * ================================================================ */
__attribute__((used, externally_visible))
void _PenaltyShootoutScorerPair(int16_t *outD5, uint16_t *outD6) {
    uint16_t d0 = _Rand2();
    uint16_t d6 = d0;
    uint8_t d5byte = simGoalScorerPairTable[d0 & 0x3F];

    /* If bit 7 of d6 is set, rotate d5 right by 4 (swap nibbles) */
    if (d6 & 0x80) {
        d5byte = (uint8_t)((d5byte >> 4) | (d5byte << 4));
    }

    d6 = d5byte;
    /* d5 = high nibble (sign-extended from byte) */
    *outD5 = (int16_t)(int8_t)(d5byte >> 4);
    /* d6 = low nibble */
    *outD6 = d6 & 0x0F;
}
