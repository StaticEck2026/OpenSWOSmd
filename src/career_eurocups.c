/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* career_eurocups.c — European Cup qualification, grid caching, title holder management.
 *
 * Chunk 65: 7 exported + 12 static functions ported from SWOS2.s.
 */

#include "career_eurocups.h"
#include "game_types.h"
#include "diy_competition.h"
#include "endian_helpers.h"
#include "career_status.h"
#include "career_teams.h"
#include "panic.h"

/* ---- ASM externs (globals defined in assembly) ---- */
extern DIYCompetitionEx diyCompetitionStart;
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
extern TeamFileEx *_VerifyTeamLoaded(uint16_t teamNum);
extern void _Career_AppendTeamToTmdTeamPool(TeamFileEx *team);

/* Already-ported C functions */
extern void _DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs(void);
extern void _DIY_SortLeagueTableByStandings(void);

/* ================================================================
 * Group A: List manipulation helpers (from loc_12B1F4, loc_12B20A)
 * ================================================================ */

/* isTeamInList — checks if team number is in a word list.
 * Asm: loc_12B1F4. Returns 1 if found, 0 if not.
 *
 * The list stores team IDs as BE byte pairs (matching Amiga `move.w`
 * semantics and the static `unk1[]` seed format) so the same bytes
 * survive a careerFileBuffer save/load round-trip on either endian
 * host. `team` is a host-native value `(country<<8)|ordinal` produced
 * by BE_READ_U16, so each list slot must also be decoded with
 * BE_READ_U16 before comparison. A naive `list[i] == team` compares
 * (ordinal<<8|country) on x86 against (country<<8|ordinal), which
 * never matches and lets duplicates slip through, eventually tripping
 * the 0x3443 TMD-pool panic when the same team is appended twice. */
static int16_t isTeamInList(uint16_t team, uint8_t *listPtr, uint16_t count)
{
    for (uint16_t i = 0; i < count; i++) {
        if (BE_READ_U16(&listPtr[i * 2]) == team)
            return 1;
    }
    return 0;
}

/* appendTeamToList — appends team number to a word list and adds to TMD pool.
 * Asm: loc_12B20A. teamGrid points to the team grid data (first word = team number).
 *
 * teamGrid[0..1] are the team's `{country, ordinal}` byte pair in
 * Amiga BE order. Preserve that byte order in the list — i.e. write
 * teamGrid[0] then teamGrid[1] — so the list ends up as a sequence of
 * BE byte pairs matching the static `unk1[]` seed format. The Amiga's
 * `move.w (a5),(a2)+` does exactly this on a BE host; on x86 we have
 * to do it explicitly because a host-native uint16 store would write
 * the bytes in LE order, and downstream consumers (the spec reader's
 * BE_READ_U16, isTeamInList, drawRandomTeamFromPoolAndCopyGrid, and
 * the careerFileBuffer save/load round-trip) all expect BE bytes. */
static void appendTeamToList(uint8_t *teamGrid, uint8_t **writePtrAddr, uint16_t *count)
{
    uint8_t *writePos = *writePtrAddr;
    writePos[0] = teamGrid[0];
    writePos[1] = teamGrid[1];
    *writePtrAddr = writePos + 2;
    (*count)++;
    _Career_AppendTeamToTmdTeamPool((TeamFileEx *)teamGrid);
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
    /* See note on BE_READ_U16 in qualifyTeamForCupWinnersCup. */
    uint16_t team = BE_READ_U16(a5);
    clearEuropeanCupTitleHolderIfMatchTeam(team);
    appendTeamToEuroCupList(a5);
}

/* qualifyTeamForCupWinnersCup — if competition is eligible, find a non-primary,
 * non-Euro-Cup-holder team from the grid for the Cup Winners' Cup.
 * Asm: Career_QualifyTeamForCupWinnersCup_IfEligibleCompetition.
 *
 * Team identity is read from the grid record's first two bytes
 * (countryNumber, teamOrdinal). On Amiga BE `move.w (a5)` produced
 * `(country<<8)|ordinal` — the same BE-packed form used by
 * cachedPrimaryQualifiedTeamIds and euroCupTitleHolderTeamId. On LE
 * x86 a raw `*(uint16_t *)a5` byte-swaps it, the duplicate-detection
 * never matches, and the function picks a slot that's already in the
 * TMD pool from Euro Cup processing — tripping
 * _Career_AppendTeamToTmdTeamPool's 0x3443 panic. Use BE_READ_U16. */
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
        uint16_t team = BE_READ_U16(a5);
        if (team == cachedPrimaryQualifiedTeamIds[0])
            continue;
        if (team == euroCupTitleHolderTeamId)
            continue;
        break;
    }
    uint16_t team = BE_READ_U16(a5);
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
            /* See note on BE_READ_U16 in qualifyTeamForCupWinnersCup. */
            uint16_t team = BE_READ_U16(a5);
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
        uint16_t team = BE_READ_U16(a5);
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
    DIYCompetitionEx *comp = &diyCompetitionStart;
    uint16_t *leaguePositions = comp->matchData.league.teamEntryOffsets;
    uint16_t *teamIdsTable = comp->matchData.league.leagueTeamIds;
    uint16_t count = 9;
    uint16_t numTeams = comp->base.numTeams;
    if (count > numTeams)
        count = numTeams;
    /* asm: subq.w #1,d2 then dbf → loops count times */
    for (int16_t i = (int16_t)(count - 1); i >= 0; i--) {
        int16_t posIdx = (int16_t)*leaguePositions++;
        /* ext.l d0 then divu.w #$12,d0: sign-extend to 32-bit then unsigned div by 18 */
        uint16_t slotIdx = (uint16_t)((uint32_t)(int32_t)posIdx / 18);
        slotIdx <<= 1;
        /* leagueTeamIds is BE-packed per 4e5f378. */
        uint16_t teamNum = BE_READ_U16((uint8_t *)teamIdsTable + slotIdx);
        *cachedIds++ = teamNum;
        TeamFileEx *teamData = _VerifyTeamLoaded(teamNum);
        copyTeamGridWords((uint8_t *)teamData, dst, 0x155);
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
    uint8_t *compRaw = (uint8_t *)&diyCompetitionStart;
    uint8_t *grid = compRaw + 0x2CE;
    uint16_t *teamIdsTable = (uint16_t *)(compRaw + 0x24E);

    for (int16_t i = 2; i >= 0; i--) {
        int8_t gridByte = (int8_t)*grid++;
        int16_t idx = gridByte;
        idx <<= 1;
        /* cupTeamIds (compRaw+0x24E) BE-packed — see copyTitleHolderGrid. */
        uint16_t teamNum = BE_READ_U16((uint8_t *)teamIdsTable + idx);
        *cachedIds++ = teamNum;
        TeamFileEx *teamData = _VerifyTeamLoaded(teamNum);
        copyTeamGridWords((uint8_t *)teamData, dst, 0x155);
        dst += sizeof(TeamFileEx);
    }
}

__attribute__((used, externally_visible))
void _DIY_CacheTertiaryQualifiedTeamGrid(void)
{
    _DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs();
    uint8_t *compRaw = (uint8_t *)&diyCompetitionStart;
    int8_t gridByte = (int8_t)*(compRaw + 0x2CE);
    int16_t idx = gridByte;
    idx <<= 1;
    uint16_t *teamIdsTable = (uint16_t *)(compRaw + 0x24E);
    /* cupTeamIds (compRaw+0x24E) BE-packed — see copyTitleHolderGrid. */
    uint16_t teamNum = BE_READ_U16((uint8_t *)teamIdsTable + idx);
    cachedTertiaryQualifiedTeamId = teamNum;
    TeamFileEx *teamData = _VerifyTeamLoaded(teamNum);
    copyTeamGridWords((uint8_t *)teamData, tertiaryQualifiedTeamGrid, 0x155);
}

/* ================================================================
 * Group D: Title holder grid copy
 * ================================================================ */

/* copyTitleHolderGrid — shared helper for the three title holder copy functions */
static void copyTitleHolderGrid(uint16_t *titleHolderTeamId, uint8_t *titleHolderGrid)
{
    _DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs();
    uint8_t *compRaw = (uint8_t *)&diyCompetitionStart;
    int8_t gridByte = (int8_t)*(compRaw + 0x2CE);
    int16_t idx = gridByte;
    idx <<= 1;
    uint16_t *teamIdsTable = (uint16_t *)(compRaw + 0x24E);
    /* compRaw+0x24E is diyCompetitionStart.cupTeamIds — BE-packed
     * `{country, ordinal}` byte pairs per the 4e5f378 convention.
     * Read with BE_READ_U16 so titleHolderTeamId holds the packed
     * value the rest of the code (qualifyTeamForCupWinnersCup /
     * qualifyTeamsForUefaCup comparisons, _VerifyTeamLoaded) expects. */
    uint16_t teamNum = BE_READ_U16((uint8_t *)teamIdsTable + idx);
    *titleHolderTeamId = teamNum;
    TeamFileEx *teamData = _VerifyTeamLoaded(teamNum);
    copyTeamGridWords((uint8_t *)teamData, titleHolderGrid, 0x155);
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

/* ---- Chunk 181: Euro Cup Slot Update functions ---- */

extern uint8_t euroCupCopy[];
extern uint8_t cupWinnersCupCopy[];
extern uint8_t uefaCupCopy[];
extern uint32_t quaternaryCompetitionPtr;

extern void _SaveSelectedTeams(void);
extern void _RestoreSelectedTeams(void);
extern void _DIY_EditCompetitionSlot(uint16_t paramA, uint16_t paramB, uint16_t paramC);
extern void _ShowDIYCupMenuWithParams(uint16_t arg0, uint16_t arg1, uint16_t arg2);

/* Career_UpdateEuroCupCompetitionSlot (SWOS2.s line 31764)
 * If euroCupCopy is the active quaternary competition, edit it interactively;
 * otherwise auto-advance it as a league competition and copy the grid. */
__attribute__((used, externally_visible))
void _Career_UpdateEuroCupCompetitionSlot(void)
{
    uint8_t *a4 = euroCupCopy;
    if ((uint32_t)a4 == quaternaryCompetitionPtr) {
        _RestoreSelectedTeams();
        _Career_CopyEuroCupsCompetitionContextToOrFromActive();
        _DIY_EditCompetitionSlot(1, 1, 0);
        _Career_CopyEuroCupTeamGridAndTitleHolder();
        _SaveSelectedTeams();
    } else {
        _Career_AutoAdvanceLeagueCompetitionFromRecord(a4);
        _Career_CopyEuroCupTeamGridAndTitleHolder();
    }
}

/* Career_UpdateCupWinnersCupCompetitionSlot (SWOS2.s line 31792)
 * Same pattern but for Cup Winners' Cup — uses ShowDIYCupMenuWithParams instead of EditSlot. */
__attribute__((used, externally_visible))
void _Career_UpdateCupWinnersCupCompetitionSlot(void)
{
    uint8_t *a4 = cupWinnersCupCopy;
    if ((uint32_t)a4 == quaternaryCompetitionPtr) {
        _RestoreSelectedTeams();
        _Career_CopyEuroCupsCompetitionContextToOrFromActive();
        _ShowDIYCupMenuWithParams(1, 1, 0);
        _Career_CopyCupWinnersCupTeamGridAndTitleHolder();
        _SaveSelectedTeams();
    } else {
        _Career_AutoAdvanceCupCompetitionFromRecord(a4);
        _Career_CopyCupWinnersCupTeamGridAndTitleHolder();
    }
}

/* Career_UpdateUefaCupCompetitionSlot (SWOS2.s line 31820)
 * Same pattern for UEFA Cup. */
__attribute__((used, externally_visible))
void _Career_UpdateUefaCupCompetitionSlot(void)
{
    uint8_t *a4 = uefaCupCopy;
    if ((uint32_t)a4 == quaternaryCompetitionPtr) {
        _RestoreSelectedTeams();
        _Career_CopyEuroCupsCompetitionContextToOrFromActive();
        _ShowDIYCupMenuWithParams(1, 1, 0);
        _Career_CopyUefaCupTeamGridAndTitleHolder();
        _SaveSelectedTeams();
    } else {
        _Career_AutoAdvanceCupCompetitionFromRecord(a4);
        _Career_CopyUefaCupTeamGridAndTitleHolder();
    }
}

/* ---- Chunk 183: European Cup Team Draw ---- */

extern uint16_t teamNoOfferAccepted;
extern uint16_t careerFileBuffer;
extern uint16_t panicReasonCode;

extern uint16_t _Rand2(void);
extern void _Career_RemoveTeamFromTmdTeamPool(uint16_t teamNum);
extern void _Career_AppendTeamToTmdTeamPool(TeamFileEx *grid);
extern void _PanicHaltLoop(void);

/* Career_DrawRandomTeamFromPoolAndCopyGrid (SWOS2.s line 31528)
 * Given a title holder team ID (d2), a destination grid (a3), a team list (a2),
 * and a count (d3), randomly select a team from the pool that isn't from the
 * same country as the title holder or the player's team, then swap it into
 * the title holder slot and update the TMD team pool. */
static void drawRandomTeamFromPoolAndCopyGrid(uint16_t titleHolder,
                                               uint8_t *dstGrid,
                                               uint16_t *teamList,
                                               uint16_t count)
{
    if (titleHolder == 0xFFFF)
        return;

    uint8_t titleHolderCountry = (uint8_t)(titleHolder >> 8);
    /* teamList stores BE byte pairs (see appendTeamToList). Read each slot
     * with BE_READ_U16 to get the (country<<8)|ordinal value the rest of
     * the function works in. */
    uint8_t *listBytes = (uint8_t *)teamList;

    uint16_t randIdx;
    uint16_t selectedTeam;
    uint8_t selectedCountry;

    for (;;) {
        randIdx = _Rand2();
        randIdx = (uint16_t)((uint32_t)randIdx * count >> 8);
        selectedTeam = BE_READ_U16(&listBytes[randIdx * 2]);
        selectedCountry = (uint8_t)(selectedTeam >> 8);

        if (selectedCountry == titleHolderCountry)
            continue;

        if (teamNoOfferAccepted != 0xFFFF) {
            if (selectedCountry == (uint8_t)(teamNoOfferAccepted >> 8))
                continue;
        } else {
            if (selectedCountry == (uint8_t)(selTeamNumber >> 8))
                continue;
        }
        break;
    }

    /* Swap: put title holder in the selected slot, take the selected team out.
     * Write the title holder back as BE bytes to match the list's encoding. */
    uint16_t removedTeam = BE_READ_U16(&listBytes[randIdx * 2]);
    BE_WRITE_U16(&listBytes[randIdx * 2], titleHolder);

    uint16_t savedCareerFile = careerFileBuffer;
    _Career_RemoveTeamFromTmdTeamPool(removedTeam);
    _Career_AppendTeamToTmdTeamPool((TeamFileEx *)dstGrid);
    if (savedCareerFile != careerFileBuffer) {
        panicReasonCode = 0x8997;
        _PanicHaltLoop();
    }
}

/* Career_DrawEuropeanCupTeamsAndCopyGrids (SWOS2.s line 31510)
 * Draws random title holder replacement teams for all three European cups. */
__attribute__((used, externally_visible))
void _Career_DrawEuropeanCupTeamsAndCopyGrids(void)
{
    drawRandomTeamFromPoolAndCopyGrid(
        euroCupTitleHolderTeamId, euroCupTitleHolderGrid,
        (uint16_t *)euroCupTeamListPtr, euroCupQualifiedTeamCount);

    drawRandomTeamFromPoolAndCopyGrid(
        cupWinnersCupTitleHolderTeamId, cupWinnersCupTitleHolderGrid,
        (uint16_t *)cupWinnersCupTeamListPtr, cupWinnersCupQualifiedTeamCount);

    drawRandomTeamFromPoolAndCopyGrid(
        uefaCupTitleHolderTeamId, uefaCupTitleHolderGrid,
        (uint16_t *)uefaCupTeamListPtr, uefaCupQualifiedTeamCount);
}

/* ---- Chunk 184: European Championship + World Cup Competition Updates ---- */

extern uint32_t currentCompetitionPtr;
extern uint8_t someTeamFlags;
extern uint16_t euroChampionshipQualTeam;
extern uint8_t europeanChampionships2Copy[];
extern uint8_t ecQualifyingGroup1Copy[];
extern uint8_t ecQualifyingGroup2Copy[];
extern uint8_t ecQualifyingGroup3Copy[];
extern uint8_t ecQualifyingGroup4Copy[];
extern uint8_t ecQualifyingGroup5Copy[];
extern uint8_t ecQualifyingGroup6Copy[];
extern uint8_t ecQualifyingGroup7Copy[];
extern uint8_t ecQualifyingGroup8Copy[];

extern uint16_t primaryCompWinnerTeam;
extern uint16_t worldCupHostTeam;
extern uint8_t worldCup2Copy[];
extern uint8_t worldCupQualifyingGroup1Copy[];
extern uint8_t worldCupQualifyingGroup2Copy[];
extern uint8_t worldCupQualifyingGroup3Copy[];
extern uint8_t worldCupQualifyingGroup4Copy[];
extern uint8_t worldCupQualifyingGroup5Copy[];
extern uint8_t worldCupQualifyingGroup6Copy[];
extern uint8_t worldCupQualifyingGroup7Copy[];
extern uint8_t worldCupQualifyingGroup8Copy[];
extern uint8_t northAmericaWorldCupQualificationCopy[];
extern uint8_t southAmericaWorldCupQualificationCopy[];
extern uint8_t africaWorldCupQualificationCopy[];
extern uint8_t asiaWorldCupQualificationCopy[];
extern uint8_t oceaniaWorldCupQualificationCopy[];

extern void _Career_CopyCompetitionFileBufferToOrFromActive(void);
extern void _ShowDiyLeaguesTournamentMenu(uint16_t type, uint16_t viewOnly, uint16_t focusOverride);
extern void _DIY_BuildQualifiedTeamListFromSortedTable(uint16_t count, uint8_t *out);
extern void _DIY_BuildQualifiedTeamListFromDiyFileBuffer(uint16_t count, uint8_t *out);
extern void _Career_AutoAdvanceInternationalCompetitionFixtures(uint8_t *record, uint16_t index);

/* Career_UpdateEuropeanChampionshipCompetitionSlot (SWOS2.s line 31555)
 * in: competition record, count of teams to qualify, output list ptr.
 * If active competition, edit interactively; else auto-advance. */
static void updateECSlot(uint8_t *competition, uint16_t count, uint8_t *out)
{
    if ((uint32_t)competition == currentCompetitionPtr) {
        _RestoreSelectedTeams();
        _Career_CopyCompetitionFileBufferToOrFromActive();
        _ShowDiyLeaguesTournamentMenu(1, 1, 0);
        _DIY_BuildQualifiedTeamListFromSortedTable(count, out);
        _SaveSelectedTeams();
    } else {
        _Career_AutoAdvanceInternationalCompetitionFixtures(competition, 0);
        _DIY_BuildQualifiedTeamListFromSortedTable(count, out);
    }
}

/* Career_UpdateEuropeanChampionshipCompetitions (SWOS2.s line 31524) */
__attribute__((used, externally_visible))
void _Career_UpdateEuropeanChampionshipCompetitions(void)
{
    if (!(someTeamFlags & 0x01))
        return;

    uint8_t *a5 = europeanChampionships2Copy;
    uint8_t offset = a5[7];
    a5 = a5 + 7 + offset;
    /* Asm `move.w euroChampionshipQualTeam,(a5)+` writes BE on m68k.
     * Use BE_WRITE_U16 so the destination byte order matches the rest
     * of europeanChampionships2Copy.unk1 (BE-packed `{country, ordinal}`
     * per the 4e5f378 convention). */
    BE_WRITE_U16(a5, euroChampionshipQualTeam);
    a5 += 2;

    /* a5 is preserved across slot calls (asm bridge saves/restores a2-a6).
     * All groups write qualified team IDs to the same output position. */
    updateECSlot(ecQualifyingGroup1Copy, 2, a5);
    updateECSlot(ecQualifyingGroup2Copy, 2, a5);
    updateECSlot(ecQualifyingGroup3Copy, 2, a5);
    updateECSlot(ecQualifyingGroup4Copy, 2, a5);
    updateECSlot(ecQualifyingGroup5Copy, 2, a5);
    updateECSlot(ecQualifyingGroup6Copy, 2, a5);
    updateECSlot(ecQualifyingGroup7Copy, 2, a5);
    updateECSlot(ecQualifyingGroup8Copy, 1, a5);
}

/* Career_UpdateWorldCupCompetitionSlot (SWOS2.s line 31638)
 * More complex: handles type 0 (league), type 2 (cup), and error cases. */
static void updateWCSlot(uint8_t *competition, uint16_t count, uint8_t *out)
{
    if ((uint32_t)competition == currentCompetitionPtr) {
        _RestoreSelectedTeams();
        if (competition[1] == 2) {
            _Career_CopyCompetitionFileBufferToOrFromActive();
            _DIY_EditCompetitionSlot(1, 1, 0);
            _DIY_BuildQualifiedTeamListFromDiyFileBuffer(count, out);
            _SaveSelectedTeams();
        } else if (competition[1] == 0) {
            _Career_CopyCompetitionFileBufferToOrFromActive();
            _ShowDiyLeaguesTournamentMenu(1, 1, 0);
            _DIY_BuildQualifiedTeamListFromSortedTable(count, out);
            _SaveSelectedTeams();
        } else {
            panicReasonCode = 0x1991;
            _PanicHaltLoop();
        }
    } else {
        if (competition[1] == 2) {
            _Career_AutoAdvanceLeagueCompetitionFromRecord(competition);
            _DIY_BuildQualifiedTeamListFromDiyFileBuffer(count, out);
        } else if (competition[1] == 0) {
            _Career_AutoAdvanceInternationalCompetitionFixtures(competition, 0);
            _DIY_BuildQualifiedTeamListFromSortedTable(count, out);
        } else {
            panicReasonCode = 0x1992;
            _PanicHaltLoop();
        }
    }
}

/* Career_UpdateWorldCupCompetitions (SWOS2.s line 31589) */
__attribute__((used, externally_visible))
void _Career_UpdateWorldCupCompetitions(void)
{
    if (someTeamFlags & 0x80)
        worldCupHostTeam = primaryCompWinnerTeam;

    if (!(someTeamFlags & 0x40))
        return;

    uint8_t *a5 = worldCup2Copy;
    uint8_t offset = a5[7];
    a5 = a5 + 7 + offset;
    /* BE write — matches the eurochampionship variant above and the
     * BE-packed convention for *.unk1 team-id lists (4e5f378). */
    BE_WRITE_U16(a5, worldCupHostTeam);
    a5 += 2;

    /* a5 is preserved across slot calls (asm bridge saves/restores a2-a6) */
    updateWCSlot(worldCupQualifyingGroup1Copy, 2, a5);
    updateWCSlot(worldCupQualifyingGroup2Copy, 2, a5);
    updateWCSlot(worldCupQualifyingGroup3Copy, 2, a5);
    updateWCSlot(worldCupQualifyingGroup4Copy, 2, a5);
    updateWCSlot(worldCupQualifyingGroup5Copy, 2, a5);
    updateWCSlot(worldCupQualifyingGroup6Copy, 2, a5);
    updateWCSlot(worldCupQualifyingGroup7Copy, 2, a5);
    updateWCSlot(worldCupQualifyingGroup8Copy, 2, a5);
    updateWCSlot(northAmericaWorldCupQualificationCopy, 3, a5);
    updateWCSlot(southAmericaWorldCupQualificationCopy, 4, a5);
    updateWCSlot(africaWorldCupQualificationCopy, 5, a5);
    updateWCSlot(asiaWorldCupQualificationCopy, 2, a5);
    updateWCSlot(oceaniaWorldCupQualificationCopy, 1, a5);
}
