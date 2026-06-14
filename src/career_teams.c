/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* career_teams.c — Team cache lookup, save/restore selected teams,
 *                   career competition team selection.
 *
 * Chunk 66 (partial): 4 exported + 1 static function ported from SWOS2.s.
 * Chunk 90: Career competition team selection (12 functions).
 */

#include "career_teams.h"
#include "endian_helpers.h"
#include "game_types.h"
#include "auto_skip_debug.h"
#include "panic.h"
#include "teams.h"

#include "diy_competition.h"
#include "file_io.h"

/* ---- Constants ---- */
#define SAVE_SIZE                   0x5582  /* bytes to save: numSelectedTeams + team data */

/* ---- ASM externs ---- */
extern uint16_t currentTeamNumber;
extern uint8_t currentTeam[];
extern uint16_t careerAutoAdvanceInProgress;
extern TeamFileEx careerTeam;
extern uint16_t selTeamNumber;
extern uint16_t numSelectedTeams;
extern uint8_t selTeamsPtr;
extern uint8_t teamSelectionKeyCache[];
extern uint16_t teamSelectionKeyCacheCount;
extern uint8_t teamSelectionKeyCacheEnd[];
extern uint16_t panicReasonCode;
extern uint16_t careerFileBuffer;     /* count of teams in TMD pool */
extern uint8_t tmdFileBuffer;         /* start of TMD team pool data (array of TeamFileEx records) */
extern uint8_t teamFileBuffer;        /* team file buffer: first word is count, then records */
extern uintptr_t teamSortBuffer[];      /* 64-entry pointer/ID temp buffer for sorting; sized for 64-bit pointers. */
extern uint8_t lastLoadedExtNumber;
extern uint8_t teamIdSelectionProcessedFlags[]; /* 64-byte flags buffer */
extern DIYTournamentState diyFileBufferCopy;  /* tournament working buffer (teamIds field was diyTournamentTeamIds) */
/* diyLeagueTeamIds/diyCupTeamIds aliases retired -> typed diyCompetitionStart
 * field access; file-scope extern here covers the early functions (the other
 * decls at ~1255/2652 are after these uses). */
extern DIYCompetitionEx diyCompetitionStart;
extern uint16_t careerLoadedFlag;
extern uint16_t teamsLoaded;
extern uint16_t skipLeagueNumbers;
extern uint16_t gameType;
extern uint8_t euroCup[];
extern uint8_t cupWinnersCup[];
extern uint8_t uefaCup[];
extern uint8_t careerRegionPickWeightsTable[];
extern uint8_t careerRegionPickWeightsTable2[];
extern uint8_t aDataEurocupTmd[];     /* "data/eurocup.tmd" */
extern uint8_t aDataEurocwcTmd[];     /* "data/eurocwc.tmd" */
extern uint8_t aDataUefacupTmd[];     /* "data/uefacup.tmd" */

/* Globals for NationalTeamAddPlayer callbacks (Chunk 108) */
extern uint16_t careerNationalTeamAddPlayerType;
extern void _SetExitMenuFlag(void);

/* ================================================================
 * findTeamInCache — look up a team number in the team cache.
 * Asm: FindTeamInCache
 *
 * Returns: pointer to team data via *teamOut,
 *          index into selTeamsPtr via *indexOut,
 *          0 if found, 1 if not found.
 * ================================================================ */
static int16_t findTeamInCache(uint16_t teamNum, TeamFileEx **teamOut, uint16_t *indexOut)
{
    /* Check currentTeam first */
    if (teamNum == currentTeamNumber) {
        *teamOut = (TeamFileEx *)currentTeam;
        *indexOut = 0;
        return 0;
    }

    /* Check careerTeam (when auto-advance in progress or matches selTeamNumber).
     * careerTeam.team's first two bytes are the {countryNumber, teamOrdinal}
     * byte pair from the loaded TMD record. teamNum is BE-packed
     * (country<<8 | ordinal) like selTeamNumber, so use PACK_BE_U16 — a raw
     * *(uint16_t *)& would read the bytes swapped on LE and never match. */
    if (careerAutoAdvanceInProgress) {
        if (teamNum == PACK_BE_U16(careerTeam.team.countryNumber,
                                   careerTeam.team.teamOrdinal)) {
            *teamOut = &careerTeam;
            *indexOut = 0;
            return 0;
        }
    }
    if (teamNum == selTeamNumber) {
        *teamOut = &careerTeam;
        *indexOut = 0;
        return 0;
    }

    /* Search selTeamsPtr array */
    uint16_t count = numSelectedTeams;
    if (count == 0)
        return 1;

    TeamFileEx *ptr = (TeamFileEx *)&selTeamsPtr;
    for (uint16_t i = 0; i < count; i++) {
        if (PACK_BE_U16(ptr->team.countryNumber, ptr->team.teamOrdinal) == teamNum) {
            *teamOut = ptr;
            *indexOut = i;
            return 0;
        }
        ptr++;
    }
    return 1;
}

/* ================================================================
 * _VerifyTeamLoaded — look up team, panic if not found.
 * Asm: VerifyTeamLoaded
 *
 * Returns pointer to team data.
 * ================================================================ */
__attribute__((used, externally_visible))
TeamFileEx *_VerifyTeamLoaded(uint16_t teamNum)
{
    TeamFileEx *team;
    uint16_t idx;
    if (findTeamInCache(teamNum, &team, &idx) != 0) {
        /* Always dump context on panic — we're crashing anyway and the
         * cache contents tell us whether the caller trusted a stale
         * teamNum or whether numSelectedTeams is wrong. */
        extern uint8_t selTeamsPtr;
        extern uint16_t numSelectedTeams;
        extern uint16_t selTeamNumber;
        SWOS_DBG(
                "[verify] _VerifyTeamLoaded(0x%04X) FAIL: country=0x%02X team=0x%02X "
                "selTeamNumber=0x%04X numSelectedTeams=%u\n",
                (unsigned)teamNum,
                (unsigned)((teamNum >> 8) & 0xFF), (unsigned)(teamNum & 0xFF),
                (unsigned)selTeamNumber, (unsigned)numSelectedTeams);
        TeamFileEx *firstFew = (TeamFileEx *)&selTeamsPtr;
        unsigned dumpN = numSelectedTeams;
        if (dumpN > 16) dumpN = 16;
        for (unsigned i = 0; i < dumpN; i++) {
            SWOS_DBG(
                    "[verify]   selTeamsPtr[%u] country=0x%02X ordinal=0x%02X\n",
                    i,
                    (unsigned)firstFew[i].team.countryNumber,
                    (unsigned)firstFew[i].team.teamOrdinal);
        }
        SWOS_DBG("[verify] diyFileBufferCopy.teamIds[0..15] =");
        for (unsigned i = 0; i < 16; i++)
            SWOS_DBG(" %04X", (unsigned)diyFileBufferCopy.teamIds[i]);
        SWOS_DBG("\n");
        SWOS_DBG_FLUSH();
        _PanicHaltLoop();
    }
    return team;
}

/* ================================================================
 * _GetTeamPointer — look up a team by its ID.
 * Asm: GetTeamPointer (d0=teamId → a0=teamPtr)
 *
 * Search order: currentTeam → careerTeam → selTeamsPtr → TMD pool → team file.
 * If loaded from team file, applies foreign market player patches.
 * ================================================================ */
extern void _Career_PatchTeamWithForeignMarketPlayers(TeamFileEx *teamPtr);
static TeamFileEx *findTeamInTmdBuffer(uint16_t teamId);

__attribute__((used, externally_visible))
TeamFileEx *_GetTeamPointer(uint16_t teamId)
{
    TeamFileEx *teamOut;
    uint16_t indexOut;

    /* Check cached teams (currentTeam, careerTeam, selTeamsPtr) */
    if (findTeamInCache(teamId, &teamOut, &indexOut) == 0)
        return teamOut;

    /* Check TMD file (career team pool) */
    TeamFileEx *tmd = findTeamInTmdBuffer(teamId);
    if (tmd)
        return tmd;

    /* Load from team file and scan for matching team ID.
     * teamFileBuffer contains TeamFileEx-sized records (684 bytes each).
     * teamId is packed (countryNumber<<8 | teamOrdinal), matching the
     * first word of each record. */
    _LoadTeamFile((uint16_t)(teamId >> 8));
    TeamFileEx *ptr = (TeamFileEx *)((uint8_t *)(&teamFileBuffer) + 2);  /* skip 2-byte count header */
    while (PACK_BE_U16(ptr->team.countryNumber, ptr->team.teamOrdinal) != teamId)
        ptr++;

    /* Apply foreign market player patches */
    _Career_PatchTeamWithForeignMarketPlayers(ptr);
    return ptr;
}

/* ================================================================
 * _CopySelectedTeamNumbers — copy first word of each team record.
 * Asm: CopySelectedTeamNumbers (d0=count, a0=dest)
 *
 * Copies the team number (first word) from each selected team
 * record to the destination buffer, with stride sizeof(TeamFileEx).
 * ================================================================ */
__attribute__((used, externally_visible))
void _CopySelectedTeamNumbers(uint16_t count, uint8_t *dest)
{
    /* Each TeamFileEx in the selTeamsPtr buffer starts with (countryNumber,
     * teamOrdinal) as two adjacent bytes. The Amiga m68k `move.w (a0)` read
     * those as a single big-endian word, so the team id is
     * `(countryNumber << 8) | teamOrdinal`. A naive `*(uint16_t *)src` on a
     * little-endian SDL host swaps the bytes — an Italian team
     * (countryNumber=0x0E, teamOrdinal=0x2C, real id 0x0E2C / 3628) reads
     * back as 0x2C0E (11278) and _VerifyTeamLoaded panics on the bogus id.
     * Use PACK_BE_U16 so both builds produce the BE-flavour id every other
     * site (findTeamInCache / _VerifyTeamLoaded etc.) compares against. */
    volatile uint8_t *src = (volatile uint8_t *)&selTeamsPtr;
    uint8_t *dst = dest;
    for (int16_t i = (int16_t)(count - 1); i >= 0; i--) {
        /* Asm `move.w (a0),(a1)+` preserves the BE `{country, ordinal}`
         * byte pair to dst on m68k. Use BE_WRITE_U16 so dst ends up
         * with the same in-memory layout on LE — matching the
         * BE-packed convention the diyCompetitionStart.matchData.league.leagueTeamIds / diyCompetitionStart.cupTeamIds /
         * diyFileBufferCopy.teamIds arrays use and that
         * _DIY_ComputeAverageTeamStrengthTier consumes via BE_READ_U16. */
        BE_WRITE_U16(dst, PACK_BE_U16(src[0], src[1]));
        dst += 2;
        src += sizeof(TeamFileEx);
    }
}

/* ================================================================
 * _SaveSelectedTeams / _RestoreSelectedTeams
 * Asm: SaveSelectedTeams / RestoreSelectedTeams
 *
 * Save: copies 0x5582 bytes from numSelectedTeams to backup buffer,
 *        then clears numSelectedTeams.
 * Restore: validates numSelectedTeams <= 32, then copies back.
 *
 * The backup buffer replaces the original hard-coded address $C9BBC.
 * ================================================================ */
static uint8_t savedTeamsBackupBuffer[SAVE_SIZE];

/* numSelectedTeams (2 B) and the selectedTeams[32] array (SAVE_SIZE-2 =
 * 32*684 = 21888 B at &selTeamsPtr) are backed up / restored as TWO independent
 * runs, so neither relies on the array sitting immediately after numSelectedTeams
 * in BSS. The array is being lifted out of the careerFileBuffer overlay into a
 * typed carrier (see docs/SELECTED_TEAMS_UNTANGLE.md); decoupling here is
 * byte-for-byte identical to the old single 0x5582 copy from &numSelectedTeams
 * while the two are still adjacent, and stays correct once the array moves. */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _SaveSelectedTeams(void)
{
    volatile uint16_t *dst = (volatile uint16_t *)savedTeamsBackupBuffer;
    *dst++ = numSelectedTeams;                              /* count -> backup[0] */
    volatile uint16_t *src = (volatile uint16_t *)&selTeamsPtr;
    uint32_t remaining = SAVE_SIZE - 2;                    /* 21888 = 32 * 684 */
    while (remaining) {
        *dst++ = *src++;
        remaining -= 2;
    }
    numSelectedTeams = 0;
}

__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _RestoreSelectedTeams(void)
{
    if (numSelectedTeams > 32) {
        _PanicHaltLoop();
    }
    volatile uint16_t *src = (volatile uint16_t *)savedTeamsBackupBuffer;
    numSelectedTeams = *src++;                              /* count from backup[0] */
    volatile uint16_t *dst = (volatile uint16_t *)&selTeamsPtr;
    uint32_t remaining = SAVE_SIZE - 2;
    while (remaining) {
        *dst++ = *src++;
        remaining -= 2;
    }
}

/* ================================================================
 * TeamSelectionKeyCache_InsertSpaceSorted  (SWOS2.s line 35159)
 *
 * Inserts space into the team selection key cache at the sorted
 * position for the given key byte. The cache is a variable-length
 * list of entries where byte 1 of each entry is the sort key and
 * word at offset 2 is the entry size (stride).
 *
 * Parameters:
 *   key            — the sort key (byte) to find insertion point for
 *   insertionBytes — number of bytes to insert (shift existing data)
 *
 * If cache count is 0, returns immediately (nothing to shift).
 * Panics if the insertion would exceed teamSelectionKeyCacheEnd.
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_TeamSelectionKeyCache_InsertSpaceSorted(uint16_t key, uint16_t insertionBytes)
{
    uint8_t *ptr = teamSelectionKeyCache;
    int16_t count = (int16_t)teamSelectionKeyCacheCount;

    if (count == 0)
        return ptr;

    count--;

    /* Walk entries until we find one with sort key > our key (bcs = d0 < mem) */
    while (count >= 0) {
        if ((uint8_t)key < ((TeamSelectionKeyCacheEntry *)ptr)->sortKey)
            goto found_insert_point;
        ptr += ((TeamSelectionKeyCacheEntry *)ptr)->entryStride;  /* advance by entry stride */
        count--;
    }

    /* Walked all entries without finding insert point — check bounds */
    if ((uintptr_t)ptr > (uintptr_t)teamSelectionKeyCacheEnd)
        goto panic;
    return ptr;

found_insert_point:
    {
        /* Calculate end of data to shift: walk remaining entries from ptr */
        uint8_t *endPtr = ptr;
        int16_t remaining = count;
        while (remaining >= 0) {
            endPtr += ((TeamSelectionKeyCacheEntry *)endPtr)->entryStride;
            remaining--;
        }

        /* Check if shifted data would overflow */
        uint8_t *newEnd = endPtr + insertionBytes;
        if ((uintptr_t)newEnd > (uintptr_t)teamSelectionKeyCacheEnd)
            goto panic;

        /* Shift data upward by insertionBytes (from end to start, word-at-a-time) */
        uint32_t shiftBytes = (uintptr_t)endPtr - (uintptr_t)ptr;
        uint16_t numWords = (uint16_t)(shiftBytes >> 1) - 1;
        {
            volatile uint16_t *src = (volatile uint16_t *)endPtr;
            volatile uint16_t *dst = (volatile uint16_t *)newEnd;
            int16_t i;
            for (i = (int16_t)numWords; i >= 0; i--) {
                *(--dst) = *(--src);
            }
        }
        return ptr;
    }

panic:
    panicReasonCode = 0x8889;
    _PanicHaltLoop();
    return ptr;  /* unreachable, satisfies compiler */
}

/* ================================================================
 * SearchTeamInTmdFile  (SWOS2.s line 38187)
 *
 * Linear search through the TMD file buffer (career team pool)
 * for a team with matching team number.
 *
 * Returns: 0 if found, 1 if not found.
 * (Original asm returns via Z flag: set=found, clear=not found)
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _SearchTeamInTmdFile(uint16_t teamNo)
{
    uint16_t count = careerFileBuffer;
    if (count == 0)
        return 1;

    TeamFileEx *ptr = (TeamFileEx *)&tmdFileBuffer;
    /* teamNo is the Amiga BE-packed `(country<<8)|ordinal`. Compose
     * the same value from the per-record byte pair via PACK_BE_U16
     * so the comparison matches on either host. */
    for (uint16_t i = 0; i < count; i++) {
        if (PACK_BE_U16(ptr->team.countryNumber,
                        ptr->team.teamOrdinal) == teamNo)
            return 0;  /* found */
        ptr++;
    }
    return 1;  /* not found */
}

/* ================================================================
 * Career_RemoveTeamFromTmdTeamPool  (SWOS2.s line 38156)
 *
 * Removes a team from the TMD team pool by team number.
 * Shifts remaining entries left to fill the gap.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _Career_RemoveTeamFromTmdTeamPool(uint16_t teamNo)
{
    uint16_t count = careerFileBuffer;
    if (count == 0)
        return;

    TeamFileEx *ptr = (TeamFileEx *)&tmdFileBuffer;
    /* See note in _SearchTeamInTmdFile — same BE-packed teamNo vs
     * adjacent {country, ordinal} bytes. */
    for (uint16_t i = 0; i < count; i++) {
        if (PACK_BE_U16(ptr->team.countryNumber,
                        ptr->team.teamOrdinal) == teamNo) {
            /* Found — shift remaining entries left */
            uint16_t remaining = count - 1 - i;
            if (remaining > 0) {
                volatile uint16_t *src = (volatile uint16_t *)&ptr[1];
                volatile uint16_t *dst = (volatile uint16_t *)ptr;
                uint32_t words = (uint32_t)remaining * (sizeof(TeamFileEx) / 2);
                for (uint32_t w = 0; w < words; w++) {
                    *dst++ = *src++;
                }
            }
            careerFileBuffer--;
            return;
        }
        ptr++;
    }
}

/* ================================================================
 * Career_AppendTeamToTmdTeamPool  (SWOS2.s line 38134)
 *
 * Appends a team record to the TMD team pool.
 * Panics (0x3443) if the team is already in the pool.
 * Copies sizeof(TeamFileEx) (684) bytes from team to end of pool.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _Career_AppendTeamToTmdTeamPool(TeamFileEx *team)
{
    /* Verify team is not already in pool */
    uint16_t teamNo = PACK_BE_U16(team->team.countryNumber, team->team.teamOrdinal);
    if (_SearchTeamInTmdFile(teamNo) == 0) {
        /* Already in pool — panic */
        panicReasonCode = 0x3443;
        _PanicHaltLoop();
    }

    /* Append at end of pool */
    uint16_t count = careerFileBuffer;
    TeamFileEx *dest = (TeamFileEx *)&tmdFileBuffer + count;
    volatile uint16_t *src = (volatile uint16_t *)team;
    volatile uint16_t *dst = (volatile uint16_t *)dest;
    for (int16_t i = 0x155; i >= 0; i--) {
        *dst++ = *src++;
    }
    careerFileBuffer++;
}

/* ================================================================
 * Chunk 90 — Career Competition Team Selection
 *
 * 12 functions for career mode team selection initialization:
 * sorting teams by price, loading euro cup TMD files,
 * appending teams from TMD/team files, dispatching to
 * competition-specific init, and random team picking by region.
 * ================================================================ */

/* --- Forward declarations for Chunk 90 asm functions --- */
extern uint16_t _Rand2(void);
extern int16_t _LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);
extern uint32_t _Career_IsTeamIdNotInListOrReturnSentinel(uint16_t teamId, uint16_t *endPtr);
extern uint32_t _Career_IsTeamIdNotInListOrReturnSentinel2(uint16_t teamId, uint16_t *endPtr);

/* ---- Static helpers ---- */

/* Simple found/not-found check without needing team pointer */
static int16_t isTeamInCache(uint16_t teamNum)
{
    TeamFileEx *teamOut;
    uint16_t indexOut;
    return findTeamInCache(teamNum, &teamOut, &indexOut);
}

/* Copy one team record (sizeof(TeamFileEx) bytes = 0x156 words) */
static void copyTeamRecord(uint8_t *dst, const uint8_t *src)
{
    volatile uint16_t *s = (volatile uint16_t *)src;
    volatile uint16_t *d = (volatile uint16_t *)dst;
    for (int16_t i = 0x155; i >= 0; i--)
        *d++ = *s++;
}

/* Find team record in TMD file buffer, return pointer or NULL */
static TeamFileEx *findTeamInTmdBuffer(uint16_t teamId)
{
    TeamFileEx *ptr = (TeamFileEx *)&tmdFileBuffer;
    uint16_t count = careerFileBuffer;
    for (uint16_t i = 0; i < count; i++) {
        if (PACK_BE_U16(ptr->team.countryNumber, ptr->team.teamOrdinal) == teamId)
            return ptr;
        ptr++;
    }
    return (TeamFileEx *)0;
}

/* Find team record in team file buffer by team ID. Each record's first
 * two bytes are {countryNumber, teamOrdinal}; comparing as a raw uint16
 * on LE byte-swaps relative to the BE-format teamId every other caller
 * passes in. Use BE_READ_U16 so the comparison sees the Amiga packed
 * value (country<<8 | ordinal). */
static uint8_t *findTeamInTeamFileBuffer(uint16_t teamId)
{
    TeamFileEx *ptr = (TeamFileEx *)(&teamFileBuffer + 2);
    uint16_t count = *(uint16_t *)&teamFileBuffer;
    for (uint16_t i = 0; i < count; i++) {
        if (PACK_BE_U16(ptr->team.countryNumber,
                        ptr->team.teamOrdinal) == teamId)
            return (uint8_t *)ptr;
        ptr++;
    }
    return (uint8_t *)0;
}

/* Append a team record to selection + increment counter.
 * Sets the teamControls flag byte.
 * Returns new destination pointer (advanced by sizeof(TeamFileEx)). */
static uint8_t *appendTeamToSelection(uint8_t *selDst, const uint8_t *teamSrc)
{
    copyTeamRecord(selDst, teamSrc);
    ((TeamFileEx *)selDst)->team.teamControls = 1;
    numSelectedTeams++;
    return selDst + sizeof(TeamFileEx);
}

/* ================================================================
 * Career_SortTeamFilePtrsByAvgPlayerPrice  (SWOS2.s line 97576)
 *
 * Loads team file for given ext number, builds pointer array
 * into teamSortBuffer, bubble-sorts by average player price
 * (descending), randomly swaps ~25% of adjacent pairs, then
 * copies sorted team IDs back to teamSortBuffer.
 *
 * in: extNumber = team file extension number (d0 in asm)
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_SortTeamFilePtrsByAvgPlayerPrice(uint16_t extNumber)
{
    _LoadTeamFile(extNumber);

    uint16_t count = *(uint16_t *)&teamFileBuffer;
    TeamFileEx *team = (TeamFileEx *)(&teamFileBuffer + 2);
    uintptr_t *sortBuf = teamSortBuffer;

    /* Build pointer array */
    uint16_t i;
    for (i = 0; i < count; i++) {
        sortBuf[i] = (uintptr_t)team;
        team++;
    }

    /* Bubble sort by average player price (descending: highest first) */
    int16_t pass;
    for (pass = (int16_t)(count - 2); pass >= 0; pass--) {
        uintptr_t *p = sortBuf;
        int16_t j;
        for (j = pass; j >= 0; j--) {
            uint16_t price1 = _GetAveragePlayerPrice((TeamFileEx *)p[0]);
            uint16_t price2 = _GetAveragePlayerPrice((TeamFileEx *)p[1]);
            if (price1 < price2) {
                /* bcc.s: branch if carry clear = price1 >= price2 (unsigned) */
                uintptr_t tmp = p[0];
                p[0] = p[1];
                p[1] = tmp;
            }
            p++;
        }
    }

    /* Random shuffle: ~25% chance to swap adjacent pairs */
    uintptr_t *p2 = sortBuf;
    for (pass = (int16_t)(count - 2); pass >= 0; pass--) {
        if ((_Rand2() & 3) == 0) {
            uintptr_t tmp = p2[0];
            p2[0] = p2[1];
            p2[1] = tmp;
        }
        p2++;
    }

    /* Copy sorted team IDs back to start of teamSortBuffer.
     * The asm uses a6 (teamSortBuffer) as both source (pointer array) and dest (ID array).
     * The pointers are 4 bytes each; IDs are 2 bytes. Since we write IDs sequentially and
     * read pointers sequentially, and 2 < 4, the writes don't corrupt unread pointers.
     *
     * Each TeamFileEx's leading 2 bytes are the BE-packed team id
     * `{countryByte, ordinalByte}` (offsets 0/1 of TeamFile). Asm
     * `move.w (a1),(a6)+` reads that as a single uint16 on m68k BE.
     * Host-native `*(uint16_t *)` on x86 LE flips the bytes — Playaveli's
     * GERMANY.CAR EC-qualifying-year repro then crashed in
     * VerifyTeamLoaded(0x1150) because the European national-team
     * 0x5011 came out byte-swapped here. Use BE_READ_U16 so the in-memory
     * uint16 we store is the packed-id form downstream code expects. */
    uint16_t *idBuf = (uint16_t *)teamSortBuffer;
    for (i = 0; i < count; i++) {
        idBuf[i] = BE_READ_U16((uint8_t *)sortBuf[i]);
    }
}

/* ================================================================
 * LoadSomeEuroCup  (SWOS2.s line 97638)
 *
 * Loads a European cup TMD file (eurocup, cup winners' cup, or
 * UEFA cup) into the TMD file buffer pool, appending after
 * existing teams.
 *
 * in: whichCup = pointer to euroCup/cupWinnersCup/uefaCup struct
 * ================================================================ */
__attribute__((used, externally_visible))
void _LoadSomeEuroCup(uint8_t *whichCup)
{
    uint8_t *filename;

    if (whichCup == euroCup)
        filename = aDataEurocupTmd;
    else if (whichCup == cupWinnersCup)
        filename = aDataEurocwcTmd;
    else if (whichCup == uefaCup)
        filename = aDataUefacupTmd;
    else
        return;

    /* Calculate destination offset past existing teams */
    uint16_t existingCount = careerFileBuffer;
    TeamFileEx *dest = (TeamFileEx *)&tmdFileBuffer + existingCount;

    uint32_t fileSize = 0;
    _LoadFileEx(filename, (uint8_t *)dest, &fileSize);

    uint16_t loadedCount = (uint16_t)(fileSize / sizeof(TeamFileEx));
    careerFileBuffer += loadedCount;

    _SetTeamGlobalNumbers(loadedCount, (uint8_t *)dest);
}

/* ================================================================
 * AppendTeamsToSelectionFromTmdOrTeamFile  (SWOS2.s line 97669)
 *
 * Given a list of team IDs, finds each team in the TMD cache or
 * loads it from the team file, and copies the full record into
 * the selection buffer (selTeamsPtr).
 *
 * in: out     = output for team IDs (a2 in asm)
 *     teamIds = array of team IDs to find (a6 in asm)
 *     count   = number of team IDs (d7 in asm)
 * ================================================================ */
__attribute__((used, externally_visible))
void _AppendTeamsToSelectionFromTmdOrTeamFile(uint16_t *out, uint16_t *teamIds, uint16_t count)
{
    uint8_t *flags = teamIdSelectionProcessedFlags;

    /* The teamIds array passed in points at packed `{country, ordinal}`
     * byte pairs from the static competition data (e.g.
     * europeanChampionships.unk1). On Amiga BE, dereferencing as uint16_t
     * gives `(country << 8) | ordinal` — the value the rest of the team
     * lookup code expects (matches `PACK_BE_U16(countryNumber,
     * teamOrdinal)` in findTeamInCache, the byte order in the team file
     * record at offset 0..1, etc.). On x86 LE the raw uint16_t read
     * comes out byte-swapped, so `teamId >> 8` returns the ordinal
     * instead of the country, _LoadTeamFile loads the wrong (or panics
     * on a missing) file, findTeamInTeamFileBuffer never matches, and
     * the for(;;) loop spins forever — exactly the EUROPEAN
     * CHAMPIONSHIPS infinite-loop symptom. Decode each entry once via
     * BE_READ_U16 into a local LE-native buffer and use that throughout. */
    uint16_t teamIdsLE[64];   /* SWOS competitions never exceed 64 teams */
    if (count > 64) count = 64;
    for (uint16_t k = 0; k < count; k++)
        teamIdsLE[k] = BE_READ_U16(&teamIds[k]);

#ifdef SWOS_DEBUG_AUTOSKIP
    SWOS_DBG("[append] count=%u, requested teamIds:\n", (unsigned)count);
    for (uint16_t k = 0; k < count; k++) {
        SWOS_DBG("[append]   in[%2u] = 0x%04X (country=%u ordinal=%u)\n",
                (unsigned)k, (unsigned)teamIdsLE[k],
                (unsigned)((teamIdsLE[k] >> 8) & 0xFF),
                (unsigned)(teamIdsLE[k] & 0xFF));
    }
#endif

    /* Phase 1: Pre-scan — mark teams already loaded in cache. */
    uint16_t i;
    TeamFileEx *tmpTeam;
    uint16_t tmpIdx;
    for (i = 0; i < count; i++) {
        flags[i] = 0;
        if (findTeamInCache(teamIdsLE[i], &tmpTeam, &tmpIdx) == 0)
            flags[i] = 1;
    }

    /* Phase 2: Process remaining teams */
    TeamFileEx *selDst = (TeamFileEx *)&selTeamsPtr + numSelectedTeams;

    for (;;) {
        int16_t foundIdx = -1;
        for (i = 0; i < count; i++) {
            if (flags[i] == 0) { foundIdx = (int16_t)i; break; }
        }
        if (foundIdx < 0)
            break;

        uint16_t teamId = teamIdsLE[foundIdx];

        TeamFileEx *tmdTeam = findTeamInTmdBuffer(teamId);
        if (tmdTeam) {
            copyTeamRecord((uint8_t *)selDst, (uint8_t *)tmdTeam);
            selDst->team.teamControls = 1;
            flags[foundIdx] = 1;
            numSelectedTeams++;
            selDst++;
            continue;
        }

        uint16_t extNum = teamId >> 8;
        _LoadTeamFile(extNum);
        uint8_t loadedExt = lastLoadedExtNumber;

#ifdef SWOS_DEBUG_AUTOSKIP
        SWOS_DBG("[append] loaded TEAM.%03u (ext=%u) for teamId=0x%04X\n",
                (unsigned)loadedExt, (unsigned)loadedExt, (unsigned)teamId);
#endif

        for (i = 0; i < count; i++) {
            if (flags[i] != 0) continue;
            if ((uint8_t)(teamIdsLE[i] >> 8) != loadedExt) continue;

            uint8_t *tfTeam = findTeamInTeamFileBuffer(teamIdsLE[i]);
#ifdef SWOS_DEBUG_AUTOSKIP
            if (tfTeam) {
                SWOS_DBG("[append]   resolved 0x%04X -> '%.16s'\n",
                        (unsigned)teamIdsLE[i],
                        (const char *)((TeamFileEx *)tfTeam)->team.teamName);
            }
#endif
            if (!tfTeam) {
                /* Always log misses — silent drops are how we got the
                 * "9 of 16 teams loaded" panic for the eurocup advance.
                 * Include the file's team count and first/last ordinals
                 * so we can tell whether the team is genuinely absent
                 * (out-of-range ordinal written by qualifier) or whether
                 * the file lookup is bogus. */
                uint16_t tfCount = *(uint16_t *)&teamFileBuffer;
                TeamFileEx *tfFirst = (TeamFileEx *)(&teamFileBuffer + 2);
                TeamFileEx *tfLast = (tfCount > 0) ? &tfFirst[tfCount - 1] : tfFirst;
                SWOS_DBG(
                        "[append]   NO MATCH for 0x%04X in TEAM.%03u "
                        "(file has %u teams; first=0x%02X%02X last=0x%02X%02X)\n",
                        (unsigned)teamIdsLE[i], (unsigned)loadedExt,
                        (unsigned)tfCount,
                        (unsigned)tfFirst->team.countryNumber,
                        (unsigned)tfFirst->team.teamOrdinal,
                        (unsigned)tfLast->team.countryNumber,
                        (unsigned)tfLast->team.teamOrdinal);
                SWOS_DBG_FLUSH();
            }
            if (tfTeam) {
                copyTeamRecord((uint8_t *)selDst, tfTeam);
                selDst->team.teamControls = 1;
                flags[i] = 1;
                numSelectedTeams++;
                selDst++;
            } else {
                /* CRITICAL: leaving flags[i]=0 when the lookup failed
                 * makes the outer for(;;) re-pick this same team next
                 * iteration → infinite loop. Mark it processed so we
                 * skip it instead of hanging. The team won't appear in
                 * the resulting selection, which is what the SDL build
                 * would otherwise show as "wrong/missing teams" in the
                 * preset WC menu. */
                flags[i] = 1;
            }
        }
    }

    /* Phase 3: Copy resolved team IDs to the output. The output arrays
     * (diyCompetitionStart.matchData.league.leagueTeamIds / diyCompetitionStart.cupTeamIds / diyFileBufferCopy.teamIds) hold
     * BE-packed `{country, ordinal}` byte pairs — the same convention
     * the static dc.b competition data uses and that downstream code
     * like _DIY_ComputeAverageTeamStrengthTier reads via BE_READ_U16
     * to match m68k `move.w` semantics. Write BE bytes so both endians
     * land at the same in-memory layout the asm produces. */
    for (i = 0; i < count; i++) {
        BE_WRITE_U16(out, teamIdsLE[i]);
        out++;
    }
}

/* ================================================================
 * Career_FillLeagueTeamSelectionFromFile  (SWOS2.s loc_15158E)
 *
 * Inline helper for Career_InitCompetitionTeamSelectionFromTableEntrySpec
 * when spec[7] == 0. Loads a team file and fills diyCompetitionStart.matchData.league.leagueTeamIds
 * with teams matching the specified league/division.
 *
 * in: extNumber     = team file ext number (d0 in asm)
 *     leagueFilter  = league value to match at offset 25 (d1 → d6)
 *     expectedCount = expected number of matching teams (d2 in asm)
 * ================================================================ */
static void fillLeagueTeamSelectionFromFile(uint16_t extNumber, uint16_t leagueFilter,
                                             int16_t expectedCount)
{
    _LoadTeamFile(extNumber);

    TeamFileEx *tfBase = (TeamFileEx *)(&teamFileBuffer + 2);
    uint16_t tfCount = *(uint16_t *)&teamFileBuffer;

    /* Verify expected count matches actual */
    uint16_t actualCount = 0;
    uint16_t j;
    for (j = 0; j < tfCount; j++) {
        if (tfBase[j].team.league == (uint8_t)leagueFilter)
            actualCount++;
    }
    if ((int16_t)actualCount != expectedCount) {
        /* [verify-fill] -O2 makes gdb's `expectedCount=...` value
         * unreliable; print the real values plus what fed them. */
        SWOS_DBG("[verify-fill] panic 0x8885: extNumber=%u leagueFilter=%u "
                        "expectedCount=%d actualCount=%u tfCount=%u\n",
                (unsigned)extNumber, (unsigned)leagueFilter,
                (int)expectedCount, (unsigned)actualCount, (unsigned)tfCount);
        SWOS_DBG_FLUSH();
        panicReasonCode = 0x8885;
        _PanicHaltLoop();
    }

    /* Fill selection with matching teams. The output array
     * (diyCompetitionStart.matchData.league.leagueTeamIds) uses the BE-packed byte-pair convention from
     * 4e5f378 — consumers read via BE_READ_U16. Use PACK_BE_U16 to
     * produce the same `(country << 8) | ordinal` value that
     * findTeamInCache / isTeamInCache compare against, and
     * BE_WRITE_U16 to store. */
    TeamFileEx *selDst = (TeamFileEx *)&selTeamsPtr + numSelectedTeams;
    uint16_t *idOut = diyCompetitionStart.matchData.league.leagueTeamIds;
    for (j = 0; j < tfCount; j++) {
        TeamFileEx *src = &tfBase[j];
        if (src->team.league == (uint8_t)leagueFilter) {
            uint16_t teamId = PACK_BE_U16(src->team.countryNumber,
                                          src->team.teamOrdinal);
            BE_WRITE_U16(idOut, teamId);
            idOut++;
            if (isTeamInCache(teamId) != 0) {
                /* Not in cache — add to selection */
                selDst = (TeamFileEx *)appendTeamToSelection(
                    (uint8_t *)selDst, (const uint8_t *)src);
            }
        }
    }
}

/* ================================================================
 * Career_TryAddRandomTeamByRegionToLists  (SWOS2.s line 97895)
 *
 * Picks a random team from teamFileBuffer matching the given
 * region/league filter. If not already in the cup team ID list,
 * adds it. If found in cache, also copies the full record to
 * selection buffer.
 *
 * Parameters are passed by pointer since asm modifies shared
 * register state (d6/a2/a4) across calls.
 *
 * Uses Career_IsTeamIdNotInListOrReturnSentinel (cup version).
 * ================================================================ */
static void tryAddRandomTeamByRegion_Cup(uint16_t **pIdOut, uint8_t **pSelOut,
                                          uint16_t *pRemaining, uint16_t filter)
{
    TeamFileEx *tfBase = (TeamFileEx *)(&teamFileBuffer + 2);
    uint16_t tfCount = *(uint16_t *)&teamFileBuffer;

    if (*pRemaining == 0)
        return;

    uint16_t rnd = _Rand2();
    uint16_t idx = (uint16_t)(((uint32_t)rnd * tfCount) >> 8);
    TeamFileEx *team = &tfBase[idx];

    if (team->team.league != (uint8_t)filter)
        return;

    uint16_t teamId = PACK_BE_U16(team->team.countryNumber,
                                  team->team.teamOrdinal);
    uint32_t result = _Career_IsTeamIdNotInListOrReturnSentinel(teamId, *pIdOut);
    if ((result & 0x10000) == 0)
        return;  /* already in list */

    /* BE-write convention (4e5f378). */
    BE_WRITE_U16(*pIdOut, teamId);
    (*pIdOut)++;
    (*pRemaining)--;

    if (isTeamInCache(teamId) == 0)
        return;  /* not in selection cache → just added ID */

    /* Copy team record to selection buffer */
    *pSelOut = appendTeamToSelection(*pSelOut, (const uint8_t *)team);
}

/* ================================================================
 * Career_FillCupTeamSelectionFromFile  (SWOS2.s loc_151628)
 *
 * Inline helper for Career_InitCompetitionTeamSelectionFromTableHeaderSpec
 * when spec[7] == 0. Fills diyCompetitionStart.cupTeamIds with teams for a cup
 * competition by random selection + region weighting.
 *
 * in: extNumber = team file ext number (d0 in asm, byte from spec[2])
 *     teamCount = total teams needed (d1 in asm)
 * ================================================================ */
static void fillCupTeamSelectionFromFile(uint16_t extNumber, int16_t teamCount)
{
    int16_t remaining = teamCount;
    uint16_t initialCount = (uint16_t)teamCount;
    uint16_t *idOut = diyCompetitionStart.cupTeamIds;

    _LoadTeamFile(extNumber);

    /* selDst stays uint8_t * here because tryAddRandomTeamByRegion_Cup
     * takes a uint8_t ** in/out parameter and reassigns it across calls. */
    uint8_t *selDst = (uint8_t *)((TeamFileEx *)&selTeamsPtr + numSelectedTeams);

    /* BE-write convention for diyCompetitionStart.cupTeamIds (4e5f378). */
    /* If not career loaded and gameType==4, add selected team first */
    if (careerLoadedFlag == 0 && gameType == 4) {
        BE_WRITE_U16(idOut, selTeamNumber); idOut++;
        remaining--;
        if (remaining == 0)
            return;
    }

    /* If gameType==3 and have selected teams, random-pick from them */
    if (gameType == 3) {
        uint16_t numSel = numSelectedTeams;
        if (numSel != 0) {
            TeamFileEx *selBase = (TeamFileEx *)&selTeamsPtr;
            int16_t tries;
            for (tries = 0x3E8; tries >= 0; tries--) {
                uint16_t rnd = _Rand2();
                uint16_t idx = (uint16_t)(((uint32_t)rnd * numSel) >> 8);
                TeamFileEx *selTeam = &selBase[idx];
                if (selTeam->team.teamControls == 1)
                    continue;  /* skip user-controlled teams */
                uint16_t teamId = PACK_BE_U16(selTeam->team.countryNumber,
                                              selTeam->team.teamOrdinal);
                uint32_t sentinel = _Career_IsTeamIdNotInListOrReturnSentinel(teamId, idOut);
                if ((sentinel & 0x10000) == 0)
                    continue;  /* already in list */
                BE_WRITE_U16(idOut, teamId); idOut++;
                remaining--;
                if (remaining == 0)
                    return;
            }
        }
    }

    /* If initialCount > 32, add all selected teams not already in list */
    if (initialCount > 0x20) {
        uint16_t numSel = numSelectedTeams;
        if (numSel != 0) {
            TeamFileEx *selTeam = (TeamFileEx *)&selTeamsPtr;
            uint16_t k;
            for (k = 0; k < numSel; k++) {
                uint16_t teamId = PACK_BE_U16(selTeam->team.countryNumber,
                                              selTeam->team.teamOrdinal);
                uint32_t sentinel = _Career_IsTeamIdNotInListOrReturnSentinel(teamId, idOut);
                if ((sentinel & 0x10000) != 0) {
                    BE_WRITE_U16(idOut, teamId); idOut++;
                    remaining--;
                    if (remaining == 0)
                        return;
                }
                selTeam++;
            }
        }
    }

    /* Multi-round region-based random picking */
    uint16_t maxRounds;
    if (initialCount >= 0x40)
        maxRounds = 3;
    else if (initialCount >= 0x20)
        maxRounds = 2;
    else
        goto weighted_pick;  /* skip rounds, go straight to weighted */

    {
        uint16_t round;
        for (round = 0; round < maxRounds; round++) {
            int16_t tries;
            for (tries = 0x3E8; tries >= 0; tries--) {
                if (remaining == 0)
                    return;
                tryAddRandomTeamByRegion_Cup(&idOut, &selDst, (uint16_t *)&remaining, round);
            }
        }
    }

weighted_pick:
    /* Weighted random region picking until done */
    {
        uint8_t *weights = careerRegionPickWeightsTable;
        /* Reload team file base for region checks */
        while (remaining > 0) {
            uint16_t rnd = _Rand2();
            uint16_t region = weights[rnd & 0x1F];
            tryAddRandomTeamByRegion_Cup(&idOut, &selDst, (uint16_t *)&remaining, region);
        }
    }
}

/* ================================================================
 * Career_FillTournamentTeamSelectionFromFile  (SWOS2.s loc_1517C2)
 *
 * Same as cup version but uses diyFileBufferCopy.teamIds,
 * Career_IsTeamIdNotInListOrReturnSentinel2, and
 * careerRegionPickWeightsTable2.
 *
 * in: extNumber = team file ext number (d0 in asm, byte from spec[2])
 *     teamCount = total teams needed (d1 in asm)
 * ================================================================ */
static void fillTournamentTeamSelectionFromFile(uint16_t extNumber, int16_t teamCount)
{
    int16_t remaining = teamCount;
    uint16_t *idOut = diyFileBufferCopy.teamIds;

    _LoadTeamFile(extNumber);

    TeamFileEx *selDst = (TeamFileEx *)&selTeamsPtr + numSelectedTeams;

    /* BE-write convention for diyFileBufferCopy.teamIds (4e5f378). */
    /* If not career loaded and gameType==4, add selected team first */
    if (careerLoadedFlag == 0 && gameType == 4) {
        BE_WRITE_U16(idOut, selTeamNumber); idOut++;
        remaining--;
        if (remaining == 0)
            return;
    }

    /* If gameType==3 and have selected teams, random-pick from them */
    if (gameType == 3) {
        uint16_t numSel = numSelectedTeams;
        if (numSel != 0) {
            TeamFileEx *selBase = (TeamFileEx *)&selTeamsPtr;
            int16_t tries;
            for (tries = 0x3E8; tries >= 0; tries--) {
                uint16_t rnd = _Rand2();
                uint16_t idx = (uint16_t)(((uint32_t)rnd * numSel) >> 8);
                TeamFileEx *selTeam = &selBase[idx];
                if (selTeam->team.teamControls == 1)
                    continue;
                uint16_t teamId = PACK_BE_U16(selTeam->team.countryNumber,
                                              selTeam->team.teamOrdinal);
                uint32_t sentinel = _Career_IsTeamIdNotInListOrReturnSentinel2(teamId, idOut);
                if ((sentinel & 0x10000) == 0)
                    continue;
                BE_WRITE_U16(idOut, teamId); idOut++;
                remaining--;
                if (remaining == 0)
                    return;
            }
        }
    }

    /* Weighted random region picking using inline version */
    {
        uint8_t *weights = careerRegionPickWeightsTable2;
        TeamFileEx *tfBase = (TeamFileEx *)(&teamFileBuffer + 2);
        uint16_t tfCount = *(uint16_t *)&teamFileBuffer;

        while (remaining > 0) {
            uint16_t rnd1 = _Rand2();
            uint8_t region = weights[rnd1 & 0x1F];

            uint16_t rnd2 = _Rand2();
            uint16_t idx = (uint16_t)(((uint32_t)rnd2 * tfCount) >> 8);
            TeamFileEx *team = &tfBase[idx];

            if (team->team.league != region)
                continue;

            uint16_t teamId = PACK_BE_U16(team->team.countryNumber,
                                          team->team.teamOrdinal);
            uint32_t sentinel = _Career_IsTeamIdNotInListOrReturnSentinel2(teamId, idOut);
            if ((sentinel & 0x10000) == 0)
                continue;

            BE_WRITE_U16(idOut, teamId); idOut++;
            remaining--;

            if (isTeamInCache(teamId) == 0)
                continue;  /* not in cache */

            selDst = (TeamFileEx *)appendTeamToSelection(
                (uint8_t *)selDst, (const uint8_t *)team);
        }
    }
}

/* ================================================================
 * Career_TryAddRandomTeamByRegionToLists  (SWOS2.s line 97895)
 *
 * Exported version called from asm. Picks a random team matching
 * the region filter from teamFileBuffer. Uses cup sentinel check.
 *
 * Asm calling convention: d2=filter, d6=remaining, a2=idOut, a4=selOut
 * These are modified as side effects. The asm bridge handles this.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_TryAddRandomTeamByRegionToLists(void)
{
    /* This function is only called from asm within loc_151628 (cup path),
       which is now ported to C (fillCupTeamSelectionFromFile).
       The exported symbol is kept for linkage but should not be called
       directly from new C code. Use tryAddRandomTeamByRegion_Cup instead. */
}

/* ================================================================
 * FatalError  (SWOS2.s line 98258)
 *
 * Triggers a fatal error with panic code 0x1939.
 * ================================================================ */
__attribute__((used, externally_visible))
void _FatalError(void)
{
    panicReasonCode = 0x1939;
    _PanicHaltLoop();
}

/* ================================================================
 * Career_InitCompetitionTeamSelectionFromTableEntrySpec  (SWOS2.s line 98279)
 *
 * Initializes team selection from a competition table entry spec.
 * If spec[7] != 0, extracts team IDs from spec and calls
 * AppendTeamsToSelectionFromTmdOrTeamFile.
 * If spec[7] == 0, loads teams by league from team file.
 *
 * in: spec = pointer to competition spec data (a0 in asm)
 *     idx  = competition index (d1 in asm)
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_InitCompetitionTeamSelectionFromTableEntrySpec(uint8_t *spec, uint16_t idx)
{
    uint8_t teamIdsOffset = spec[7];
    if (teamIdsOffset != 0) {
        /* Team IDs embedded in spec at offset 7 + teamIdsOffset.
         * Every CompetitionCup*_2_N struct has a `char *text[2]` pair
         * sitting between the cup descriptor and the embedded `unk1[]`
         * team list. On 64-bit those two pointers grow from 4 to 8 bytes
         * each, pushing `unk1` 8 bytes later. The data tables store the
         * 32-bit Amiga offset in spec[7]; derive the host-side delta from
         * sizeof so we don't bake another raw offset. */
        uint16_t idsOff = teamIdsOffset + 2u * (sizeof(char *) - sizeof(uint32_t));
        int16_t count = (int16_t)(int8_t)spec[13];  /* signed ext.w */
        uint16_t *teamIds = (uint16_t *)(spec + 7 + idsOff);
        _AppendTeamsToSelectionFromTmdOrTeamFile(diyCompetitionStart.matchData.league.leagueTeamIds, teamIds, (uint16_t)count);
    } else {
        /* Load by league from team file */
        uint16_t entryOff = idx * 6;
        int16_t expectedCount = (int16_t)(int8_t)spec[13 + entryOff];
        uint16_t extNumber = spec[2];
        /* [verify-fill] Trace the values that feed fillLeagueTeamSelectionFromFile. */
        SWOS_DBG("[verify-fill] InitFromTableEntrySpec(spec=%p, idx=%u): "
                        "spec[1]=%u spec[2]=%u spec[7]=%u spec[13]=%u "
                        "entryOff=%u spec[13+entryOff]=%u expectedCount=%d\n",
                (void *)spec, (unsigned)idx,
                (unsigned)spec[1], (unsigned)spec[2], (unsigned)spec[7],
                (unsigned)spec[13],
                (unsigned)entryOff, (unsigned)spec[13 + entryOff],
                (int)expectedCount);
        SWOS_DBG_FLUSH();
        fillLeagueTeamSelectionFromFile(extNumber, idx /* = league filter */, expectedCount);
    }
}

/* ================================================================
 * Career_InitCompetitionTeamSelectionFromTableHeaderSpec  (SWOS2.s line 98298)
 *
 * Initializes team selection from a competition table header spec.
 * If spec[7] != 0, extracts team IDs and uses AppendTeamsToSelection.
 * If spec[7] == 0, fills cup team selection from team file.
 *
 * in: spec = pointer to competition spec data (a0 in asm)
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_InitCompetitionTeamSelectionFromTableHeaderSpec(uint8_t *spec)
{
    uint8_t teamIdsOffset = spec[7];
    if (teamIdsOffset != 0) {
        /* See TableEntrySpec above for the +text[2]-growth delta rationale. */
        uint16_t idsOff = teamIdsOffset + 2u * (sizeof(char *) - sizeof(uint32_t));
        uint16_t count = spec[10];  /* andi.w #$FF,d7 — unsigned byte */
        uint16_t *teamIds = (uint16_t *)(spec + 7 + idsOff);
        _AppendTeamsToSelectionFromTmdOrTeamFile(diyCompetitionStart.cupTeamIds, teamIds, count);
    } else {
        uint16_t extNumber = spec[2];
        int16_t teamCount = (int16_t)(int8_t)spec[10];  /* ext.w d1 — signed */
        fillCupTeamSelectionFromFile(extNumber, teamCount);
    }
}

/* ================================================================
 * Career_InitCompetitionTeamSelectionFromAltTableHeaderSpec  (SWOS2.s line 98315)
 *
 * Same as TableHeaderSpec but uses tournament arrays and offset 15.
 *
 * in: spec = pointer to competition spec data (a0 in asm)
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_InitCompetitionTeamSelectionFromAltTableHeaderSpec(uint8_t *spec)
{
    uint8_t teamIdsOffset = spec[7];
    if (teamIdsOffset != 0) {
        /* See TableEntrySpec above for the +text[2]-growth delta rationale. */
        uint16_t idsOff = teamIdsOffset + 2u * (sizeof(char *) - sizeof(uint32_t));
        uint16_t count = spec[15];  /* andi.w #$FF,d7 — unsigned byte */
        uint16_t *teamIds = (uint16_t *)(spec + 7 + idsOff);
        _AppendTeamsToSelectionFromTmdOrTeamFile(diyFileBufferCopy.teamIds, teamIds, count);
    } else {
        uint16_t extNumber = spec[2];
        int16_t teamCount = (int16_t)(int8_t)spec[15];  /* ext.w d1 — signed */
        fillTournamentTeamSelectionFromFile(extNumber, teamCount);
    }
}

/* ================================================================
 * Career_InitCompetitionTeamSelection_Dispatch  (SWOS2.s line 98270)
 *
 * Dispatcher that checks spec[1] and routes to the appropriate
 * team selection initialization function.
 *
 * in: spec = pointer to competition spec (a0 in asm)
 *     idx  = competition index (d1 in asm)
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_InitCompetitionTeamSelection_Dispatch(uint8_t *spec, uint16_t idx)
{
    uint8_t type = spec[1];
    if (type == 0)
        _Career_InitCompetitionTeamSelectionFromTableEntrySpec(spec, idx);
    else if (type == 1)
        _Career_InitCompetitionTeamSelectionFromTableHeaderSpec(spec);
    else
        _Career_InitCompetitionTeamSelectionFromAltTableHeaderSpec(spec);
}

/* ================================================================
 * Chunk 92 — Career Competition State & Team Selection Key Cache
 *
 * 12 functions for competition state buffer copy (Load/Store + 4
 * wrappers), team selection key cache navigation (4 Get* functions),
 * Career_UpdateCareerFinishedFlag, and
 * Career_AdvanceToNextTeamSelectionKeyCacheEntry.
 * ================================================================ */

/* --- Additional ASM externs for Chunk 92 --- */
extern uint8_t competitionFileBuffer[];
extern uint8_t competitionContext[];
extern uint8_t tertiaryCompetitionContext[];
extern uint8_t euroCupsCompetitionContext[];
extern DIYCompetitionEx diyCompetitionStart;
extern DIYTournamentState diyFileBufferCopy;
extern uint16_t teamSelectionKeyCacheOffset;
extern uint16_t teamSelectionKeyCacheIndex;
extern uint16_t seasonFinished;
extern uint16_t careerFinished;
extern uint16_t careerSlotProcessingInProgress;
extern uint16_t homeMarketUpdateBacklog;
extern uint16_t foreignMarketUpdateBacklog;
extern uint16_t offerExpiryIterationsRemaining;

extern void _ApplyWeeklyFinancialDeductions(void);
extern void _Career_FillCurrentSeasonInfo(void);

/* ================================================================
 * _Career_LoadCompetitionSlotStateFromBuffer  (SWOS2.s line 36092)
 *
 * Copies competition state from a buffer to the active area.
 * Type 0: sizeof(DIYCompetitionEx)/2 words to diyCompetitionStart — the
 *         whole league image through the relocated leagueStandings tail
 *         (was a hardcoded 0x39A words = the old standings-end 0x734, before
 *         the standings untangle moved them off the cup scratch).
 * Type 1: 546 words (0x222) to diyCompetitionStart (cup: through teamOffsetTable;
 *         cups have no league standings, so no growth needed)
 * Type 2+: tournament — field-map the slot into diyFileBufferCopy (master)
 *          + diyCompetitionStart (current-stage standings). The original
 *          SWOS2.s did a flat 1450-word (0x5AA) copy into diyFileBufferCopy
 *          because the master and the current-stage buffer were contiguous
 *          (diyCompetitionStart == diyFileBufferCopy + 0x420). The step-3
 *          untangle (cf8cedd) made diyFileBufferCopy a typed struct
 *          (teamIndexOrder @0x41E) and diyCompetitionStart a standalone
 *          buffer, so a flat copy now both MISALIGNS the differing layouts
 *          and overruns the master struct into pitchChunkyBuffer. Use the
 *          same field-map the preset path uses (_DIY_CfbBufToTournament),
 *          parameterised by the slot base.
 *
 * in: src = pointer to competition buffer (a0 in asm)
 * ================================================================ */
extern void _DIY_CfbBufToTournament(const uint8_t *cfb);
extern void _DIY_TournamentToCfbBuf(uint8_t *cfb);
extern void _DIY_BuildTeamSelectionPointerCache(void);

__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _Career_LoadCompetitionSlotStateFromBuffer(uint8_t *src)
{
    uint16_t type = *(uint16_t *)src;
    volatile uint16_t *s = (volatile uint16_t *)src;

    if (type == 0) {
        volatile uint16_t *d = (volatile uint16_t *)&diyCompetitionStart;
        for (int16_t i = (int16_t)(sizeof(DIYCompetitionEx) / 2 - 1); i >= 0; i--)
            *d++ = *s++;
    } else if (type == 1) {
        volatile uint16_t *d = (volatile uint16_t *)&diyCompetitionStart;
        for (int16_t i = 0x221; i >= 0; i--)
            *d++ = *s++;
    } else {
        _DIY_CfbBufToTournament(src);
    }

#ifdef SWOS_DEBUG_AUTOSKIP
    {
        DIYCompetitionEx *bex = &diyCompetitionStart;
        SWOS_AUTOSKIP_LOG("[bug20-load] LoadSlotState src=%p type=%u "
                          "compId=0x%04X minSubstitutes=%u playEachTeam=%u "
                          "leagueMatchProgress=%u leagueCurrentSlot=%u "
                          "totalMatchesPerSectionLive=%d",
                          (void *)src, (unsigned)type,
                          (unsigned)*(uint16_t *)&bex->base.compId,
                          (unsigned)bex->minSubstitutes,
                          (unsigned)bex->base.playEachTeam,
                          (unsigned)bex->leagueMatchProgress,
                          (unsigned)bex->leagueCurrentSlot,
                          (int)bex->matchData.league.totalMatchesPerSectionLive);
    }
#endif
}

/* ================================================================
 * _Career_StoreCompetitionSlotStateToBuffer  (SWOS2.s line 36120)
 *
 * Copies competition state from the active area to a buffer.
 * Type determined by the first word of the buffer.
 * Type 0: sizeof(DIYCompetitionEx)/2 words from diyCompetitionStart (whole
 *         league image incl. relocated leagueStandings; was 0x39A words)
 * Type 1: 546 words from diyCompetitionStart
 * Type 2+: tournament — field-map diyFileBufferCopy (master) +
 *          diyCompetitionStart (current-stage standings) INTO the slot via
 *          _DIY_TournamentToCfbBuf. See the load counterpart above for why a
 *          flat 1450-word copy no longer works post-untangle.
 *
 * in: dst = pointer to competition buffer (a0 in asm)
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _Career_StoreCompetitionSlotStateToBuffer(uint8_t *dst)
{
    uint16_t type = *(uint16_t *)dst;
    volatile uint16_t *d = (volatile uint16_t *)dst;

    if (type == 0) {
        volatile uint16_t *s = (volatile uint16_t *)&diyCompetitionStart;
        for (int16_t i = (int16_t)(sizeof(DIYCompetitionEx) / 2 - 1); i >= 0; i--)
            *d++ = *s++;
    } else if (type == 1) {
        volatile uint16_t *s = (volatile uint16_t *)&diyCompetitionStart;
        for (int16_t i = 0x221; i >= 0; i--)
            *d++ = *s++;
    } else {
        _DIY_TournamentToCfbBuf(dst);
    }
}

/* ================================================================
 * _Career_CopyCompetitionFileBufferToOrFromActive  (SWOS2.s line 36060)
 *
 * Loads competition state from competitionFileBuffer.
 * Note: the "Store" path after the bra.w is dead code.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_CopyCompetitionFileBufferToOrFromActive(void)
{
    _Career_LoadCompetitionSlotStateFromBuffer(competitionFileBuffer);
}

/* ================================================================
 * _Career_CopyCompetitionContextToOrFromActive  (SWOS2.s line 36068)
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_CopyCompetitionContextToOrFromActive(void)
{
    _Career_LoadCompetitionSlotStateFromBuffer(competitionContext);
}

/* ================================================================
 * _Career_CopyTertiaryCompetitionContextToOrFromActive  (SWOS2.s line 36076)
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_CopyTertiaryCompetitionContextToOrFromActive(void)
{
    _Career_LoadCompetitionSlotStateFromBuffer(tertiaryCompetitionContext);
}

/* ================================================================
 * _Career_CopyEuroCupsCompetitionContextToOrFromActive  (SWOS2.s line 36084)
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_CopyEuroCupsCompetitionContextToOrFromActive(void)
{
    _Career_LoadCompetitionSlotStateFromBuffer(euroCupsCompetitionContext);
}

/* ================================================================
 * _TeamSelectionKeyCache_GetListPtr  (SWOS2.s line 36436)
 *
 * Walks the team selection key cache to find the entry at the
 * given list index. Each entry has a stride word at offset 2.
 *
 * in: listIndex = 0-based index (d0 in asm, masked to 8 bits)
 * returns: pointer to the entry (a0 in asm)
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_TeamSelectionKeyCache_GetListPtr(uint16_t listIndex)
{
    uint8_t *ptr = teamSelectionKeyCache;
    uint16_t idx = listIndex & 0xFF;
    if (idx == 0)
        return ptr;
    for (int16_t i = (int16_t)(idx - 1); i >= 0; i--) {
        /* Stride bounds check — same defence as
         * _BuildGameListArrayFromTeamSelectionKeyCache. The career-mode
         * writer overflows the 22-byte slot and stomps slot[N+1]'s stride
         * field with team-ID bytes (`0E 0A` on the player's team), which
         * reads as 0x0A0E=2574 on LE. Walking via that stride lands deep
         * in the overlaid careerForeignMarketPlayers region and the next
         * read picks up real foreign-player records as a fake cache
         * header. On Amiga BE the corrupted bytes read as 0x0E0A=3594
         * which happens to land on zero bytes. Clamp to legitimate slot
         * sizes (10..256) and freeze the walker at the last sane entry —
         * same approach as the builder walker fix. See
         * docs/OVERLAPPED_DATA.md entry #8. */
        uint16_t stride = ((TeamSelectionKeyCacheEntry *)ptr)->entryStride;
        if (stride < 10 || stride > 256)
            return ptr;
        ptr += stride;
    }
    return ptr;
}

/* ================================================================
 * _TeamSelectionKeyCache_GetListElementPtr  (SWOS2.s line 36426)
 *
 * Gets a specific element within a key cache list entry.
 * Elements are 12 bytes each.
 *
 * in: listIndex    = list index (d0 in asm)
 *     elementIndex = element index (d1 in asm, masked to 8 bits)
 * returns: pointer to the element (a1 in asm)
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_TeamSelectionKeyCache_GetListElementPtr(uint16_t listIndex, uint16_t elementIndex)
{
    uint8_t *listPtr = _TeamSelectionKeyCache_GetListPtr(listIndex);
    uint32_t offset = (uint32_t)(elementIndex & 0xFF) * 12;
    return listPtr + offset;
}

/* ================================================================
 * _TeamSelectionKeyCache_GetCurrentListPtr  (SWOS2.s line 36468)
 *
 * Returns pointer to the current entry in the key cache.
 *
 * returns: pointer to current entry (a0 in asm)
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_TeamSelectionKeyCache_GetCurrentListPtr(void)
{
    return teamSelectionKeyCache + teamSelectionKeyCacheOffset;
}

/* ================================================================
 * _TeamSelectionKeyCache_GetCurrentSelectedElementPtr  (SWOS2.s line 36449)
 *
 * Returns pointer to the current selected element within the
 * current cache entry. Adjusts by (byte[5] - byte[6]) * 12 if
 * byte[6] >= 0 and the difference is nonzero.
 *
 * returns: pointer to element (a1 in asm)
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_TeamSelectionKeyCache_GetCurrentSelectedElementPtr(void)
{
    uint8_t *ptr = teamSelectionKeyCache + teamSelectionKeyCacheOffset;
    uint8_t *result = ptr;

    if ((int8_t)ptr[6] < 0)
        return result;

    uint8_t diff = ptr[5] - ptr[6];
    if (diff == 0)
        return result;

    result += (uint32_t)diff * 12;
    return result;
}

/* ================================================================
 * _TeamSelectionKeyCache_GetCurrentMatchSlot — typed wrapper for the
 * post-Phase-1F struct-named match-slot access.
 *
 * The asm helper above returns `entry + diff*12`, which is 10 bytes
 * before the actual match slot start (writes happen at +0x0B). This
 * wrapper returns the properly-aligned slot pointer so callers can
 * use `slot->matchFlags / packedGoals / aggregate / penaltyScore`
 * instead of raw `elem[0x0B / 0x10 / 0x12 / 0x14]` byte arithmetic.
 *
 * Always returns a valid pointer (matches the asm — never NULL); when
 * remainingElements is 0 or negative the slot resolves to entry+sizeof, as the
 * original does, instead of faulting.
 * ================================================================ */
TeamSelectionKeyCacheMatchSlot *_TeamSelectionKeyCache_GetCurrentMatchSlot(void)
{
    TeamSelectionKeyCacheEntry *entry =
        (TeamSelectionKeyCacheEntry *)(teamSelectionKeyCache + teamSelectionKeyCacheOffset);
    /* Match the asm GetCurrentSelectedElementPtr (SWOS2.s:60723): its `bmi`
     * branch returns a1 = entry (diff folded to 0) when remainingElements is
     * negative — a valid pointer, NEVER null. The SDL port added a `return NULL`
     * here, which the sole caller (_Career_UpdateStatsAfterMatch) dereferences
     * unguarded → an SDL-only SIGSEGV on a two-leg cup tie whose VIEW-RESULT
     * replays drive remainingElements past 0 (the Amiga survives the same path).
     * This is a C-vs-asm divergence fix, not a panic patch-out. NOTE: the deeper
     * two-leg over-decrement (cursor not advancing to the leg-2 entry) is a
     * separate item still to be root-caused; this restores parity with the asm. */
    uint8_t diff;
    if (entry->remainingElements < 0)
        diff = 0;
    else
        diff = (uint8_t)entry->totalElements - (uint8_t)entry->remainingElements;
    return (TeamSelectionKeyCacheMatchSlot *)((uint8_t *)entry +
                                              sizeof(TeamSelectionKeyCacheEntry) +
                                              (uint32_t)diff * 12);
}

/* ================================================================
 * _Career_UpdateCareerFinishedFlag  (SWOS2.s line 36148)
 *
 * Checks if all remaining key cache entries have been processed.
 * Sets careerFinished = 1 if season is over or no entries with
 * byte[4]==0 have byte[6]!=0. Sets careerFinished = 0 otherwise.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_UpdateCareerFinishedFlag(void)
{
    if (seasonFinished != 0) {
        careerFinished = 1;
        return;
    }

    TeamSelectionKeyCacheEntry *entry =
        (TeamSelectionKeyCacheEntry *)_TeamSelectionKeyCache_GetCurrentListPtr();
    if (entry->remainingElements != 0) {
        careerFinished = 0;
        return;
    }

    /* Walk from current entry to end of cache */
    entry = (TeamSelectionKeyCacheEntry *)_TeamSelectionKeyCache_GetCurrentListPtr();
    uint16_t idx = teamSelectionKeyCacheIndex;

    for (;;) {
        if (entry->phaseFlag == 0) {
            if (entry->remainingElements != 0) {
                careerFinished = 0;
                return;
            }
        }
        entry = (TeamSelectionKeyCacheEntry *)((uint8_t *)entry + entry->entryStride);
        idx++;
        if (idx == teamSelectionKeyCacheCount)
            break;
    }

    careerFinished = 1;
}

/* ================================================================
 * _Career_AdvanceToNextTeamSelectionKeyCacheEntry  (SWOS2.s line 36477)
 *
 * Advances to the next unprocessed entry in the key cache.
 * If gameType==4 (career), increments financial counters and
 * applies weekly deductions first.
 * If no more entries remain, sets seasonFinished and careerFinished
 * and fills season info.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_AdvanceToNextTeamSelectionKeyCacheEntry(void)
{
    if (gameType == 4) {
        homeMarketUpdateBacklog++;
        foreignMarketUpdateBacklog++;
        offerExpiryIterationsRemaining++;
        _ApplyWeeklyFinancialDeductions();
    }

    careerSlotProcessingInProgress = 0;
    *(uint16_t *)teamSelectionKeyCacheEnd = 0;

    uint8_t *ptr = teamSelectionKeyCache + teamSelectionKeyCacheOffset;
    uint16_t d0 = teamSelectionKeyCacheIndex;

    for (;;) {
        d0++;
        if (d0 == teamSelectionKeyCacheCount) {
#ifdef SWOS_DEBUG_AUTOSKIP
            SWOS_AUTOSKIP_LOG("[bug3-league] AdvanceToNext: END-OF-CACHE — setting seasonFinished=1 "
                              "careerFinished=1 keyCacheCount=%u",
                              (unsigned)teamSelectionKeyCacheCount);
#endif
            seasonFinished = 1;
            careerFinished = 1;
            _Career_FillCurrentSeasonInfo();
            return;
        }
        /* Stride bounds check — see _TeamSelectionKeyCache_GetListPtr for
         * the full story (career-mode 22-byte slot overflow corrupts the
         * stride field). If we'd step outside the legitimate slot-size
         * range, treat the season as finished early rather than walking
         * into the careerForeignMarketPlayers overlay. */
        uint16_t stride = ((TeamSelectionKeyCacheEntry *)ptr)->entryStride;
        if (stride < 10 || stride > 256) {
            seasonFinished = 1;
            careerFinished = 1;
            _Career_FillCurrentSeasonInfo();
            return;
        }
        ptr += stride;
        if (ptr[4] == 0) {
            teamSelectionKeyCacheIndex = d0;
            teamSelectionKeyCacheOffset = (uint16_t)((uintptr_t)ptr - (uintptr_t)teamSelectionKeyCache);
            return;
        }
    }
}

/* ================================================================
 * Chunk 93 — Career Match Flow & Competition Slot Processing
 *
 * 6 functions for career match progression: dispatching auto-sim
 * loops, interactive competition slot menus, and match flow control.
 * ================================================================ */

/* --- Additional ASM externs for Chunk 93 --- */
extern uint8_t *careerCompetitionSlotPtrTable[];
extern uint16_t careerMatchSimulatedFlag;
extern uint8_t viewRandomPhase;

extern void _DIY_UpdateCompetitionProgress(void);

/* C functions for menu/competition dispatch */
extern void _ShowDiyLeaguesTournamentMenu(uint16_t type, uint16_t viewOnly, uint16_t focusOverride);
extern void _ShowDIYCupMenuWithParams(uint16_t arg0, uint16_t arg1, uint16_t arg2);
extern void _DIY_EditCompetitionSlot(uint16_t paramA, uint16_t paramB, uint16_t paramC);

/* C functions for match sim */
extern uint16_t _DIY_PlayOrSimulateLeagueFixture(int16_t allowSquadChanges, int16_t selector);
extern uint16_t _DIY_PlayCupMatchAndAdvance(int16_t allowSquadChanges, int16_t mode);
extern void _DIY_LoadCurrentCompetitionSlot(void);

/* C functions for bracket/team grid (called from DIY_AdvanceToNextCompetitionSlot) */
extern void _DIY_BuildSeededBracketAndUpdateScores(uint8_t *a2, uint8_t *a3,
                                                     uint8_t *a4, uint8_t *a5);
extern void _DIY_CopyTeamGridFromCompToDIY(uint8_t *a2, uint8_t *a3,
                                              uint8_t *a4, uint8_t *a5);

/* Already-ported C functions used here */
extern void _BuildCareerTeamUnavailablePlayerList(void);
extern uint16_t _ShouldApplyRandomPlayerStatusAfterSim(void);
extern void _ClearCareerMatchPlayerStatusBucket(void);

/* ================================================================
 * _Career_ProcessCurrentCompetitionSlotForMatch  (SWOS2.s line 36288)
 *
 * Gets the current competition slot, loads its state, dispatches
 * to the appropriate auto-simulate function by type, then stores
 * the state back.
 *
 * Type 0 (league): AutoSimulateLeagueFixturesUntilHumanTeam
 * Type 1 (cup): AutoSimulateCupMatchesUntilHumanTeam
 * Type 2 (hybrid): AutoSimulatePendingMatchUntilHumanTeam
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_ProcessCurrentCompetitionSlotForMatch(void)
{
    uint8_t *entry = _TeamSelectionKeyCache_GetCurrentListPtr();
    int16_t slotIdx = (int16_t)(int8_t)entry[0];
    uint8_t *slotPtr = careerCompetitionSlotPtrTable[slotIdx];

    _Career_LoadCompetitionSlotStateFromBuffer(slotPtr);

    uint16_t type = *(uint16_t *)slotPtr;
    if (type == 1)
        _Career_AutoSimulateCupMatchesUntilHumanTeam();
    else if (type == 2)
        _Career_AutoSimulatePendingMatchUntilHumanTeam();
    else
        _Career_AutoSimulateLeagueFixturesUntilHumanTeam();

    _Career_StoreCompetitionSlotStateToBuffer(slotPtr);
}

/* ================================================================
 * _Career_ProcessCurrentCompetitionSlotAfterMatch  (SWOS2.s line 36320)
 *
 * Processes the current slot after a match. Panics if
 * careerSlotProcessingInProgress is set. Dispatches by type:
 * Type 0: AutoPlayOrSimulateLeagueFixtureLoop + maybe rebuild caches
 * Type 1: AutoPlayCupMatchAndAdvanceLoop
 * Type 2: UpdateHybridCompetitionProgressAndMaybeAdvanceSlot
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_ProcessCurrentCompetitionSlotAfterMatch(void)
{
    uint8_t *entry = _TeamSelectionKeyCache_GetCurrentListPtr();

    if (careerSlotProcessingInProgress != 0) {
        panicReasonCode = 0x3490;
        _PanicHaltLoop();
    }

    int16_t slotIdx = (int16_t)(int8_t)entry[0];
    uint8_t *slotPtr = careerCompetitionSlotPtrTable[slotIdx];

    _Career_LoadCompetitionSlotStateFromBuffer(slotPtr);

    uint16_t type = *(uint16_t *)slotPtr;
    if (type == 1) {
        _Career_AutoPlayCupMatchAndAdvanceLoop();
        _Career_StoreCompetitionSlotStateToBuffer(slotPtr);
    } else if (type == 2) {
        _Career_UpdateHybridCompetitionProgressAndMaybeAdvanceSlot();
        _Career_StoreCompetitionSlotStateToBuffer(slotPtr);
    } else {
        /* Type 0: league */
        _Career_AutoPlayOrSimulateLeagueFixtureLoop();
        _Career_StoreCompetitionSlotStateToBuffer(slotPtr);
        /* slotPtr+0x2C = compId/slotId word (DIYCompetition.compId field) */
        if (*(uint16_t *)&((DIYCompetition *)slotPtr)->compId != 0)
            return;
        /* slotPtr+0x60 = first byte of competition's extended (DIYCompetitionEx)
         * region = minSubstitutes */
        if (((DIYCompetitionEx *)slotPtr)->minSubstitutes != 0)
            return;
        _DIY_RebuildCompetitionAndTeamSelectionCaches();
    }
}

/* ================================================================
 * _Career_OpenCompetitionSlotMenuAndApplyChanges  (SWOS2.s line 36216)
 *
 * Opens the competition slot menu for the current entry, applies
 * any changes (match sim, cup progress, etc.), then stores state
 * back. Also handles post-match status updates and cache rebuild.
 *
 * in: d2 = menu parameter (1 from PlayMatchMenuLoop, 0 from NextMatch)
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_OpenCompetitionSlotMenuAndApplyChanges(uint16_t d2)
{
    careerMatchSimulatedFlag = 0;

    uint8_t *entry = _TeamSelectionKeyCache_GetCurrentListPtr();
    viewRandomPhase = entry[1];

    int16_t slotIdx = (int16_t)(int8_t)entry[0];
    uint8_t *slotPtr = careerCompetitionSlotPtrTable[slotIdx];

    _Career_LoadCompetitionSlotStateFromBuffer(slotPtr);

    /* Slot type is stored as a native uint16 word — same byte order
     * that _Career_LoadCompetitionSlotStateFromBuffer / _Career_StoreCompetitionSlotStateToBuffer
     * read/write. Using BE_READ_U16 here mis-routes cup slots to the
     * league menu on LE hosts, which then crashes sorting an
     * uninitialized league table. */
    uint16_t type = *(uint16_t *)slotPtr;

    /* [bug23-dispatch] Diagnostic for the View-Result-on-semi-final
     * crash: log the slot type the dispatch sees + the cache entry
     * + currentCompetitionPtr's first byte so we can see why a
     * World Cup knockout is routing to the LEAGUE branch. */
    {
        extern uint8_t *currentCompetitionPtr;
        const uint8_t *cp = (const uint8_t *)(uintptr_t)currentCompetitionPtr;
        SWOS_DBG("[bug23-dispatch] slotIdx=%d slotPtr=%p type=%u "
                        "(branch=%s) entry[0..9]=%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X "
                        "currentCompetitionPtr=%p comp[0..3]=%02X %02X %02X %02X\n",
                (int)slotIdx, (void *)slotPtr, (unsigned)type,
                type == 1 ? "CUP" : (type == 2 ? "HYBRID" : "LEAGUE"),
                entry[0], entry[1], entry[2], entry[3], entry[4],
                entry[5], entry[6], entry[7], entry[8], entry[9],
                (void *)cp,
                cp ? cp[0] : 0, cp ? cp[1] : 0, cp ? cp[2] : 0, cp ? cp[3] : 0);
    }
    if (type == 1) {
        /* Cup */
        /* The flat slot load above restored bracketScratch.teamOffsetTable,
         * which caches selTeams *indices* (step-4 rework: was BE byte offsets
         * on the Amiga). Those indices stay valid on the Amiga (selTeams is a
         * fixed-address BSS array with stable contents) but go stale on SDL
         * because the careerFileBuffer overlay repopulates selTeams between
         * store and load — typically slots 62/63 end up pointing at
         * selTeams[0] (the player's frozen copy) instead of the real team,
         * dragging the player into the wrong fixture (locked VIEW RESULT /
         * squadChangesAllowed=0) and desyncing the keycache that
         * _DIY_RecordCupBracketProgress walks below (panic 0x6666). cupTeamIds
         * was restored consistent with the current selTeams, so re-derive the
         * index cache. Done HERE (the player-facing cup menu, run once per
         * open) rather than inside _Career_LoadCompetitionSlotStateFromBuffer,
         * which is also called from the auto-sim/auto-play loops where selTeams
         * may not match the loaded competition and a rebuild would spin. */
        _DIY_BuildTeamSelectionPointerCache();
        /* Auto-simulate CPU-only fixtures BEFORE the menu draws so that
         * currentMatchIndex points at the career team's fixture when the
         * cup menu first opens. Otherwise _DIY_CompareCurrentMatchTeamCategories
         * sees a CPU-vs-CPU pair (both teamControls=1), returns 1, and
         * _DesignDIYCup_UpdateMenuEntriesVisibilityAndSelection takes the
         * VIEW branch — landing the cursor on aViewResult instead of aMatch.
         *
         * Gate on entry[6] (count of human-team fixtures in this round).
         * AutoSimulateCupMatchesUntilHumanTeam exits only when a human-
         * controlled team appears in the next fixture — if the player has
         * been eliminated from this cup, *no* future fixture has a human
         * team and the loop runs forever (cup result display freeze on
         * NEXT MATCH at end of season). When entry[6] == 0 there is no
         * player fixture to scroll the cursor onto anyway, so skipping
         * the deviation costs nothing; the asm's ProcessAfterMatch path
         * (via AutoPlayCupMatchAndAdvanceLoop) handles the elimination
         * round correctly because UpdateStatsAfterMatch sets
         * careerSlotProcessingInProgress = roundComplete after every
         * match, including CPU-only ones, giving that loop a real exit.
         *
         * The original asm runs auto-sim only after the menu (in
         * Career_ProcessCurrentCompetitionSlotForMatch, reached only when
         * entry[6] > 0). Gating here matches that condition. */
        TeamSelectionKeyCacheEntry *cacheEntry =
            (TeamSelectionKeyCacheEntry *)_TeamSelectionKeyCache_GetCurrentListPtr();
        if (cacheEntry->remainingElements != 0) {
            _Career_AutoSimulateCupMatchesUntilHumanTeam();
        }
        _ShowDIYCupMenuWithParams(1, 0, d2);
        if (diyCompetitionStart.base.roundComplete != 0)
            _DIY_UpdateCompetitionProgress();
    } else if (type == 2) {
        /* Hybrid */
        _DIY_EditCompetitionSlot(1, 0, d2);
        if (diyFileBufferCopy.header.competitionFinished != 0)
            _DIY_AdvanceToNextCompetitionSlot();
    } else {
        /* League */
        _ShowDiyLeaguesTournamentMenu(1, 0, d2);
    }

    _Career_StoreCompetitionSlotStateToBuffer(slotPtr);

    if (careerMatchSimulatedFlag != 0) {
        _BuildCareerTeamUnavailablePlayerList();
        if (_ShouldApplyRandomPlayerStatusAfterSim() == 0)
            _ClearCareerMatchPlayerStatusBucket();
    }

    /* Rebuild caches if competition is done (type==0, no round/phase left) */
    if (*(uint16_t *)&((DIYCompetition *)slotPtr)->compId != 0)
        return;
    if (*(uint16_t *)slotPtr != 0)
        return;
    if (((DIYCompetitionEx *)slotPtr)->minSubstitutes != 0)
        return;
    _DIY_RebuildCompetitionAndTeamSelectionCaches();
}

/* ================================================================
 * _CareerPlayMatch  (SWOS2.s line 36120)
 *
 * Post-match loop: processes remaining competition slots and
 * advances until the season is finished. Panics if careerFinished
 * is 0 (no more matches flagged).
 * ================================================================ */
__attribute__((used, externally_visible))
void _CareerPlayMatch(void)
{
    if (careerFinished == 0) {
        panicReasonCode = 0x8342;
        _PanicHaltLoop();
    }

    if (careerSlotProcessingInProgress == 0) {
        _Career_ProcessCurrentCompetitionSlotAfterMatch();
    }

    _Career_AdvanceToNextTeamSelectionKeyCacheEntry();

    while (seasonFinished == 0) {
        _Career_ProcessCurrentCompetitionSlotAfterMatch();
        _Career_AdvanceToNextTeamSelectionKeyCacheEntry();
    }
}

/* ================================================================
 * _Career_PlayMatchMenuLoop  (SWOS2.s line 36138)
 *
 * Interactive match menu loop. Skips auto-processed slots,
 * opens the competition menu for the current slot, then handles
 * post-menu state. Panics if careerFinished != 0.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_PlayMatchMenuLoop(void)
{
    if (careerFinished != 0) {
        panicReasonCode = 0x8340;
        _PanicHaltLoop();
    }

    uint8_t *entry = _TeamSelectionKeyCache_GetCurrentListPtr();

    /* Skip auto-processed slots (byte[6] <= 0 or negative) */
    for (;;) {
        int8_t b6 = (int8_t)entry[6];
        if (b6 > 0)
            break;  /* found an active slot */
        _Career_ProcessCurrentCompetitionSlotAfterMatch();
        _Career_AdvanceToNextTeamSelectionKeyCacheEntry();
        entry = _TeamSelectionKeyCache_GetCurrentListPtr();
    }

    /* Process for match and open menu */
    _Career_ProcessCurrentCompetitionSlotForMatch();
    _Career_OpenCompetitionSlotMenuAndApplyChanges(1);

    if (careerSlotProcessingInProgress == 0) {
        entry = _TeamSelectionKeyCache_GetCurrentListPtr();
        int8_t b6 = (int8_t)entry[6];
        if (b6 > 0) {
            _Career_ProcessCurrentCompetitionSlotForMatch();
            goto done_no_advance_pml;
        }
        _Career_ProcessCurrentCompetitionSlotAfterMatch();
    }
    _Career_AdvanceToNextTeamSelectionKeyCacheEntry();

done_no_advance_pml:
    _Career_UpdateCareerFinishedFlag();
    entry = _TeamSelectionKeyCache_GetCurrentListPtr();
    viewRandomPhase = entry[1];
}

/* ================================================================
 * _Career_NextMatch  (SWOS2.s line 36177)
 *
 * Advances to the next match in the career. Panics if
 * seasonFinished is set. Checks current slot type and dispatches
 * to either PlayMatchMenuLoop or opens the slot menu directly.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_NextMatch(void)
{
    if (seasonFinished != 0) {
        panicReasonCode = 0x8341;
        _PanicHaltLoop();
    }

    uint8_t *entry = _TeamSelectionKeyCache_GetCurrentListPtr();
    int16_t slotIdx = (int16_t)(int8_t)entry[0];
    uint8_t *slotPtr = careerCompetitionSlotPtrTable[slotIdx];

    /* If type==0 (league) and league.hasGroupsDiy != 0, use PlayMatchMenuLoop */
    if (*(uint16_t *)slotPtr == 0 &&
        ((DIYCompetitionEx *)slotPtr)->matchData.league.hasGroupsDiy != 0) {
        _Career_PlayMatchMenuLoop();
        return;
    }

    _Career_OpenCompetitionSlotMenuAndApplyChanges(0);

    if (careerSlotProcessingInProgress == 0) {
        entry = _TeamSelectionKeyCache_GetCurrentListPtr();
        int8_t b6 = (int8_t)entry[6];
        if (b6 > 0) {
            _Career_ProcessCurrentCompetitionSlotForMatch();
            goto done_no_advance_next;
        }
        _Career_ProcessCurrentCompetitionSlotAfterMatch();
    }
    _Career_AdvanceToNextTeamSelectionKeyCacheEntry();

done_no_advance_next:
    _Career_UpdateCareerFinishedFlag();
    entry = _TeamSelectionKeyCache_GetCurrentListPtr();
    viewRandomPhase = entry[1];
}

/* ================================================================
 * Chunk 94 — Career Auto-Simulate & DIY Competition Slot Advancement
 *
 * 8 functions ported from SWOS2.s lines 36261–36366, 31498–31536.
 * ================================================================ */

/* fixtureTeam1CompPtr / fixtureTeam2CompPtr set by _DIY_GetCurrentFixtureTeamPtrs */
extern uint8_t *fixtureTeam1CompPtr;
extern uint8_t *fixtureTeam2CompPtr;

/* ================================================================
 * _Career_AutoSimulateLeagueFixturesUntilHumanTeam (SWOS2.s line 36261)
 *
 * Loop: get current fixture teams, if both are AI (controls != 1)
 * then simulate the fixture and repeat. Stops when a human team
 * is involved in the current fixture.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_AutoSimulateLeagueFixturesUntilHumanTeam(void)
{
    for (;;) {
        _DIY_GetCurrentFixtureTeamPtrs();
        uint8_t *comp1 = fixtureTeam1CompPtr;
        uint8_t *comp2 = fixtureTeam2CompPtr;

        /* Check team1: DIYTeamStats teamSelOffset (+0x2B4) is selTeamsPtr-relative */
        TeamFile *team1 = (TeamFile *)diy_resolve_team(((DIYTeamStats *)(comp1 + DIY_STANDINGS_OFFSET))->teamSelOffset);
        if (team1->teamControls != 1) return;  /* human team found */

        /* Check team2 */
        TeamFile *team2 = (TeamFile *)diy_resolve_team(((DIYTeamStats *)(comp2 + DIY_STANDINGS_OFFSET))->teamSelOffset);
        if (team2->teamControls != 1) return;  /* human team found */

        /* Both AI: simulate */
        _DIY_PlayOrSimulateLeagueFixture(0, 0);
    }
}

/* ================================================================
 * getCurrentMatchTeamSlots — C port of DIY_GetCurrentMatchTeamSlots
 * (SWOS2.s line 32656)
 *
 * Computes two pointers into diyCompetitionStart for the current
 * cup match's team slots (based on team grid at the current match index).
 * ================================================================ */
static void getCurrentMatchTeamSlots(uint8_t **slot1Out, uint8_t **slot2Out)
{
    uint8_t *base = (uint8_t *)&diyCompetitionStart;
    uint16_t matchIdx = diyCompetitionStart.matchData.cup.currentMatchIndex;
    uint16_t off = matchIdx * 2;
    uint8_t *cupTeamGrid = diyCompetitionStart.teamGridPairings;

    int8_t t1 = (int8_t)cupTeamGrid[off];
    *slot1Out = base + (int16_t)t1 * 4;

    int8_t t2 = (int8_t)cupTeamGrid[off + 1];
    *slot2Out = base + (int16_t)t2 * 4;
}

/* Like getCurrentMatchTeamSlots, but returns clean slot indices for use
 * with bracketScratch.teamOffsetTable[] instead of the asm-style "fake"
 * pointers `base + slotIdx*4`. */
static void getCurrentMatchTeamSlotIndices(int16_t *slot1Idx, int16_t *slot2Idx)
{
    uint16_t matchIdx = diyCompetitionStart.matchData.cup.currentMatchIndex;
    uint8_t *grid = diyCompetitionStart.teamGridPairings;
    *slot1Idx = (int16_t)(int8_t)grid[matchIdx * 2];
    *slot2Idx = (int16_t)(int8_t)grid[matchIdx * 2 + 1];
}

/* ================================================================
 * _Career_AutoSimulateCupMatchesUntilHumanTeam (SWOS2.s line 36279)
 *
 * Loop: get current cup match team slots, if both are AI then
 * simulate the cup match and repeat. Stops when a human team
 * is involved.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_AutoSimulateCupMatchesUntilHumanTeam(void)
{
#ifdef SWOS_DEBUG_AUTOSKIP
    int iter = 0;
    DIYCompetitionEx *bex0 = &diyCompetitionStart;
    SWOS_AUTOSKIP_LOG("[bug4-cup] AutoSimUntilHuman ENTER compId=0x%04X "
                      "roundIdx=%d/%d matchIdx=%d/%d roundComplete=%d "
                      "competitionFinished=%d",
                      (unsigned)*(uint16_t *)&bex0->base.compId,
                      (int)bex0->base.currentRoundIndex, (int)bex0->base.numRounds,
                      (int)bex0->matchData.cup.currentMatchIndex,
                      (int)bex0->base.totalMatchesInRound,
                      (int)bex0->base.roundComplete,
                      (int)bex0->base.competitionFinished);
#endif
    for (;;) {
        int16_t slot1Idx, slot2Idx;
        getCurrentMatchTeamSlotIndices(&slot1Idx, &slot2Idx);

        /* Read team pointer from bracketScratch.teamOffsetTable[slotIdx] */
        DIYCompetitionEx *baseEx = &diyCompetitionStart;
        TeamFile *team1 = (TeamFile *)diy_resolve_team(baseEx->bracketScratch.teamOffsetTable[slot1Idx]);
        if (team1->teamControls != 1) {
#ifdef SWOS_DEBUG_AUTOSKIP
            SWOS_AUTOSKIP_LOG("[bug4-cup] AutoSimUntilHuman EXIT (team1 human/coach) "
                              "iter=%d slot1=%d team1Ctrl=%d", iter,
                              (int)slot1Idx, (int)team1->teamControls);
#endif
            return;
        }

        TeamFile *team2 = (TeamFile *)diy_resolve_team(baseEx->bracketScratch.teamOffsetTable[slot2Idx]);
        if (team2->teamControls != 1) {
#ifdef SWOS_DEBUG_AUTOSKIP
            SWOS_AUTOSKIP_LOG("[bug4-cup] AutoSimUntilHuman EXIT (team2 human/coach) "
                              "iter=%d slot2=%d team2Ctrl=%d", iter,
                              (int)slot2Idx, (int)team2->teamControls);
#endif
            return;
        }

        /* Both AI: simulate */
#ifdef SWOS_DEBUG_AUTOSKIP
        iter++;
        if (iter > 200) {
            SWOS_AUTOSKIP_LOG("[bug4-cup] AutoSimUntilHuman EMERGENCY BAIL iter>%d — "
                              "infinite loop suspected, returning to avoid hang", iter);
            return;
        }
#endif
        _DIY_PlayCupMatchAndAdvance(0, 0);

        /* The asm loop exits ONLY when a human/coach team appears in the next
         * fixture. On SDL the career team can be absent from the round this loop
         * is scanning — the bracket grid and the fixture keycache can disagree
         * about which round holds the player's match, and draw->replay cups
         * (roundPenalties==2) leave matches unresolved so the round never clears
         * normally — so no human is ever found and the loop re-sims the same
         * round forever (cup freeze). _DIY_PlayCupMatchAndAdvance sets
         * roundComplete (and wraps currentMatchIndex to 0) once every fixture in
         * the round has been simulated; return on that so the caller
         * (_Career_OpenCompetitionSlotMenuAndApplyChanges) runs the round-advance
         * / replay setup and the cup actually progresses to the player's round
         * (or finishes). In the normal case the human team is found mid-round and
         * we return above before roundComplete is set, so this never fires. */
        if (diyCompetitionStart.base.roundComplete != 0)
            return;
    }
}

/* ================================================================
 * _Career_AutoSimulatePendingMatchUntilHumanTeam (SWOS2.s line 36297)
 *
 * Checks the current competition slot type and dispatches to
 * either league or cup auto-simulate.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_AutoSimulatePendingMatchUntilHumanTeam(void)
{
    DIYCompetition *bufComp = &diyFileBufferCopy.header;
    uint8_t *buf = (uint8_t *)&diyFileBufferCopy;
    uint16_t slotIdx = bufComp->roundComplete;
    DIYTournamentSlot *slot = (DIYTournamentSlot *)(buf + (uint32_t)slotIdx * 0x76 + 0x15A);

    if (slot->isLeagueFormat != 0)
        _Career_AutoSimulateLeagueFixturesUntilHumanTeam();
    else
        _Career_AutoSimulateCupMatchesUntilHumanTeam();
}

/* ================================================================
 * _Career_AutoPlayOrSimulateLeagueFixtureLoop (SWOS2.s line 36308)
 *
 * Loop: simulate league fixtures until the competition reaches
 * a state where careerSlotProcessingInProgress is set (indicating
 * a human match is pending) and the competition's $1C0 field is
 * nonzero (fixtures remain).
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_AutoPlayOrSimulateLeagueFixtureLoop(void)
{
    for (;;) {
        uint16_t field1C0 = diyCompetitionStart.leagueCurrentSlot;
        if (field1C0 != 0 && careerSlotProcessingInProgress != 0)
            return;
        _DIY_PlayOrSimulateLeagueFixture(0, 0);
    }
}

/* ================================================================
 * _Career_AutoPlayCupMatchAndAdvanceLoop (SWOS2.s line 36325)
 *
 * Loop: simulate cup matches until careerSlotProcessingInProgress
 * is set and the cup's $54 field is nonzero. On exit, calls
 * DIY_UpdateCompetitionProgress.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_AutoPlayCupMatchAndAdvanceLoop(void)
{
#ifdef SWOS_DEBUG_AUTOSKIP
    int iter = 0;
    DIYCompetitionEx *bex0 = &diyCompetitionStart;
    SWOS_AUTOSKIP_LOG("[bug4-cup] AutoPlayCupLoop ENTER compId=0x%04X "
                      "roundIdx=%d/%d matchIdx=%d/%d roundComplete=%d "
                      "competitionFinished=%d careerSlotProcessingInProgress=%d",
                      (unsigned)*(uint16_t *)&bex0->base.compId,
                      (int)bex0->base.currentRoundIndex, (int)bex0->base.numRounds,
                      (int)bex0->matchData.cup.currentMatchIndex,
                      (int)bex0->base.totalMatchesInRound,
                      (int)bex0->base.roundComplete,
                      (int)bex0->base.competitionFinished,
                      (int)careerSlotProcessingInProgress);
#endif
    for (;;) {
        uint16_t field54 = (uint16_t)diyCompetitionStart.base.roundComplete;
        if (field54 != 0 && careerSlotProcessingInProgress != 0) {
#ifdef SWOS_DEBUG_AUTOSKIP
            SWOS_AUTOSKIP_LOG("[bug4-cup] AutoPlayCupLoop EXIT (round complete + processing) "
                              "iter=%d", iter);
#endif
            _DIY_UpdateCompetitionProgress();
            return;
        }
#ifdef SWOS_DEBUG_AUTOSKIP
        iter++;
        if (iter > 200) {
            SWOS_AUTOSKIP_LOG("[bug4-cup] AutoPlayCupLoop EMERGENCY BAIL iter>%d — "
                              "infinite loop suspected; roundComplete=%d "
                              "careerSlotProcessingInProgress=%d",
                              iter, (int)field54, (int)careerSlotProcessingInProgress);
            return;
        }
#endif
        _DIY_PlayCupMatchAndAdvance(0, 0);
    }
}

/* ================================================================
 * _Career_UpdateHybridCompetitionProgressAndMaybeAdvanceSlot
 * (SWOS2.s line 36341)
 *
 * For hybrid competition slots: checks slot type and dispatches
 * to either cup or league loop, then copies progress fields from
 * diyCompetitionStart back to diyFileBufferCopy, and optionally
 * advances to the next competition slot.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_UpdateHybridCompetitionProgressAndMaybeAdvanceSlot(void)
{
    DIYCompetition *bufComp = &diyFileBufferCopy.header;
    uint8_t *buf = (uint8_t *)&diyFileBufferCopy;
    uint16_t slotIdx = bufComp->roundComplete;
    DIYTournamentSlot *slot = (DIYTournamentSlot *)(buf + (uint32_t)slotIdx * 0x76 + 0x15A);

    DIYCompetition *comp = &diyCompetitionStart.base;

    if (slot->isLeagueFormat != 0) {
        /* League type slot */
        _Career_AutoPlayOrSimulateLeagueFixtureLoop();

        /* Copy progress fields: comp→buf */
        bufComp->matchStarted = comp->matchStarted;
        bufComp->careerTeamInvolved = comp->careerTeamInvolved;
        bufComp->resultCode     = comp->resultCode;

        /* Advance if diyCompetitionStart.minSubstitutes == 0 */
        if (diyCompetitionStart.minSubstitutes == 0)
            _DIY_AdvanceToNextCompetitionSlot();
    } else {
        /* Cup type slot */
        _Career_AutoPlayCupMatchAndAdvanceLoop();

        /* Copy progress fields: comp→buf (4 fields) */
        bufComp->matchStarted  = comp->matchStarted;
        bufComp->careerTeamInvolved  = comp->careerTeamInvolved;
        bufComp->resultCode      = comp->resultCode;
        bufComp->hasGroups       = comp->roundIterator;

        /* Advance if competition finished */
        if (comp->competitionFinished != 0)
            _DIY_AdvanceToNextCompetitionSlot();
    }
}

/* ================================================================
 * _DIY_AdvanceToNextCompetitionSlot (SWOS2.s line 31498)
 *
 * Advances to the next competition slot in the diyFileBufferCopy.
 * If max slots reached, marks competition as finished. Otherwise
 * loads the next slot via DIY_LoadCurrentCompetitionSlot.
 * For career modes (gameType 3/4), does additional post-load
 * processing via the asm dispatch.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_AdvanceToNextCompetitionSlot(void)
{
    uint8_t *buf = (uint8_t *)&diyFileBufferCopy;
    DIYCompetition *bufComp = &diyFileBufferCopy.header;

    /* Already at end? */
    if (bufComp->currentRoundIndex != 0)
        return;

    uint16_t currentSlot = bufComp->roundComplete;
    uint16_t maxSlots = bufComp->totalMatchesInRound;

    if ((uint16_t)(currentSlot + 1) == maxSlots) {
        /* Reached max slots: mark competition finished */
        bufComp->isActive = 2;
        bufComp->currentRoundIndex = 1;
        return;
    }

    /* Determine slot type */
    uint8_t *slot = buf + (uint32_t)currentSlot * 0x76;
    DIYTournamentSlot *slotRec = (DIYTournamentSlot *)(slot + 0x15A);
    uint8_t *comp = (uint8_t *)&diyCompetitionStart;

    if (slotRec->isLeagueFormat != 0) {
        /* League type: build seeded bracket */
        _DIY_BuildSeededBracketAndUpdateScores(buf, slot, comp, comp);
    } else {
        /* Cup type: copy team grid */
        _DIY_CopyTeamGridFromCompToDIY(buf, slot, comp, comp);
    }

    /* Increment current slot index */
    bufComp->roundComplete = currentSlot + 1;

    /* Load the new slot */
    _DIY_LoadCurrentCompetitionSlot();

    if (careerLoadedFlag != 0)
        return;

    /* Career modes: additional post-load processing (was loc_12D4B0) */
    if (gameType == 4 || gameType == 3)
        _DIY_PostLoadCompetitionDispatch();
}

/* ================================================================
 * NationalTeamAddPlayer menu callbacks (Chunk 108)
 * Set careerNationalTeamAddPlayerType and exit menu.
 * Asm: NationalTeamAddPlayerHomeOnSelect, NationalTeamAddPlayerAbroadOnSelect,
 *      NationalTeamAddPlayerExit
 * ================================================================ */
__attribute__((used, externally_visible))
void _NationalTeamAddPlayerHomeOnSelect(void)
{
    careerNationalTeamAddPlayerType = 0;
    _SetExitMenuFlag();
}

__attribute__((used, externally_visible))
void _NationalTeamAddPlayerAbroadOnSelect(void)
{
    careerNationalTeamAddPlayerType = 1;
    _SetExitMenuFlag();
}

__attribute__((used, externally_visible))
void _NationalTeamAddPlayerExit(void)
{
    careerNationalTeamAddPlayerType = 0xFFFF;
    _SetExitMenuFlag();
}

/* ================================================================
 * Chunk 148: Career_UpdateNationalTeamMenuEntries,
 *            Career_UpdateRemovePlayersMenuEntry
 * ================================================================ */

extern uint16_t careerNumPlayers;
extern uint16_t careerNationalTeamHasPendingAddPlayer;
extern uint32_t careerIntlPlayersRemoveMask;
extern MenuEntry menuEntries[];
extern uint8_t aASquad[];              /* "%a SQUAD" */
extern uint8_t aSelect1MoreInt[];      /* "SELECT 1 MORE INTERNATIONAL SQUAD PLAYE..." */
extern uint8_t aSelect0MoreInt[];      /* "SELECT %0 MORE INTERNATIONAL SQUAD PLAY..." */
extern uint8_t aRemovePlayer[];        /* "REMOVE PLAYER" */
extern uint8_t aRemovePlayers[];       /* "REMOVE PLAYERS" */

extern void _PrintFormatted(const uint8_t *fmt, uint8_t *out,
                            uint16_t n0, uint16_t n1, uint16_t n2, uint16_t n3,
                            uint16_t n4, uint16_t n5, uint16_t n6,
                            const uint8_t *strA, const uint8_t *strB, const uint8_t *strC);
extern uint8_t *_StringCopy(const uint8_t *src, uint8_t *dst);

/* Career_UpdateNationalTeamMenuEntries: update visibility of national team
 * squad menu entries based on careerNumPlayers and pending add state. */
__attribute__((used, externally_visible))
void _Career_UpdateNationalTeamMenuEntries(void)
{
    /* Initially hide entries 24 and 25 */
    menuEntries[24].isInvisible = 1;
    menuEntries[25].isInvisible = 1;

    /* Show entry 24 if careerNumPlayers > 16, entry 25 if > 17 */
    if (careerNumPlayers > 0x10) {
        menuEntries[24].isInvisible = 0;
        if (careerNumPlayers > 0x11) {
            menuEntries[25].isInvisible = 0;
        }
    }

    /* Format "%a SQUAD" with team name into entry 0 */
    _PrintFormatted(aASquad, (uint8_t *)menuEntries[0].fg,
                    0, 0, 0, 0, 0, 0, 0,
                    (const uint8_t *)&careerTeam.team.teamName[0], 0, 0);

    /* Default state: show entry 4, hide entry 5, show entry 3, hide entry 2 */
    menuEntries[4].isInvisible = 0;
    menuEntries[5].isInvisible = 1;
    menuEntries[3].isInvisible = 0;
    menuEntries[2].isInvisible = 1;
    menuEntries[0].y = 0x1A;

    if (careerNationalTeamHasPendingAddPlayer != 0) {
        /* Pending add: hide entry 4, show entry 5, hide entry 3, show entry 2 */
        menuEntries[4].isInvisible = 1;
        menuEntries[5].isInvisible = 0;
        menuEntries[3].isInvisible = 1;
        menuEntries[2].isInvisible = 0;
        menuEntries[0].y = 0x12;
    }

    /* Entry 1: "SELECT X MORE INTERNATIONAL SQUAD PLAYERS" */
    menuEntries[1].isInvisible = 1;
    if (careerNationalTeamHasPendingAddPlayer == 0 && careerNumPlayers != 0x14) {
        menuEntries[1].isInvisible = 0;
        uint8_t *dst = (uint8_t *)menuEntries[1].fg;
        uint16_t remaining = 0x14 - careerNumPlayers;
        if (remaining == 1) {
            _StringCopy(aSelect1MoreInt, dst);
        } else {
            _PrintFormatted(aSelect0MoreInt, dst,
                            remaining, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }
    }
}

/* Career_UpdateRemovePlayersMenuEntry: show/hide "REMOVE PLAYER(S)" entry
 * based on careerIntlPlayersRemoveMask bit count. */
__attribute__((used, externally_visible))
void _Career_UpdateRemovePlayersMenuEntry(void)
{
    menuEntries[6].isInvisible = 1;

    if (careerNationalTeamHasPendingAddPlayer != 0)
        return;

    if (careerIntlPlayersRemoveMask == 0)
        return;

    menuEntries[6].isInvisible = 0;

    /* Count bits set in mask */
    uint32_t mask = careerIntlPlayersRemoveMask;
    uint16_t count = 0;
    int16_t bit;
    for (bit = 31; bit >= 0; bit--) {
        if (mask & (1UL << bit))
            count++;
    }

    menuEntries[6].fg = (uintptr_t)aRemovePlayer;   /* singular */
    if (count != 1) {
        menuEntries[6].fg = (uintptr_t)aRemovePlayers;  /* plural */
    }
}

/* ================================================================
 * Chunk 151: GetTeamLeague
 *
 * in: d0 = team number (high byte = file ext, low byte = team id)
 * out: d0.w = league number (signed)
 *
 * Loads the team file, scans for matching team number in the
 * teamFileBuffer, returns the league byte from that team record.
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _GetTeamLeague(uint16_t teamNumber)
{
    uint16_t extNumber = teamNumber >> 8;
    teamsLoaded = 0;
    skipLeagueNumbers = 1;
    _LoadTeamFile(extNumber);
    skipLeagueNumbers = 0;
    teamsLoaded = 0;

    /* Scan teamFileBuffer+2 for matching team number. teamNumber is
     * the Amiga BE-packed `(country << 8) | ordinal` form, so build
     * the same value from the per-record byte pair. */
    TeamFileEx *p = (TeamFileEx *)(&teamFileBuffer + 2);
    while (PACK_BE_U16(p->team.countryNumber, p->team.teamOrdinal) != teamNumber)
        p++;

    return (int8_t)p->team.league;
}

/* ================================================================
 * Career_CopyUnavailablePlayerToCareerTeamSlot (SWOS2.s line 69229)
 *
 * Copies an international player from another team into a career
 * team's "unavailable" slot. Updates the name table, lookup table,
 * copies 38 bytes of player data (preserving one control byte),
 * clears status bytes, and refreshes bench player numbers.
 *
 * Asm: a0=srcPlayer, a1=dstTeam, a2=srcName,
 *      d2=srcTeamNumber, d1=srcPlayerIndex, d3=dstSlotIndex
 * ================================================================ */
extern uint8_t careerUnavailablePlayerNames[];
extern uint8_t careerUnavailablePlayerLookupTable[];

extern uint8_t *_StringCopy(const uint8_t *src, uint8_t *dst);
extern void _SetBenchPlayersNumbers(TeamFileEx *team);

__attribute__((used, externally_visible))
void _Career_CopyUnavailablePlayerToCareerTeamSlot(uint8_t *srcPlayer, uint8_t *dstTeam,
    uint8_t *srcName, uint16_t srcTeamNumber, uint16_t srcPlayerIndex, uint16_t dstSlotIndex)
{
    /* Copy player name to careerUnavailablePlayerNames (17-byte entries) */
    uint8_t *namesDst = careerUnavailablePlayerNames + (uint16_t)(dstSlotIndex * 17);
    _StringCopy(srcName, namesDst);

    /* Store team number and player index in lookup table. Each slot is
     * 4 bytes: { uint16 teamNumber, uint16 playerIndex }. */
    {
        uint16_t *uplBase = (uint16_t *)careerUnavailablePlayerLookupTable;
        uplBase[dstSlotIndex * 2 + 0] = srcTeamNumber;
        uplBase[dstSlotIndex * 2 + 1] = srcPlayerIndex;
    }

    /* Save shirtNumber before overwriting with player data copy */
    PlayerFile *dstPf = (PlayerFile *)(dstTeam + 76);  /* PlayerFile at TeamFileEx offset 0x4C */
    PlayerFile *srcPf = (PlayerFile *)(srcPlayer + 76);
    uint8_t savedShirtNumber = dstPf->shirtNumber;

    /* Copy 19 words (38 bytes = one PlayerFile entry worth of data) */
    uint16_t *src = (uint16_t *)srcPf;
    uint16_t *dst = (uint16_t *)dstPf;
    for (int16_t i = 0x12; i >= 0; i--) {
        *dst++ = *src++;
    }

    /* Restore shirtNumber (control byte preserved across copy) */
    dstPf->shirtNumber = savedShirtNumber;

    /* Clear goal stats */
    dstPf->goalStats[0] = 0;
    dstPf->goalStats[1] = 0;
    dstPf->goalStats[2] = 0;
    dstPf->goalStats[3] = 0;

    /* Update bench player numbers for the career team */
    _SetBenchPlayersNumbers(&careerTeam);
}

/* ================================================================
 * GetPlayerPositionFromCurrent  (SWOS2.s line 69400)
 *
 * Searches currentMatchPlayers[0..19] for the given player index.
 * Returns the position (0-based index) where found.
 * Panics (code 0x5771) if the player is not found.
 *
 * Asm: in d1 = playerIndex, out d1 = position
 * ================================================================ */
extern uint8_t currentMatchPlayers[];

__attribute__((used, externally_visible))
uint16_t _GetPlayerPositionFromCurrent(uint16_t playerIndex)
{
    for (uint16_t i = 0; i < 20; i++) {
        if (currentMatchPlayers[i] == (uint8_t)playerIndex)
            return i;
    }
    panicReasonCode = 0x5771;
    _PanicHaltLoop();
    /* not reached */
}

/* ================================================================
 * Career_RemoveInternationalPlayersByMask  (SWOS2.s line 69359)
 *
 * Iterates over 20 international player slots in careerTeam (38
 * bytes per slot). For each bit set in removeMask:
 *   - Clears the lookup table entry (word at slot*4) to 0xFFFF
 *   - Finds the player's position in currentMatchPlayers
 *   - If position >= 16: shifts array entries to remove the player
 *     from that position, rotating them to position 19
 *   - Marks the player as removed (0xFF at offset 0x4F in slot)
 *   - Decrements careerNumPlayers
 *
 * Asm: in d0 = removeMask (32-bit)
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_RemoveInternationalPlayersByMask(uint32_t removeMask)
{
    uint8_t *slot = (uint8_t *)&careerTeam;

    uint16_t *uplBase = (uint16_t *)careerUnavailablePlayerLookupTable;
    for (uint16_t i = 0; i < 20; i++) {
        if (removeMask & (1 << i)) {
            /* Clear lookup table entry's teamNumber slot. */
            uplBase[i * 2] = 0xFFFF;

            /* Find position of this player slot in currentMatchPlayers */
            uint16_t pos = _GetPlayerPositionFromCurrent(i);

            if (pos >= 16) {
                /* Shift currentMatchPlayers entries down from pos+1,
                 * rotating the removed entry to the end (position 19).
                 * Use volatile to prevent GCC from optimizing to memmove. */
                volatile uint8_t *mp = &currentMatchPlayers[pos];
                int16_t count = 18 - (int16_t)pos;
                if (count >= 0) {
                    uint8_t saved = *mp;
                    do {
                        *mp = *(mp + 1);
                        mp++;
                        count--;
                    } while (count >= 0);
                    *mp = saved;
                }
            }

            /* Mark player as removed (playerName[0] = 0xFF) */
            {
                PlayerFile *pf = (PlayerFile *)(slot + 76);
                pf->playerName[0] = (int8_t)0xFF;
            }
            careerNumPlayers--;
        }
        slot += PLAYERFILE_SIZE;  /* advance to next 38-byte player slot */
    }
}

/* ================================================================
 * Career_InitInternationalSquadAndLookupTablesForNewSeason
 *                                              (SWOS2.s line 69360)
 *
 * Initializes the 20 international player slots for a new career
 * season. For each slot:
 *   - Clears the lookup table entry to 0xFFFF
 *   - Sets currentMatchPlayers[slot] = slot
 *   - Gets the player via GetPlayer(selTeamNumber, slot)
 *   - Marks playerName[0] = 0xFF (removed)
 *   - For slots 0-10: assigns default squad slot from
 *     careerDefaultInternationalSquadSlotsTable to shirtNumber
 *   - Modifies positionAndFace: clears bits 5-7, then sets bits 5-6
 *     for all slots except 0 and 11
 * Finally resets careerNumPlayers to 0.
 *
 * Asm: in a0 = teamPtr (unused/dead parameter)
 * ================================================================ */
extern uint8_t careerDefaultInternationalSquadSlotsTable[];
extern uint8_t *_GetPlayer(uint16_t teamNumber, uint16_t playerFileIndex);

__attribute__((used, externally_visible))
void _Career_InitInternationalSquadAndLookupTablesForNewSeason(TeamFileEx *teamPtr)
{
    (void)teamPtr;  /* dead parameter in original asm */

    uint8_t  *squadSlots = careerDefaultInternationalSquadSlotsTable;
    uint16_t *uplBase    = (uint16_t *)careerUnavailablePlayerLookupTable;
    uint16_t  squadSlotIdx = 0;

    for (uint16_t i = 0; i < 20; i++) {
        /* Clear lookup table entry's teamNumber slot. */
        uplBase[i * 2] = 0xFFFF;

        /* Set currentMatchPlayers[i] = i */
        currentMatchPlayers[i] = (uint8_t)i;

        /* Get player pointer for this slot */
        uint8_t *player = _GetPlayer(selTeamNumber, i);
        PlayerFile *pf = (PlayerFile *)(player + 76);  /* PlayerFile at TeamFileEx offset 0x4C */

        /* Mark player as removed (playerName[0] = 0xFF) */
        pf->playerName[0] = (int8_t)0xFF;

        /* For first 11 slots, assign default position from table */
        if (i < 11) {
            pf->shirtNumber = squadSlots[squadSlotIdx++];
        }

        /* Modify positionAndFace:
         * Clear bits 5-7 (andi.w #$FF1F clears bits 5-7 of low byte)
         * Then set bits 5-6 (0x60) for all slots except 0 and 11 */
        uint8_t val = pf->positionAndFace;
        val &= 0x1F;
        if (i != 0 && i != 11) {
            val |= 0x60;
        }
        pf->positionAndFace = val;
    }

    careerNumPlayers = 0;
}


/* ================================================================
 * Chunk 196 — TeamSelectionKeyCache entry builders + Career_SetCurrentCompetitionSlot
 * ================================================================ */

/* Forward declarations for functions called from Career_SetCurrentCompetitionSlot */
extern uint8_t *currentCompetitionPtr;
extern uint8_t *careerCompetitionSlotPtrTable[];
extern uint16_t isNationalSelector;
extern uint16_t teamSelectionKeyCacheOffset;
extern uint16_t teamSelectionKeyCacheIndex;
extern DIYTournamentState diyFileBufferCopy;
extern DIYCompetitionEx diyCompetitionStart;
extern void _Career_LoadCompetitionSlotStateFromBuffer(uint8_t *src);
extern void _Career_StoreCompetitionSlotStateToBuffer(uint8_t *dst);
extern void _Career_InitCompetitionTeamSelection_Dispatch(uint8_t *spec, uint16_t idx);
extern void _Career_ApplyForeignMarketPlayersToSelectedTeams(void);
extern void _InitCompetitionFromRecord(uint16_t index, uint8_t *record);
extern void _DIY_UpdateCompetitionProgressOrAdvanceSlot(uint8_t *blk);
extern void _CopySelectedTeamNumbers(uint16_t count, uint8_t *dest);

/* ================================================================
 * _TeamSelectionKeyCache_AddNextRoundCupEntry
 * (SWOS2.s line 31279)
 *
 * Inserts a new cup entry into the teamSelectionKeyCache for the
 * next-round cup path (competition type 1). Sets cup round string
 * index, team slot, key, stride, and clears flags.
 *
 * Returns: updated output pointer (past the newly written entry).
 * ================================================================ */
static uint8_t *addNextRoundCupEntry(uint8_t **srcPtr, uint16_t idx, uint16_t total,
                                      uint16_t teamId, uint16_t slotLen, uint8_t *base)
{
    uint8_t *src = *srcPtr;
    uint8_t key = *src;
    uint8_t *out = _TeamSelectionKeyCache_InsertSpaceSorted(key, slotLen);
    TeamSelectionKeyCacheEntry *e = (TeamSelectionKeyCacheEntry *)out;
    teamSelectionKeyCacheCount++;
    e->sortKey = *src;
    (*srcPtr)++;

    /* GetNextCupRoundStringIndex(idx, total, subRoundCounter) */
    uint16_t cupRoundIdx = _GetNextCupRoundStringIndex(idx, total, ((DIYCompetition *)base)->subRoundCounter);
    e->cupRoundStringIdx = (uint8_t)cupRoundIdx;
    e->roundIdx          = (uint8_t)idx;
    e->compId            = (uint8_t)teamId;
    e->entryStride       = slotLen;
    e->phaseFlag         = 0;
    e->totalElements     = 0;
    e->remainingElements = 0;

    return out;
}

/* ================================================================
 * _TeamSelectionKeyCache_AddCupEntry
 * (SWOS2.s line 31353)
 *
 * Inserts a new cup entry for the multi-slot competition path
 * (competition type 2+). Uses GetCupRoundStringIndex for the
 * round label. Returns updated output pointer.
 * ================================================================ */
static uint8_t *addCupEntry(uint8_t **srcPtr, uint16_t teamSlot, uint16_t roundIdx,
                             uint16_t total, uint16_t slotLen, uint8_t *base,
                             uint16_t opt)
{
    uint8_t *src = *srcPtr;
    uint8_t key = *src;
    uint8_t *out = _TeamSelectionKeyCache_InsertSpaceSorted(key, slotLen);
    TeamSelectionKeyCacheEntry *e = (TeamSelectionKeyCacheEntry *)out;
    teamSelectionKeyCacheCount++;
    e->sortKey = *src;
    (*srcPtr)++;

    /* GetCupRoundStringIndex: d0=round, d1=slot.isLeagueFormat, d2=total,
     * d3=slot.numTeams, d4=opt. base+0x15A = current slot record. */
    DIYTournamentSlot *slotRec = (DIYTournamentSlot *)(base + 0x15A);
    uint16_t cupRoundIdx = _GetCupRoundStringIndex(roundIdx,
                                                    slotRec->isLeagueFormat,
                                                    total,
                                                    slotRec->numTeams,
                                                    opt);
    e->cupRoundStringIdx = (uint8_t)cupRoundIdx;
    e->roundIdx          = (uint8_t)roundIdx;
    e->compId            = (uint8_t)teamSlot;
    e->entryStride       = slotLen;
    e->phaseFlag         = 0;
    e->totalElements     = 0;
    e->remainingElements = 0;

    return out;
}

/* ================================================================
 * _TeamSelectionKeyCache_BuildFromCompetitionBlock
 * (SWOS2.s line 31190)
 *
 * Builds the team selection key cache from a competition block.
 * Dispatches based on *(uint16_t *)blk:
 *   0 = league type
 *   1 = cup type (single competition)
 *   other = multi-slot competition
 * ================================================================ */
__attribute__((used, externally_visible))
void _TeamSelectionKeyCache_BuildFromCompetitionBlock(uint8_t *blk)
{
    DIYCompetition *comp = (DIYCompetition *)blk;
    uint16_t type = comp->competitionType;

    if (type == 0) {
        /* League type */
        DIYCompetitionEx *baseEx = (DIYCompetitionEx *)blk;
        uint16_t slotLen;

        if (gameType == 4) {
            slotLen = 0x16;
        } else {
            slotLen = comp->numTeams;
            slotLen >>= 1;
            if (slotLen > 0x18)
                slotLen = 0x18;
            slotLen = slotLen * 0xC + 0xA;
        }

        /* +0x1CE = leagueSectionState.phaseStartMonths (per-round month bytes) */
        uint8_t *src = baseEx->leagueSectionState.phaseStartMonths;
        uint16_t count = baseEx->leagueTotalMatches;
        uint16_t teamSlot = *(uint16_t *)&baseEx->base.compId;  /* slot/team index */
        int16_t i = (int16_t)count - 1;

        while (i >= 0) {
            uint8_t key = *src;
            uint8_t *out = _TeamSelectionKeyCache_InsertSpaceSorted(key, slotLen);
            TeamSelectionKeyCacheEntry *e = (TeamSelectionKeyCacheEntry *)out;
            teamSelectionKeyCacheCount++;
            e->sortKey = *src++;
            e->compId = (uint8_t)teamSlot;
            e->entryStride = slotLen;
            e->phaseFlag = 0;
            e->totalElements = 0;
            e->remainingElements = 0;
            out += e->entryStride;  /* adda.w 2(a4),a4 */
            i--;
        }
        return;
    }

    if (type == 1) {
        /* Cup type (single competition) */
        DIYCompetitionEx *baseEx = (DIYCompetitionEx *)blk;
        uint8_t *base = blk;         /* a6: constant base for AddNextRoundCupEntry */
        uint16_t teamId = *(uint16_t *)&baseEx->base.compId;  /* slot/team index */
        /* +0x338 = bracketScratch.progTable */
        uint8_t *src = baseEx->bracketScratch.progTable;
        uint16_t numRounds = comp->numRounds;
        uint16_t teamsPerRound = comp->numTeams;
        uint16_t roundIdx = 0;

        while (roundIdx != numRounds) {
            uint16_t slotLen;
            uint16_t halved = teamsPerRound >> 1;

            if (gameType == 4) {
                slotLen = 0x16;
            } else {
                slotLen = halved;
                if (slotLen > 0x18)
                    slotLen = 0x18;
                slotLen = slotLen * 0xC + 0xA;
            }

            uint8_t *out = addNextRoundCupEntry(&src, roundIdx, numRounds, teamId, slotLen, base);
            TeamSelectionKeyCacheEntry *e = (TeamSelectionKeyCacheEntry *)out;

            /* roundPtr+0x66 advances by 2 each iteration → cup roundLegs[roundIdx]
             * roundPtr+0x7E with stride 2 → cup roundPenalties[roundIdx] */
            if (baseEx->matchData.cup.roundLegs[roundIdx] == 2) {
                /* Two-leg match */
                e->legFlag = 1;
                out += e->entryStride;
                out = addNextRoundCupEntry(&src, roundIdx, numRounds, teamId, slotLen, base);
                e = (TeamSelectionKeyCacheEntry *)out;
                e->legFlag = 2;
                out += e->entryStride;
            } else {
                /* Single leg */
                e->legFlag = 0;
                out += e->entryStride;
                if (baseEx->matchData.cup.roundPenalties[roundIdx] == 1) {
                    /* No replay match */
                } else {
                    /* Replay match */
                    out = addNextRoundCupEntry(&src, roundIdx, numRounds, teamId, slotLen, base);
                    e = (TeamSelectionKeyCacheEntry *)out;
                    e->legFlag = 3;
                    e->phaseFlag = 1;
                    out += e->entryStride;
                }
            }

            roundIdx++;
            teamsPerRound = halved;  /* teamsPerRound halves each round */
        }
        return;
    }

    /* Type 2+ — multi-slot competition */
    {
        uint8_t *base = blk;
        DIYCompetitionEx *baseEx = (DIYCompetitionEx *)blk;
        /* +0x15A = first slot's _pad_00 word (used as opt for GetCupRoundStringIndex) */
        DIYTournamentSlot *firstSlot = (DIYTournamentSlot *)(base + 0x15A);
        uint16_t opt = *(uint16_t *)firstSlot->_pad_00;
        uint16_t teamSlot = *(uint16_t *)&baseEx->base.compId;
        uint8_t *out = teamSelectionKeyCache;
        TeamSelectionKeyCacheEntry *e;
        uint16_t numSlots = comp->totalMatchesInRound;
        uint16_t slotIdx = 0;

        while (slotIdx != numSlots) {
            uint16_t slotLen;
            DIYTournamentSlot *slotRec = (DIYTournamentSlot *)(base + 0x15A);

            if (gameType == 4) {
                slotLen = 0x16;
            } else {
                slotLen = slotRec->numTeams;
                slotLen >>= 1;
                if (slotLen > 0x18)
                    slotLen = 0x18;
                slotLen = slotLen * 0xC + 0xA;
            }

            uint8_t *src = slotRec->progressionTable;

            if (slotRec->isLeagueFormat == 0) {
                /* Cup sub-slot */
                out = addCupEntry(&src, teamSlot, slotIdx, numSlots, slotLen, base, opt);
                e = (TeamSelectionKeyCacheEntry *)out;

                if (slotRec->formatFlag16E == 2) {
                    /* Two-leg */
                    e->legFlag = 1;
                    out = addCupEntry(&src, teamSlot, slotIdx, numSlots, slotLen, base, opt);
                    e = (TeamSelectionKeyCacheEntry *)out;
                    e->legFlag = 2;
                } else {
                    e->legFlag = 0;
                    if (slotRec->formatFlag172 != 1) {
                        /* Has replay */
                        out = addCupEntry(&src, teamSlot, slotIdx, numSlots, slotLen, base, opt);
                        e = (TeamSelectionKeyCacheEntry *)out;
                        e->legFlag = 3;
                        e->phaseFlag = 1;
                    }
                }
            } else {
                /* League sub-slot */
                uint16_t leagueRounds = slotRec->matchesPerSlot;
                int16_t j = (int16_t)leagueRounds - 1;
                while (j >= 0) {
                    out = addCupEntry(&src, teamSlot, slotIdx, numSlots, slotLen, base, opt);
                    e = (TeamSelectionKeyCacheEntry *)out;
                    e->legFlag = 0;
                    out += e->entryStride;
                    j--;
                }
            }

            base += 0x76;  /* advance to next slot record */
            slotIdx++;
        }
    }
}
