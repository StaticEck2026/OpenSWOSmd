/* career_eurocups.c — European Cup qualification, grid caching, title holder management.
 *
 * Chunk 65: 7 exported + 12 static functions ported from SWOS2.s.
 */

#include "career_eurocups.h"
#include "game_types.h"
#include "diy_competition.h"
#include "panic.h"

/* ---- ASM externs (globals defined in assembly) ---- */
extern uint8_t diyCompetitionStart[];
extern uint8_t primaryQualifiedTeamGrids[];
extern uint8_t tertiaryQualifiedTeamGrid[];
extern uint8_t euroCupTitleHolderGrid[];
extern uint8_t cupWinnersCupTitleHolderGrid[];
extern uint8_t uefaCupTitleHolderGrid[];

extern uint16_t euroCupTitleHolderTeamId;
extern uint16_t cupWinnersCupTitleHolderTeamId;
extern uint16_t uefaCupTitleHolderTeamId;
extern uint16_t cachedPrimaryQualifiedTeamIds[];
extern uint16_t cachedSecondaryQualifiedTeamIds[];
extern uint16_t cachedTertiaryQualifiedTeamId;

extern uint8_t euroCupEligibleCompetitionIds[];
extern uint8_t cupWinnersCupEligibleCompetitionIds[];
extern uint8_t uefaCupEligibleCompetitionIds[];

extern uint8_t currentCompetitionId;
extern uint16_t euroCupQualifiedTeamCount;
extern uint16_t cupWinnersCupQualifiedTeamCount;
extern uint16_t uefaCupQualifiedTeamCount;

extern uint8_t *euroCupTeamListPtr;
extern uint8_t *cupWinnersCupTeamListPtr;
extern uint8_t *uefaCupTeamListPtr;
extern uint8_t *euroCupTeamListWritePtr;
extern uint8_t *cupWinnersCupTeamListWritePtr;
extern uint8_t *uefaCupTeamListWritePtr;

extern uint32_t countriesTable[];
extern uint16_t selTeamNumber;

/* C-to-asm bridges */
extern uint8_t *_VerifyTeamLoaded(uint16_t teamNum);
extern void _CCareer_AppendTeamToTmdTeamPool(uint8_t *team);

/* Already-ported C functions */
extern void _DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs(void);
extern void _DIY_SortLeagueTableByStandings(void);

/* ================================================================
 * Group A: List manipulation helpers (from loc_12B1F4, loc_12B20A)
 * ================================================================ */

/* isTeamInList — checks if team number is in a word list.
 * Asm: loc_12B1F4. Returns 1 if found, 0 if not. */
static int16_t isTeamInList(uint16_t team, uint8_t *listPtr, uint16_t count)
{
    uint16_t *list = (uint16_t *)listPtr;
    for (uint16_t i = 0; i < count; i++) {
        if (list[i] == team)
            return 1;
    }
    return 0;
}

/* appendTeamToList — appends team number to a word list and adds to TMD pool.
 * Asm: loc_12B20A. teamGrid points to the team grid data (first word = team number). */
static void appendTeamToList(uint8_t *teamGrid, uint8_t **writePtrAddr, uint16_t *count)
{
    uint16_t *writePos = (uint16_t *)(*writePtrAddr);
    *writePos = *(uint16_t *)teamGrid;
    *writePtrAddr = (uint8_t *)(writePos + 1);
    (*count)++;
    _CCareer_AppendTeamToTmdTeamPool(teamGrid);
}

/* tryConsumeCompetitionIdFromList — searches byte list for currentCompetitionId,
 * marks found entry as 0xFE. Returns 1 if found, 0 if not.
 * Asm: TryConsumeCompetitionIdFromList */
static int16_t tryConsumeCompetitionIdFromList(uint8_t *list)
{
    uint8_t id = currentCompetitionId;
    while (*list != 0xFF) {
        if (*list == id) {
            *list = 0xFE;
            return 1;
        }
        list++;
    }
    return 0;
}

/* isTeamInCupWinnersCupList — checks if team d0 is in the CWC qualified list.
 * Asm: IsTeamInCupWinnersCupList */
static int16_t isTeamInCupWinnersCupList(uint16_t team)
{
    return isTeamInList(team, cupWinnersCupTeamListPtr, cupWinnersCupQualifiedTeamCount);
}

/* isTeamInUefaCupList — checks if team d0 is in the UEFA Cup qualified list.
 * Asm: IsTeamInUefaCupList */
static int16_t isTeamInUefaCupList(uint16_t team)
{
    return isTeamInList(team, uefaCupTeamListPtr, uefaCupQualifiedTeamCount);
}

/* clearEuropeanCupTitleHolderIfMatchTeam — clears the matching title holder.
 * Asm: Career_ClearEuropeanCupTitleHolderIfMatchTeam */
static void clearEuropeanCupTitleHolderIfMatchTeam(uint16_t team)
{
    if (team == euroCupTitleHolderTeamId) {
        euroCupTitleHolderTeamId = 0xFFFF;
        return;
    }
    if (team == cupWinnersCupTitleHolderTeamId) {
        cupWinnersCupTitleHolderTeamId = 0xFFFF;
        return;
    }
    if (team == uefaCupTitleHolderTeamId) {
        uefaCupTitleHolderTeamId = 0xFFFF;
    }
}

/* appendTeamToEuroCupList — appends team grid to Euro Cup list.
 * Asm: AppendTeamToEuroCupList */
static void appendTeamToEuroCupList(uint8_t *teamGrid)
{
    appendTeamToList(teamGrid, &euroCupTeamListWritePtr, &euroCupQualifiedTeamCount);
}

/* appendTeamToCupWinnersCupList — appends team grid to CWC list.
 * Asm: AppendTeamToCupWinnersCupList */
static void appendTeamToCupWinnersCupList(uint8_t *teamGrid)
{
    appendTeamToList(teamGrid, &cupWinnersCupTeamListWritePtr, &cupWinnersCupQualifiedTeamCount);
}

/* appendTeamToUefaCupList — appends team grid to UEFA Cup list.
 * Asm: AppendTeamToUefaCupList */
static void appendTeamToUefaCupList(uint8_t *teamGrid)
{
    appendTeamToList(teamGrid, &uefaCupTeamListWritePtr, &uefaCupQualifiedTeamCount);
}

/* ================================================================
 * Group B: Qualification logic
 * ================================================================ */

/* qualifyTeamForEuroCup — if competition is eligible, qualify the primary
 * league winner for the Euro Cup.
 * Asm: Career_QualifyTeamForEuroCup_IfEligibleCompetition */
static void qualifyTeamForEuroCup(void)
{
    if (!tryConsumeCompetitionIdFromList(euroCupEligibleCompetitionIds))
        return;
    uint8_t *a5 = primaryQualifiedTeamGrids;
    uint16_t team = *(uint16_t *)a5;
    clearEuropeanCupTitleHolderIfMatchTeam(team);
    appendTeamToEuroCupList(a5);
}

/* qualifyTeamForCupWinnersCup — if competition is eligible, find a non-primary,
 * non-Euro-Cup-holder team from the grid for the Cup Winners' Cup.
 * Asm: Career_QualifyTeamForCupWinnersCup_IfEligibleCompetition */
static void qualifyTeamForCupWinnersCup(void)
{
    if (!tryConsumeCompetitionIdFromList(cupWinnersCupEligibleCompetitionIds))
        return;
    /* Start at slot 8 (primaryQualifiedTeamGrids + 0x1560), advance to find
     * a team that doesn't match the primary cached team or Euro Cup holder.
     * Boundary spill: walks through contiguous BSS buffers. */
    uint8_t *a5 = primaryQualifiedTeamGrids + (8 * sizeof(TeamFileEx));
    for (;;) {
        a5 += sizeof(TeamFileEx);
        uint16_t team = *(uint16_t *)a5;
        if (team == cachedPrimaryQualifiedTeamIds[0])
            continue;
        if (team == euroCupTitleHolderTeamId)
            continue;
        break;
    }
    uint16_t team = *(uint16_t *)a5;
    clearEuropeanCupTitleHolderIfMatchTeam(team);
    appendTeamToCupWinnersCupList(a5);
}

/* qualifyTeamsForUefaCup — for each eligible competition ID, find a team
 * from the grid that hasn't been claimed by any other European cup.
 * Asm: Career_QualifyTeamsForUefaCup_IfEligibleCompetition */
static void qualifyTeamsForUefaCup(void)
{
    uint8_t *a5;
    if (cachedTertiaryQualifiedTeamId != 0xFFFF) {
        a5 = tertiaryQualifiedTeamGrid;
    } else {
        a5 = primaryQualifiedTeamGrids + sizeof(TeamFileEx); /* slot 1 */
    }
    a5 -= sizeof(TeamFileEx);

    while (tryConsumeCompetitionIdFromList(uefaCupEligibleCompetitionIds)) {
        for (;;) {
            a5 += sizeof(TeamFileEx);
            if (a5 == euroCupTitleHolderGrid) {
                a5 = primaryQualifiedTeamGrids + sizeof(TeamFileEx); /* wrap to slot 1 */
            }
            uint16_t team = *(uint16_t *)a5;
            if (team == cachedPrimaryQualifiedTeamIds[0])
                continue;
            if (team == cachedSecondaryQualifiedTeamIds[0])
                continue;
            if (team == euroCupTitleHolderTeamId)
                continue;
            if (team == cupWinnersCupTitleHolderTeamId)
                continue;
            if (isTeamInCupWinnersCupList(team))
                continue;
            if (isTeamInUefaCupList(team))
                continue;
            break;
        }
        uint16_t team = *(uint16_t *)a5;
        clearEuropeanCupTitleHolderIfMatchTeam(team);
        appendTeamToUefaCupList(a5);
    }
}

/* ================================================================
 * Group B: Exported qualification entry point
 * ================================================================ */

__attribute__((used, externally_visible))
void _Career_ProcessCompetitionForEuropeanCupQualification(void)
{
    /* countriesTable is uint32_t[] (pointer table); index by competition ID */
    uint16_t idx = (uint8_t)currentCompetitionId;
    uint8_t *countryPtr = (uint8_t *)countriesTable[idx];
    if (*countryPtr != 0)
        return; /* not European (first byte != 0) → skip */
    qualifyTeamForEuroCup();
    qualifyTeamForCupWinnersCup();
    qualifyTeamsForUefaCup();
}

/* ================================================================
 * Group C: Grid caching — copy team data from loaded teams to grids
 * ================================================================ */

/* copyTeamGridWords — volatile word copy to prevent memcpy optimization */
static void copyTeamGridWords(uint8_t *src, uint8_t *dst, uint16_t wordCount)
{
    volatile uint16_t *vs = (volatile uint16_t *)src;
    volatile uint16_t *vd = (volatile uint16_t *)dst;
    for (int16_t i = (int16_t)wordCount; i >= 0; i--) {
        *vd++ = *vs++;
    }
}

__attribute__((used, externally_visible))
void _DIY_CachePrimaryQualifiedTeamsGrid(void)
{
    uint8_t *dst = primaryQualifiedTeamGrids;
    uint16_t *cachedIds = cachedPrimaryQualifiedTeamIds;
    uint8_t *comp = diyCompetitionStart;
    uint16_t *leaguePositions = (uint16_t *)(comp + 0x6E);
    uint16_t *teamIdsTable = (uint16_t *)(comp + 0x12E);
    uint16_t count = 9;
    uint16_t numTeams = *(uint16_t *)(comp + 0x32);
    if (count > numTeams)
        count = numTeams;
    /* asm: subq.w #1,d2 then dbf → loops count times */
    for (int16_t i = (int16_t)(count - 1); i >= 0; i--) {
        int16_t posIdx = (int16_t)*leaguePositions++;
        /* ext.l d0 then divu.w #$12,d0: sign-extend to 32-bit then unsigned div by 18 */
        uint16_t slotIdx = (uint16_t)((uint32_t)(int32_t)posIdx / 18);
        slotIdx <<= 1;
        uint16_t teamNum = *(uint16_t *)((uint8_t *)teamIdsTable + slotIdx);
        *cachedIds++ = teamNum;
        uint8_t *teamData = _VerifyTeamLoaded(teamNum);
        copyTeamGridWords(teamData, dst, 0x155);
        dst += sizeof(TeamFileEx);
    }
}

__attribute__((used, externally_visible))
void _DIY_CacheSecondaryQualifiedTeamsGrid(void)
{
    _DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs();
    /* asm: lea (primaryQualifiedTeamGrids+0x180C),a1 — slot 9 (0x2AC * 9 = 0x180C) */
    uint8_t *dst = primaryQualifiedTeamGrids + 0x180C;
    uint16_t *cachedIds = cachedSecondaryQualifiedTeamIds;
    uint8_t *comp = diyCompetitionStart;
    uint8_t *grid = comp + 0x2CE;
    uint16_t *teamIdsTable = (uint16_t *)(comp + 0x24E);

    for (int16_t i = 2; i >= 0; i--) {
        int8_t gridByte = (int8_t)*grid++;
        int16_t idx = gridByte;
        idx <<= 1;
        uint16_t teamNum = *(uint16_t *)((uint8_t *)teamIdsTable + idx);
        *cachedIds++ = teamNum;
        uint8_t *teamData = _VerifyTeamLoaded(teamNum);
        copyTeamGridWords(teamData, dst, 0x155);
        dst += sizeof(TeamFileEx);
    }
}

__attribute__((used, externally_visible))
void _DIY_CacheTertiaryQualifiedTeamGrid(void)
{
    _DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs();
    uint8_t *comp = diyCompetitionStart;
    int8_t gridByte = (int8_t)*(comp + 0x2CE);
    int16_t idx = gridByte;
    idx <<= 1;
    uint16_t *teamIdsTable = (uint16_t *)(comp + 0x24E);
    uint16_t teamNum = *(uint16_t *)((uint8_t *)teamIdsTable + idx);
    cachedTertiaryQualifiedTeamId = teamNum;
    uint8_t *teamData = _VerifyTeamLoaded(teamNum);
    copyTeamGridWords(teamData, tertiaryQualifiedTeamGrid, 0x155);
}

/* ================================================================
 * Group D: Title holder grid copy
 * ================================================================ */

/* copyTitleHolderGrid — shared helper for the three title holder copy functions */
static void copyTitleHolderGrid(uint16_t *titleHolderTeamId, uint8_t *titleHolderGrid)
{
    _DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs();
    uint8_t *comp = diyCompetitionStart;
    int8_t gridByte = (int8_t)*(comp + 0x2CE);
    int16_t idx = gridByte;
    idx <<= 1;
    uint16_t *teamIdsTable = (uint16_t *)(comp + 0x24E);
    uint16_t teamNum = *(uint16_t *)((uint8_t *)teamIdsTable + idx);
    *titleHolderTeamId = teamNum;
    uint8_t *teamData = _VerifyTeamLoaded(teamNum);
    copyTeamGridWords(teamData, titleHolderGrid, 0x155);
}

__attribute__((used, externally_visible))
void _Career_CopyEuroCupTeamGridAndTitleHolder(void)
{
    copyTitleHolderGrid(&euroCupTitleHolderTeamId, euroCupTitleHolderGrid);
}

__attribute__((used, externally_visible))
void _Career_CopyCupWinnersCupTeamGridAndTitleHolder(void)
{
    copyTitleHolderGrid(&cupWinnersCupTitleHolderTeamId, cupWinnersCupTitleHolderGrid);
}

__attribute__((used, externally_visible))
void _Career_CopyUefaCupTeamGridAndTitleHolder(void)
{
    copyTitleHolderGrid(&uefaCupTitleHolderTeamId, uefaCupTitleHolderGrid);
}
