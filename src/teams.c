#include "teams.h"
#include "career_teams.h"
#include "file_io.h"
#include "menus.h"
#include "string_utils.h"
#include "panic.h"
#include "diy_competition.h"
#include "career_status.h"

/* ---- Constants ---- */
#define GAME_TYPE_CAREER            4
#define CN_EUROPE   80
#define CN_OCEANIA  85

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint16_t teamsLoaded;
extern uint8_t lastLoadedExtNumber;
extern uint8_t teamFileBuffer[];
extern uint8_t teamFilenameBuffer[];
extern uint16_t teamFileTeamCount;
extern uint32_t teamFileDataSize;
extern uint16_t gameType;
extern uint16_t skipLeagueNumbers;
extern uint8_t diyLeagueSeasonStateTable[];
extern volatile uint16_t panicReasonCode;
extern uint16_t isNationalTeam;
extern uint16_t numSelectedTeams;
extern uint8_t selTeamsPtr;

/* Data labels (mutable strings and tables in SWOS2.s) */
extern uint8_t aDataTeamNnn[];      /* "data/team.nnn" */
extern uint8_t aCustomsEdt[];       /* "CUSTOMS.EDT" */
extern uint8_t aEditedCustomTe[];   /* "EDITED CUSTOM TEAMS FILE NOT FOUND" */
extern uint16_t teamsCountryNumbers[];
extern uint8_t avgTeamValues[];
extern uint16_t avgTeamValueBases[];

/* Globals for GetPlayerRandomNum / GetDisplayedPlayerValue */
extern uint16_t currentSeasonShowing;
extern uint8_t newRandomSeedCopy;
extern uint8_t kRandomTable[];
extern uint8_t currentTeam[];
extern uint8_t playerPositions[];
extern uint8_t careerTeam[];
extern uint8_t careerNumPlayers[];
extern uint8_t viewRandomPhase;
extern uint8_t displayedPlayerValueRandomDeltaTable[];

/* Globals for GetPlayer / GetPlayerAtIndex / GetPlayerSlotIndexByPlayerNumber */
extern uint16_t currentTeamNumber;
extern uint16_t selTeamNumber;
extern uint8_t currentMatchPlayers[];
extern uint16_t _cGetPlayerAtIndexFileIdx;

/* Globals for national selector functions */
extern uint16_t careerJobOfferMode;
extern uint32_t nationalSelectorSeasonMaskA;
extern uint32_t nationalSelectorSeasonMaskB;
extern uint8_t nationalTeams[];
extern uint16_t numNationalTeams;

/* ================================================================
 * SetTeamGlobalNumbers  (SWOS2.s line 112842)
 *
 * For each team in the buffer, computes the global team number
 * from the country number and team-within-country number using
 * the teamsCountryNumbers lookup table.
 *
 * Asm register convention:
 *   in:  d0 = numTeams, a0 = pointer to first team entry
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetTeamGlobalNumbers(uint16_t numTeams, uint8_t *firstTeam)
{
    uint16_t i;
    TeamFileEx *team;

    if (numTeams == 0)
        return;

    team = (TeamFileEx *)firstTeam;
    for (i = numTeams; i > 0; i--) {
        uint16_t countryNum = team->team.countryNumber & 0xFF;
        uint16_t localNum;
        uint16_t globalNum;

        countryNum = countryNum << 1;   /* word index */
        globalNum = *((uint16_t *)((uint8_t *)teamsCountryNumbers + countryNum));

        localNum = team->team.teamOrdinal & 0xFF;
        globalNum += localNum;

        team->team.globalTeamNumber = (int16_t)globalNum;

        team++;
    }
}

/* ================================================================
 * setLeagueNumbers  (SWOS2.s line 121389)
 *
 * For each team in teamFileBuffer, reads the global team number
 * and uses diyLeagueSeasonStateTable to add a league offset
 * to the team's league field.
 * ================================================================ */
static void setLeagueNumbers(void)
{
    uint8_t *teamBuf = teamFileBuffer;
    uint16_t count = *((uint16_t *)teamBuf);
    TeamFileEx *team;
    uint16_t i;

    if (count == 0)
        return;

    team = (TeamFileEx *)(teamBuf + 2);
    for (i = count; i > 0; i--) {
        uint16_t globalNum = (uint16_t)team->team.globalTeamNumber;
        uint8_t leagueAdj = diyLeagueSeasonStateTable[globalNum];
        team->team.league += leagueAdj;
        team++;
    }
}

/* ================================================================
 * setLoadedTeamsGlobalNumbers  (SWOS2.s line 121381)
 *
 * Reads team count from teamFileBuffer header and calls
 * SetTeamGlobalNumbers on the first team entry (offset +2).
 * ================================================================ */
static void setLoadedTeamsGlobalNumbers(void)
{
    uint16_t count = *((uint16_t *)teamFileBuffer);
    _SetTeamGlobalNumbers(count, teamFileBuffer + 2);
}

/* ================================================================
 * LoadTeamFile  (SWOS2.s line 121301)
 *
 * Loads a team file given an extension number.
 * - extNumber == 100 → loads "CUSTOMS.EDT"
 * - otherwise → constructs "data/team.NNN" filename
 *
 * If already loaded with same extNumber, returns immediately.
 * On successful load, sets teamsCountryNumbers and optionally
 * league numbers (for career mode).
 *
 * Asm register convention:
 *   in:  d0 = extNumber (uint8_t effective)
 *   out: d0 = 0 (success) or 1 (error), Z flag reflects d0
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _LoadTeamFile(uint16_t extNumber)
{
    uint8_t ext8 = (uint8_t)(extNumber & 0xFF);
    int16_t result;
    uint32_t fileSize;

    /* Check if already loaded with same extension */
    if (teamsLoaded != 0) {
        if (lastLoadedExtNumber == ext8)
            return 0;   /* already loaded */
    }

    teamsLoaded = 1;

    if (ext8 == 100) {
        /* Custom teams file */
        lastLoadedExtNumber = ext8;
        result = _LoadFileEx(aCustomsEdt, teamFileBuffer, &fileSize);
        if (result != 0) {
            /* Custom file load failure */
            if ((uint16_t)result == 0xCD) {
                _ShowErrorMenu(aEditedCustomTe);
            } else {
                _ShowFsErrorByCode((uint16_t)result);
            }
            teamsLoaded = 0;
            return 1;
        }
        if (fileSize > 64000u) {
            panicReasonCode = 0x1939;
            _PanicHaltLoop();
        }
        setLoadedTeamsGlobalNumbers();
    } else {
        /* Normal team file: construct "data/team.NNN" */
        uint8_t hundreds, tens, ones;
        uint16_t val;

        lastLoadedExtNumber = ext8;

        val = (uint16_t)ext8;
        hundreds = (uint8_t)(val / 100);
        val = val % 100;
        tens = (uint8_t)(val / 10);
        ones = (uint8_t)(val % 10);

        aDataTeamNnn[0xA] = hundreds + '0';
        aDataTeamNnn[0xB] = tens + '0';
        aDataTeamNnn[0xC] = ones + '0';

        _StringCopy(aDataTeamNnn, teamFilenameBuffer);

        result = _LoadFileEx(teamFilenameBuffer, teamFileBuffer, &fileSize);
        if (result != 0) {
            /* Non-custom file load failure: panic and halt */
            panicReasonCode = 0x8008;
            _PanicHaltLoop();
            /* never returns */
        }
        if (fileSize > 64000u) {
            panicReasonCode = 0x1939;
            _PanicHaltLoop();
        }
        setLoadedTeamsGlobalNumbers();

        /* In career mode, also set league numbers */
        if (gameType == GAME_TYPE_CAREER && skipLeagueNumbers == 0) {
            setLeagueNumbers();
        }
    }

    /* Update team count and data size */
    {
        uint16_t count = *((uint16_t *)teamFileBuffer);
        teamFileTeamCount = count;
        teamFileDataSize = (uint32_t)count * sizeof(TeamFileEx) + 2;
    }

    return 0;
}

/* ================================================================
 * CheckIsTeamNational  (SWOS2.s line 29246)
 *
 * Sets isNationalTeam to 1 if the first selected team's continent
 * byte is between CN_EUROPE (80) and CN_OCEANIA (85) inclusive,
 * i.e. the team is a national team. Otherwise sets it to 0.
 * ================================================================ */
__attribute__((used, externally_visible))
void _CheckIsTeamNational(void)
{
    isNationalTeam = 0;
    if (numSelectedTeams == 0)
        return;
    if (selTeamsPtr < CN_EUROPE)
        return;
    if (selTeamsPtr > CN_OCEANIA)
        return;
    isNationalTeam = 1;
}

/* ================================================================
 * GetTeamValueBase  (SWOS2.s line 28508)
 *
 * Given an average player price (byte), looks up the team value
 * base through two tables: avgTeamValues (byte→index) then
 * avgTeamValueBases (index→value).
 *
 * Asm register convention:
 *   in:  d0.b = avgPrice (ext.w sign-extends to word index)
 *   out: d0.w = team value base
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetTeamValueBase(uint16_t avgPrice)
{
    int16_t index = (int16_t)(int8_t)(avgPrice & 0xFF);  /* ext.w d0 */
    uint8_t tableVal = avgTeamValues[index];
    return avgTeamValueBases[tableVal];
}

/* ================================================================
 * CalcGateReceiptsBaseFromTeamValue  (SWOS2.s line 87365)
 *
 * Computes gate receipts base value from average player price.
 * Calls GetTeamValueBase then applies: base * 1000 / 768 * 1536.
 *
 * Asm register convention:
 *   in:  d0 = avgPrice (from GetAveragePlayerPrice)
 *   out: d0 = gate receipts base (d0.w after final mulu.w)
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _CalcGateReceiptsBaseFromTeamValue(uint16_t avgPrice)
{
    uint16_t base = _GetTeamValueBase(avgPrice);
    uint32_t val = (uint32_t)base * 1000;          /* mulu.w #1000,d0 */
    uint16_t quotient = (uint16_t)(val / 768);     /* divu.w #768,d0 → d0.w */
    uint32_t result = (uint32_t)quotient * 1536;   /* mulu.w #1536,d0 */
    return (uint16_t)result;
}

/* ================================================================
 * IsTeamNational  (SWOS2.s line 28526)
 *
 * Returns 1 if the team is "national" — either not in career mode,
 * or the team's continent byte is in [CN_EUROPE, CN_OCEANIA].
 * Returns 0 otherwise (career mode + non-national continent).
 *
 * Asm register convention:
 *   in:  a1 = pointer to team general info (continent at byte 0)
 *   out: d0.w = 1 (national) or 0 (not national), Z flag set if 0
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _IsTeamNational(uint8_t *teamGeneral)
{
    if (gameType != GAME_TYPE_CAREER)
        return 1;
    if (*teamGeneral < CN_EUROPE)
        return 0;
    if (*teamGeneral > CN_OCEANIA)
        return 0;
    return 1;
}

/* ================================================================
 * GetAveragePlayerPriceInSelectedTeams  (SWOS2.s line 28663)
 *
 * Computes the average player price across all selected teams
 * by calling GetAveragePlayerPrice for each team and dividing
 * the total by the number of selected teams.
 *
 * Asm register convention:
 *   out: d0.w = average price across all selected teams
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetAveragePlayerPriceInSelectedTeams(void)
{
    uint16_t i;
    uint16_t total;
    uint8_t *team;
    int32_t ext;

    if (numSelectedTeams == 0)
        return 0;

    team = &selTeamsPtr;
    total = 0;
    for (i = numSelectedTeams - 1; ; i--) {
        total += _GetAveragePlayerPrice(team);
        team += sizeof(TeamFileEx);
        if (i == 0) break;
    }

    /* ext.l d0 + divu.w: sign-extend total to 32-bit then unsigned divide */
    ext = (int32_t)(int16_t)total;
    return (uint16_t)((uint32_t)ext / numSelectedTeams);
}

/* ================================================================
 * GetPlayerRandomNum  (SWOS2.s line 28486)
 *
 * Computes a deterministic "random" number for a player by XORing
 * several player data bytes together with the current season and
 * random seed, then looking up the result in kRandomTable.
 *
 * Asm register convention:
 *   in:  a0 = pointer into team entry (offset by playerIndex * 38)
 *   out: d0.w = random number (0-255)
 *   clobbers: d1, a0
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetPlayerRandomNum(uint8_t *teamEntry)
{
    PlayerFile *pf = (PlayerFile *)(teamEntry + 76);
    uint8_t d0, d1;

    d0 = (uint8_t)pf->price;                       /* move.b PLAYERFILE_PRICE+$4C(a0),d0 */
    d1 = pf->positionAndFace;                       /* move.b PLAYERFILE_POSITIONANDFACE+$4C(a0),d1 */
    d1 &= 0xFB;                                     /* andi.b #$FB,d1 */
    d0 ^= d1;                                       /* eor.b d1,d0 */
    d1 = (uint8_t)pf->playerName[0];               /* move.b PLAYERFILE_PLAYERNAME+$4C(a0),d1 */
    d0 ^= d1;                                       /* eor.b d1,d0 */
    d1 = (uint8_t)pf->playerName[10];              /* move.b PLAYERFILE_PLAYERNAME+$56(a0),d1 */
    d0 ^= d1;                                       /* eor.b d1,d0 */
    d1 = (uint8_t)currentSeasonShowing;             /* move.w (currentSeasonShowing).l,d1; eor uses d1.b */
    d0 ^= d1;                                       /* eor.b d1,d0 */
    d1 = newRandomSeedCopy;                         /* move.b (newRandomSeedCopy).l,d1 */
    d0 ^= d1;                                       /* eor.b d1,d0 */

    return (uint16_t)kRandomTable[d0];              /* andi.w #$FF,d0; move.b (kRandomTable+d0.w),d0 */
}

/* ================================================================
 * GetDisplayedPlayerValue  (SWOS2.s line 28567)
 *
 * Returns the displayed player value (1-80) for the player at ptr.
 *
 * Two code paths:
 * - "Normal" path: if ptr is within currentTeam or careerTeam buffers,
 *   uses GetAgeAdjustedPlayerFormFactor + avgTeamValues lookup.
 * - "Random" path: for pointers outside those buffers, uses a
 *   randomized approach (GetPlayerRandomNum + delta table), though
 *   the random delta is ultimately discarded due to a move.w d2,d0
 *   overwrite in the original asm.
 *
 * Result is clamped to [1, 0x50].
 *
 * Asm register convention:
 *   in:  a0 = pointer into team data (adjusted by playerIndex * 38)
 *   out: d0.w = displayed value (1-80)
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetDisplayedPlayerValue(uint8_t *ptr)
{
    /* Check if ptr is within the currentTeam buffer [currentTeam, playerPositions) */
    if (ptr >= currentTeam && ptr < playerPositions)
        goto normal_path;

    /* Check if ptr is within the careerTeam buffer [careerTeam, careerNumPlayers) */
    if (ptr >= careerTeam && ptr < careerNumPlayers)
        goto normal_path;

    /* --- Random/career path (ptr not in currentTeam or careerTeam) --- */
    {
        PlayerFile *pf = (PlayerFile *)(ptr + 76);
        uint8_t priceByte = (uint8_t)pf->price;
        int16_t priceIndex = (int16_t)(int8_t)priceByte;  /* ext.w d2 */

        /* Note: asm calls GetPlayerRandomNum + displayedPlayerValueRandomDeltaTable
           lookup here, but the result is discarded by "move.w d2,d0" which overwrites
           d0.w with d2.w. The random delta does not affect the final value. */

        /* avgTeamValues lookup: move.b (avgTeamValues + d2.w),d2 changes only d2.b;
           d2 high byte is preserved from ext.w, so d2.w = (sign_byte : tableVal).
           Then move.w d2,d0 → d0.w = d2.w. For prices < 128, high byte is 0x00,
           so d0.w = tableVal. For prices >= 128, high byte is 0xFF, d0.w is negative. */
        if (priceIndex < 0) {
            return 1;   /* price >= 128: ext.w gives 0xFF high byte → negative → clamp to 1 */
        }

        {
            uint8_t tableVal = avgTeamValues[priceIndex];
            /* d2.w = 0x00YY since priceIndex >= 0 → high byte from ext.w is 0x00 */
            uint16_t result = (uint16_t)tableVal;

            if ((int16_t)result < 1)
                return 1;
            if (result > 0x50)
                return 0x50;
            return result;
        }
    }

normal_path:
    /* --- Normal path (ptr within currentTeam or careerTeam) --- */
    {
        int16_t formFactor = _GetAgeAdjustedPlayerFormFactor(ptr);
        PlayerFile *pf = (PlayerFile *)(ptr + 76);
        uint8_t priceByte = (uint8_t)pf->price;
        int16_t priceIndex = (int16_t)(int8_t)priceByte;  /* ext.w d0 */
        uint8_t tableVal = avgTeamValues[priceIndex];      /* move.b (avgTeamValues + d0.w),d0 */
        /* After move.b, d0.b = tableVal, d0 high byte preserved from ext.w.
           But then add.w d1,d0 adds formFactor to the FULL word d0.w.
           d0.w after move.b = (sign_byte : tableVal). Then d0.w += formFactor. */
        int16_t d0w = (int16_t)(((uint16_t)priceIndex & 0xFF00) | tableVal);
        d0w += formFactor;                                 /* add.w d1,d0 */

        if (d0w < 1)
            return 1;
        if ((uint16_t)d0w > 0x50)
            return 0x50;
        return (uint16_t)d0w;
    }
}

/* ================================================================
 * InitPlayerValueDisplayStruct  (SWOS2.s line 28512)
 *
 * Computes the displayed player value, maps it to a sprite frame
 * index (0x71-0x7A), and writes a 4-word struct at dst+0x22.
 *
 * Asm register convention:
 *   in:  a0 = player entry pointer, a1 = dst struct pointer
 *   out: writes 4 words at dst+0x22..dst+0x28
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitPlayerValueDisplayStruct(uint8_t *playerEntry, uint8_t *dst)
{
    uint16_t val = _GetDisplayedPlayerValue(playerEntry);   /* bsr GetDisplayedPlayerValue */
    val -= 1;                                               /* subq.w #1,d0 */
    val >>= 3;                                              /* lsr.w #3,d0 */
    val += 0x71;                                            /* addi.w #$71,d0 */

    *(uint16_t *)(dst + 0x22) = 3;                          /* move.w #3,$22(a1) */
    *(uint16_t *)(dst + 0x24) = 0;                          /* move.w #0,$24(a1) */
    *(uint16_t *)(dst + 0x26) = val;                        /* move.w d0,$26(a1) */
    *(uint16_t *)(dst + 0x28) = 0;                          /* clr.w $28(a1) */
}

/* ================================================================
 * SnapToNearestAvgTeamValueBase  (SWOS2.s line 71597)
 *
 * Scans avgTeamValueBases for the nearest entry to the given value.
 * Uses midpoint rounding: at the exact midpoint between two entries,
 * rounds UP (returns the higher entry) because lsr.w #1 truncates.
 *
 * Asm register convention:
 *   in:  d0.w = value
 *   out: d0.w = nearest avgTeamValueBases entry
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _SnapToNearestAvgTeamValueBase(uint16_t value)
{
    uint16_t *p;
    uint16_t upper, lower, midpoint;

    if (value == 0)
        return 0;

    p = avgTeamValueBases;

    /* Scan until we find an entry >= value */
    while (value > *p)                          /* cmp.w (a0)+,d0; bhi loop */
        p++;

    upper = *p;                                 /* -2(a0) after post-increment */
    lower = *(p - 1);                           /* -4(a0) after post-increment */

    midpoint = ((upper - lower) >> 1) + lower;  /* lsr.w #1 = truncating /2 */

    if (value < midpoint)                       /* cmp.w d1,d0; bcs → return lower */
        return lower;

    return upper;                               /* value >= midpoint → return upper */
}

/* ================================================================
 * SnapUpToAvgTeamValueBase  (SWOS2.s line 71619)
 *
 * Scans avgTeamValueBases for the first entry >= value.
 *
 * Asm register convention:
 *   in:  d0.w = value
 *   out: d0.w = first avgTeamValueBases entry >= value
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _SnapUpToAvgTeamValueBase(uint16_t value)
{
    uint16_t *p;

    if (value == 0)
        return 0;

    p = avgTeamValueBases;

    /* Scan until we find an entry >= value */
    while (value > *p)                          /* cmp.w (a0)+,d0; bhi loop */
        p++;

    return *p;                                  /* -2(a0) after post-increment */
}

/* ================================================================
 * RotatePlayersLeft  (SWOS2.s line 59148)
 *
 * Rotates the substitute player slots left by one position.
 * The first substitute (at offset maxSubs + 11) moves to the end,
 * and all other substitutes shift left.
 *
 * Register convention:
 *   in:  a0 = team pointer
 *        d0 = maximum number of substitutes
 *
 * If teamPtr == &currentTeam, uses playerPositions array.
 * If teamPtr == &careerTeam, uses currentMatchPlayers array.
 * Otherwise, uses teamPtr + 0x3C (TEAMFILE_PLAYERS).
 * ================================================================ */
extern uint8_t currentMatchPlayers[];
extern uint16_t numPlayers;

__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _RotatePlayersLeft(uint8_t *teamPtr, uint16_t maxSubs)
{
    uint8_t *arr;
    int16_t count;

    if (teamPtr == currentTeam) {
        arr = playerPositions;
        count = (int16_t)numPlayers;
    } else if (teamPtr == careerTeam) {
        arr = currentMatchPlayers;
        count = (int16_t)*(uint16_t *)careerNumPlayers;
    } else {
        arr = teamPtr + 0x3C;
        count = 0x10;
    }

    uint16_t offset = maxSubs + 11;
    arr += offset;
    count -= (int16_t)offset;
    count -= 2;

    /* Rotate left: save first element, shift remaining, put saved at end */
    uint8_t saved = arr[0];
    for (int16_t i = count; i >= 0; i--) {
        arr[0] = arr[1];
        arr++;
    }
    *arr = saved;
}

/* ================================================================
 * AddTeamToBuffer  (SWOS2.s line 40491)
 *
 * Copies a 684-byte team record to the end of the selTeamsPtr
 * buffer, increments numSelectedTeams, and returns a pointer to
 * the newly added entry.
 *
 * Asm register convention:
 *   in:  a0 = source team pointer
 *   out: a0 = pointer to copy in selTeamsPtr buffer
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
uint8_t *_AddTeamToBuffer(uint8_t *team)
{
    uint8_t *dest = &selTeamsPtr + (uint32_t)numSelectedTeams * 684;
    uint16_t *src16 = (uint16_t *)team;
    uint16_t *dst16 = (uint16_t *)dest;
    uint16_t i;
    for (i = 342; i != 0; i--)
        *dst16++ = *src16++;
    numSelectedTeams++;
    return dest;
}

/* ================================================================
 * FindTeam  (SWOS2.s line 40528)
 *
 * Searches the selTeamsPtr buffer for a team with the same ID
 * (first word) as the given team. Returns a pointer to the found
 * entry and the remaining entry count via output pointer.
 * Panics if not found.
 *
 * Asm register convention:
 *   in:  a0 = team to search for (uses first word as ID)
 *   out: a0 = found team in selTeamsPtr buffer
 *        d2 = remaining entries after found one
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_FindTeam(uint8_t *teamToFind, uint16_t *outRemainingCount)
{
    uint16_t teamId = *(uint16_t *)teamToFind;
    uint16_t count = numSelectedTeams;
    if (count == 0)
        goto panic;
    count--;
    uint8_t *ptr = &selTeamsPtr;
    uint16_t remaining = count;
    while (1) {
        if (*(uint16_t *)ptr == teamId) {
            *outRemainingCount = remaining;
            return ptr;
        }
        ptr += 684;
        if (remaining == 0)
            break;
        remaining--;
    }
panic:
    panicReasonCode = 0x6987;
    _PanicHaltLoop();
    /* never reached */
    *outRemainingCount = 0;
    return (uint8_t *)0;
}

/* ================================================================
 * DeselectTeam  (SWOS2.s line 40507)
 *
 * Removes a team from the selTeamsPtr buffer by finding it,
 * shifting remaining entries down, and decrementing numSelectedTeams.
 *
 * Asm register convention:
 *   in:  a0 = team to remove
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _DeselectTeam(uint8_t *teamToRemove)
{
    uint16_t remaining;
    uint8_t *found = _FindTeam(teamToRemove, &remaining);
    if (remaining != 0) {
        uint16_t *dst = (uint16_t *)found;
        uint16_t *src = (uint16_t *)(found + 684);
        uint32_t words = (uint32_t)remaining * 342;
        while (words != 0) {
            *dst++ = *src++;
            words--;
        }
    }
    numSelectedTeams--;
}

/* ================================================================
 * Chunk 91 — Team/Player Lookup & National Selector Functions
 *
 * GetPlayer, GetPlayerAtIndex, GetPlayerSlotIndexByPlayerNumber:
 * foundational player record lookup used throughout the game.
 *
 * National selector functions:
 * IsTeamCodeOkForNationalSelector, IsPlayerOkForNationalSelector,
 * GetNationalityAndTeamFile, GetNationalTeamNumberFromTeamFileId,
 * GetNationalTeamNumber, GetNationalityRecordPtr.
 * ================================================================ */

/* ================================================================
 * _GetPlayer — get pointer to player record in team file data.
 * Asm: GetPlayer (d0=teamNumber, d1=playerFileIndex → a0=playerPtr)
 *
 * Returns: teamBase + playerFileIndex * sizeof(PlayerFile).
 * Callers add 76 (sizeof(TeamFile)) to reach actual player data fields.
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_GetPlayer(uint16_t teamNumber, uint16_t playerFileIndex)
{
    uint8_t *teamBase;

    if (teamNumber == currentTeamNumber)
        teamBase = currentTeam;
    else if (teamNumber == selTeamNumber)
        teamBase = careerTeam;
    else
        teamBase = _GetTeamPointer(teamNumber);

    return teamBase + (uint16_t)(playerFileIndex * sizeof(PlayerFile));
}

/* ================================================================
 * _GetPlayerAtIndex — get player pointer from team number + ordinal.
 * Asm: GetPlayerAtIndex (d0=teamNumber, d1=ordinal → a0=playerPtr, d1=fileIndex)
 *
 * Remaps ordinal through position table (playerPositions for currentTeam,
 * currentMatchPlayers for selTeam, TEAMFILE_PLAYERNUMBERS for generic teams),
 * then calls GetPlayer with the remapped file index.
 *
 * Side effect: writes remapped file index to _cGetPlayerAtIndexFileIdx.
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_GetPlayerAtIndex(uint16_t teamNumber, uint16_t ordinal)
{
    uint16_t fileIndex;

    if (teamNumber == currentTeamNumber) {
        fileIndex = (uint16_t)playerPositions[ordinal];
    } else if (teamNumber == selTeamNumber) {
        fileIndex = (uint16_t)currentMatchPlayers[ordinal];
    } else {
        if (ordinal >= 16) {
            panicReasonCode = 0x1146;
            _PanicHaltLoop();
        }
        uint8_t *teamPtr = _GetTeamPointer(teamNumber);
        fileIndex = (uint16_t)((TeamFile *)teamPtr)->playerNumbers[ordinal];
    }

    _cGetPlayerAtIndexFileIdx = fileIndex;
    return _GetPlayer(teamNumber, fileIndex);
}

/* ================================================================
 * _GetPlayerSlotIndexByPlayerNumber — reverse lookup: player number → slot index.
 * Asm: GetPlayerSlotIndexByPlayerNumber (d0=teamNumber, d1=playerNumber → d1=slotIndex)
 *
 * For selTeamNumber: searches currentMatchPlayers[], panics if index >= careerNumPlayers.
 * For other teams: validates playerNumber < 16, searches team[TEAMFILE_PLAYERNUMBERS+i].
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetPlayerSlotIndexByPlayerNumber(uint16_t teamNumber, uint16_t playerNumber)
{
    uint16_t idx = 0;

    if (teamNumber == selTeamNumber) {
        /* Search currentMatchPlayers for matching player number */
        while (currentMatchPlayers[idx] != (uint8_t)playerNumber)
            idx++;
        if (idx >= *(uint16_t *)careerNumPlayers) {
            panicReasonCode = 0x2617;
            _PanicHaltLoop();
        }
    } else {
        /* Validate input */
        if (playerNumber >= 16) {
            panicReasonCode = 0x2618;
            _PanicHaltLoop();
        }
        /* Search team's player number table */
        uint8_t *teamPtr = _GetTeamPointer(teamNumber);
        while (((TeamFile *)teamPtr)->playerNumbers[idx] != (uint8_t)playerNumber)
            idx++;
        if (idx >= 16) {
            panicReasonCode = 0x2619;
            _PanicHaltLoop();
        }
    }

    return idx;
}

/* ================================================================
 * _GetNationalityRecordPtr — find nationality record by team file+ordinal.
 * Asm: GetNationalityRecordPtr (d0=teamFileAndOrdinal → a0=recordPtr or NULL)
 *
 * Each nationalTeams record is 6 bytes:
 *   [0..1] teamFileAndOrdinal (word)
 *   [2]    nationality code
 *   [3]    team file ID
 *   [4..5] other data
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_GetNationalityRecordPtr(uint16_t teamFileAndOrdinal)
{
    uint8_t *ptr = nationalTeams;
    uint16_t count = numNationalTeams;
    for (uint16_t i = 0; i < count; i++) {
        if (*(uint16_t *)ptr == teamFileAndOrdinal)
            return ptr;
        ptr += 6;
    }
    return (uint8_t *)0;
}

/* ================================================================
 * _GetNationalTeamNumber — find national team number by nationality code.
 * Asm: GetNationalTeamNumber (d0=nationality → d0=teamNumber or -1)
 *
 * Searches nationalTeams[i][2] for matching nationality byte.
 * Returns the team file+ordinal word from the matching record, or 0xFFFF.
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _GetNationalTeamNumber(uint16_t nationality)
{
    uint8_t *ptr = nationalTeams;
    uint16_t count = numNationalTeams;
    for (uint16_t i = 0; i < count; i++) {
        if (ptr[2] == (uint8_t)nationality)
            return (int16_t)(*(uint16_t *)ptr);
        ptr += 6;
    }
    return -1;
}

/* ================================================================
 * _GetNationalTeamNumberFromTeamFileId — find national team number by team file ID.
 * Asm: GetNationalTeamNumberFromTeamFileId (d0=teamFileId → d0=teamNumber or -1)
 *
 * Searches nationalTeams[i][3] for matching team file ID byte.
 * Returns the team file+ordinal word from the matching record, or 0xFFFF.
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _GetNationalTeamNumberFromTeamFileId(uint16_t teamFileId)
{
    uint8_t *ptr = nationalTeams;
    uint16_t count = numNationalTeams;
    for (uint16_t i = 0; i < count; i++) {
        if (ptr[3] == (uint8_t)teamFileId)
            return (int16_t)(*(uint16_t *)ptr);
        ptr += 6;
    }
    return -1;
}

/* ================================================================
 * _GetNationalityAndTeamFile — look up nationality and team file ID.
 * Asm: GetNationalityAndTeamFile (d0=teamFileAndOrdinal → d0=nationality, d1=teamFileId)
 *
 * Returns packed uint32_t: (nationality << 16) | teamFileId.
 * Asm bridge unpacks via swap+move.
 * Panics (0x4543) if not found.
 * ================================================================ */
__attribute__((used, externally_visible))
uint32_t _GetNationalityAndTeamFile(uint16_t teamFileAndOrdinal)
{
    uint8_t *record = _GetNationalityRecordPtr(teamFileAndOrdinal);
    if (!record) {
        panicReasonCode = 0x4543;
        _PanicHaltLoop();
    }
    uint8_t nationality = record[2];
    uint8_t teamFileId = record[3];
    return ((uint32_t)nationality << 16) | (uint32_t)teamFileId;
}

/* ================================================================
 * _IsTeamCodeOkForNationalSelector — check if team code allows national selection.
 * Asm: IsTeamCodeOkForNationalSelector (d0=code → Z flag from btst)
 *
 * Returns: 0 = not OK, non-zero = OK.
 * If not in job offer mode and currentSeasonShowing < 3, returns 0.
 * Otherwise tests bit currentSeasonShowing in the appropriate season mask
 * (mask A for code 'P', mask B for other codes).
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _IsTeamCodeOkForNationalSelector(uint16_t code)
{
    /* Early exit if not in job offer mode and season < 3 */
    if (careerJobOfferMode == 0 && currentSeasonShowing < 3)
        return 0;

    /* Pick mask based on code */
    uint32_t mask;
    if ((uint8_t)code == 0x50)  /* 'P' */
        mask = nationalSelectorSeasonMaskA;
    else
        mask = nationalSelectorSeasonMaskB;

    /* Test bit currentSeasonShowing in mask */
    uint16_t bit = currentSeasonShowing;
    return (int16_t)((mask >> bit) & 1);
}

/* ================================================================
 * _IsPlayerOkForNationalSelector — check if player national selection is possible.
 * Asm: IsPlayerOkForNationalSelector (→ Z flag)
 *
 * Returns: 0 = not OK, non-zero = OK.
 * If not in job offer mode and season < 3, returns 0.
 * Otherwise returns OK if bit currentSeasonShowing is set in either mask A or mask B.
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _IsPlayerOkForNationalSelector(void)
{
    /* Early exit if not in job offer mode and season < 3 */
    if (careerJobOfferMode == 0 && currentSeasonShowing < 3)
        return 0;

    uint16_t bit = currentSeasonShowing;
    /* Check mask A first */
    if ((nationalSelectorSeasonMaskA >> bit) & 1)
        return 1;
    /* Check mask B */
    return (int16_t)((nationalSelectorSeasonMaskB >> bit) & 1);
}

/* ================================================================
 * Chunk 95 — SetBenchPlayersNumbers, Career_RemovePlayerFromSelectedTeamRoster,
 *            RemovePlayerFromTeam, replacement helpers
 * ================================================================ */

extern uint8_t *_GetPlayerAtIndex(uint16_t teamNumber, uint16_t ordinal);
extern void _Career_RemovePlayerWeeklyWageAndRecalcWeeklyWages(uint16_t playerIndex);
extern void _Career_UpdatePlayerWeeklyWageCache(uint16_t playerIndex);
extern void _TrainingMatch_RemoveSavedPlayerId(uint16_t value);
extern void _Career_RemovePlayerFromTeamInternal(uint16_t teamNumber, uint16_t playerOrd);
extern void _PrunePlayerOffersAgainstRosterLimits(void);

/* asm bridges for functions not yet ported to C */
extern void _CCareer_GenerateTrialistForTeamPlayer(uint16_t playerIndex);

/* asm data externs */
extern uint16_t removedPlayerStatAccum;


/* ================================================================
 * _SetBenchPlayersNumbers — assign shirt numbers 12-16 to bench players.
 * Asm: SetBenchPlayersNumbers (a0=team ptr)
 *
 * Loops players at ordinal index 11-15. For each, calls GetPlayerAtIndex
 * to get the player record pointer, then writes (ordinal + 1) as the
 * shirt number at offset PF_SHIRTNUM from that raw pointer.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetBenchPlayersNumbers(uint8_t *teamPtr)
{
    uint16_t teamNum = *(uint16_t *)teamPtr;
    uint16_t d1;

    for (d1 = 11; d1 < 16; d1++) {
        uint8_t *player = _GetPlayerAtIndex(teamNum, d1);
        PlayerFile *pf = (PlayerFile *)(player + 76);
        pf->shirtNumber = (int8_t)(d1 + 1);
    }
}

/* ================================================================
 * Replacement position search order tables (from FindReplacementPlayerByPositionOrder)
 * Position codes: 0=GK, 1=RB, 2=LB, 3=Def, 4=RW, 5=LW, 6=Mid, 7=Att
 * ================================================================ */
static const uint8_t replPosSearchOrderGK[]  = {0,3,6,7,1,2,4,5,0xFF};
static const uint8_t replPosSearchOrderRB[]  = {1,3,2,4,6,5,7,0,0xFF};
static const uint8_t replPosSearchOrderLB[]  = {2,3,1,5,6,4,7,0,0xFF};
static const uint8_t replPosSearchOrderDef[] = {3,1,2,6,4,5,7,0,0xFF};
static const uint8_t replPosSearchOrderRW[]  = {4,6,5,3,1,2,7,0,0xFF};
static const uint8_t replPosSearchOrderLW[]  = {5,6,4,3,2,1,7,0,0xFF};
static const uint8_t replPosSearchOrderMid[] = {6,4,5,3,1,2,7,0,0xFF};
static const uint8_t replPosSearchOrderAtt[] = {7,6,4,5,3,1,2,0,0xFF};

static const uint8_t *replPosSearchOrderTable[8] = {
    replPosSearchOrderGK,
    replPosSearchOrderRB,
    replPosSearchOrderLB,
    replPosSearchOrderDef,
    replPosSearchOrderRW,
    replPosSearchOrderLW,
    replPosSearchOrderMid,
    replPosSearchOrderAtt,
};

/* ================================================================
 * findReplacementPlayerByPositionOrder — find best replacement in a range.
 * Asm: FindReplacementPlayerByPositionOrder
 *   d1=removedPlayerOrdinal, d5=rangeStart, d6=rangeEnd
 *   Returns best candidate ordinal in d2, also sets a3=player ptr.
 *
 * Priority: match position, prefer trialist/trial players ($9A/$9B form),
 * then compare by skill byte ($6C = price).
 * ================================================================ */
static uint16_t findReplacementPlayerByPositionOrder(
    uint16_t removedOrdinal, uint16_t rangeStart, uint16_t rangeEnd)
{
    /* Get the removed player to determine its position */
    uint8_t *removedPlayer = _GetPlayerAtIndex(selTeamNumber, removedOrdinal);
    PlayerFile *removedPf = (PlayerFile *)(removedPlayer + 76);
    uint16_t posIdx = (uint16_t)((removedPf->positionAndFace & 0xE0) >> 5);
    const uint8_t *searchOrder = replPosSearchOrderTable[posIdx];

    while (1) {
        uint8_t desiredPos = *searchOrder++;
        if (desiredPos == 0xFF) {
            /* Exhausted all position priorities — panic */
            panicReasonCode = 0x2719;
            _PanicHaltLoop();
        }

        int16_t bestSkill = -1;      /* d3: 0xFF as signed byte = -1 */
        uint16_t bestIdx = 0xFFFF;   /* d2 */
        PlayerFile *bestPf = (void *)0;   /* a3 */

        uint16_t i;
        for (i = rangeStart; i < rangeEnd; i++) {
            uint8_t *candidate = _GetPlayerAtIndex(selTeamNumber, i);
            PlayerFile *candPf = (PlayerFile *)(candidate + 76);
            uint8_t candPos = (uint8_t)((candPf->positionAndFace & 0xE0) >> 5);

            if (candPos != desiredPos)
                continue;

            if (bestSkill < 0) {
                /* No candidate yet — take this one */
                goto accept;
            }

            /* Check if existing best is trialist/trial */
            if ((uint8_t)bestPf->someFlag == 0x9B || (uint8_t)bestPf->someFlag == 0x9A) {
                /* Best IS trialist — compare skill directly */
                if ((uint8_t)bestSkill >= (uint8_t)candPf->price)
                    continue;
                goto accept;
            }

            /* Best is NOT trialist — if candidate IS trialist, prefer candidate */
            if ((uint8_t)candPf->someFlag == 0x9B || (uint8_t)candPf->someFlag == 0x9A) {
                goto accept;
            }

            /* Neither is trialist — compare by skill */
            if ((uint8_t)bestSkill >= (uint8_t)candPf->price)
                continue;

        accept:
            bestIdx = i;
            bestSkill = (int16_t)candPf->price;
            bestPf = candPf;
        }

        if (bestIdx != 0xFFFF) {
            return bestIdx;
        }
        /* No match for this position — try next priority */
    }
}

/* ================================================================
 * _Career_RemovePlayerFromSelectedTeamRoster — remove player at given index.
 * Asm: Career_RemovePlayerFromSelectedTeamRoster (d1=index)
 *
 * Accumulates 4 stat bytes from removed player into removedPlayerStatAccum.
 * Three paths based on slot tier:
 * - Starting (0-10): find bench replacement, swap in
 * - Bench (11-15): find reserve replacement or generate trialist if at 16
 * - Reserve (16+): straight array compaction
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_RemovePlayerFromSelectedTeamRoster(uint16_t index)
{
    uint8_t *player;
    uint16_t d0, d1, d2, d3;
    uint8_t *a0;
    uint8_t *a1;

    d1 = index;

    /* Get player record and accumulate stats */
    player = _GetPlayer(selTeamNumber, d1);
    {
        PlayerFile *pf = (PlayerFile *)(player + 76);
        d0 = (uint16_t)pf->goalStats[0];
        removedPlayerStatAccum += d0;
        d0 = (uint16_t)pf->goalStats[1];
        removedPlayerStatAccum += d0;
        d0 = (uint16_t)pf->goalStats[2];
        removedPlayerStatAccum += d0;
        d0 = (uint16_t)pf->goalStats[3];
        removedPlayerStatAccum += d0;
    }

    /* Remove weekly wage */
    _Career_RemovePlayerWeeklyWageAndRecalcWeeklyWages(d1);

    /* Get slot index by player number */
    d1 = _GetPlayerSlotIndexByPlayerNumber(selTeamNumber, d1);

    if (d1 < 0x0B) {
        /* Starting player (slots 0-10) */
        d2 = d1;

        /* Find bench replacement */
        d2 = findReplacementPlayerByPositionOrder(d1, 0x0B, 0x10);

        /* Swap in currentMatchPlayers */
        {
            uint8_t tmp = currentMatchPlayers[d2];
            currentMatchPlayers[d2] = currentMatchPlayers[d1];
            currentMatchPlayers[d1] = tmp;
        }

        /* Swap shirt numbers between the two */
        {
            uint8_t *p1 = _GetPlayer(selTeamNumber, (uint16_t)(int8_t)currentMatchPlayers[d1]);
            a1 = p1;
            uint8_t *p2 = _GetPlayer(selTeamNumber, (uint16_t)(int8_t)currentMatchPlayers[d2]);
            PlayerFile *pf1 = (PlayerFile *)(a1 + 76);
            PlayerFile *pf2 = (PlayerFile *)(p2 + 76);
            int8_t tmpShirt = pf2->shirtNumber;
            pf2->shirtNumber = pf1->shirtNumber;
            pf1->shirtNumber = tmpShirt;
        }

        d1 = d2;  /* now d1 = bench slot to be removed */
        /* Fall through to bench path */
        goto bench_path;
    }

    if (d1 < 0x10) {
        /* Bench player (slots 11-15) */
        d2 = d1;
    bench_path:
        if (*(uint16_t *)careerNumPlayers == 16) {
            /* Exactly 16 players — generate trialist replacement */
            d1 = (uint16_t)(int8_t)currentMatchPlayers[d2];
            _CCareer_GenerateTrialistForTeamPlayer(d1);
            _Career_UpdatePlayerWeeklyWageCache(d1);
            return;
        }

        /* Find reserve replacement */
        d3 = d2;
        {
            /* SelectReserveReplacementForBenchPlayer: range 16..careerNumPlayers */
            d1 = d2;
            d2 = findReplacementPlayerByPositionOrder(d1, 0x10, *(uint16_t *)careerNumPlayers);
        }

        /* Swap in currentMatchPlayers: d3 is bench slot, d2 is reserve slot */
        {
            uint8_t tmp = currentMatchPlayers[d3];
            currentMatchPlayers[d3] = currentMatchPlayers[d2];
            currentMatchPlayers[d2] = tmp;
        }

        /* Swap shirt numbers */
        {
            uint8_t *p1 = _GetPlayer(selTeamNumber, (uint16_t)(int8_t)currentMatchPlayers[d2]);
            a1 = p1;
            uint8_t *p2 = _GetPlayer(selTeamNumber, (uint16_t)(int8_t)currentMatchPlayers[d3]);
            PlayerFile *pf1 = (PlayerFile *)(a1 + 76);
            PlayerFile *pf2 = (PlayerFile *)(p2 + 76);
            int8_t tmpShirt = pf2->shirtNumber;
            pf2->shirtNumber = pf1->shirtNumber;
            pf1->shirtNumber = tmpShirt;
        }

        d1 = (uint16_t)(int8_t)currentMatchPlayers[d3];
        d3 = d2;  /* d3 = reserve slot to compact from */
        goto compact;
    }

    /* Reserve player (slots 16+) */
    d3 = d1;
    d1 = (uint16_t)(int8_t)currentMatchPlayers[d1];

compact:
    /* Shift currentMatchPlayers array to fill the gap at d3 */
    {
        uint16_t count = *(uint16_t *)careerNumPlayers - d3;
        volatile uint8_t *va0 = &currentMatchPlayers[d3];
        if (count >= 2) {
            uint16_t i;
            for (i = 0; i < count - 1; i++) {
                va0[i] = va0[i + 1];
            }
        }
        /* Sentinel at end */
        va0[count >= 2 ? count - 1 : 0] = 0xFF;
    }

    /* Write "*ERR* " into the removed player's name slot */
    {
        uint8_t *pRem = _GetPlayer(selTeamNumber, d1);
        PlayerFile *pf = (PlayerFile *)(pRem + 76);
        pf->playerName[0] = '*';
        pf->playerName[1] = 'E';
        pf->playerName[2] = 'R';
        pf->playerName[3] = 'R';
        pf->playerName[4] = '*';
        pf->playerName[5] = ' ';
    }

    _TrainingMatch_RemoveSavedPlayerId(d1);
    *(uint16_t *)careerNumPlayers -= 1;
}

/* ================================================================
 * _RemovePlayerFromTeam — wrapper that removes player from team data
 * and from the roster, sets dirty flag, prunes offers.
 * Asm: RemovePlayerFromTeam (d1=player ordinal, uses selTeamNumber)
 * ================================================================ */
__attribute__((used, externally_visible))
void _RemovePlayerFromTeam(uint16_t playerOrd)
{
    _Career_RemovePlayerFromTeamInternal(selTeamNumber, playerOrd);
    _Career_RemovePlayerFromSelectedTeamRoster(playerOrd);
    ((TeamFile *)careerTeam)->someFlag = 1;
    _PrunePlayerOffersAgainstRosterLimits();
}
