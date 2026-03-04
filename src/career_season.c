/* career_season.c — Career season management, qualification, team list building.
 *
 * Chunks 67-68: 2 exported functions ported from SWOS2.s.
 */

#include "career_season.h"
#include "diy_competition.h"
#include "game_types.h"
#include "random.h"

/* ---- ASM externs ---- */
extern uint16_t selTeamNumber;
extern uint16_t isNationalSelector;
extern uint16_t teamNoOfferAccepted;
extern uint16_t seasonReview;
extern uint16_t currentMessage;

extern uint8_t euroCupCopy[];
extern uint8_t cupWinnersCupCopy[];
extern uint8_t uefaCupCopy[];

extern uint8_t *euroCupTeamListPtr;
extern uint8_t *euroCupTeamListWritePtr;
extern uint8_t *cupWinnersCupTeamListPtr;
extern uint8_t *cupWinnersCupTeamListWritePtr;
extern uint8_t *uefaCupTeamListPtr;
extern uint8_t *uefaCupTeamListWritePtr;

extern uint16_t euroCupQualifiedTeamCount;
extern uint16_t cupWinnersCupQualifiedTeamCount;
extern uint16_t uefaCupQualifiedTeamCount;

extern uint8_t euroCupQualGroupSeedLists_4_6_11[];
extern uint8_t euroCupQualGroupSeedListsReduced_4_6_11[];
extern uint8_t euroCupQualGroupSeedBuckets_15_18_22[];

extern uint32_t countriesTable[];

extern uint8_t euroCupEligibleCompetitionIds[];
extern uint8_t cupWinnersCupEligibleCompetitionIds[];
extern uint8_t uefaCupEligibleCompetitionIds[];

/* ================================================================
 * Career_FillRandomTeamListExcludingSelected  (SWOS2.s line 34278)
 *
 * Fills competition team list from a table of seed group entries,
 * performing a Fisher-Yates shuffle on each group. Tracks how
 * many times the selected team naturally appears and injects
 * additional copies at the end if needed.
 *
 * Asm: d0 = excludeCount, a0 = table, a1 = out
 *
 * Table format: each entry is {uint32_t groupPtr, uint16_t groupSize,
 *   uint16_t copyCount}, terminated by groupPtr == 0xFFFFFFFF.
 *
 * After filling: appends 0xFF terminator, then if excludeCount
 * natural appearances haven't been found, walks backward and
 * replaces non-selTeam entries with selTeam to reach the target.
 *
 * NOTE: selTeamNumber MSB = country/league index (68k move.b reads MSB
 * on big-endian). Seed list entries are country indices, so we compare
 * against (selTeamNumber >> 8).
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_FillRandomTeamListExcludingSelected(uint16_t excludeCount,
                                                  uint8_t *table, uint8_t *compIds)
{
    /* move.b (selTeamNumber).l,d0 — reads MSB on big-endian 68k */
    uint8_t selTeam = (uint8_t)(selTeamNumber >> 8);
    int16_t d7 = (int16_t)excludeCount;
    uint8_t *out = compIds;

    for (;;) {
        /* Read group pointer (4 bytes) */
        uint8_t *groupPtr = *(uint8_t **)table;
        table += 4;
        if ((uint32_t)groupPtr == 0xFFFFFFFF)
            break;

        /* Read group size and copy count (2 words each) */
        uint16_t groupSize = *(uint16_t *)table;
        table += 2;
        uint16_t copyCount = *(uint16_t *)table;
        table += 2;

        /* Fisher-Yates shuffle if groupSize != copyCount */
        if (groupSize != copyCount) {
            uint16_t n = groupSize;     /* d3 = groupSize */
            for (int16_t i = (int16_t)(groupSize - 1); i >= 0; i--) {
                uint16_t r = _Rand2();
                /* mulu.w d3,d0; lsr.w #8,d0 — index = low_word(r * n) >> 8 */
                uint16_t idx = (uint16_t)((uint32_t)r * n) >> 8;
                /* Swap groupPtr[i] and groupPtr[idx] */
                uint8_t tmp = groupPtr[i];
                groupPtr[i] = groupPtr[idx];
                groupPtr[idx] = tmp;
            }
        }

        /* Copy copyCount entries, tracking selTeam matches */
        for (int16_t j = (int16_t)(copyCount - 1); j >= 0; j--) {
            *out++ = *groupPtr++;
            if (*(out - 1) == selTeam) {
                d7--;   /* one less replacement needed */
            }
        }
    }

    /* Append 0xFF terminator */
    *out = 0xFF;

    /* Check if we still need to inject selTeam entries */
    d7--;
    if (d7 < 0)
        return;

    /* Walk backward, skip selTeam entries, replace others with selTeam */
    do {
        while (*(--out) == selTeam)
            ;   /* skip entries already containing selTeam */
        *out = selTeam;
    } while (--d7 >= 0);
}

/* ================================================================
 * Career_PrepareEuropeanCupQualificationState  (SWOS2.s line 33743)
 *
 * Sets up team lists for European Cup, Cup Winners' Cup, and UEFA Cup
 * qualification draws. For each cup:
 *   - Computes team list write pointer from cup copy data
 *   - Clears qualified team count
 *   - Determines exclude count (1 if player's country is European)
 *   - Calls Career_FillRandomTeamListExcludingSelected to fill the
 *     shuffled team list
 *
 * UEFA Cup has a special pre-scan: counts how many times the selected
 * team's country index appears across all seed bucket data (contiguous
 * from euroCupQualSeedBucket15 through euroCupQualSeedBucket22Full,
 * terminated by 0xFF).
 *
 * Temporarily overrides selTeamNumber from teamNoOfferAccepted if a
 * national team transfer is pending (team byte in 0x50-0x55 range).
 * Saves and restores both selTeamNumber and isNationalSelector.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_PrepareEuropeanCupQualificationState(void)
{
    /* Save state — asm: move.w (selTeamNumber),-(sp); move.w (isNationalSelector),-(sp) */
    uint16_t savedSelTeam = selTeamNumber;
    uint16_t savedIsNational = isNationalSelector;

    /* If a team offer has been accepted, temporarily use that team number */
    if (teamNoOfferAccepted != 0xFFFF) {
        selTeamNumber = teamNoOfferAccepted;
        /* move.b (selTeamNumber).l,d0 — reads MSB (country/league index) */
        uint8_t teamByte = (uint8_t)(selTeamNumber >> 8);
        if (teamByte >= 0x50 && teamByte <= 0x55) {
            /* National team range — set flag (asm: move.b #1,(isNationalSelector)) */
            isNationalSelector = 1;
        }
    }

    /* --- European Cup --- */
    {
        uint8_t *base = euroCupCopy;
        uint8_t offset = base[7];
        uint8_t *teamList = &base[7 + offset];
        euroCupTeamListPtr = teamList;
        euroCupTeamListWritePtr = teamList;
        euroCupQualifiedTeamCount = 0;

        uint16_t excludeCount = 0;
        if (isNationalSelector == 0) {
            uint8_t teamByte = (uint8_t)(selTeamNumber >> 8);
            uint8_t *countryPtr = (uint8_t *)countriesTable[teamByte];
            if (*countryPtr == 0) {
                excludeCount = 1;
            }
        }
        _Career_FillRandomTeamListExcludingSelected(excludeCount,
            euroCupQualGroupSeedLists_4_6_11, euroCupEligibleCompetitionIds);
    }

    /* --- Cup Winners' Cup --- */
    {
        uint8_t *base = cupWinnersCupCopy;
        uint8_t offset = base[7];
        uint8_t *teamList = &base[7 + offset];
        cupWinnersCupTeamListPtr = teamList;
        cupWinnersCupTeamListWritePtr = teamList;
        cupWinnersCupQualifiedTeamCount = 0;

        uint16_t excludeCount = 0;
        if (isNationalSelector == 0) {
            uint8_t teamByte = (uint8_t)(selTeamNumber >> 8);
            uint8_t *countryPtr = (uint8_t *)countriesTable[teamByte];
            if (*countryPtr == 0) {
                excludeCount = 1;
            }
        }
        _Career_FillRandomTeamListExcludingSelected(excludeCount,
            euroCupQualGroupSeedListsReduced_4_6_11, cupWinnersCupEligibleCompetitionIds);
    }

    /* --- UEFA Cup --- */
    {
        uint8_t *base = uefaCupCopy;
        uint8_t offset = base[7];
        uint8_t *teamList = &base[7 + offset];
        uefaCupTeamListPtr = teamList;
        uefaCupTeamListWritePtr = teamList;
        uefaCupQualifiedTeamCount = 0;

        /* Special count: scan ALL seed bucket data (contiguous from first bucket
         * through euroCupQualSeedBucket22Full which ends with 0xFF) to count how
         * many times the selected team's country index appears.
         * Asm: movea.l (euroCupQualGroupSeedBuckets_15_18_22).l,a0 — loads first
         * entry's pointer (to start of contiguous bucket data). */
        uint8_t *bucketData = *(uint8_t **)euroCupQualGroupSeedBuckets_15_18_22;
        uint16_t excludeCount = 0;
        if (isNationalSelector == 0) {
            uint8_t teamByte = (uint8_t)(selTeamNumber >> 8);
            while (*bucketData != 0xFF) {
                if (*bucketData == teamByte) {
                    excludeCount++;
                }
                bucketData++;
            }
        }
        _Career_FillRandomTeamListExcludingSelected(excludeCount,
            euroCupQualGroupSeedBuckets_15_18_22, uefaCupEligibleCompetitionIds);
    }

    /* Restore state — asm: move.w (sp)+,(isNationalSelector); move.w (sp)+,(selTeamNumber) */
    isNationalSelector = savedIsNational;
    selTeamNumber = savedSelTeam;
}

/* ---- Chunk 145: GetCompetitionAbbrevStr ---- */
/* SWOS2.s line 54905 — returns abbreviation string for a competition pointer.
 * Compares pointer values against known competition buffer addresses. */

extern uint8_t friendlyCopy[];
extern uint8_t worldCup2Copy[];
extern uint8_t worldCupQualifyingGroup1Copy[];
extern uint8_t oceaniaWorldCupQualificationCopy[];
extern uint8_t europeanChampionships2Copy[];
extern uint8_t ecQualifyingGroup1Copy[];
extern uint8_t ecQualifyingGroup8Copy[];
extern uint8_t northAmericanCupCopy[];
extern uint8_t copaAmericaCopy[];
extern uint8_t africanNationsCupCopy[];
extern uint8_t asianCupCopy[];
extern uint8_t oceaniaCupCopy[];
extern uint8_t invitationTournamentCopy[];
extern uint8_t tourCopy[];

extern uint16_t panicReasonCode;
extern void _PanicHaltLoop(void);

__attribute__((used, externally_visible))
uint8_t *_GetCompetitionAbbrevStr(uint8_t *competition)
{
    if (competition == 0)
        return (uint8_t *)"FR";
    if (competition == friendlyCopy)
        return (uint8_t *)"FR";
    if (competition == worldCup2Copy)
        return (uint8_t *)"WC";
    if (competition >= worldCupQualifyingGroup1Copy &&
        competition <= oceaniaWorldCupQualificationCopy)
        return (uint8_t *)"WCQ";
    if (competition == europeanChampionships2Copy)
        return (uint8_t *)"EC";
    if (competition >= ecQualifyingGroup1Copy &&
        competition <= ecQualifyingGroup8Copy)
        return (uint8_t *)"ECQ";
    if (competition == northAmericanCupCopy)
        return (uint8_t *)"NAC";
    if (competition == copaAmericaCopy)
        return (uint8_t *)"CA";
    if (competition == africanNationsCupCopy)
        return (uint8_t *)"ANC";
    if (competition == asianCupCopy)
        return (uint8_t *)"AC";
    if (competition == oceaniaCupCopy)
        return (uint8_t *)"OC";
    if (competition == invitationTournamentCopy)
        return (uint8_t *)"TNM";
    if (competition == tourCopy)
        return (uint8_t *)"TR";

    panicReasonCode = 0x2184;
    _PanicHaltLoop();
    return (uint8_t *)"FR"; /* unreachable */
}

/* ================================================================
 * Chunk 161: ShowSeasonOpinion  (SWOS2.s line 35922)
 *
 * Maps the season review score (0-4+) to a chairman message constant
 * and calls ShowChairmanMessage.
 * ================================================================ */
extern void _ShowChairmanMessage(void);

#define EXCELLENT_JOB_THIS_SEASON   27
#define GOOD_SEASON                 28
#define UP_AND_DOWN_SEASON2         29
#define NOT_A_VERY_GOOD_SEASON2     30
#define VERY_DISSAPOINTING_SEASON   31

__attribute__((used, externally_visible))
void _ShowSeasonOpinion(void)
{
    uint16_t d6 = seasonReview;
    uint16_t msg;
    if (d6 >= 4)
        msg = VERY_DISSAPOINTING_SEASON;
    else if (d6 == 3)
        msg = NOT_A_VERY_GOOD_SEASON2;
    else if (d6 == 2)
        msg = UP_AND_DOWN_SEASON2;
    else if (d6 == 1)
        msg = GOOD_SEASON;
    else
        msg = EXCELLENT_JOB_THIS_SEASON;

    currentMessage = msg;
    _ShowChairmanMessage();
}

/* ================================================================
 * Career_UpdateLastNationalityCall (SWOS2.s line 35081)
 *
 * Determines which national team the current player's nationality
 * maps to, and stores it in lastNationalityCall (or 0xFFFF if the
 * player's national team IS the selected team, or if not found).
 *
 * Returns the value stored in lastNationalityCall.
 * ================================================================ */
extern uint8_t  plNationality;
extern uint16_t lastNationalityCall;
extern int16_t _GetNationalTeamNumber(uint16_t nationality);
extern int16_t _GetNationalTeamNumberFromTeamFileId(uint16_t teamFileId);

__attribute__((used, externally_visible))
uint16_t _Career_UpdateLastNationalityCall(void)
{
    int16_t d0;
    uint8_t nat = plNationality;

    if (nat != 0xFF) {
        d0 = _GetNationalTeamNumber((uint16_t)nat);
        if (d0 != (int16_t)0xFFFF)
            goto check_self;
    }

    /* Nationality not found or is 0xFF — try team file ID fallback */
    d0 = (int16_t)0xFFFF;
    if (isNationalSelector != 0)
        goto check_self;

    /* move.b (selTeamNumber).l,d0 — reads high byte on big-endian 68k.
     * This extracts the team file ID (country file index) from selTeamNumber. */
    d0 = _GetNationalTeamNumberFromTeamFileId(selTeamNumber >> 8);

check_self:
    /* If the national team IS the selected team, set to 0xFFFF (no call-up) */
    if ((uint16_t)d0 == selTeamNumber) {
        d0 = (int16_t)0xFFFF;
    }

    lastNationalityCall = (uint16_t)d0;
    return (uint16_t)d0;
}

/* ================================================================
 * Chunk 174: Season Review functions (SWOS2.s lines 35525-35785)
 *
 * 4 functions:
 *   SeasonReview_AdjustWeightsForCompetition (static helper)
 *   SeasonReview_ComputeCategoryForCompetition (static helper)
 *   SeasonFinished_ComputeSeasonReview
 *   ShowChairmanOpinionAboutTheSeason
 * ================================================================ */

extern uint8_t competitionFileBuffer[];
extern uint8_t competitionContext[];
extern uint8_t tertiaryCompetitionContext[];
extern uint8_t euroCupsCompetitionContext[];

extern uint32_t currentCompetitionPtr;
extern uint32_t secondaryCompetitionPtr;
extern uint32_t tertiaryCompetitionPtr;
extern uint32_t quaternaryCompetitionPtr;

extern uint16_t wonWorldCup;
extern uint16_t wonContinentalCup;
extern uint16_t findTeamInDIY_SuppressSideEffectsFlag;

/* ---- SeasonReview_AdjustWeightsForCompetition (SWOS2.s line 35686) ----
 * Classifies a competition by comparing its pointer against known addresses,
 * then multiplies the category by a weight factor and accumulates the weight.
 * Only called from SeasonFinished_ComputeSeasonReview (static). */
static void adjustWeightsForCompetition(uint8_t *compPtr, uint16_t *weightAccum,
                                         uint16_t *weightedCategory)
{
    /* World Cup: weight 8 */
    if (compPtr == worldCup2Copy) {
        *weightedCategory *= 8;
        *weightAccum += 8;
        return;
    }
    /* Friendly / Invitation Tournament / Tour: weight 1, d7 NOT multiplied */
    if (compPtr == friendlyCopy || compPtr == invitationTournamentCopy ||
        compPtr == tourCopy) {
        *weightAccum += 1;
        return;
    }
    /* Continental cups: weight 6 */
    if (compPtr == europeanChampionships2Copy || compPtr == northAmericanCupCopy ||
        compPtr == copaAmericaCopy || compPtr == africanNationsCupCopy ||
        compPtr == asianCupCopy || compPtr == oceaniaCupCopy) {
        *weightedCategory *= 6;
        *weightAccum += 6;
        return;
    }
    /* World Cup qualifiers (range check): weight 4 */
    if ((uint32_t)compPtr >= (uint32_t)worldCupQualifyingGroup1Copy &&
        (uint32_t)compPtr <= (uint32_t)oceaniaWorldCupQualificationCopy) {
        *weightedCategory *= 4;
        *weightAccum += 4;
        return;
    }
    /* Default: domestic competition, weight 3 */
    *weightedCategory *= 3;
    *weightAccum += 3;
}

/* ---- SeasonReview_ComputeCategoryForCompetition (SWOS2.s line 35733) ----
 * Computes season review category (0=best, 4=worst) for a competition buffer.
 * Returns -1 if team not found. Only called from SeasonFinished_ComputeSeasonReview. */
static int16_t computeCategoryForCompetition(uint8_t *comp)
{
    findTeamInDIY_SuppressSideEffectsFlag = 1;
    int16_t position = (int16_t)_FindTeamInDIY(selTeamNumber, comp);
    findTeamInDIY_SuppressSideEffectsFlag = 0;

    if (position < 0)
        return -1;

    uint16_t rank = _GetMoreExpensiveTeamsCount(selTeamNumber, comp);
    uint16_t numTeams = *(uint16_t *)(comp + 50);

    /* ratio = (rank - position) * 100 / numTeams (signed) */
    int32_t product = (int32_t)(int16_t)((int16_t)rank - position) * 100;
    int16_t ratio = (int16_t)(product / (int16_t)numTeams);

    int16_t category = 0;
    if (position != 0) {
        if (ratio < 30) {
            category = 1;
            if (ratio < 10) {
                category = 2;
                if (ratio <= -10) {
                    category = 3;
                    if (ratio <= -30)
                        category = 4;
                }
            }
        }
    }

    /* Position percentile (unsigned): position * 100 / numTeams */
    uint16_t percentile = (uint16_t)((uint16_t)position * 100) / numTeams;

    /* Top 25%: upgrade category by 1 (guarded: not if already 0) */
    if (percentile <= 25 && category != 0)
        category--;

    /* Bottom 75%: downgrade category by 1 (guarded: not if already 4) */
    if (percentile >= 75 && category != 4)
        category++;

    return category;
}

/* ---- SeasonFinished_ComputeSeasonReview (SWOS2.s line 35638) ----
 * Computes weighted average season review from primary and secondary
 * competitions, with bonuses for World Cup / Continental Cup wins.
 * Stores result in seasonReview global. */
__attribute__((used, externally_visible))
void _SeasonFinished_ComputeSeasonReview(void)
{
    uint16_t d5 = 0;  /* weight accumulator */
    uint16_t d6 = 0;  /* weighted score accumulator */

    /* Primary competition */
    int16_t cat = computeCategoryForCompetition(competitionFileBuffer);
    if (cat >= 0) {
        uint16_t ucat = (uint16_t)cat;
        adjustWeightsForCompetition((uint8_t *)currentCompetitionPtr, &d5, &ucat);
        d6 += ucat;
    }

    /* Secondary competition (if exists) */
    if (secondaryCompetitionPtr != 0) {
        cat = computeCategoryForCompetition(competitionContext);
        if (cat >= 0) {
            uint16_t ucat = (uint16_t)cat;
            adjustWeightsForCompetition((uint8_t *)secondaryCompetitionPtr, &d5, &ucat);
            d6 += ucat;
        }
    }

    /* Weighted average with rounding: (d6 + d5/2) / d5 */
    d6 += (d5 >> 1);
    /* ext.l d6; divu.w d5,d6 — sign-extend d6 word to long, unsigned divide */
    uint32_t d6_32 = (uint32_t)(int32_t)(int16_t)d6;
    d6 = (uint16_t)(d6_32 / (uint32_t)d5);

    /* Bonus: World Cup win → best rating; Continental Cup win → -2 */
    if (wonWorldCup != 0) {
        d6 = 0;
    } else if (wonContinentalCup != 0) {
        d6 -= 2;
    }

    /* Clamp to >= 0 */
    if ((int16_t)d6 < 0)
        d6 = 0;

    seasonReview = d6;
}

/* ---- ShowChairmanOpinionAboutTheSeason (SWOS2.s line 35525) ----
 * For national teams, delegates to ShowSeasonOpinion.
 * For club teams, computes category with inline logic (same as ComputeCategory
 * but WITHOUT suppressing side effects), checks secondary/tertiary/quaternary
 * competitions for top-2 finishes, then maps category to a chairman message. */
#define CHAIRMAN_MSG_EXCELLENT   13
#define CHAIRMAN_MSG_GOOD        14
#define CHAIRMAN_MSG_AVERAGE     15
#define CHAIRMAN_MSG_POOR        16
#define CHAIRMAN_MSG_TERRIBLE    17

__attribute__((used, externally_visible))
void _ShowChairmanOpinionAboutTheSeason(void)
{
    /* National teams → use season review score */
    if (isNationalSelector != 0) {
        _ShowSeasonOpinion();
        return;
    }

    /* --- Compute category for primary competition (inline) --- */
    uint8_t *comp = competitionFileBuffer;
    int16_t position = (int16_t)_FindTeamInDIY(selTeamNumber, comp);
    uint16_t rank = _GetMoreExpensiveTeamsCount(selTeamNumber, comp);
    uint16_t numTeams = *(uint16_t *)(comp + 50);

    int32_t product = (int32_t)(int16_t)((int16_t)rank - position) * 100;
    int16_t ratio = (int16_t)(product / (int16_t)numTeams);

    int16_t d7 = 0;
    if (position != 0) {
        if (ratio < 30) {
            d7 = 1;
            if (ratio < 10) {
                d7 = 2;
                if (ratio <= -10) {
                    d7 = 3;
                    if (ratio <= -30)
                        d7 = 4;
                }
            }
        }
    }

    /* Percentile adjustments (NO guards for d7==0 or d7==4) */
    uint16_t percentile = (uint16_t)((uint16_t)position * 100) / numTeams;
    if (percentile <= 25)
        d7--;
    if (percentile >= 75)
        d7++;

    /* Secondary competition: top-2 finish → -2 */
    if (secondaryCompetitionPtr != 0) {
        int16_t pos2 = (int16_t)_FindTeamInDIY(selTeamNumber, competitionContext);
        if ((uint16_t)pos2 < 2)
            d7 -= 2;
    }

    /* Tertiary competition: top-2 finish → -2 */
    if (tertiaryCompetitionPtr != 0) {
        int16_t pos3 = (int16_t)_FindTeamInDIY(selTeamNumber, tertiaryCompetitionContext);
        if ((uint16_t)pos3 < 2)
            d7 -= 2;
    }

    /* Quaternary competition (Euro cups): top-2 finish → -3 */
    if (quaternaryCompetitionPtr != 0) {
        int16_t pos4 = (int16_t)_FindTeamInDIY(selTeamNumber, euroCupsCompetitionContext);
        if ((uint16_t)pos4 < 2)
            d7 -= 3;
    }

    /* Clamp d7 to >= 0 */
    if (d7 < 0)
        d7 = 0;

    /* Map category to chairman message */
    uint16_t msg;
    if (d7 >= 4)
        msg = CHAIRMAN_MSG_TERRIBLE;
    else if (d7 == 3)
        msg = CHAIRMAN_MSG_POOR;
    else if (d7 == 2)
        msg = CHAIRMAN_MSG_AVERAGE;
    else if (d7 == 1)
        msg = CHAIRMAN_MSG_GOOD;
    else
        msg = CHAIRMAN_MSG_EXCELLENT;

    currentMessage = msg;
    _ShowChairmanMessage();
}
