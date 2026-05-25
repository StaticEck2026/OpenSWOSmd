/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* career_season.c — Career season management, qualification, team list building.
 *
 * Chunks 67-68: 2 exported functions ported from SWOS2.s.
 */

#include "career_season.h"
#include "career_eurocups.h"
#include "career_init.h"
#include "career_status.h"
#include "career_teams.h"
#include "diy_competition.h"
#include "game_types.h"
#include "job_offers.h"
#include "panic.h"
#include "random.h"
#include "statistics.h"
#include "transfer_market.h"

/* ---- ASM externs ---- */
extern uint16_t selTeamNumber;
extern uint16_t isNationalSelector;
extern uint16_t teamNoOfferAccepted;
extern uint16_t seasonReview;
extern volatile uint16_t currentMessage;

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
 *   _SeasonFinished_ComputeSeasonReview
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
 * Only called from _SeasonFinished_ComputeSeasonReview (static). */
static void adjustWeightsForCompetition(uint8_t *compPtr, uint16_t *weightAccum,
                                         uint16_t *weightedCategory)
{
    /* World Cup: weight 8 */
    if (compPtr == worldCup2Copy) {
        *weightedCategory *= 8;
        *weightAccum += 8;
        return;
    }
    /* _Friendly / Invitation Tournament / Tour: weight 1, d7 NOT multiplied */
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
 * Returns -1 if team not found. Only called from _SeasonFinished_ComputeSeasonReview. */
static int16_t computeCategoryForCompetition(uint8_t *comp)
{
    findTeamInDIY_SuppressSideEffectsFlag = 1;
    int16_t position = (int16_t)_FindTeamInDIY(selTeamNumber, (DIYCompetition *)comp);
    findTeamInDIY_SuppressSideEffectsFlag = 0;

    if (position < 0)
        return -1;

    uint16_t rank = _GetMoreExpensiveTeamsCount(selTeamNumber, (DIYCompetition *)comp);
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

/* ---- _SeasonFinished_ComputeSeasonReview (SWOS2.s line 35638) ----
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
    int16_t position = (int16_t)_FindTeamInDIY(selTeamNumber, (DIYCompetition *)comp);
    uint16_t rank = _GetMoreExpensiveTeamsCount(selTeamNumber, (DIYCompetition *)comp);
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
        int16_t pos2 = (int16_t)_FindTeamInDIY(selTeamNumber, (DIYCompetition *)competitionContext);
        if ((uint16_t)pos2 < 2)
            d7 -= 2;
    }

    /* Tertiary competition: top-2 finish → -2 */
    if (tertiaryCompetitionPtr != 0) {
        int16_t pos3 = (int16_t)_FindTeamInDIY(selTeamNumber, (DIYCompetition *)tertiaryCompetitionContext);
        if ((uint16_t)pos3 < 2)
            d7 -= 2;
    }

    /* Quaternary competition (Euro cups): top-2 finish → -3 */
    if (quaternaryCompetitionPtr != 0) {
        int16_t pos4 = (int16_t)_FindTeamInDIY(selTeamNumber, (DIYCompetition *)euroCupsCompetitionContext);
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

/* ---- Chunk 181: BeginContractNegotiationSequence ---- */

extern uint16_t contractNegotiationActive;
extern uint16_t timeToNegotiate;
extern uint16_t lastOfferPrimaryPlayerOrdinal;
extern uint16_t offerNegotiationAttemptCount;
extern uint16_t financeTotalsNeedResetFlag;

extern void _Career_MaybeGenerateJobOffer(void);

/* BeginContractNegotiationSequence (SWOS2.s line 28035)
 * Called after a career match to begin contract negotiation phase.
 * Sets up negotiation globals, commits finances, processes offer expiry,
 * and potentially generates a new job offer.
 * a0=comp (diyCompetitionStart), a1=team ptr passed through to UpdateCareerFinancesAfterMatch. */
__attribute__((used, externally_visible))
void _BeginContractNegotiationSequence(DIYCompetitionEx *comp, TeamFileEx *team)
{
    contractNegotiationActive = 1;
    timeToNegotiate = 2;
    lastOfferPrimaryPlayerOrdinal = 0xFFFF;
    offerNegotiationAttemptCount = 0;
    _Career_CommitNewBalanceAndResetFinanceTotals();
    financeTotalsNeedResetFlag = 1;
    _UpdateCareerFinancesAfterMatch(comp, team);
    _ProcessOfferExpiryCountdown();
    _JobOffers_MaybeExpireSomeNewOffers();
    _Career_MaybeGenerateJobOffer();
}

/* ---- Chunk 182: Competition Season Processing ---- */

extern uint32_t secondaryCompetitionPtr;
extern uint32_t tertiaryCompetitionPtr;

extern uint32_t competitionsTable[];
extern uint16_t cachedTertiaryQualifiedTeamId;
extern uint16_t panicReasonCode;
extern uint8_t currentCompetitionId;

/* Forward declaration for static function defined in Chunk 186 below */
static void processPrimaryCompetitionForNextSeason(uint8_t *competition);

/* Career_ProcessSecondaryCompetitionForNextSeason (SWOS2.s line 31973)
 * Process secondary competition for next season: if active, edit interactively;
 * otherwise auto-advance as cup and cache qualified grid. */
__attribute__((used, externally_visible))
void _Career_ProcessSecondaryCompetitionForNextSeason(uint8_t *competition)
{
    if (competition[1] != 1) {
        panicReasonCode = 0x3003;
        _PanicHaltLoop();
        return;
    }
    if ((uint32_t)competition == secondaryCompetitionPtr) {
        _RestoreSelectedTeams();
        _Career_CopyCompetitionContextToOrFromActive();
        _ShowDIYCupMenuWithParams(1, 1, 0);
        _DIY_CacheSecondaryQualifiedTeamsGrid();
        _SaveSelectedTeams();
    } else {
        _Career_AutoAdvanceCupCompetitionFromRecord(competition);
        _DIY_CacheSecondaryQualifiedTeamsGrid();
    }
}

/* Career_ProcessTertiaryCompetitionForNextSeason (SWOS2.s line 32000)
 * Same pattern for tertiary competition. */
__attribute__((used, externally_visible))
void _Career_ProcessTertiaryCompetitionForNextSeason(uint8_t *competition)
{
    if (competition[1] != 1) {
        panicReasonCode = 0x3003;
        _PanicHaltLoop();
        return;
    }
    if ((uint32_t)competition == tertiaryCompetitionPtr) {
        _RestoreSelectedTeams();
        _Career_CopyTertiaryCompetitionContextToOrFromActive();
        _ShowDIYCupMenuWithParams(1, 1, 0);
        _DIY_CacheTertiaryQualifiedTeamGrid();
        _SaveSelectedTeams();
    } else {
        _Career_AutoAdvanceCupCompetitionFromRecord(competition);
        _DIY_CacheTertiaryQualifiedTeamGrid();
    }
}

/* Career_ProcessCompetitionForNextSeason (SWOS2.s line 31908)
 * Main dispatcher: processes primary, secondary, tertiary competitions.
 * d0 = competition ID. */
__attribute__((used, externally_visible))
void _Career_ProcessCompetitionForNextSeason(uint16_t compId)
{
    currentCompetitionId = (uint8_t)compId;
    cachedTertiaryQualifiedTeamId = 0xFFFF;

    uint32_t *tableEntry = (uint32_t *)competitionsTable[compId & 0xFF];

    /* Entry layout: [0]=primary ptr, [1]=skip, [2]=secondary ptr, [3]=tertiary ptr */
    uint8_t *primary = (uint8_t *)tableEntry[0];
    processPrimaryCompetitionForNextSeason(primary);

    uint8_t *secondary = (uint8_t *)tableEntry[2];
    if ((uint32_t)secondary != 0xFFFFFFFF) {
        _Career_ProcessSecondaryCompetitionForNextSeason(secondary);

        uint8_t *tertiary = (uint8_t *)tableEntry[3];
        if ((uint32_t)tertiary != 0xFFFFFFFF) {
            _Career_ProcessTertiaryCompetitionForNextSeason(tertiary);
        }
    }

    _DIY_ValidateLeagueScratchBuffer();
}

/* ================================================================
 * Chunk 185: Career Cup Pick List System (SWOS2.s lines 32019-33004)
 *
 * 13 functions: pickRandomNationalTeamId, fillWorldCupQualifyingGroupTeams,
 * setupWorldCupQualifyingGroups, setCupPickListCommon (helper for 5 regions),
 * setCupPickList_Europe, setupFriendlyCupTeams, setupInvitationTournamentTeams,
 * setupTourCupTeams, _Career_SetCupPickList_ForSelectedRegion (exported).
 * ================================================================ */

/* --- Additional externs for Chunk 185 --- */
extern uint8_t someTeamFlags;
extern uint16_t qualifyingStage1PendingFlag0;
extern uint16_t qualifyingStage1PendingFlag1;
extern uint16_t worldCupHostTeam;
extern uint16_t worldCupQualifyingTeam;
extern uint16_t euroChampionshipQualTeam;
extern uint16_t natTeamsRandLimit2;
extern uint8_t nationalTeams[];
extern uint32_t careerCupPickList[];
extern TeamFileEx careerTeam;

/* Competition qualification copies */
extern uint8_t worldCupQualifyingGroup8Copy[];
extern uint8_t northAmericaWorldCupQualificationCopy[];
extern uint8_t southAmericaWorldCupQualificationCopy[];
extern uint8_t africaWorldCupQualificationCopy[];
extern uint8_t asiaWorldCupQualificationCopy[];

/* Cup pick list data arrays (stay in asm, contain relocatable pointers) */
extern uint32_t cupPickOceaniaWorldCupFriendly[];
extern uint32_t cupPickOceaniaWorldCupInvite[];
extern uint32_t cupPickOceaniaOceaniaCupFinalist[];
extern uint32_t cupPickOceaniaOceaniaCupNonFinalist[];
extern uint32_t cupPickOceaniaOceaniaCupFriendly[];
extern uint32_t cupPickOceaniaInviteFriendly[];

extern uint32_t cupPickNAmericaWorldCupFriendly[];
extern uint32_t cupPickNAmericaWorldCupInvite[];
extern uint32_t cupPickNAmericaNaCupFinalist[];
extern uint32_t cupPickNAmericaNaCupNonFinalist[];
extern uint32_t cupPickNAmericaNaCupFriendly[];
extern uint32_t cupPickNAmericaInviteFriendly[];

extern uint32_t cupPickAsiaWorldCupFriendly[];
extern uint32_t cupPickAsiaWorldCupInvite[];
extern uint32_t cupPickAsiaAsianCupFinalist[];
extern uint32_t cupPickAsiaAsianCupNonFinalist[];
extern uint32_t cupPickAsiaAsianCupFriendly[];
extern uint32_t cupPickAsiaInviteFriendly[];

extern uint32_t cupPickAfricaWorldCupFriendly[];
extern uint32_t cupPickAfricaWorldCupInvite[];
extern uint32_t cupPickAfricaAfricanCupFinalist[];
extern uint32_t cupPickAfricaAfricanCupNonFinalist[];
extern uint32_t cupPickAfricaAfricanCupFriendly[];
extern uint32_t cupPickAfricaInviteFriendly[];

extern uint32_t cupPickSAmericaWorldCupFriendly[];
extern uint32_t cupPickSAmericaWorldCupInvite[];
extern uint32_t cupPickSAmericaCopaAmericaFinalist[];
extern uint32_t cupPickSAmericaCopaAmericaNonFinalist[];
extern uint32_t cupPickSAmericaCopaAmericaFriendly[];
extern uint32_t cupPickSAmericaInviteFriendly[];

extern uint32_t cupPickListEuropeWorldCupFriendly[];
extern uint32_t cupPickLisEuropeWorldCupInviteFriendly[];
extern uint32_t cupPickLisEuropeEuroChampsFriendly[];
extern uint32_t cupPickListEuropeEuroChampsInviteFriendly[];
extern uint32_t cupPickListEuropeInviteTournamentFriendly[];
extern uint32_t cupPickEuropeNoCupFriendly[];
extern uint32_t cupPickEuropeNoCupFriendlyAlt[];
extern uint32_t cupPickListEuropeInviteTournamentFriendlyAlt[];

extern uint32_t ecQualifyingGroupsTable[];
extern uint32_t worldCupQualifyingGroupsTable[];
extern uint32_t tourStringTable[];

/* Forward declarations */
static void setupWorldCupQualifyingGroups(uint16_t **teamListPtr);

/* ---- pickRandomNationalTeamId (SWOS2.s line 32962) ----
 * Picks a random national team, optionally filtered by region and price.
 * Retries up to 100 times with price filtering, then continues without.
 * Ensures no duplicates against the current list. */
static uint16_t pickRandomNationalTeamId(uint16_t minPrice, uint16_t maxPrice,
                                          uint8_t regionFilter,
                                          uint16_t *currentList, uint16_t countSoFar)
{
    int16_t retries = 99;
    for (;;) {
        uint16_t r = _Rand();
        uint32_t idx = (uint32_t)r * natTeamsRandLimit2;
        idx >>= 8;
        uint8_t *entry = nationalTeams + (uint16_t)(idx * 6);
        uint16_t teamId = *(uint16_t *)entry;

        if (regionFilter != 0) {
            if ((uint8_t)(teamId >> 8) != regionFilter)
                continue;
        }
        if (teamId == selTeamNumber)
            continue;

        if (retries >= 0) {
            uint16_t price = *(uint16_t *)(entry + 4);
            if (price < minPrice || price > maxPrice) {
                retries--;
                continue;
            }
        }

        if (countSoFar == 0)
            return teamId;

        int16_t dup = 0;
        uint16_t *p = currentList;
        for (uint16_t i = countSoFar; i > 0; i--) {
            if (*p++ == teamId) { dup = 1; break; }
        }
        if (!dup)
            return teamId;

        if (retries >= 0)
            retries--;
    }
}

/* ---- fillWorldCupQualifyingGroupTeams (SWOS2.s line 32761) ----
 * Fills a world cup qualifying group with teams from the sorted list.
 * Places the player's team if their region matches. */
static void fillWorldCupQualifyingGroupTeams(uint8_t regionCode, uint8_t *group,
                                              uint16_t **teamListPtr)
{
    _Career_SortTeamFilePtrsByAvgPlayerPrice(regionCode);
    uint8_t *origGroup = group;
    uint8_t compType = group[1];
    int16_t teamCount;

    if (compType == 0) {
        teamCount = (int8_t)group[0xD];
    } else if (compType == 2) {
        teamCount = (int8_t)group[0xF];
    } else {
        panicReasonCode = 0x7623;
        _PanicHaltLoop();
        return;
    }

    uint8_t hdrSize = group[7];
    uint16_t *teamSlots = (uint16_t *)(group + 7 + (hdrSize & 0xFF));

    uint16_t firstTeam;
    if (selTeamNumber != worldCupHostTeam &&
        selTeamNumber != worldCupQualifyingTeam &&
        (uint8_t)(selTeamNumber >> 8) == regionCode) {
        careerCupPickList[0] = (uint32_t)origGroup;
        firstTeam = selTeamNumber;
    } else {
        do {
            firstTeam = *(*teamListPtr)++;
        } while (firstTeam == selTeamNumber ||
                 firstTeam == worldCupHostTeam ||
                 firstTeam == worldCupQualifyingTeam);
    }

    *teamSlots++ = firstTeam;
    for (int16_t i = teamCount - 1; i > 0; i--) {
        uint16_t teamId;
        do {
            teamId = *(*teamListPtr)++;
        } while (teamId == selTeamNumber ||
                 teamId == worldCupHostTeam ||
                 teamId == worldCupQualifyingTeam);
        *teamSlots++ = teamId;
    }
}

/* ---- setupWorldCupQualifyingGroups (SWOS2.s line 32649) ----
 * Sets up all world cup qualifying groups: 8 European groups (column-major fill),
 * then 5 continental groups. Only runs during continental cup year (bit 6). */
static void setupWorldCupQualifyingGroups(uint16_t **teamListPtr)
{
    if (!(someTeamFlags & 0x40))
        return;

    /* Calculate group 8 team count based on European host/qualifying */
    int16_t d5 = 6;
    int skipQualCheck = 0;
    if ((uint8_t)(worldCupHostTeam >> 8) == 0x50) {
        d5--;
        if (worldCupHostTeam == worldCupQualifyingTeam)
            skipQualCheck = 1;
    }
    if (!skipQualCheck && (uint8_t)(worldCupQualifyingTeam >> 8) == 0x50)
        d5--;
    if (d5 & 1)
        d5--;
    worldCupQualifyingGroup8Copy[0xD] = (uint8_t)d5;

    _Career_SortTeamFilePtrsByAvgPlayerPrice(0x50);
    uint16_t randOff = _Rand2() & 0x1C;
    uint8_t *selectedGroup = (uint8_t *)worldCupQualifyingGroupsTable[randOff >> 2];
    int16_t slotsRemaining = 0x30 - 6 + d5;
    d5 <<= 1;
    int16_t colOffset = 0;

    for (;;) {
        for (int16_t g = 0; g < 8; g++) {
            uint8_t *group = (uint8_t *)worldCupQualifyingGroupsTable[g];
            uint8_t hdrSize = group[7];
            uint16_t *slot = (uint16_t *)(group + 7 + (hdrSize & 0xFF) + colOffset);

            uint16_t teamId;
            if (colOffset == 0 && group == selectedGroup &&
                selTeamNumber != worldCupHostTeam &&
                selTeamNumber != worldCupQualifyingTeam &&
                (uint8_t)(selTeamNumber >> 8) == 0x50) {
                careerCupPickList[0] = (uint32_t)group;
                teamId = selTeamNumber;
            } else {
                if (group == worldCupQualifyingGroup8Copy && colOffset >= d5)
                    continue;
                do {
                    teamId = *(*teamListPtr)++;
                } while (teamId == selTeamNumber);
            }

            while (teamId == worldCupHostTeam || teamId == worldCupQualifyingTeam) {
                do {
                    teamId = *(*teamListPtr)++;
                } while (teamId == selTeamNumber);
            }

            *slot = teamId;
            if (--slotsRemaining == 0)
                goto wc_groups_done;
        }
        colOffset += 2;
    }

wc_groups_done:
    fillWorldCupQualifyingGroupTeams(0x53, northAmericaWorldCupQualificationCopy, teamListPtr);
    fillWorldCupQualifyingGroupTeams(0x52, southAmericaWorldCupQualificationCopy, teamListPtr);
    fillWorldCupQualifyingGroupTeams(0x51, africaWorldCupQualificationCopy, teamListPtr);
    fillWorldCupQualifyingGroupTeams(0x54, asiaWorldCupQualificationCopy, teamListPtr);

    /* Oceania: adjust team count based on Oceanian host/qualifying */
    uint8_t oceaniaCount = 4;
    int skipOceaniaQual = 0;
    if ((uint8_t)(worldCupHostTeam >> 8) == 0x55) {
        oceaniaCount--;
        if (worldCupHostTeam == worldCupQualifyingTeam)
            skipOceaniaQual = 1;
    }
    if (!skipOceaniaQual && (uint8_t)(worldCupQualifyingTeam >> 8) == 0x55)
        oceaniaCount--;
    if (oceaniaCount & 1)
        oceaniaCount--;
    oceaniaWorldCupQualificationCopy[0xD] = oceaniaCount;
    fillWorldCupQualifyingGroupTeams(0x55, oceaniaWorldCupQualificationCopy, teamListPtr);
}

/* ---- setCupPickListCommon (helper for 5 simple regional functions) ----
 * Implements the identical cup pick list selection logic used by Oceania,
 * NorthAmerica, Asia, Africa, and SouthAmerica. */
static void setCupPickListCommon(uint32_t *worldCupFriendly, uint32_t *worldCupInvite,
                                  uint32_t *cupFinalist, uint32_t *cupNonFinalist,
                                  uint32_t *cupFriendly, uint32_t *inviteFriendly,
                                  uint16_t **teamListPtr)
{
    uint32_t *src = worldCupFriendly;
    if (someTeamFlags & 0x80) {
        if (qualifyingStage1PendingFlag0 == 0)
            src = worldCupInvite;
    } else if (someTeamFlags & 0x40) {
        src = cupFinalist;
        if (selTeamNumber != worldCupHostTeam && selTeamNumber != worldCupQualifyingTeam)
            src = cupNonFinalist;
    } else {
        src = cupFriendly;
        if (!(someTeamFlags & 0x02)) {
            if (someTeamFlags != 0) {
                panicReasonCode = 0x3775;
                _PanicHaltLoop();
                return;
            }
            src = inviteFriendly;
        }
    }

    careerCupPickList[0] = src[0];
    careerCupPickList[1] = src[1];
    careerCupPickList[2] = src[2];
    careerCupPickList[3] = src[3];
    careerCupPickList[4] = src[4];

    _Career_RandomizeInvitationTournamentCup();
    setupWorldCupQualifyingGroups(teamListPtr);
}

/* ---- setCupPickList_Europe (SWOS2.s line 32469) ----
 * Europe has a different flag structure and includes EC group filling. */
static void setCupPickList_Europe(uint16_t **teamListPtr)
{
    uint32_t *src;
    if (someTeamFlags & 0x80) {
        src = cupPickListEuropeWorldCupFriendly;
        if (qualifyingStage1PendingFlag0 == 0)
            src = cupPickLisEuropeWorldCupInviteFriendly;
    } else if (someTeamFlags & 0x02) {
        src = cupPickLisEuropeEuroChampsFriendly;
        if (qualifyingStage1PendingFlag1 == 0)
            src = cupPickListEuropeEuroChampsInviteFriendly;
    } else if (someTeamFlags & 0x40) {
        src = cupPickListEuropeInviteTournamentFriendlyAlt;
        if (selTeamNumber != worldCupHostTeam && selTeamNumber != worldCupQualifyingTeam)
            src = cupPickEuropeNoCupFriendlyAlt;
    } else if (someTeamFlags & 0x01) {
        src = cupPickListEuropeInviteTournamentFriendly;
        if (selTeamNumber != euroChampionshipQualTeam)
            src = cupPickEuropeNoCupFriendly;
    } else {
        panicReasonCode = 0x3775;
        _PanicHaltLoop();
        return;
    }

    careerCupPickList[0] = src[0];
    careerCupPickList[1] = src[1];
    careerCupPickList[2] = src[2];
    careerCupPickList[3] = src[3];
    careerCupPickList[4] = src[4];

    _Career_RandomizeInvitationTournamentCup();
    setupWorldCupQualifyingGroups(teamListPtr);

    /* EC group filling (only during EC qualifying year, bit 0) */
    if (!(someTeamFlags & 0x01))
        return;

    _Career_SortTeamFilePtrsByAvgPlayerPrice(0x50);
    uint16_t randIdx = (uint16_t)((uint32_t)_Rand2() * 7 >> 8);
    uint8_t *selectedGroup = (uint8_t *)ecQualifyingGroupsTable[randIdx];
    int16_t slotsRemaining = 0x2E; /* 46 */
    int16_t colOffset = 0;

    for (;;) {
        for (int16_t g = 0; g < 8; g++) {
            uint8_t *group = (uint8_t *)ecQualifyingGroupsTable[g];
            uint8_t hdrSize = group[7];
            uint16_t *slot = (uint16_t *)(group + 7 + (hdrSize & 0xFF) + colOffset);

            uint16_t teamId;
            int readFromList = 1;
            if (colOffset == 0 && group == selectedGroup) {
                if (selTeamNumber != euroChampionshipQualTeam) {
                    careerCupPickList[0] = (uint32_t)group;
                    teamId = selTeamNumber;
                    readFromList = 0;
                }
            }

            if (readFromList) {
                if (group == ecQualifyingGroup8Copy && (uint16_t)colOffset >= 8)
                    continue;
                do {
                    teamId = *(*teamListPtr)++;
                } while (teamId == selTeamNumber);
            }

            while (teamId == euroChampionshipQualTeam) {
                do {
                    teamId = *(*teamListPtr)++;
                } while (teamId == selTeamNumber);
            }

            *slot = teamId;
            if (--slotsRemaining == 0)
                return;
        }
        colOffset += 2;
    }
}

/* ---- setupFriendlyCupTeams (SWOS2.s line 32860) ----
 * Fills the friendly competition team list with teams of similar strength. */
static void setupFriendlyCupTeams(void)
{
    uint16_t avgPrice = _GetAveragePlayerPrice(&careerTeam);
    uint16_t minPrice = (uint16_t)((uint32_t)avgPrice * 75 / 100);
    uint16_t maxPrice = (uint16_t)((uint32_t)avgPrice * 125 / 100);

    uint8_t *comp = friendlyCopy;
    int16_t teamCount = (int8_t)comp[0xD];
    uint8_t hdrSize = comp[7];
    uint16_t *teamList = (uint16_t *)(comp + 7 + (hdrSize & 0xFF));
    uint16_t *listBase = teamList;

    *teamList++ = selTeamNumber;
    uint16_t countSoFar = 1;

    for (int16_t i = teamCount - 2; i >= 0; i--) {
        uint16_t teamId = pickRandomNationalTeamId(minPrice, maxPrice, 0, listBase, countSoFar);
        *teamList++ = teamId;
        countSoFar++;
    }
}

/* ---- setupInvitationTournamentTeams (SWOS2.s line 32889) ----
 * Fills the invitation tournament team list (same pattern as friendly). */
static void setupInvitationTournamentTeams(void)
{
    uint16_t avgPrice = _GetAveragePlayerPrice(&careerTeam);
    uint16_t minPrice = (uint16_t)((uint32_t)avgPrice * 75 / 100);
    uint16_t maxPrice = (uint16_t)((uint32_t)avgPrice * 125 / 100);

    uint8_t *comp = invitationTournamentCopy;
    int16_t teamCount = (int8_t)comp[0xD];
    uint8_t hdrSize = comp[7];
    uint16_t *teamList = (uint16_t *)(comp + 7 + (hdrSize & 0xFF));
    uint16_t *listBase = teamList;

    *teamList++ = selTeamNumber;
    uint16_t countSoFar = 1;

    for (int16_t i = teamCount - 2; i >= 0; i--) {
        uint16_t teamId = pickRandomNationalTeamId(minPrice, maxPrice, 0, listBase, countSoFar);
        *teamList++ = teamId;
        countSoFar++;
    }
}

/* ---- setupTourCupTeams (SWOS2.s line 32918) ----
 * Sets up the tour cup with a random region's teams. */
static void setupTourCupTeams(void)
{
    uint16_t randRegion = (uint16_t)((uint32_t)_Rand() * 6 >> 8);
    uint8_t regionCode = (uint8_t)(randRegion + 0x50);

    uint8_t *tour = tourCopy;
    int8_t nameOff = (int8_t)tour[5];
    *(uint32_t *)(tour + 5 + nameOff) = tourStringTable[randRegion];

    int16_t teamCount = (int8_t)tour[0xD];
    uint8_t hdrSize = tour[7];
    uint16_t *teamList = (uint16_t *)(tour + 7 + (hdrSize & 0xFF));
    uint16_t *listBase = teamList;

    *teamList++ = selTeamNumber;
    uint16_t countSoFar = 1;

    for (int16_t i = teamCount - 2; i >= 0; i--) {
        uint16_t teamId = pickRandomNationalTeamId(0, 0x31, regionCode, listBase, countSoFar);
        *teamList++ = teamId;
        countSoFar++;
    }
}

/* ---- Career_SetCupPickList_ForSelectedRegion (SWOS2.s line 32820) ----
 * Dispatches to the regional cup pick list function based on team region,
 * then sets up friendly, invitation tournament, and tour cup teams. */
__attribute__((used, externally_visible))
void _Career_SetCupPickList_ForSelectedRegion(uint16_t *teamList)
{
    uint8_t region = (uint8_t)(selTeamNumber >> 8);
    uint16_t **tlp = &teamList;

    switch (region) {
    case 0x50: /* Europe */
        setCupPickList_Europe(tlp);
        break;
    case 0x52: /* South America */
        setCupPickListCommon(cupPickSAmericaWorldCupFriendly, cupPickSAmericaWorldCupInvite,
            cupPickSAmericaCopaAmericaFinalist, cupPickSAmericaCopaAmericaNonFinalist,
            cupPickSAmericaCopaAmericaFriendly, cupPickSAmericaInviteFriendly, tlp);
        break;
    case 0x51: /* Africa */
        setCupPickListCommon(cupPickAfricaWorldCupFriendly, cupPickAfricaWorldCupInvite,
            cupPickAfricaAfricanCupFinalist, cupPickAfricaAfricanCupNonFinalist,
            cupPickAfricaAfricanCupFriendly, cupPickAfricaInviteFriendly, tlp);
        break;
    case 0x54: /* Asia */
        setCupPickListCommon(cupPickAsiaWorldCupFriendly, cupPickAsiaWorldCupInvite,
            cupPickAsiaAsianCupFinalist, cupPickAsiaAsianCupNonFinalist,
            cupPickAsiaAsianCupFriendly, cupPickAsiaInviteFriendly, tlp);
        break;
    case 0x53: /* North America */
        setCupPickListCommon(cupPickNAmericaWorldCupFriendly, cupPickNAmericaWorldCupInvite,
            cupPickNAmericaNaCupFinalist, cupPickNAmericaNaCupNonFinalist,
            cupPickNAmericaNaCupFriendly, cupPickNAmericaInviteFriendly, tlp);
        break;
    case 0x55: /* Oceania */
        setCupPickListCommon(cupPickOceaniaWorldCupFriendly, cupPickOceaniaWorldCupInvite,
            cupPickOceaniaOceaniaCupFinalist, cupPickOceaniaOceaniaCupNonFinalist,
            cupPickOceaniaOceaniaCupFriendly, cupPickOceaniaInviteFriendly, tlp);
        break;
    default:
        panicReasonCode = 0x4434;
        _PanicHaltLoop();
        return;
    }

    setupFriendlyCupTeams();
    setupInvitationTournamentTeams();
    setupTourCupTeams();
}

/* ====================================================================
 * Chunk 186: Career Season Update Functions
 * ===================================================================*/

/* ---- ASM externs for Chunk 186 ---- */
extern uint16_t playPromotedAnimation;
extern uint16_t playRelegatedAnimation;
extern volatile uint16_t seasonUpdateInProgress;
extern uint16_t careerFileBuffer;
extern uint16_t euroCupQualifiedTeamCount;
extern uint16_t cupWinnersCupQualifiedTeamCount;
extern uint16_t uefaCupQualifiedTeamCount;
extern uint8_t careerCompetitionIdRemapTable[];
extern uint16_t diyQualifiedTeamIndices[];
extern uint16_t currentDivisionIndex;
extern uint16_t youreSacked;
extern uint32_t diyCompetitionPtr;
extern uint8_t diyCompetitionContext[];

/* ---- Career_ProcessDiyLeagueQualificationEntry ----
 * (SWOS2.s line 31903)
 * Processes a single DIY league qualification entry during season transition.
 * index d7 selects which division qualification entry to process.
 * competition a4 is the competition record. */
static void processDiyLeagueQualificationEntry(uint16_t index, uint8_t *competition)
{
    uint8_t *a2 = competition + index * 6;
    uint8_t d0 = a2[0x12];

    if (d0 == 0)
        return;

    /* Check if this is the last division OR if the offset matches the next
     * division's offset + 6. If neither, skip processing. */
    uint16_t nextDiv = index + 1;
    if ((uint8_t)nextDiv != competition[9]) {
        uint8_t d1 = a2[0x18] + 6;
        if (d0 == d1)
            return;
    }

    /* Advance past qualification header */
    a2 = a2 + 0x12 + d0;

    /* Read competition pointer from qualification data */
    uint8_t *qualComp = *(uint8_t **)a2;
    a2 += 4;
    /* Skip 4-byte gap */
    a2 += 4;

    if (!youreSacked && (uint32_t)qualComp == diyCompetitionPtr) {
        /* Player's competition — interactive processing */
        int8_t teamCount = *(int8_t *)(qualComp + 0xA);
        a2 += (int16_t)teamCount * 2;

        _RestoreSelectedTeams();
        _Career_LoadCompetitionSlotStateFromBuffer(diyCompetitionContext);
        _ShowDIYCupMenuWithParams(1, 1, 0);
        _DIY_ApplyPromotionRelegationWeightsToScratch(a2);
        _SaveSelectedTeams();
    } else {
        /* Not player's competition — auto-fill teams from qualification indices */
        uint16_t savedSelTeam = selTeamNumber;
        uint8_t savedCtrl = careerTeam.team.teamControls;

        selTeamNumber = 0xFFFF;
        careerTeam.team.teamControls = 1;

        uint8_t nameOff = qualComp[7];
        if (nameOff == 0) {
            panicReasonCode = 0x3003;
            _PanicHaltLoop();
            return;
        }
        uint8_t *teamData = qualComp + 7 + nameOff;

        int8_t count = *(int8_t *)(qualComp + 0xA);
        int16_t cnt = (int16_t)count - 1;

        for (int16_t i = cnt; i >= 0; i--) {
            uint16_t qIdx = *(uint16_t *)a2;
            a2 += 2;
            *(uint16_t *)teamData = diyQualifiedTeamIndices[qIdx];
            teamData += 2;
        }

        /* Auto-advance this competition via cup competition path */
        _Career_AutoAdvanceCupCompetitionFromRecord(qualComp);
        _DIY_ApplyPromotionRelegationWeightsToScratch(a2);

        careerTeam.team.teamControls = savedCtrl;
        selTeamNumber = savedSelTeam;
    }
}

/* ---- Career_ProcessPrimaryCompetitionForNextSeason ----
 * (SWOS2.s line 31825)
 * Processes the primary competition for the next season.
 * Handles league (type 0) and cup (type 2) competitions differently. */
static void processPrimaryCompetitionForNextSeason(uint8_t *competition)
{
    uint8_t compType = competition[1];

    if (compType == 2) {
        /* Cup competition */
        if ((uint32_t)competition == currentCompetitionPtr) {
            _RestoreSelectedTeams();
            _Career_CopyCompetitionFileBufferToOrFromActive();
            _DIY_EditCompetitionSlot(1, 1, 0);
            _SaveSelectedTeams();
        } else {
            /* Auto-advance via league competition record path
             * (asm calls Career_AutoAdvanceLeagueCompetitionFromRecord even for cups) */
            _Career_AutoAdvanceLeagueCompetitionFromRecord(competition);
        }
        return;
    }

    if (compType != 0) {
        /* Unknown type → panic */
        panicReasonCode = 0x3003;
        _PanicHaltLoop();
        return;
    }

    /* League competition — loop through all divisions */
    uint8_t numDivisions = competition[9];
    uint16_t d7;

    for (d7 = 0; d7 < numDivisions; d7++) {
        if ((uint32_t)competition == currentCompetitionPtr &&
            d7 == currentDivisionIndex) {
            /* Player's current division — interactive */
            _RestoreSelectedTeams();
            _Career_CopyCompetitionFileBufferToOrFromActive();
            _ShowDiyLeaguesTournamentMenu(1, 1, 0);
            if (d7 == 0)
                _DIY_CachePrimaryQualifiedTeamsGrid();
            _SaveSelectedTeams();
        } else {
            /* Other division — auto-advance */
            _Career_AutoAdvanceInternationalCompetitionFixtures(competition, d7);
            if (d7 == 0)
                _DIY_CachePrimaryQualifiedTeamsGrid();
        }
        _DIY_UpdatePromotionRelegationScratchCounts();
        _DIY_CopyCompetitionTeamIndexTable((uint8_t *)diyQualifiedTeamIndices);
    }

    /* Process qualification entries for all divisions */
    for (d7 = 0; d7 < numDivisions; d7++) {
        processDiyLeagueQualificationEntry(d7, competition);
    }
}

/* ---- Career_AdvanceToNextSeason_UpdateCompetitions ----
 * (SWOS2.s line 31686)
 * Main season transition function. Updates all competition slots,
 * processes next season, and handles European cup draws. */
__attribute__((used, externally_visible))
void _Career_AdvanceToNextSeason_UpdateCompetitions(void)
{
    _DIY_LoadLeagueScratchFromTable();

    playPromotedAnimation = 0;
    playRelegatedAnimation = 0;

    _Career_PrepareEuropeanCupQualificationState();
    _SaveSelectedTeams();

    seasonUpdateInProgress = 1;

    if (isNationalSelector) {
        if ((uint8_t)(selTeamNumber >> 8) == 0x50)
            _Career_UpdateEuropeanChampionshipCompetitions();
        _Career_UpdateWorldCupCompetitions();
    }

    _Career_UpdateEuroCupCompetitionSlot();
    _Career_UpdateCupWinnersCupCompetitionSlot();
    _Career_UpdateUefaCupCompetitionSlot();

    careerFileBuffer = 0;

    if (!isNationalSelector) {
        uint16_t compId = (uint8_t)(selTeamNumber >> 8);
        _Career_ProcessCompetitionForNextSeason(compId);
        _Career_ProcessCompetitionForEuropeanCupQualification();
    }

    /* Loop through all competition IDs in the remap table */
    uint8_t *remapPtr = careerCompetitionIdRemapTable;
    for (;;) {
        uint8_t compByte = *remapPtr++;
        if (compByte == 0xFF)
            break;
        if (!isNationalSelector && compByte == (uint8_t)(selTeamNumber >> 8))
            continue;
        _Career_ProcessCompetitionForNextSeason((uint16_t)compByte);
        _Career_ProcessCompetitionForEuropeanCupQualification();
    }

    if (careerFileBuffer == 0x50) {
        _Career_DrawEuropeanCupTeamsAndCopyGrids();
        seasonUpdateInProgress = 0;
        _RestoreSelectedTeams();

        if (euroCupQualifiedTeamCount == 16 &&
            cupWinnersCupQualifiedTeamCount == 32 &&
            uefaCupQualifiedTeamCount == 32 &&
            careerFileBuffer == 0x50) {
            _Career_StorePromotionRelegationInSeasonInfo();
            return;
        }
        panicReasonCode = 0x8998;
        _PanicHaltLoop();
        return;
    }

    panicReasonCode = 0x8999;
    _PanicHaltLoop();
}
