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
#include "endian_helpers.h"
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

extern void *countriesTable[];

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

    /* Iterate as the typed struct so the per-entry stride matches the
     * pointer width on both targets. The original code advanced `table`
     * by hardcoded 4+2+2=8 bytes per iteration, which is correct on
     * 32-bit Amiga but skips 4 bytes per entry on 64-bit (where the
     * pointer field is 8 bytes wide, making each entry 12 bytes total).
     * That misalignment SIGSEGV'd inside the seed-list copy when
     * advancing from EUROCUP qualification setup at season transition.
     * Struct mirrors the definition in swos_amiga_ptr_imports.c. */
    typedef struct __attribute__((packed)) {
        void    *ptr;
        uint16_t size;
        uint16_t count;
    } SeedListEntryLocal;
    SeedListEntryLocal *e = (SeedListEntryLocal *)table;
    for (;;) {
        uint8_t *groupPtr = (uint8_t *)e->ptr;
        if (groupPtr == (uint8_t *)(uintptr_t)-1)
            break;

        uint16_t groupSize = e->size;
        uint16_t copyCount = e->count;
        e++;

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
 * from euroCupQualSeedBucketBlock.bucket15 through .bucket22Full,
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
        /* base[7] is the 32-bit Amiga byte distance from base+7 to the
         * embedded unk1[] qualified-team list. Every cup *Copy struct has a
         * `char *text[2]` pair between the descriptor and unk1[]; on 64-bit
         * those two pointers widen 8->16 bytes, pushing unk1[] +8. Without
         * this delta the list pointer lands inside text[1], the qualified
         * teams overwrite the name pointer, and _DrawMenuText later
         * dereferences team-id bytes as a string (SIGSEGV). Same
         * compensation as _Career_SelectEuroCupSlotIfTeamParticipates
         * (career_init.c) and the AltTableHeader readers (career_teams.c). */
        uint16_t offset = base[7] + 2u * (sizeof(char *) - sizeof(uint32_t));
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
        /* +text[2]-growth delta — see European Cup block above. */
        uint16_t offset = base[7] + 2u * (sizeof(char *) - sizeof(uint32_t));
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
        /* +text[2]-growth delta — see European Cup block above. */
        uint16_t offset = base[7] + 2u * (sizeof(char *) - sizeof(uint32_t));
        uint8_t *teamList = &base[7 + offset];
        uefaCupTeamListPtr = teamList;
        uefaCupTeamListWritePtr = teamList;
        uefaCupQualifiedTeamCount = 0;

        /* Special count: scan ALL seed bucket data (contiguous from first bucket
         * through euroCupQualSeedBucketBlock.bucket22Full which ends with 0xFF) to count how
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

extern CompetitionSlotBuffer primaryCompetitionBuf;          /* slot-0 untangle (S6): typed buffer */
extern CompetitionSlotBuffer competitionContextBuf;          /* slot-1 untangle (S4): typed buffer */
extern CompetitionSlotBuffer tertiaryCompetitionContextBuf;   /* slot-2 untangle (S3): typed buffer */
extern CompetitionSlotBuffer euroCupsCompetitionContextBuf;   /* slot-3 untangle (S5): typed buffer */

extern uint8_t *currentCompetitionPtr;
extern uint8_t *secondaryCompetitionPtr;
extern uint8_t *tertiaryCompetitionPtr;
extern uint8_t *quaternaryCompetitionPtr;

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
    if ((uintptr_t)compPtr >= (uintptr_t)worldCupQualifyingGroup1Copy &&
        (uintptr_t)compPtr <= (uintptr_t)oceaniaWorldCupQualificationCopy) {
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
    uint16_t numTeams = (uint16_t)((DIYCompetition *)comp)->numTeams;  /* 0x32; typed access auto-corrects the 64-bit namePtr shift */

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
    int16_t cat = computeCategoryForCompetition((uint8_t *)&primaryCompetitionBuf);
    if (cat >= 0) {
        uint16_t ucat = (uint16_t)cat;
        adjustWeightsForCompetition((uint8_t *)currentCompetitionPtr, &d5, &ucat);
        d6 += ucat;
    }

    /* Secondary competition (if exists) */
    if (secondaryCompetitionPtr != 0) {
        cat = computeCategoryForCompetition((uint8_t *)&competitionContextBuf);
        if (cat >= 0) {
            uint16_t ucat = (uint16_t)cat;
            adjustWeightsForCompetition((uint8_t *)secondaryCompetitionPtr, &d5, &ucat);
            d6 += ucat;
        }
    }

    /* Weighted average with rounding: (d6 + d5/2) / d5.
     * Asm `divu.w d5,d6` on a zero divisor triggers a m68k trap #5;
     * the Amiga default handler resumes seamlessly with d6 unchanged
     * (in our path d6 is also 0 because no comp contributed a
     * category, so the implicit result is 0). On x86 the same divide
     * raises SIGFPE — guard it. Reached on national-selector mode
     * when neither the primary nor secondary comp's team list
     * contains selTeamNumber (e.g. first FINISH SEASON for GERMANY
     * managing only national selector with no qualified-team match
     * yet — Playaveli's repro). */
    if (d5 != 0) {
        d6 += (d5 >> 1);
        /* ext.l d6; divu.w d5,d6 — sign-extend d6 word to long, unsigned divide */
        uint32_t d6_32 = (uint32_t)(int32_t)(int16_t)d6;
        d6 = (uint16_t)(d6_32 / (uint32_t)d5);
    } else {
        d6 = 0;
    }

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
    uint8_t *comp = (uint8_t *)&primaryCompetitionBuf;
    int16_t position = (int16_t)_FindTeamInDIY(selTeamNumber, (DIYCompetition *)comp);
    uint16_t rank = _GetMoreExpensiveTeamsCount(selTeamNumber, (DIYCompetition *)comp);
    uint16_t numTeams = (uint16_t)((DIYCompetition *)comp)->numTeams;  /* 0x32; typed access auto-corrects the 64-bit namePtr shift */

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
        int16_t pos2 = (int16_t)_FindTeamInDIY(selTeamNumber, (DIYCompetition *)&competitionContextBuf);
        if ((uint16_t)pos2 < 2)
            d7 -= 2;
    }

    /* Tertiary competition: top-2 finish → -2 */
    if (tertiaryCompetitionPtr != 0) {
        int16_t pos3 = (int16_t)_FindTeamInDIY(selTeamNumber, (DIYCompetition *)&tertiaryCompetitionContextBuf);
        if ((uint16_t)pos3 < 2)
            d7 -= 2;
    }

    /* Quaternary competition (Euro cups): top-2 finish → -3 */
    if (quaternaryCompetitionPtr != 0) {
        int16_t pos4 = (int16_t)_FindTeamInDIY(selTeamNumber, (DIYCompetition *)&euroCupsCompetitionContextBuf);
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

extern uint8_t *secondaryCompetitionPtr;
extern uint8_t *tertiaryCompetitionPtr;

extern void *competitionsTable[];
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
    if (((Competition *)competition)->type != 1) {
        panicReasonCode = 0x3003;
        _PanicHaltLoop();
        return;
    }
    if (competition == secondaryCompetitionPtr) {
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
    if (((Competition *)competition)->type != 1) {
        panicReasonCode = 0x3003;
        _PanicHaltLoop();
        return;
    }
    if (competition == tertiaryCompetitionPtr) {
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

    /* Walk competitionsTable[idx]'s sub-table as void** so each slot is
     * native pointer-width. Was uint32_t* — read half-pointers on 64-bit. */
    void **tableEntry = (void **)competitionsTable[compId & 0xFF];

    /* Entry layout: [0]=primary ptr, [1]=skip, [2]=secondary ptr, [3]=tertiary ptr */
    uint8_t *primary = (uint8_t *)tableEntry[0];
    processPrimaryCompetitionForNextSeason(primary);

    uint8_t *secondary = (uint8_t *)tableEntry[2];
    if (secondary != (uint8_t *)(uintptr_t)-1) {
        _Career_ProcessSecondaryCompetitionForNextSeason(secondary);

        uint8_t *tertiary = (uint8_t *)tableEntry[3];
        if (tertiary != (uint8_t *)(uintptr_t)-1) {
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
/* careerCupPickList: a 5-entry array of COMPETITION POINTERS (not uint32) —
 * holds the national-coach cup-pick-list comp records (copied from the
 * cupPick* tables, which competition_data.c defines as CompetitionCup *[5]).
 * Was extern'd uint32_t[] here, which truncated the 64-bit pointers and made
 * _Rebuild_CompetitionSlotPointers (reads it as void**) deref garbage. Now a
 * pointer array; see the standalone definition in swos_data_typed_stubs.c. */
extern void *careerCupPickList[];
extern TeamFileEx careerTeam;
extern uint8_t teamSortBuffer[];

/* Competition qualification copies */
extern uint8_t worldCupQualifyingGroup8Copy[];
extern uint8_t northAmericaWorldCupQualificationCopy[];
extern uint8_t southAmericaWorldCupQualificationCopy[];
extern uint8_t africaWorldCupQualificationCopy[];
extern uint8_t asiaWorldCupQualificationCopy[];

/* Cup pick list data arrays (stay in asm, contain relocatable pointers) */
extern void *cupPickOceaniaWorldCupFriendly[];
extern void *cupPickOceaniaWorldCupInvite[];
extern void *cupPickOceaniaOceaniaCupFinalist[];
extern void *cupPickOceaniaOceaniaCupNonFinalist[];
extern void *cupPickOceaniaOceaniaCupFriendly[];
extern void *cupPickOceaniaInviteFriendly[];

extern void *cupPickNAmericaWorldCupFriendly[];
extern void *cupPickNAmericaWorldCupInvite[];
extern void *cupPickNAmericaNaCupFinalist[];
extern void *cupPickNAmericaNaCupNonFinalist[];
extern void *cupPickNAmericaNaCupFriendly[];
extern void *cupPickNAmericaInviteFriendly[];

extern void *cupPickAsiaWorldCupFriendly[];
extern void *cupPickAsiaWorldCupInvite[];
extern void *cupPickAsiaAsianCupFinalist[];
extern void *cupPickAsiaAsianCupNonFinalist[];
extern void *cupPickAsiaAsianCupFriendly[];
extern void *cupPickAsiaInviteFriendly[];

extern void *cupPickAfricaWorldCupFriendly[];
extern void *cupPickAfricaWorldCupInvite[];
extern void *cupPickAfricaAfricanCupFinalist[];
extern void *cupPickAfricaAfricanCupNonFinalist[];
extern void *cupPickAfricaAfricanCupFriendly[];
extern void *cupPickAfricaInviteFriendly[];

extern void *cupPickSAmericaWorldCupFriendly[];
extern void *cupPickSAmericaWorldCupInvite[];
extern void *cupPickSAmericaCopaAmericaFinalist[];
extern void *cupPickSAmericaCopaAmericaNonFinalist[];
extern void *cupPickSAmericaCopaAmericaFriendly[];
extern void *cupPickSAmericaInviteFriendly[];

extern void *cupPickListEuropeWorldCupFriendly[];
extern void *cupPickLisEuropeWorldCupInviteFriendly[];
extern void *cupPickLisEuropeEuroChampsFriendly[];
extern void *cupPickListEuropeEuroChampsInviteFriendly[];
extern void *cupPickListEuropeInviteTournamentFriendly[];
extern void *cupPickEuropeNoCupFriendly[];
extern void *cupPickEuropeNoCupFriendlyAlt[];
extern void *cupPickListEuropeInviteTournamentFriendlyAlt[];

/* Real storage is `CompetitionCup *[8]` in competition_data.c:1175.
 * Same trap shape as worldCupQualifyingGroupsTable below — see
 * comment there. Use `void *[]` so the stride is native pointer-width. */
extern void    *ecQualifyingGroupsTable[];
/* Real storage is `CompetitionCup *[8]` in competition_data.c:1191
 * — native pointer-width table. The legacy extern said `uint32_t[]`
 * (Amiga 32-bit pointer slot) which read half a pointer on 64-bit
 * at every `(uint8_t *)tbl[i]` consumer site. Declared as
 * `void *[]` here to avoid pulling in competition_data.h's other
 * typedefs (some clash with locals in this file). */
extern void    *worldCupQualifyingGroupsTable[];
/* Real storage is `char *tourStringTable[6]` (competition_data.c). Declaring
 * it uint32_t here truncated each entry to 4 bytes on 64-bit (wrong stride
 * AND a truncated pointer value), corrupting the runtime tour-name store. */
extern char    *tourStringTable[];

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
        NationalTeamRecord *rec = (NationalTeamRecord *)nationalTeams + (uint16_t)idx;
        /* teamId is compared against selTeamNumber (BE-packed), so
         * compose the same form via PACK_BE_U16 on either host. */
        uint16_t teamId = PACK_BE_U16(rec->countryNumber, rec->teamOrdinal);

        if (regionFilter != 0) {
            if (rec->countryNumber != regionFilter)
                continue;
        }
        if (teamId == selTeamNumber)
            continue;

        if (retries >= 0) {
            uint16_t price = rec->skillOrPrice;
            if (price < minPrice || price > maxPrice) {
                retries--;
                continue;
            }
        }

        if (countSoFar == 0)
            return teamId;

        /* currentList holds BE-packed team ids (setupFriendlyCupTeams /
         * setupInvitationTournamentTeams / setupTourCupTeams now use
         * BE_WRITE_U16 to match the dc.b storage). Read the same way
         * so the host-native teamId compares equal to the matching
         * entry on both endians. */
        int16_t dup = 0;
        uint16_t *p = currentList;
        for (uint16_t i = countSoFar; i > 0; i--) {
            if (BE_READ_U16(p) == teamId) { dup = 1; break; }
            p++;
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
    /* Sort leaves a6=teamSortBuffer in the asm; re-point our explicit
     * teamListPtr at the same buffer the sort just populated so the
     * `*(*teamListPtr)++` reads walk the sorted team ids for this
     * region. See setCupPickList_Europe for the full rationale. */
    *teamListPtr = (uint16_t *)teamSortBuffer;
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
    /* +text[2]-growth delta — the embedded unk1[] team list sits 8 bytes
     * later on 64-bit (cup struct's char *text[2] widens 8->16). Without it
     * the writes land in text[1] and stomp the comp name pointer. Matches
     * the readers in career_teams.c / career_init.c. */
    uint16_t *teamSlots = (uint16_t *)(group + 7 + (hdrSize & 0xFF)
                          + 2u * (sizeof(char *) - sizeof(uint32_t)));

    uint16_t firstTeam;
    if (selTeamNumber != worldCupHostTeam &&
        selTeamNumber != worldCupQualifyingTeam &&
        (uint8_t)(selTeamNumber >> 8) == regionCode) {
        careerCupPickList[0] = (void *)origGroup;
        firstTeam = selTeamNumber;
    } else {
        do {
            firstTeam = *(*teamListPtr)++;
        } while (firstTeam == selTeamNumber ||
                 firstTeam == worldCupHostTeam ||
                 firstTeam == worldCupQualifyingTeam);
    }

    /* Slot writes use BE byte order to match the static-init `dc.b
     * country,ordinal` competition data — downstream
     * _AppendTeamsToSelectionFromTmdOrTeamFile reads each slot with
     * BE_READ_U16, so host-native writes on LE produced byte-swapped
     * ids (Playaveli's GERMANY.CAR repro: VerifyTeamLoaded(0x0350)
     * panic after the new sort fix had stored the correct 0x5003 in
     * teamSortBuffer). asm `move.w` writes BE on m68k; mirror with
     * BE_WRITE_U16 so both endians match the dc.b layout. */
    BE_WRITE_U16(teamSlots, firstTeam); teamSlots++;
    for (int16_t i = teamCount - 1; i > 0; i--) {
        uint16_t teamId;
        do {
            teamId = *(*teamListPtr)++;
        } while (teamId == selTeamNumber ||
                 teamId == worldCupHostTeam ||
                 teamId == worldCupQualifyingTeam);
        BE_WRITE_U16(teamSlots, teamId); teamSlots++;
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
    /* Sort leaves a6=teamSortBuffer in the asm — see comment in
     * fillWorldCupQualifyingGroupTeams. Re-point teamListPtr at the
     * just-populated buffer. */
    *teamListPtr = (uint16_t *)teamSortBuffer;
    uint16_t randOff = _Rand2() & 0x1C;
    uint8_t *selectedGroup = (uint8_t *)worldCupQualifyingGroupsTable[randOff >> 2];
    int16_t slotsRemaining = 0x30 - 6 + d5;
    d5 <<= 1;
    int16_t colOffset = 0;

    for (;;) {
        for (int16_t g = 0; g < 8; g++) {
            uint8_t *group = (uint8_t *)worldCupQualifyingGroupsTable[g];
            uint8_t hdrSize = group[7];
            /* +text[2]-growth delta — see fillWorldCupQualifyingGroupTeams. */
            uint16_t *slot = (uint16_t *)(group + 7 + (hdrSize & 0xFF)
                            + 2u * (sizeof(char *) - sizeof(uint32_t)) + colOffset);

            uint16_t teamId;
            if (colOffset == 0 && group == selectedGroup &&
                selTeamNumber != worldCupHostTeam &&
                selTeamNumber != worldCupQualifyingTeam &&
                (uint8_t)(selTeamNumber >> 8) == 0x50) {
                careerCupPickList[0] = (void *)group;
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

            /* BE write — see fillWorldCupQualifyingGroupTeams comment. */
            BE_WRITE_U16(slot, teamId);
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
static void setCupPickListCommon(void **worldCupFriendly, void **worldCupInvite,
                                  void **cupFinalist, void **cupNonFinalist,
                                  void **cupFriendly, void **inviteFriendly,
                                  uint16_t **teamListPtr)
{
    void **src = worldCupFriendly;
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
    void **src;
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
    /* Asm "register-return" pattern: Career_SortTeamFilePtrsByAvgPlayerPrice's
     * final instruction is `lea (teamSortBuffer).l,a6` and its movem doesn't
     * restore a6, so the sort intentionally leaks a6 = teamSortBuffer to the
     * caller. Career_SetCupPickList_Europe's group-fill loop then reads
     * `(a6)+` to consume the just-sorted team id list. Our C translation
     * threaded a teamListPtr through but the outer caller passes NULL — fix
     * by pointing it at teamSortBuffer here, after the sort populates it. */
    *teamListPtr = (uint16_t *)teamSortBuffer;
    uint16_t randIdx = (uint16_t)((uint32_t)_Rand2() * 7 >> 8);
    uint8_t *selectedGroup = (uint8_t *)ecQualifyingGroupsTable[randIdx];
    int16_t slotsRemaining = 0x2E; /* 46 */
    int16_t colOffset = 0;

    for (;;) {
        for (int16_t g = 0; g < 8; g++) {
            uint8_t *group = (uint8_t *)ecQualifyingGroupsTable[g];
            uint8_t hdrSize = group[7];
            /* +text[2]-growth delta — see fillWorldCupQualifyingGroupTeams. */
            uint16_t *slot = (uint16_t *)(group + 7 + (hdrSize & 0xFF)
                            + 2u * (sizeof(char *) - sizeof(uint32_t)) + colOffset);

            uint16_t teamId;
            int readFromList = 1;
            if (colOffset == 0 && group == selectedGroup) {
                if (selTeamNumber != euroChampionshipQualTeam) {
                    careerCupPickList[0] = (void *)group;
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

            /* BE write — see fillWorldCupQualifyingGroupTeams comment. */
            BE_WRITE_U16(slot, teamId);
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
    int16_t teamCount = (int8_t)((CompCupHeader *)comp)->roundStringBase;
    uint16_t *teamList = (uint16_t *)comp_team_seed_list((Competition *)comp);
    uint16_t *listBase = teamList;

    /* BE writes so the slot bytes match the static-init dc.b layout
     * (downstream _AppendTeamsToSelectionFromTmdOrTeamFile reads with
     * BE_READ_U16). pickRandomNationalTeamId's dup-check also reads
     * via BE_READ_U16. */
    BE_WRITE_U16(teamList, selTeamNumber); teamList++;
    uint16_t countSoFar = 1;

    for (int16_t i = teamCount - 2; i >= 0; i--) {
        uint16_t teamId = pickRandomNationalTeamId(minPrice, maxPrice, 0, listBase, countSoFar);
        BE_WRITE_U16(teamList, teamId); teamList++;
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
    int16_t teamCount = (int8_t)((CompCupHeader *)comp)->roundStringBase;
    uint16_t *teamList = (uint16_t *)comp_team_seed_list((Competition *)comp);
    uint16_t *listBase = teamList;

    /* BE writes — see setupFriendlyCupTeams comment. */
    BE_WRITE_U16(teamList, selTeamNumber); teamList++;
    uint16_t countSoFar = 1;

    for (int16_t i = teamCount - 2; i >= 0; i--) {
        uint16_t teamId = pickRandomNationalTeamId(minPrice, maxPrice, 0, listBase, countSoFar);
        BE_WRITE_U16(teamList, teamId); teamList++;
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
    /* tour+5+nameOff targets text[0] (name-table start, doesn't shift), but
     * the slot is a full char* — write all 8 bytes on 64-bit, not 4. */
    *(char **)(tour + 5 + nameOff) = tourStringTable[randRegion];

    int16_t teamCount = (int8_t)tour[0xD];
    uint8_t hdrSize = tour[7];
    /* +text[2]-growth delta — see fillWorldCupQualifyingGroupTeams. */
    uint16_t *teamList = (uint16_t *)(tour + 7 + (hdrSize & 0xFF)
                        + 2u * (sizeof(char *) - sizeof(uint32_t)));
    uint16_t *listBase = teamList;

    /* BE writes — see setupFriendlyCupTeams comment. */
    BE_WRITE_U16(teamList, selTeamNumber); teamList++;
    uint16_t countSoFar = 1;

    for (int16_t i = teamCount - 2; i >= 0; i--) {
        uint16_t teamId = pickRandomNationalTeamId(0, 0x31, regionCode, listBase, countSoFar);
        BE_WRITE_U16(teamList, teamId); teamList++;
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
extern uint8_t *diyCompetitionPtr;
extern CompetitionSlotBuffer diyCompetitionContextBuf;   /* slot-4 untangle (S2): typed buffer */

/* England's leagues — standalone globals, consolidated from the old
 * EnglishLeaguesBlock (competition_data.c). competition_data.h is kept out of
 * this TU (opaque forward decls for several competition tables); CompetitionLeague
 * / Leagues come from game_types.h, so extern the four globals here. */
extern CompetitionLeague englishLeague;
extern Leagues englishLeagues1, englishLeagues2, englishLeagues3;
extern Playoffs div1Playoffs;
extern Playoffs div2Playoffs;
extern Playoffs div3Playoffs;

/* ---- Career_ProcessDiyLeagueQualificationEntry ----
 * (SWOS2.s line 31903)
 * Processes a single DIY league qualification entry during season transition.
 * index d7 selects which division qualification entry to process.
 * competition a4 is the competition record. */
/* Refactored from asm-style byte walking to typed struct access (May 2026).
 *
 * SWOS2.s Career_ProcessDiyLeagueQualificationEntry (line 58055) walks the
 * country's leagues block via byte offsets stored in
 * `divisions[i].qualifierLinkOffset` (was unk1) to
 * find a `Playoffs *` qualification competition and a qIdx pair list. The
 * walk assumed the 32-bit Amiga struct layout. On 64-bit several involved
 * structs grow because of pointer-field widening:
 *   - CompetitionLeague_4_8_2_24: text[8]+unk2[2] grow → 102 → 142 bytes
 *   - Leagues_4_2_24: Leagues_4+playoffs+leagues2 grow → 80 → 104 bytes
 *   - Playoffs: text[2] grows → 32 → 40 bytes
 *
 * The static `divisions[i].qualifierLinkOffset` byte hints were authored for
 * 32-bit, so the walk `competition + i*6 + 0x12 + qualifierLinkOffset` lands
 * inside text[8] (a string pointer) instead of unk2[0] on 64-bit, returning
 * garbage as qualComp.
 *
 * In practice only the English block (EnglishLeaguesBlock) ever exercises
 * this walk — its divisions[1..3].qualifierLinkOffset are non-zero (46, 120,
 * 194) while every other country has all-zero division.qualifierLinkOffset and
 * exits at the early
 * `if (d0 == 0) return;` test. The typed dispatch below is English-specific.
 *
 * For each English division, by static data analysis:
 *   index=1: walk lands at englishLeague.unk2[0] = &div1Playoffs;
 *            qIdx pairs live in englishLeague.unk3[].
 *   index=2: walk lands at englishLeagues1.playoffs = &div2Playoffs;
 *            qIdx pairs in englishLeagues1.qualifierIndexPairs[].
 *   index=3: walk lands at englishLeagues2.playoffs = &div3Playoffs;
 *            qIdx pairs in englishLeagues2.qualifierIndexPairs[].
 *
 * teamData = qualComp + 7 + qualComp[7] in the asm; for all three Playoffs
 * statics qualComp->comp.teamListOffset = 17 (Amiga byte offset to qualifiedTeamIds[0]).
 * On 64-bit Playoffs.qualifiedTeamIds sits 8 bytes later (text[2] grew). Using
 * typed `&qualComp->qualifiedTeamIds[0]` resolves correctly on both targets. */
static void processDiyLeagueQualificationEntry(uint16_t index, uint8_t *competition)
{
    uint8_t *a2 = competition + index * 6;
    uint8_t d0 = a2[0x12];

    if (d0 == 0)
        return;

    /* Last-division-same-offset gate (matches asm `cmp 9(a4),d1` and the
     * `addq.b #6,d1; cmp.b d1,d0` test). Byte 9 of CompetitionLeague is
     * divisionsCount. Both bytes here are within the original Amiga
     * struct's head (offsets < 0x28), so no namePtr shift applies. */
    uint16_t nextDiv = index + 1;
    if ((uint8_t)nextDiv != (uint8_t)((CompLeagueHeader *)competition)->divisionsCount) {
        uint8_t d1 = a2[0x18] + 6;
        if (d0 == d1)
            return;
    }

    /* Typed dispatch (see header comment for derivation). The walk via
     * `divisions[i].qualifierLinkOffset` is replaced with direct struct-member
     * access. */
    if (competition != (uint8_t *)&englishLeague) {
        /* Non-English country reached the walk despite divisions[i].qualifierLinkOffset
         * being expected zero — unexpected. Bail safely rather than
         * re-introduce the broken byte walk. */
        return;
    }

    Playoffs *qualComp;
    uint8_t   *teamIdxList;
    switch (index) {
    case 1:
        qualComp    = (Playoffs *)englishLeague.playoffLinkPtrs[0]; /* &div1Playoffs */
        teamIdxList = (uint8_t *)englishLeague.promotionLinkData;
        break;
    case 2:
        qualComp    = englishLeagues1.playoffs;              /* &div2Playoffs */
        teamIdxList = (uint8_t *)englishLeagues1.qualifierIndexPairs;
        break;
    case 3:
        qualComp    = englishLeagues2.playoffs;              /* &div3Playoffs */
        teamIdxList = (uint8_t *)englishLeagues2.qualifierIndexPairs;
        break;
    default:
        return; /* Premier (index=0) returned via early-exit. */
    }

    int8_t count = (int8_t)qualComp->teams;   /* qualified-team count (was playoffHeader[10]) */

    if (!youreSacked && (uint8_t *)qualComp == diyCompetitionPtr) {
        HLOG("promotion-playoff INTERACTIVE (player in playoff): index=%u teams=%d\n",
             (unsigned)index, (int)count);
        /* Player's competition — interactive processing. Advance the
         * teamIdxList past the consumed qIdx pairs (asm `lea (a2,d0.w),a2`
         * with d0 = count*2) so DIY_ApplyPromotionRelegationWeights sees
         * the post-list region. */
        teamIdxList += (int16_t)count * 2;

        _RestoreSelectedTeams();
        _Career_LoadCompetitionSlotStateFromBuffer((uint8_t *)&diyCompetitionContextBuf);
        _ShowDIYCupMenuWithParams(1, 1, 0);
        _DIY_ApplyPromotionRelegationWeightsToScratch(teamIdxList);
        _SaveSelectedTeams();
    } else {
        HLOG("promotion-playoff auto: index=%u teams=%d\n", (unsigned)index, (int)count);
        /* Not player's competition — auto-fill teams from qualification indices. */
        uint16_t savedSelTeam = selTeamNumber;
        uint8_t  savedCtrl    = careerTeam.team.teamControls;

        selTeamNumber = 0xFFFF;
        careerTeam.team.teamControls = 1;

        /* Sanity check matching the asm `beq.w loc_12BCD0` — Playoffs
         * statics all have comp.teamListOffset=17, so 0 should never appear. */
        if (comp_team_seed_list(&qualComp->comp) == NULL) {
            panicReasonCode = 0x3003;
            _PanicHaltLoop();
            return;
        }

        /* teamData lands at qualComp->qualifiedTeamIds[0] on both targets via
         * typed access (replaces the asm's `lea 7(a1,d0.w),a0` byte walk where
         * d0 = qualComp->comp.teamListOffset = 17). On 32-bit byte 7+17=24 =
         * qualifiedTeamIds[0]; on 64-bit the same logical position is byte 32
         * because text[2] widened. Typed lookup picks the right byte on each. */
        uint8_t *teamData = (uint8_t *)&qualComp->qualifiedTeamIds[0];

        int16_t cnt = (int16_t)count - 1;

        /* qIdx pair stream is BE on m68k (`move.w (a2)+,d0`) — keep
         * BE_READ_U16 to recover the value on LE hosts. diyQualifiedTeamIndices
         * is BE-packed (4e5f378) and the destination unk2 is also read by
         * downstream code with BE_READ_U16 — write BE to match. */
        for (int16_t i = cnt; i >= 0; i--) {
            uint16_t qIdx = BE_READ_U16(teamIdxList);
            teamIdxList += 2;
            uint16_t teamId = BE_READ_U16(&diyQualifiedTeamIndices[qIdx]);
            BE_WRITE_U16(teamData, teamId);
            teamData += 2;
        }

        /* Auto-advance this competition via cup competition path. */
        _Career_AutoAdvanceCupCompetitionFromRecord((uint8_t *)qualComp);
        _DIY_ApplyPromotionRelegationWeightsToScratch(teamIdxList);

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
    int8_t compType = ((Competition *)competition)->type;

    if (compType == 2) {
        /* Cup competition */
        if (competition == currentCompetitionPtr) {
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

    /* League competition — loop through all divisions.
     *
     * `qualCursor` mirrors the asm's a3 register: set ONCE before the
     * loop, advanced inside DIY_CopyCompetitionTeamIndexTable on each
     * iteration. This way each division's promotion list APPENDS to
     * diyQualifiedTeamIndices rather than overwriting from base, so
     * the per-division qualification entries below see the cumulative
     * list (matches Career_ProcessPrimaryCompetitionForNextSeason at
     * SWOS2.s line 57992 / loc_12BDC8). Resetting to base each call
     * caused only the last division's data to be visible — and the
     * qualification-loop's _Career_AutoAdvanceCupCompetitionFromRecord
     * picked teams from the wrong division, putting promotion weights
     * into wrong scratch slots and tripping the 0x5555 validate panic. */
    uint8_t numDivisions = (uint8_t)((CompLeagueHeader *)competition)->divisionsCount;
    uint16_t d7;
    uint8_t *qualCursor = (uint8_t *)diyQualifiedTeamIndices;

    for (d7 = 0; d7 < numDivisions; d7++) {
        if (competition == currentCompetitionPtr &&
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
        qualCursor = _DIY_CopyCompetitionTeamIndexTable(qualCursor);
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
