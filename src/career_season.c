/* career_season.c — Career season management, qualification, team list building.
 *
 * Chunks 67-68: 2 exported functions ported from SWOS2.s.
 */

#include "career_season.h"
#include "game_types.h"
#include "random.h"

/* ---- ASM externs ---- */
extern uint16_t selTeamNumber;
extern uint16_t isNationalSelector;
extern uint16_t teamNoOfferAccepted;

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
