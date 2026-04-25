/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* training_match.c — Training match state save/restore functions.
 *
 * Chunk 78: 7 exported functions ported from SWOS2.s lines 38310-38512.
 * Chunk 176: 5 functions — sub swap candidates, trial players for new season.
 */

#include "training_match.h"
#include "endian_helpers.h"
#include "game_types.h"
#include "panic.h"
#include "career_status.h"

/* ---- ASM externs ---- */
extern uint8_t trainingMatchSavedPlayers[];     /* 32 bytes — player file indices */
extern uint16_t trainingMatchSavedPlayerPos;
extern uint8_t trainingMatchSavedShirtNums[];   /* 32 bytes */
extern uint8_t trainingMatchSavedTactics;
extern uint8_t lastMatchSavedShirtNums[];       /* 32 bytes */
extern uint8_t lastMatchPlayers[];              /* 32 bytes */
extern uint8_t currentMatchPlayers[];           /* 32 bytes */
extern uint8_t currentTactics;
extern uint16_t lastMatchSavedPlayerPos;
extern uint8_t lastMatchSavedTeamControls;
extern uint16_t lastMatchSavedCoachOrPlayer;
extern uint16_t coachOrPlayer;
extern TeamFileEx careerTeam;
extern uint16_t selTeamNumber;
extern uint16_t careerNumPlayers;
extern uint16_t selTeamSelectedPlayerPosition;
extern uint16_t panicReasonCode;

/* ---- C-callable ASM bridges ---- */
extern uint8_t *_GetPlayer(uint16_t teamNumber, uint16_t playerIndex);

/* Shirt number offset from GetPlayer result: TEAMFILE_PLAYERS(76) + PLAYERFILE_SHIRTNUMBER(2) */
#define GETPLAYER_SHIRTNUMBER_OFF  0x4E

/* ================================================================
 * TrainingMatch_SaveSelectedPlayerId  (SWOS2.s line 38310)
 *
 * Saves a player file index into the trainingMatchSavedPlayers
 * array at the position given by careerNumPlayers.
 * ================================================================ */
__attribute__((used, externally_visible))
void _TrainingMatch_SaveSelectedPlayerId(uint16_t value)
{
    trainingMatchSavedPlayers[careerNumPlayers] = (uint8_t)value;
}

/* ================================================================
 * TrainingMatch_RemoveSavedPlayerId  (SWOS2.s line 38320)
 *
 * Removes a player from the saved players array. If the player
 * was in the starting 11 (index < 11), finds the best replacement
 * from substitutes (index 11+) based on same position group and
 * highest price, then swaps them. Finally shifts the array to
 * compact and marks the last slot as 0xAA (unused).
 * ================================================================ */
__attribute__((used, externally_visible))
void _TrainingMatch_RemoveSavedPlayerId(uint16_t value)
{
    /* Clear saved player position if it matches */
    if (value == trainingMatchSavedPlayerPos)
        trainingMatchSavedPlayerPos = 0xFFFF;

    /* Find the player in the saved players array */
    int16_t foundIdx = -1;
    int16_t numPl = (int16_t)careerNumPlayers;
    for (int16_t i = numPl - 1; i >= 0; i--) {
        if (trainingMatchSavedPlayers[i] == (uint8_t)value) {
            foundIdx = i;
            break;
        }
    }
    if (foundIdx < 0) {
        panicReasonCode = 0x1231;
        _PanicHaltLoop();
        return;
    }

    int16_t shiftFrom = foundIdx;

    /* If in starting 11 (index < 11), find best replacement from subs */
    if (foundIdx < 11) {
        /* Get position group of the removed player */
        uint8_t *targetPlayer = _GetPlayer(selTeamNumber, value);
        PlayerFile *targetPf = (PlayerFile *)(targetPlayer + sizeof(TeamFile));
        uint8_t targetPosGroup = (targetPf->positionAndFace & 0xE0) >> 5;

        /* Search substitutes (index 11 to careerNumPlayers-1) for best replacement */
        int8_t bestPrice = -1;   /* 0xFF as signed byte */
        int16_t bestIdx = -1;
        for (int16_t d4 = 11; d4 < numPl; d4++) {
            int16_t pIdx = (int16_t)(int8_t)trainingMatchSavedPlayers[d4];
            PlayerFile *subPf = &((TeamFileEx *)&careerTeam)->players[pIdx];
            uint8_t posGroup = (subPf->positionAndFace & 0xE0) >> 5;
            if (posGroup != targetPosGroup)
                continue;
            int8_t price = (int8_t)subPf->price;
            if (price > bestPrice) {
                bestPrice = price;
                bestIdx = d4;
            }
        }

        /* If no same-position replacement found, default to index 11 */
        if (bestPrice < 0)
            bestIdx = 11;

        /* Swap: copy shirt number and player entry */
        uint8_t removedPlayerFileIdx = trainingMatchSavedPlayers[foundIdx];
        uint8_t replacementPlayerFileIdx = trainingMatchSavedPlayers[bestIdx];
        trainingMatchSavedShirtNums[replacementPlayerFileIdx] =
            trainingMatchSavedShirtNums[removedPlayerFileIdx];
        trainingMatchSavedPlayers[foundIdx] = trainingMatchSavedPlayers[bestIdx];
        shiftFrom = bestIdx;
    }

    /* Shift entries from shiftFrom+1 down by 1 */
    int16_t shiftCount = numPl - shiftFrom - 2;
    for (int16_t j = 0; j <= shiftCount; j++) {
        trainingMatchSavedPlayers[shiftFrom + j] =
            trainingMatchSavedPlayers[shiftFrom + j + 1];
    }

    /* Mark last entry as unused */
    trainingMatchSavedPlayers[numPl - 1] = 0xAA;
}

/* ================================================================
 * TrainingMatch_SaveLastMatchState  (SWOS2.s line 38395)
 *
 * Saves the current match lineup state (shirt numbers, player
 * assignments, tactics, position, controls, coach mode).
 * Note: loops 32 times (indices 31..0), extending beyond the
 * 16-player TeamFileEx into adjacent memory — this matches the
 * original asm behavior.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _TrainingMatch_SaveLastMatchState(void)
{
    /* Save shirt numbers via GetPlayer for all 32 indices */
    for (int16_t i = 31; i >= 0; i--) {
        uint8_t *player = _GetPlayer(selTeamNumber, (uint16_t)i);
        lastMatchSavedShirtNums[i] = player[GETPLAYER_SHIRTNUMBER_OFF];
    }

    /* Copy currentMatchPlayers to lastMatchPlayers */
    for (int16_t i = 0; i < 32; i++) {
        lastMatchPlayers[i] = currentMatchPlayers[i];
    }

    /* Save tactics, player position, team controls, coach mode */
    currentTactics = careerTeam.team.tactics;
    lastMatchSavedPlayerPos = selTeamSelectedPlayerPosition;
    lastMatchSavedTeamControls = careerTeam.team.teamControls;
    lastMatchSavedCoachOrPlayer = coachOrPlayer;
}

/* ================================================================
 * TrainingMatch_RestoreLastMatchState  (SWOS2.s line 38420)
 *
 * Restores the previously saved match lineup state.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _TrainingMatch_RestoreLastMatchState(void)
{
    /* Restore coach mode, team controls, player position, tactics */
    coachOrPlayer = lastMatchSavedCoachOrPlayer;
    careerTeam.team.teamControls = lastMatchSavedTeamControls;
    selTeamSelectedPlayerPosition = lastMatchSavedPlayerPos;
    careerTeam.team.tactics = currentTactics;

    /* Copy lastMatchPlayers to currentMatchPlayers */
    for (int16_t i = 0; i < 32; i++) {
        currentMatchPlayers[i] = lastMatchPlayers[i];
    }

    /* Restore shirt numbers via GetPlayer for all 32 indices */
    for (int16_t i = 31; i >= 0; i--) {
        uint8_t *player = _GetPlayer(selTeamNumber, (uint16_t)i);
        player[GETPLAYER_SHIRTNUMBER_OFF] = lastMatchSavedShirtNums[i];
    }
}

/* ================================================================
 * TrainingMatch_RestoreSavedTeamSelectionState  (SWOS2.s line 38445)
 *
 * Restores team selection state from training match saved data.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _TrainingMatch_RestoreSavedTeamSelectionState(void)
{
    /* Copy trainingMatchSavedPlayers to currentMatchPlayers */
    for (int16_t i = 0; i < 32; i++) {
        currentMatchPlayers[i] = trainingMatchSavedPlayers[i];
    }

    /* Restore tactics and player position */
    careerTeam.team.tactics = trainingMatchSavedTactics;
    selTeamSelectedPlayerPosition = trainingMatchSavedPlayerPos;

    /* Restore shirt numbers via GetPlayer for all 32 indices */
    for (int16_t i = 31; i >= 0; i--) {
        uint8_t *player = _GetPlayer(selTeamNumber, (uint16_t)i);
        player[GETPLAYER_SHIRTNUMBER_OFF] = trainingMatchSavedShirtNums[i];
    }
}

/* ================================================================
 * TrainingMatch_SaveTeamSelectionState  (SWOS2.s line 38467)
 *
 * Saves current team selection state for training match.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _TrainingMatch_SaveTeamSelectionState(void)
{
    /* Copy currentMatchPlayers to trainingMatchSavedPlayers */
    for (int16_t i = 0; i < 32; i++) {
        trainingMatchSavedPlayers[i] = currentMatchPlayers[i];
    }

    /* Save tactics and player position */
    trainingMatchSavedTactics = careerTeam.team.tactics;
    trainingMatchSavedPlayerPos = selTeamSelectedPlayerPosition;

    /* Save shirt numbers via GetPlayer for all 32 indices */
    for (int16_t i = 31; i >= 0; i--) {
        uint8_t *player = _GetPlayer(selTeamNumber, (uint16_t)i);
        trainingMatchSavedShirtNums[i] = player[GETPLAYER_SHIRTNUMBER_OFF];
    }
}

/* ================================================================
 * CheckIfTeamIsChanged  (SWOS2.s line 38493)
 *
 * Checks if the current team lineup differs from the last match.
 * Compares the first 11 players, tactics, and selected player
 * position.
 *
 * Returns: 0 if same (Z set in asm), 1 if different (Z clear).
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _CheckIfTeamIsChanged(void)
{
    /* Compare first 11 player entries */
    for (int16_t i = 0; i <= 10; i++) {
        if (lastMatchPlayers[i] != currentMatchPlayers[i])
            return 1;
    }

    /* Compare tactics */
    if (currentTactics != careerTeam.team.tactics)
        return 1;

    /* Compare selected player position */
    if (lastMatchSavedPlayerPos != selTeamSelectedPlayerPosition)
        return 1;

    return 0;
}

/* ================================================================
 * Chunk 176 — Training match sub swap candidates and trial players
 * ================================================================ */

/* Additional externs for Chunk 176 */
extern uint16_t isNationalSelector;
extern TeamFileEx currentTeam;
extern uint16_t _Rand2(void);
extern uint8_t *_GetPlayerAtIndex(uint16_t teamNumber, uint16_t ordinal);

/* ================================================================
 * isPlayerBetterSwapCandidate (static)  (SWOS2.s line 36086)
 *
 * Checks if candidate player is a better substitute swap than current best.
 * Players with someFlag 0x9A/0x9B (trialists/special) are avoided in favor
 * of normal players. Between two players of same type, higher price wins.
 *
 * Returns 0 if candidate IS better (should replace), 1 if NOT better.
 * bestSkill < 0 (0xFF) means uninitialized — any candidate wins.
 * ================================================================ */
static uint16_t isPlayerBetterSwapCandidate(uint8_t *candidate, uint8_t *best, int8_t bestSkill)
{
    PlayerFile *candPf = (PlayerFile *)(candidate + 76);
    PlayerFile *bestPf = (PlayerFile *)(best + 76);

    if (bestSkill < 0)
        goto compare_skill;

    uint8_t candFlag = candPf->someFlag;
    int16_t candHasFlag = (candFlag == 0x9B || candFlag == 0x9A);

    if (candHasFlag) {
        /* Candidate has special flag */
        uint8_t bestFlag = bestPf->someFlag;
        if (bestFlag == 0x9B || bestFlag == 0x9A) {
            /* Both have flag — compare by price */
            goto compare_skill;
        }
        /* Candidate has flag, best doesn't — NOT better */
        return 1;
    } else {
        /* Candidate doesn't have flag */
        uint8_t bestFlag = bestPf->someFlag;
        if (bestFlag == 0x9B || bestFlag == 0x9A) {
            /* Best has flag, candidate doesn't — candidate IS better */
            return 0;
        }
        /* Neither has flag — fall through to compare skill */
    }

compare_skill:
    /* cmp.b $6C(a0),d0 + bge → if bestSkill >= candidate.price, NOT better */
    if (bestSkill >= candPf->price)
        return 1;  /* NOT better */

    return 0;  /* IS better */
}

/* ================================================================
 * findBestSubSwapCandidates (static)  (SWOS2.s line 36045)
 *
 * Iterates players from index 11 to careerNumPlayers-1, finding the
 * two best non-goalkeeper outfield players to swap into sub positions.
 *
 * Returns: best1Index, best2Index (swap target indices),
 *          best1Skill, best2Skill (0xFF = no candidate found).
 * ================================================================ */
typedef struct {
    uint16_t best1Index;  /* d2 */
    uint16_t best2Index;  /* d3 */
    int8_t   best1Skill;  /* d4 */
    int8_t   best2Skill;  /* d5 */
} SubSwapResult;

static SubSwapResult findBestSubSwapCandidates(void)
{
    SubSwapResult result;
    result.best1Skill = -1;  /* 0xFF */
    result.best2Skill = -1;  /* 0xFF */
    result.best1Index = 0;
    result.best2Index = 0;

    uint8_t *best1Ptr = 0;
    uint8_t *best2Ptr = 0;

    for (uint16_t i = 11; i != careerNumPlayers; i++) {
        uint8_t *player = _GetPlayerAtIndex(selTeamNumber, i);
        PlayerFile *pf = (PlayerFile *)(player + 76);

        /* Extract position from positionAndFace (bits 7:5) */
        uint16_t position = (pf->positionAndFace & 0xE0) >> 5;
        if (position == 0)  /* Skip goalkeepers */
            continue;

        /* Check against best1 */
        if (isPlayerBetterSwapCandidate(player, best1Ptr ? best1Ptr : player,
                                         result.best1Skill) == 0) {
            /* Candidate replaces best1; demote best1 to best2 */
            best2Ptr = best1Ptr;
            result.best2Skill = result.best1Skill;
            result.best2Index = result.best1Index;

            best1Ptr = player;
            result.best1Skill = pf->price;
            result.best1Index = i;
            continue;
        }

        /* Check against best2 */
        if (isPlayerBetterSwapCandidate(player, best2Ptr ? best2Ptr : player,
                                         result.best2Skill) == 0) {
            /* Candidate replaces best2 */
            best2Ptr = player;
            result.best2Skill = pf->price;
            result.best2Index = i;
        }
    }

    return result;
}

/* ================================================================
 * TrainingMatch_ApplyUseMatchTeamAndSwapSubs  (SWOS2.s line 36022)
 *
 * Saves team selection state, copies tactics from career to current team,
 * finds best substitute swap candidates and swaps them into positions 11/12
 * of the trainingMatchSavedPlayers array.
 * ================================================================ */
__attribute__((used, externally_visible))
void _TrainingMatch_ApplyUseMatchTeamAndSwapSubs(void)
{
    _TrainingMatch_SaveTeamSelectionState();

    /* Copy tactics byte from careerTeam to currentTeam */
    currentTeam.team.tactics = careerTeam.team.tactics;

    /* bra.w *+4 → fall through to loc_130264 */
    SubSwapResult sr = findBestSubSwapCandidates();

    /* Swap best1 into position 11 if found */
    if (sr.best1Skill >= 0) {
        uint8_t temp = trainingMatchSavedPlayers[11];
        trainingMatchSavedPlayers[11] = trainingMatchSavedPlayers[sr.best1Index];
        trainingMatchSavedPlayers[sr.best1Index] = temp;
    }

    /* Swap best2 into position 12 if found */
    if (sr.best2Skill >= 0) {
        uint8_t temp = trainingMatchSavedPlayers[12];
        trainingMatchSavedPlayers[12] = trainingMatchSavedPlayers[sr.best2Index];
        trainingMatchSavedPlayers[sr.best2Index] = temp;
    }
}

/* ================================================================
 * TrainingMatch_TryAddTrialPlayerAtPosition  (SWOS2.s line 36143)
 *
 * Adds a trial player at the given position, then with ~40% probability
 * (random >= 0x99) marks the player's someFlag as 0x9A (trialist) and
 * recalculates their weekly wage.
 * ================================================================ */
__attribute__((used, externally_visible))
void _TrainingMatch_TryAddTrialPlayerAtPosition(uint16_t position)
{
    uint8_t *player = _TrainingMatch_AddTrialPlayerAtPosition(position);
    uint16_t r = _Rand2();
    if (r < 0x99)
        return;

    /* Mark as trialist */
    PlayerFile *pf = (PlayerFile *)(player + 76);
    pf->someFlag = (int8_t)0x9A;

    /* Recover the player index from trainingMatchSavedPlayers.
     * AddTrialPlayerAtPosition saved freeSlot at index (careerNumPlayers-1)
     * via _CTrainingMatch_SaveSelectedPlayerId before incrementing count. */
    uint16_t playerIndex = trainingMatchSavedPlayers[careerNumPlayers - 1];
    _Career_UpdatePlayerWeeklyWageCache(playerIndex);
}

/* ================================================================
 * TrainingMatch_AddTrialPlayersForNewSeason  (SWOS2.s line 36116)
 *
 * If not national selector, adds 10 trial players at specific positions
 * (2,1,5,4,3,3,6,6,7,7), then applies team swap with best substitutes.
 * ================================================================ */
__attribute__((used, externally_visible))
void _TrainingMatch_AddTrialPlayersForNewSeason(void)
{
    if (isNationalSelector != 0)
        return;

    _TrainingMatch_TryAddTrialPlayerAtPosition(2);
    _TrainingMatch_TryAddTrialPlayerAtPosition(1);
    _TrainingMatch_TryAddTrialPlayerAtPosition(5);
    _TrainingMatch_TryAddTrialPlayerAtPosition(4);
    _TrainingMatch_TryAddTrialPlayerAtPosition(3);
    _TrainingMatch_TryAddTrialPlayerAtPosition(3);
    _TrainingMatch_TryAddTrialPlayerAtPosition(6);
    _TrainingMatch_TryAddTrialPlayerAtPosition(6);
    _TrainingMatch_TryAddTrialPlayerAtPosition(7);
    _TrainingMatch_TryAddTrialPlayerAtPosition(7);

    _TrainingMatch_ApplyUseMatchTeamAndSwapSubs();
}

/* ================================================================
 * Chunk 200 — PlayTrainingGame + TrainingMatch_BuildCurrentTeamFromCareerTeam
 * ================================================================ */

/* Additional externs for Chunk 200 */
extern uint16_t trainingGame;
extern uint16_t currentTeamNumber;
extern uint16_t currentTeamSelectedPlayerPosition;
extern uint8_t playerPositions[];
extern uint16_t numPlayers;
extern uint8_t teamAName[];
extern uint8_t teamBName[];
extern uint32_t teamTacticsTable[];
extern uint32_t tacticPosMapPtrs[];
extern uint32_t trainingPosOrderTables[];
extern uint16_t pitchTypeOrSeason;
extern uint16_t friendlyPitchType;
extern uint16_t friendlySeason;
extern uint16_t minSubstitutes;
extern uint16_t maxSubstitutes;
extern void _TrainingMatch_AdjustSelectedTeamRosterForTrialists(uint16_t teamNum, uint16_t baseCount, TeamFileEx *teamPtr);
extern void _InitAndPlayGame(TeamFileEx *team1, TeamFileEx *team2);
extern uint8_t *_GetPlayerAtIndex(uint16_t teamNumber, uint16_t ordinal);
extern uint16_t _IsPlayerInjured(PlayerFile *playerFile, int16_t mode);
extern uint8_t *_StringCopy(const uint8_t *src, uint8_t *dst);
extern uint16_t _GetStringLength(const uint8_t *str);
extern void _PanicHaltLoop(void);

/* ================================================================
 * TrainingMatch_BuildCurrentTeamFromCareerTeam  (SWOS2.s line 32807)
 *
 * Builds a training match team from the career team:
 * 1. Copies first 76 bytes (team header) preserving currentTeam's tactics
 * 2. Copies non-injured players' data (38 bytes each) from career team
 * 3. Assigns players to tactical positions based on position groups
 *    and training priority order tables, preferring non-trialists
 * 4. Assigns shirt numbers (1-13) to all players
 * 5. Builds team A/B names for the training match
 * ================================================================ */
__attribute__((used, externally_visible))
void _TrainingMatch_BuildCurrentTeamFromCareerTeam(void)
{
    uint8_t *a5 = (uint8_t *)&careerTeam;
    uint8_t *a6 = (uint8_t *)&currentTeam;
    TeamFileEx *teamEx = (TeamFileEx *)a6;

    currentTeamSelectedPlayerPosition = 0xFFFF;

    /* Save tactics byte from currentTeam before overwrite */
    uint8_t savedTactics = teamEx->team.tactics;

    /* Copy first 0x4C (76) bytes from careerTeam to currentTeam */
    {
        volatile uint8_t *src = (volatile uint8_t *)a5;
        volatile uint8_t *dst = (volatile uint8_t *)a6;
        for (int16_t i = 0x4B; i >= 0; i--)
            *dst++ = *src++;
    }

    /* Restore tactics byte */
    teamEx->team.tactics = savedTactics;

    /* Copy non-injured player data from career team to current team */
    uint8_t *a4 = playerPositions;
    numPlayers = 0;
    uint8_t *destPlayer = (uint8_t *)&teamEx->players[0];
    uint16_t d6 = 0; /* output player index */
    int16_t d7 = (int16_t)careerNumPlayers - 1;

    /* Loop from careerNumPlayers-1 down to 13 (inclusive) */
    while ((uint16_t)d7 >= 13) {
        uint8_t *playerBase = _GetPlayerAtIndex(selTeamNumber, (uint16_t)d7);

        if (_IsPlayerInjured((PlayerFile *)playerBase, 0) == 0) {
            /* Not injured — copy 38 bytes (sizeof(PlayerFile)) from player data */
            PlayerFile *srcPf = (PlayerFile *)(playerBase + 76);
            volatile uint8_t *srcV = (volatile uint8_t *)srcPf;
            volatile uint8_t *dstV = (volatile uint8_t *)destPlayer;
            for (int16_t i = PLAYERFILE_SIZE - 1; i >= 0; i--)
                *dstV++ = *srcV++;
            destPlayer = (uint8_t *)dstV;
            *a4++ = (uint8_t)d6;
            numPlayers++;
            d6++;
        }

        d7--;
    }

    /* Set team metadata */
    teamEx->team.teamOrdinal = 0x99;
    teamEx->team.teamControls = 1;
    /* Asm: move.w (a6),(currentTeamNumber) — reads first word = packed (countryNumber<<8 | teamOrdinal) */
    currentTeamNumber = PACK_BE_U16(teamEx->team.countryNumber, teamEx->team.teamOrdinal);

    /* Look up tactics and position map */
    int16_t tacticsIdx = (int8_t)teamEx->team.tactics;
    uint8_t *tacticsData = (uint8_t *)teamTacticsTable[(uint16_t)tacticsIdx];
    int16_t posMapIdx = (int8_t)tacticsData[0x171];
    uint8_t *posMap = (uint8_t *)tacticPosMapPtrs[(uint16_t)posMapIdx];

    a4 = playerPositions;
    uint32_t assignedMask = 0; /* bit d6 set = position d6 assigned */
    uint16_t d4 = 0; /* priority order index */

    /* Outer loop: iterate through priority levels */
    for (;;) {
        /* Inner loop: iterate through 11 tactical positions */
        for (d6 = 0; d6 < 11; d6++) {
            if (assignedMask & (1u << d6))
                continue; /* position already assigned */

            int16_t posGroup = (int8_t)posMap[d6];
            uint8_t *posOrder = (uint8_t *)trainingPosOrderTables[(uint16_t)posGroup];
            int8_t targetPos = (int8_t)posOrder[d4];
            if (targetPos < 0) {
                /* No valid position order entry — panic */
                panicReasonCode = 0x2323;
                _PanicHaltLoop();
                return;
            }

            /* Search for best available player for this position */
            int8_t bestSkill = -1; /* 0xFF as signed byte */
            uint8_t *bestPlayer = (void *)0;
            int8_t bestIdx = 0;

            for (int16_t d1 = (int16_t)numPlayers - 1; d1 >= 0; d1--) {
                if (assignedMask & (1u << d1))
                    continue; /* player slot already assigned */

                uint8_t *player = _GetPlayerAtIndex(currentTeamNumber, (uint16_t)d1);
                PlayerFile *pf = (PlayerFile *)(player + 76);
                uint8_t positionGroup = (pf->positionAndFace & 0xE0) >> 5;
                if (positionGroup != (uint8_t)targetPos)
                    continue; /* position doesn't match */

                PlayerFile *bestPf = bestPlayer ? (PlayerFile *)(bestPlayer + 76) : (PlayerFile *)0;

                if (bestPlayer == (void *)0) {
                    /* First matching player — go to skill compare */
                    goto skillCompare;
                }

                /* Check trialist status */
                int16_t isCurrentTrialist = ((uint8_t)pf->someFlag == 0x9B || (uint8_t)pf->someFlag == 0x9A);
                int16_t isBestTrialist = ((uint8_t)bestPf->someFlag == 0x9B || (uint8_t)bestPf->someFlag == 0x9A);

                if (isCurrentTrialist) {
                    if (!isBestTrialist)
                        continue; /* best is non-trialist, keep it */
                    /* both trialist — compare skills */
                } else {
                    if (isBestTrialist)
                        goto updateBest; /* replace trialist with non-trialist */
                    /* both non-trialist — compare skills */
                }

            skillCompare:
                if (bestSkill > (int8_t)pf->price)
                    continue; /* best is better, keep it */

            updateBest:
                bestIdx = (int8_t)d1;
                bestSkill = (int8_t)pf->price;
                bestPlayer = player;
            }

            if (bestSkill < 0)
                continue; /* no match found (still -1) */

            /* Swap playerPositions[bestIdx] and playerPositions[d6] */
            int16_t d3 = (int16_t)(int8_t)bestIdx;
            uint8_t tmp = a4[d3];
            a4[d3] = a4[d6];
            a4[d6] = tmp;
            assignedMask |= (1u << d6);

            if (assignedMask == 0x7FF)
                goto allAssigned; /* all 11 positions assigned */
        }
        d4++;
    }

allAssigned:
    /* Assign shirt numbers to players 0-12 */
    for (int16_t d1 = 12; d1 >= 0; d1--) {
        uint8_t *player = _GetPlayerAtIndex(currentTeamNumber, (uint16_t)d1);
        PlayerFile *pf = (PlayerFile *)(player + 76);
        pf->shirtNumber = (int8_t)(d1 + 1);
    }

    /* Build team A name */
    _StringCopy((const uint8_t *)(a5 + 5), teamAName); /* careerTeam + TEAMFILE_TEAMNAME(5) */
    uint16_t len = _GetStringLength(teamAName);
    uint8_t *p = teamAName + len; /* points to null terminator */
    if (len > 14) {
        if (len == 16)
            p -= 3;
        else
            p -= 2;
        *p++ = '.';
    }
    *p++ = ' ';
    *p++ = 'A';
    *p = 0;

    /* Build team B name (copy A, replace 'A' with 'B') */
    uint8_t *endPos = _StringCopy(teamAName, teamBName);
    *(endPos - 1) = 'B';
}

/* ================================================================
 * PlayTrainingGame  (SWOS2.s line 32771)
 *
 * Launches a training match from career mode:
 * 1. Adjusts roster for trialists
 * 2. Saves/restores match state
 * 3. Builds current team from career team
 * 4. Sets up friendly match parameters
 * 5. Plays the game
 * 6. Cleans up
 * ================================================================ */
__attribute__((used, externally_visible))
void _PlayTrainingGame(void)
{
    trainingGame = 1;

    _TrainingMatch_AdjustSelectedTeamRosterForTrialists(1, 2, &careerTeam);
    _TrainingMatch_SaveLastMatchState();
    _TrainingMatch_RestoreSavedTeamSelectionState();
    _TrainingMatch_BuildCurrentTeamFromCareerTeam();

    pitchTypeOrSeason = 0;
    friendlyPitchType = 0xFFFF; /* RANDOM */
    friendlySeason = 8;
    minSubstitutes = 2;
    maxSubstitutes = 2;

    _InitAndPlayGame(&careerTeam, &currentTeam);

    currentTeamNumber = 0xFFFF;
    trainingGame = 0;

    _TrainingMatch_SaveTeamSelectionState();
    _TrainingMatch_RestoreLastMatchState();
}
