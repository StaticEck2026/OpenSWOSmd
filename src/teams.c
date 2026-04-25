/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "teams.h"
#include "career_teams.h"
#include "file_io.h"
#include "deterministic.h"
#include "menus.h"
#include "string_utils.h"
#include "input_text.h"
#include "panic.h"
#include "diy_competition.h"
#include "career_status.h"
#include "play_match_menu.h"
#include "career_init.h"

/* ---- Constants ---- */
#define GAME_TYPE_CAREER            4
#define ORIGINAL_CUSTOM_TEAMS   2
#define EDITED_CUSTOM_TEAMS     3
#define CN_EUROPE           80
#define CN_AFRICA           81
#define CN_SOUTH_AMERICA    82
#define CN_NORTH_AMERICA    83
#define CN_ASIA             84
#define CN_OCEANIA          85

/* ---- External functions ---- */
extern void _SetExitMenuFlag(void);

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
extern TeamFileEx careerTeam;
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

/* Globals for team selection menu (Chunks 107-109) */
extern uint16_t changeTeams;
extern uint16_t chooseTeamsResult;
extern uint16_t viewSelectedTeams;
extern uint16_t abortSelectTeams;
extern uint16_t exitSelectTeams;
extern int16_t  topOfStackMark;

/* Globals for SelectTeamsFinalMenu */
extern uint8_t customsLeague[];
extern uint32_t competitionsTable[];
extern uint32_t teamNames[];
extern uint32_t leaguesPtrTable[];
extern uint16_t leagueNumbersTable[];
extern uint32_t countryNames[];
extern uint16_t numLeaguesInCountry;
extern uint16_t chooseTeamsNumTeams;
extern int16_t  selectedTeamIndex;
extern int16_t  selectedLeagueIndex;
extern uint16_t showCupsAndOther;
extern uint16_t playSeason;
extern uint16_t loadTeam;
extern uint16_t stayInSelectTeams;
extern uint16_t exitFlag;
extern uint16_t isContestContinentalCup;
extern uint16_t isCountryNumber;
extern uint16_t gotCountryName;
extern uint16_t numberOfLeagues;
extern uint32_t selectedContest;
extern uint16_t selectTeamsNoLimit;
extern uint32_t chooseTeamsHeader1stPart;
extern uint32_t selectedTeamText;
extern uint32_t gameTypeStr;
extern uint32_t playWhatStr;
extern uint32_t entryText2;
extern uint8_t  someTeamFlags;
extern uint16_t isNationalSelector;
extern uint16_t chooseCompetitionOnly;
extern uint16_t countryNumber;
extern uint16_t leagueNumber;
extern uint16_t contestLeagueNumber;
extern const char *divisionStringTable[];
extern uint8_t  selectTeamsMenu[];
extern uint8_t  chooseTeamsFriendlyMenu[];
extern uint8_t  chooseTeamsFriendlyMenu2[];
extern uint8_t  europeanChampionships[];
extern uint8_t  worldCup[];
extern uint8_t  africanNationsCup[];
extern uint8_t  copaAmerica[];
extern uint8_t  northAmericanCup[];
extern uint8_t  asianCup[];
extern uint8_t  oceaniaCup[];
extern uint8_t  northAmericaWorldCupQualification[];
extern uint8_t  southAmericaWorldCupQualification[];
extern uint8_t  africaWorldCupQualification[];
extern uint8_t  asiaWorldCupQualification[];
extern uint8_t  oceaniaWorldCupQualification[];
extern uint8_t  aEditedCustomTe_0[]; /* "EDITED CUSTOM TEAMS" */
extern uint8_t  aPlaySeason[];       /* "PLAY SEASON" */
extern uint8_t  aPlayTournament[];   /* "PLAY TOURNAMENT" */
extern uint8_t  aPlayCup[];          /* "PLAY CUP" */
extern uint8_t  aPlayLeague[];       /* "PLAY LEAGUE" */
extern const char aCup_0[];          /* "CUP" */
extern const char aLeague_1[];       /* "LEAGUE" */
extern uint8_t  countriesStack[];

/* Globals for team navigation stack (Chunk 109) */
extern uint32_t countriesTable[];
extern uint8_t *curCountryStr;
extern uint8_t *countryStackPtr;
extern uint16_t teamDepthLevel;
extern uint16_t selectedTeamsStack[];
extern uint16_t lastSelectedTeam;
extern uint16_t prevSelectedTeamEntry;
extern uint16_t isCountryNumber;
extern uint16_t gotCountryName;
extern uint16_t numberOfLeagues;
extern uint16_t isContestContinentalCup;
extern uint32_t continentalCups[];
extern uint32_t selectedContest;
extern uint16_t teamsType;
extern uint32_t nationalTeamCompetitionsAndQualifications[];

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
#ifdef SWOS_SDL_BUILD
#include "endian_helpers.h"
/* Team files are stored big-endian (Amiga m68k native). On a little-endian
 * host, every multi-byte field would otherwise read byte-swapped. Within a
 * 684-byte team record only two uint16 fields exist:
 *   teamFileBuffer[0..1]              = team count (file header)
 *   teamFileBuffer[2 + i*684 + 2..3]  = TeamFile.globalTeamNumber per record
 * Everything else (TeamFile name fields, PlayerFile attributes) is
 * single-byte. Swap once after load so subsequent native reads work.
 *
 * Note: (countryNumber, teamOrdinal) at offsets 0/1 of each record are
 * two SEPARATE single-byte fields. Callers that want the 16-bit packed
 * team id must use PACK_BE_U16(), not a cast — see endian_helpers.h. */
static void byteswap_loaded_team_file(void)
{
    uint8_t *buf = teamFileBuffer;
    uint16_t count = BE_READ_U16(buf);
    *(uint16_t *)buf = count;                      /* store in host order */
    uint8_t *rec = buf + 2;
    for (uint16_t i = 0; i < count; i++) {
        *(uint16_t *)(rec + 2) = BE_READ_U16(rec + 2);
        rec += 684;
    }
}
#endif

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
#ifdef SWOS_SDL_BUILD
        byteswap_loaded_team_file();
#endif
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
#ifdef SWOS_SDL_BUILD
        byteswap_loaded_team_file();
#endif
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
uint16_t _IsTeamNational(TeamFileEx *team)
{
    if (gameType != GAME_TYPE_CAREER)
        return 1;
    if (team->team.countryNumber < CN_EUROPE)
        return 0;
    if (team->team.countryNumber > CN_OCEANIA)
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
    TeamFileEx *team;
    int32_t ext;

    if (numSelectedTeams == 0)
        return 0;

    team = (TeamFileEx *)&selTeamsPtr;
    total = 0;
    for (i = numSelectedTeams - 1; ; i--) {
        total += _GetAveragePlayerPrice(team);
        team++;
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
uint16_t _GetPlayerRandomNum(PlayerFile *playerFile)
{
    PlayerFile *pf = &playerFile[2];
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
    if (ptr >= (uint8_t *)&careerTeam && ptr < careerNumPlayers)
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
void _RotatePlayersLeft(TeamFileEx *team, uint16_t maxSubs)
{
    uint8_t *arr;
    int16_t count;

    if ((uint8_t *)team == currentTeam) {
        arr = playerPositions;
        count = (int16_t)numPlayers;
    } else if (team == &careerTeam) {
        arr = currentMatchPlayers;
        count = (int16_t)*(uint16_t *)careerNumPlayers;
    } else {
        arr = team->team.playerNumbers;
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
TeamFileEx *_AddTeamToBuffer(TeamFileEx *team)
{
    TeamFileEx *dest = (TeamFileEx *)(&selTeamsPtr + (uint32_t)numSelectedTeams * 684);
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
TeamFileEx *_FindTeam(TeamFileEx *teamToFind, uint16_t *outRemainingCount)
{
    uint16_t teamId = *(uint16_t *)teamToFind;  /* first word: countryNumber + teamOrdinal */
    uint16_t count = numSelectedTeams;
    if (count == 0)
        goto panic;
    count--;
    TeamFileEx *ptr = (TeamFileEx *)&selTeamsPtr;
    uint16_t remaining = count;
    while (1) {
        if (*(uint16_t *)ptr == teamId) {
            if (outRemainingCount)
                *outRemainingCount = remaining;
            return ptr;
        }
        ptr++;
        if (remaining == 0)
            break;
        remaining--;
    }
panic:
    panicReasonCode = 0x6987;
    _PanicHaltLoop();
    /* never reached */
    *outRemainingCount = 0;
    return (TeamFileEx *)0;
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
void _DeselectTeam(TeamFileEx *teamToRemove)
{
    uint16_t remaining;
    TeamFileEx *found = _FindTeam(teamToRemove, &remaining);
    if (remaining != 0) {
        uint16_t *dst = (uint16_t *)found;
        uint16_t *src = (uint16_t *)(found + 1);
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
        teamBase = (uint8_t *)&careerTeam;
    else
        teamBase = (uint8_t *)_GetTeamPointer(teamNumber);

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
        TeamFileEx *teamPtr = _GetTeamPointer(teamNumber);
        fileIndex = (uint16_t)teamPtr->team.playerNumbers[ordinal];
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
        uint16_t limit = *(uint16_t *)careerNumPlayers;
        while (currentMatchPlayers[idx] != (uint8_t)playerNumber) {
            idx++;
            if (idx >= limit) {
                panicReasonCode = 0x2617;
                _PanicHaltLoop();
            }
        }
    } else {
        /* Validate input */
        if (playerNumber >= 16) {
            panicReasonCode = 0x2618;
            _PanicHaltLoop();
        }
        /* Search team's player number table */
        TeamFileEx *teamPtr = _GetTeamPointer(teamNumber);
        while (teamPtr->team.playerNumbers[idx] != (uint8_t)playerNumber) {
            idx++;
            if (idx >= 16) {
                panicReasonCode = 0x2619;
                _PanicHaltLoop();
            }
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
extern void _Career_GenerateTrialistForTeamPlayer(uint16_t playerIndex);

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
void _SetBenchPlayersNumbers(TeamFileEx *team)
{
    uint16_t teamNum = BE_READ_U16(team);  /* first word: countryNumber + teamOrdinal */
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
            _Career_GenerateTrialistForTeamPlayer(d1);
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
    careerTeam.team.someFlag = 1;
    _PrunePlayerOffersAgainstRosterLimits();
}

/* ================================================================
 * _DeselectAllTeams (Chunk 107)
 * Removes all selectable teams from the selected teams buffer.
 * Teams with teamControls != 0 are skipped (not deselected).
 * Asm: DeselectAllTeams
 * ================================================================ */
__attribute__((used, externally_visible))
void _DeselectAllTeams(void)
{
    TeamFileEx *ptr = (TeamFileEx *)&selTeamsPtr;
    if (numSelectedTeams == 0)
        return;

    uint16_t idx = 0;
    while (idx < numSelectedTeams) {
        if (ptr->team.teamControls == 0) {
            /* DeselectTeam removes this entry and shifts the rest down,
             * so don't advance the pointer or index. */
            _DeselectTeam(ptr);
        } else {
            ptr++;
            idx++;
        }
    }
}

/* ================================================================
 * _ChangeTeams (Chunk 107)
 * Menu callback: deselect all teams, flag "change teams", exit menu.
 * Asm: ChangeTeams
 * ================================================================ */
__attribute__((used, externally_visible))
void _ChangeTeams(void)
{
    _DeselectAllTeams();
    changeTeams = 1;
    chooseTeamsResult = 0;
    viewSelectedTeams = 0;
    abortSelectTeams = 0;
    exitSelectTeams = 0;
    _SetExitMenuFlag();
}

/* ================================================================
 * _AbortChooseTeamsFriendly (Chunk 107)
 * Menu callback: deselect all, flag abort, exit menu.
 * Asm: AbortChooseTeamsFriendly
 * ================================================================ */
__attribute__((used, externally_visible))
void _AbortChooseTeamsFriendly(void)
{
    _DeselectAllTeams();
    abortSelectTeams = 1;
    changeTeams = 0;
    chooseTeamsResult = 0;
    viewSelectedTeams = 0;
    exitSelectTeams = 0;
    _SetExitMenuFlag();
}

/* ================================================================
 * _ChooseTeamsFriendlyOnSelect76 (Chunk 107)
 * Menu callback: deselect all, flag result chosen, exit menu.
 * Asm: ChooseTeamsFriendlyOnSelect76
 * ================================================================ */
__attribute__((used, externally_visible))
void _ChooseTeamsFriendlyOnSelect76(void)
{
    _DeselectAllTeams();
    chooseTeamsResult = 1;
    changeTeams = 0;
    viewSelectedTeams = 0;
    abortSelectTeams = 0;
    exitSelectTeams = 0;
    _SetExitMenuFlag();
}

/* ================================================================
 * _ViewSelected (Chunk 108)
 * Menu callback: flag "view selected teams", clear others, exit menu.
 * Asm: ViewSelected
 * ================================================================ */
__attribute__((used, externally_visible))
void _ViewSelected(void)
{
    viewSelectedTeams = 1;
    chooseTeamsResult = 0;
    changeTeams = 0;
    abortSelectTeams = 0;
    exitSelectTeams = 0;
    _SetExitMenuFlag();
}

/* ================================================================
 * _ExitChooseTeams2 (Chunk 108)
 * Menu callback: exit team selection. If topOfStackMark >= 0, sets
 * exitSelectTeams=1; otherwise sets abortSelectTeams=1.
 * Asm: ExitChooseTeams2
 * ================================================================ */
__attribute__((used, externally_visible))
void _ExitChooseTeams2(void)
{
    chooseTeamsResult = 0;
    viewSelectedTeams = 0;
    changeTeams = 0;
    if (topOfStackMark >= 0) {
        exitSelectTeams = 1;
        abortSelectTeams = 0;
    } else {
        abortSelectTeams = 1;
        exitSelectTeams = 0;
    }
    _SetExitMenuFlag();
}

/* ================================================================
 * _ChooseTeamsPlayMatchSelected (Chunk 108)
 * Menu callback: flag "play match selected", clear others, exit menu.
 * Asm: ChooseTeamsPlayMatchSelected
 * ================================================================ */
__attribute__((used, externally_visible))
void _ChooseTeamsPlayMatchSelected(void)
{
    chooseTeamsResult = 1;
    viewSelectedTeams = 0;
    changeTeams = 0;
    abortSelectTeams = 0;
    exitSelectTeams = 0;
    _SetExitMenuFlag();
}

/* ================================================================
 * Team navigation stack & contest checking (Chunk 109)
 * Asm: GetCountryName, PushContinent, PopContinent,
 *      SetSelectedTeamsStackTop, PushLastSelectedTeam,
 *      GetSelectedTeamsStackTop, MarkSelectedTeamsStackLocation,
 *      IsContestContinentalCup, SetTeamsTypeForSelectedContest,
 *      IsClubTeamsContest
 * ================================================================ */

/* GetCountryName: look up country name from countriesTable.
 * countriesTable[d0] is a pointer to a length-prefixed string; skip byte 0. */
__attribute__((used, externally_visible))
uint8_t *_GetCountryName(uint16_t countryId)
{
    uint8_t *entry = (uint8_t *)countriesTable[countryId];
    uint8_t *name = entry + 1;  /* skip length byte */
    curCountryStr = name;
    return name;
}

/* PushContinent: push country id onto continent stack */
__attribute__((used, externally_visible))
void _PushContinent(uint16_t countryId)
{
    uint8_t *ptr = countryStackPtr;
    *ptr++ = (uint8_t)countryId;
    countryStackPtr = ptr;
    teamDepthLevel++;
}

/* PopContinent: pop country id from continent stack */
__attribute__((used, externally_visible))
uint16_t _PopContinent(void)
{
    uint8_t *ptr = countryStackPtr;
    ptr--;
    uint8_t val = *ptr;
    *ptr = CN_OCEANIA;  /* sentinel value */
    countryStackPtr = ptr;
    teamDepthLevel--;
    return (uint16_t)val;
}

/* SetSelectedTeamsStackTop: write -1 at current depth in selectedTeamsStack */
__attribute__((used, externally_visible))
void _SetSelectedTeamsStackTop(void)
{
    selectedTeamsStack[teamDepthLevel] = 0xFFFF;
}

/* PushLastSelectedTeam: store lastSelectedTeam at current depth */
__attribute__((used, externally_visible))
void _PushLastSelectedTeam(void)
{
    selectedTeamsStack[teamDepthLevel] = lastSelectedTeam;
}

/* GetSelectedTeamsStackTop: read from stack into prevSelectedTeamEntry */
__attribute__((used, externally_visible))
void _GetSelectedTeamsStackTop(void)
{
    prevSelectedTeamEntry = selectedTeamsStack[teamDepthLevel];
}

/* MarkSelectedTeamsStackLocation: set topOfStackMark based on state flags */
__attribute__((used, externally_visible))
void _MarkSelectedTeamsStackLocation(void)
{
    topOfStackMark = (int16_t)teamDepthLevel;

    if (isCountryNumber != 0)
        topOfStackMark--;

    if (gotCountryName != 0 && numberOfLeagues == 1) {
        topOfStackMark = -1;
        return;
    }

    if (isContestContinentalCup != 0) {
        topOfStackMark = -1;
    }
}

/* IsContestContinentalCup: check if selectedContest is in continentalCups list */
__attribute__((used, externally_visible))
void _IsContestContinentalCup(void)
{
    uint32_t *ptr = continentalCups;
    for (;;) {
        uint32_t entry = *ptr++;
        if (entry == 0xFFFFFFFF)
            return;
        if (entry == selectedContest) {
            isContestContinentalCup = 1;
            return;
        }
    }
}

/* IsClubTeamsContest: check if contest is in nationalTeamCompetitionsAndQualifications.
 * Returns 0 if found (national team contest), 1 if not found (club teams contest). */
__attribute__((used, externally_visible))
uint16_t _IsClubTeamsContest(uint32_t contest)
{
    uint32_t *ptr = nationalTeamCompetitionsAndQualifications;
    /* Asm: cmpi.w #$FFFF,(a0) checks high word; terminator is 0xFFFFFFFF */
    while (*ptr != 0xFFFFFFFF) {
        if (*ptr == contest)
            return 0;
        ptr++;
    }
    return 1;
}

/* SetTeamsTypeForSelectedContest: set teamsType based on contest type */
__attribute__((used, externally_visible))
void _SetTeamsTypeForSelectedContest(void)
{
    teamsType = 1;
    if (_IsClubTeamsContest(selectedContest) == 0)
        teamsType = 0;
}

/* ================================================================
 * Chunk 151: FormatPlayerValueOrStatusText
 *
 * in: a0 = team entry base (player data at +0x4C)
 *     a1 = destination buffer
 *
 * Checks player status byte at offset 0x71 from base:
 *   0x9B -> "TRIAL"
 *   0x9A -> "RES"
 * Then checks flag byte at offset 0x66, bit 1:
 *   set -> "LOAN"
 * Otherwise: format as monetary value (K or M).
 * ================================================================ */
extern const char aTrial[];     /* "TRIAL" */
extern const char aRes[];       /* "RES" */
extern const char aLoan[];      /* "LOAN" */
extern const char a01m[];       /* "\x9C%0.%1M" */
extern const char a0m[];        /* "\x9C%0M" */
extern const char a1k[];        /* "\x9C%1K" */

__attribute__((used, externally_visible))
void _FormatPlayerValueOrStatusText(uint8_t *base, uint8_t *dst)
{
    /* Check status byte (someFlag in PlayerFile) */
    PlayerFile *pf = (PlayerFile *)(base + sizeof(TeamFile));
    if ((uint8_t)pf->someFlag == 0x9B) {
        _StringCopy((const uint8_t *)aTrial, dst);
        return;
    }
    if ((uint8_t)pf->someFlag == 0x9A) {
        _StringCopy((const uint8_t *)aRes, dst);
        return;
    }
    /* Check loan flag (positionAndFace bit 1) */
    if (pf->positionAndFace & 0x02) {
        _StringCopy((const uint8_t *)aLoan, dst);
        return;
    }

    /* Normal player: get displayed value, look up base price */
    uint16_t displayedVal = _GetDisplayedPlayerValue(base);
    uint16_t price = avgTeamValueBases[displayedVal];

    /* loc_127414: format value as currency string.
     * price is in thousands of pounds. Split into millions and sub-millions. */
    uint16_t millions = price / 1000;
    uint16_t subK = price % 1000;

    if (millions == 0) {
        /* Less than 1M: format as "XK" — %1=price */
        _PrintFormatted((const uint8_t *)a1k, dst, 0, price, 0, 0, 0, 0, 0, 0, 0, 0);
    } else if (subK == 0) {
        /* Exact millions: format as "XM" — %0=millions */
        _PrintFormatted((const uint8_t *)a0m, dst, millions, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    } else {
        /* Millions with decimal: check precision needed */
        uint16_t tens = subK / 10;      /* 2 most significant digits */
        uint16_t hundreds = tens / 10;   /* most significant digit */
        uint16_t tensRemainder = tens % 10;

        if (tensRemainder != 0) {
            /* Two decimal places: "X.YZM" — %0=millions, %1=tens */
            _PrintFormatted((const uint8_t *)a01m, dst, millions, tens, 0, 0, 0, 0, 0, 0, 0, 0);
        } else {
            /* One decimal place: "X.YM" — %0=millions, %1=hundreds */
            _PrintFormatted((const uint8_t *)a01m, dst, millions, hundreds, 0, 0, 0, 0, 0, 0, 0, 0);
        }
    }
}

/* ---- Chunk 219-220: ChooseTeams layout, team entries, selection ---- */

extern MenuEntry menuEntries[];
extern uint16_t chooseTeamsFriendly_leftListNumEntries;
extern uint16_t chooseTeamsFriendly_middleListNumEntries;
extern uint16_t chooseTeamsFriendly_rightListNumEntries;
extern uint16_t chooseTeamsFriendly_menuTopY;
extern uint16_t chooseTeamsFriendly_menuBottomY;
extern uint16_t viewTeamsColumn1;
extern uint16_t viewTeamsColumn2;
extern uint16_t viewTeamsColumn3;
extern uint16_t viewTeamsStartY;
extern uint16_t viewTeamsAbortY;
extern uint16_t selTeamsUpperBound;
extern uint8_t teamNumbersIndices[];
extern uint32_t teamPointers[];
extern uint16_t numSelectedTeamsCopy;
extern uint16_t numNeededTeams;
extern uint32_t playWhatStr;
extern uint8_t aChooseTeams[];
extern uint8_t aChangeTeams[];
extern uint8_t aSelect1MoreTea[];
extern uint8_t aSelect0MoreTea[];
extern uint8_t aSelect1LessTea[];
extern uint8_t aSelect0LessTea[];
extern uint8_t aError[];
extern int16_t chooseTeamsFriendly_changeTeamsPromptDrawList[];

static inline void setEntryBgColor(MenuEntry *e, uint16_t v) { setBgColorWord(e, v); }

__attribute__((used, externally_visible))
void _ChooseTeamsFriendly_UpdateMenuEntryYLayout(void)
{
    uint16_t topY = menuEntries[0].y + menuEntries[0].dwHeight;
    chooseTeamsFriendly_menuTopY = topY;
    uint16_t bottomY = menuEntries[56].y;
    chooseTeamsFriendly_menuBottomY = bottomY;

    /* d0 = max of left/middle/right list counts */
    uint16_t d0 = chooseTeamsFriendly_leftListNumEntries;
    if (d0 < chooseTeamsFriendly_middleListNumEntries)
        d0 = chooseTeamsFriendly_middleListNumEntries;
    if (d0 < chooseTeamsFriendly_rightListNumEntries)
        d0 = chooseTeamsFriendly_rightListNumEntries;

    d0 *= 10;
    int16_t d1 = (int16_t)(bottomY - topY) - (int16_t)d0;
    d1 -= (int16_t)menuEntries[1].dwHeight;
    /* ext.l d1; divu #3,d1 — unsigned division of sign-extended value */
    d1 = (int16_t)((uint32_t)(int32_t)d1 / 3);

    int16_t d2 = d1 + (int16_t)topY;

    /* Set y of entries 1..6 */
    MenuEntry *e = &menuEntries[1];
    int16_t i;
    for (i = 0; i < 6; i++) {
        e->y = (uint16_t)d2;
        e++;
    }

    /* d2 += menuEntries[1].dwHeight, then d1 += d2 */
    d2 += (int16_t)menuEntries[1].dwHeight;
    d1 += d2;

    /* 3 columns × 16 rows starting at entry 7 */
    e = &menuEntries[7];
    int16_t col;
    for (col = 0; col < 3; col++) {
        int16_t yPos = d1;
        int16_t row;
        for (row = 0; row < 16; row++) {
            e->y = (uint16_t)yPos;
            yPos += 10;
            e++;
        }
    }
}

__attribute__((used, externally_visible))
void _ViewSelTeamsSetColumns(void)
{
    /* d1 = max of column counts */
    uint16_t d1 = viewTeamsColumn1;
    if (d1 < viewTeamsColumn2)
        d1 = viewTeamsColumn2;
    if (d1 < viewTeamsColumn3)
        d1 = viewTeamsColumn3;

    uint16_t startY = menuEntries[0].y + menuEntries[0].dwHeight;
    viewTeamsStartY = startY;
    uint16_t abortY = menuEntries[75].y;
    viewTeamsAbortY = abortY;

    uint16_t headerHeight = menuEntries[2].dwHeight;
    uint16_t d4 = 10;

    /* Find largest d4 where entries fit */
    for (;;) {
        uint16_t totalHeight = d1 * d4;
        int16_t remaining = (int16_t)(abortY - startY) - (int16_t)headerHeight - (int16_t)totalHeight;
        if (remaining >= 4)
            break;
        d4--;
    }

    int16_t space = (int16_t)(abortY - startY) - (int16_t)headerHeight - (int16_t)(d1 * d4);
    /* ext.l d1; divu #3,d1 */
    int16_t gap = (int16_t)((uint32_t)(int32_t)space / 3);

    int16_t d2 = gap + (int16_t)startY;

    /* Set y of entries 2..7 */
    MenuEntry *e = &menuEntries[2];
    int16_t i;
    for (i = 0; i < 6; i++) {
        e->y = (uint16_t)d2;
        e++;
    }

    /* d2 += menuEntries[2].dwHeight, then gap += d2 */
    d2 += (int16_t)menuEntries[2].dwHeight;
    gap += d2;

    /* 3 columns × 22 rows starting at entry 8 */
    e = &menuEntries[8];
    int16_t col;
    for (col = 0; col < 3; col++) {
        int16_t yPos = gap;
        int16_t row;
        for (row = 0; row < 22; row++) {
            e->y = (uint16_t)yPos;
            e->dwHeight = d4 - 1;
            yPos += (int16_t)d4;
            e++;
        }
    }
}

/* ---- Chunk 220: SetTeamEntriesStringsAndColors, SetChooseChangeTeamsEntries,
       ChooseTeamsFriendly_DrawChangeTeamsPrompt, ChooseTeamsFriendlyCommon ---- */

#define TEAM_NOT_SELECTED 0
#define TEAMFILE_TEAMCONTROLS 4
#define TEAMFILE_TEAMNAME 5

__attribute__((used, externally_visible))
void _SetTeamEntriesStringsAndColors(void)
{
    uint8_t *indices = teamNumbersIndices;
    MenuEntry *e = &menuEntries[8];
    uint16_t d6 = 0;

    for (;;) {
        e->isInvisible = 1;

        if (d6 < selTeamsUpperBound) {
            for (;;) {
                uint16_t ord = e->ordinal;
                ord -= 8;
                uint16_t col = ord / 22;
                uint16_t row = ord % 22;

                uint16_t colCount;
                if (col == 0)
                    colCount = viewTeamsColumn1;
                else if (col == 1)
                    colCount = viewTeamsColumn2;
                else
                    colCount = viewTeamsColumn3;

                if (row < colCount) {
                    /* visible entry — set string and color */
                    e->isInvisible = 0;
                    int16_t idx = (int8_t)*indices++;
                    uint8_t *team = (uint8_t *)teamPointers[idx];
                    e->fg = (uint32_t)(team + TEAMFILE_TEAMNAME);

                    uint16_t color = PINK_TO_BROWN_9;
                    uint8_t ctrl = *(team + TEAMFILE_TEAMCONTROLS);
                    if (ctrl != TEAM_NOT_SELECTED) {
                        color = RED_10;
                        if (ctrl != TEAM_COMPUTER) {
                            color = BLUE_TO_PURPLE_11;
                            if (ctrl != TEAM_PLAYER_COACH)
                                color = BRIGHT_BLUE_TO_BLUE_13;
                        }
                    }
                    setEntryBgColor(e, color);
                    break;
                }

                /* skip invisible entry, advance to next */
                e++;
                if (e->ordinal == 74)
                    return;
                e->isInvisible = 1;
                continue;
            }
        }

        e++;
        if (e->ordinal == 74)
            return;
        d6++;
    }
}

__attribute__((used, externally_visible))
void _SetChooseChangeTeamsEntries(void)
{
    /* Entry 74 (0x1030/0x38) = "CHOOSE TEAMS" / "CHANGE TEAMS" label */
    MenuEntry *labelEntry = &menuEntries[74];
    uint8_t *label = aChooseTeams;
    if (numSelectedTeamsCopy == numNeededTeams)
        label = aChangeTeams;
    labelEntry->fg = (uint32_t)label;

    /* Entry 76 (0x10A0/0x38) = play prompt, Entry 77 (0x10D8/0x38) = select more/less */
    MenuEntry *playEntry = &menuEntries[76];
    MenuEntry *selectEntry = &menuEntries[77];

    if (numSelectedTeamsCopy == numNeededTeams) {
        playEntry->isInvisible = 0;
        selectEntry->isInvisible = 1;
        playEntry->content = ENTRY_STRING;
        playEntry->fg = playWhatStr;
    } else {
        playEntry->isInvisible = 1;
        selectEntry->isInvisible = 0;
    }

    uint8_t *dst = (uint8_t *)selectEntry->fg;
    int16_t diff = (int16_t)numSelectedTeamsCopy - (int16_t)numNeededTeams;
    if (diff == 0) {
        _StringCopy(aError, dst);
        return;
    }
    if (diff < 0) {
        diff = -diff;
        if (diff == 1) {
            _StringCopy(aSelect1MoreTea, dst);
            return;
        }
        _PrintFormatted(aSelect0MoreTea, dst, (uint32_t)diff, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    } else {
        if (diff == 1) {
            _StringCopy(aSelect1LessTea, dst);
            return;
        }
        _PrintFormatted(aSelect0LessTea, dst, (uint32_t)diff, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

__attribute__((used, externally_visible))
void _ChooseTeamsFriendly_DrawChangeTeamsPrompt(MenuEntry *savedA5)
{
    _SetChooseChangeTeamsEntries();
    _DrawMenuItemsWaitForVSync(chooseTeamsFriendly_changeTeamsPromptDrawList);
}

__attribute__((used, externally_visible))
void _ChooseTeamsFriendlyCommon(MenuEntry *entry)
{
    uint16_t d0 = entry->ordinal;
    d0 -= 8;
    /* Convert menu ordinal to linear team index across 3 columns */
    if (d0 >= 22) {
        if (d0 >= 44) {
            d0 -= 22;
            d0 += viewTeamsColumn2;
        }
        d0 -= 22;
        d0 += viewTeamsColumn1;
    }

    int8_t idx = (int8_t)teamNumbersIndices[d0];
    int16_t idx16 = (int16_t)idx;
    uint8_t *team = (uint8_t *)teamPointers[(uint16_t)idx16];
    uint8_t ctrl = *(team + TEAMFILE_TEAMCONTROLS);

    if (ctrl == TEAM_NOT_SELECTED) {
        setEntryBgColor(entry, RED_10);
        *(team + TEAMFILE_TEAMCONTROLS) = TEAM_COMPUTER;
        numSelectedTeamsCopy++;
    } else if (ctrl == TEAM_COMPUTER) {
        *(team + TEAMFILE_TEAMCONTROLS) = TEAM_PLAYER_COACH;
        setEntryBgColor(entry, BLUE_TO_PURPLE_11);
    } else if (ctrl == TEAM_PLAYER_COACH) {
        *(team + TEAMFILE_TEAMCONTROLS) = TEAM_COACH;
        setEntryBgColor(entry, BRIGHT_BLUE_TO_BLUE_13);
    } else {
        setEntryBgColor(entry, PINK_TO_BROWN_9);
        *(team + TEAMFILE_TEAMCONTROLS) = TEAM_NOT_SELECTED;
        numSelectedTeamsCopy--;
    }

    _ChooseTeamsFriendly_DrawChangeTeamsPrompt(entry);
    _CallDrawMenuItem(entry);
}

/* ---- Chunk 221: ChooseTeamsFriendlyCommon2 ---- */

extern uint8_t teamsIndex[];
extern uint32_t teamsPtrTable[];
extern uint32_t selectedTeamPtr;
extern uint16_t selectedTeamCountryNumber;
extern uint32_t chooseTeamsFunc;
extern uint16_t careerSquadScreenDidSelectPlayerFlag;
extern uint16_t abortSelectTeams;
extern uint16_t chooseTeamsResult;
extern uint8_t teamsTable[];
extern uint16_t numSelectedTeams;

extern void _Bureaucracy(void);
extern void _SetExitMenuFlag(void);

__attribute__((used, externally_visible))
void _ChooseTeamsFriendlyCommon2(MenuEntry *entry)
{
    uint16_t d0 = entry->ordinal;
    d0 -= 7;
    /* Convert menu ordinal to linear team index across 3 columns of 16 */
    if (d0 >= 16) {
        if (d0 >= 32) {
            d0 -= 16;
            d0 += chooseTeamsFriendly_middleListNumEntries;
        }
        d0 -= 16;
        d0 += chooseTeamsFriendly_leftListNumEntries;
    }

    int8_t idx = (int8_t)teamsIndex[d0];
    int16_t idx16 = (int16_t)idx;
    TeamFileEx *team = (TeamFileEx *)teamsPtrTable[(uint16_t)idx16];
    selectedTeamPtr = (uint32_t)team;
    selectedTeamCountryNumber = BE_READ_U16(team);

    if (chooseTeamsFunc != 0) {
        /* Asm: a0 = team pointer when jsr (a6) is called */
        void (*func)(uint8_t *) = (void (*)(uint8_t *))chooseTeamsFunc;
        func((uint8_t *)team);
        if (careerSquadScreenDidSelectPlayerFlag != 0) {
            abortSelectTeams = 1;
            chooseTeamsResult = 0;
            _SetExitMenuFlag();
            return;
        }
        return;
    }

    if (careerSquadScreenDidSelectPlayerFlag != 0) {
        abortSelectTeams = 1;
        chooseTeamsResult = 0;
        _SetExitMenuFlag();
        return;
    }

    uint8_t teamOrd = team->team.teamOrdinal;
    uint8_t *tableEntry = &teamsTable[teamOrd];
    uint8_t ctrl = *tableEntry;

    if (ctrl == 0) {
        /* unselected → select as CPU */
        if (numSelectedTeams >= 64)
            return;
        *tableEntry = 1;
        setEntryBgColor(entry, RED_10);
        TeamFileEx *copied = _AddTeamToBuffer(team);
        copied->team.teamControls = 1;
    } else if (ctrl == 1) {
        /* CPU → player coach */
        *tableEntry = 2;
        TeamFileEx *found = _FindTeam(team, (uint16_t *)0);
        found->team.teamControls = 2;
        setEntryBgColor(entry, BLUE_TO_PURPLE_11);
    } else if (ctrl == 2) {
        /* player coach → coach */
        *tableEntry = 3;
        TeamFileEx *found2 = _FindTeam(team, (uint16_t *)0);
        found2->team.teamControls = 3;
        setEntryBgColor(entry, BRIGHT_BLUE_TO_BLUE_13);
    } else {
        /* coach → unselect */
        *tableEntry = 0;
        setEntryBgColor(entry, PINK_TO_BROWN_9);
        _DeselectTeam(team);
    }

    _Bureaucracy();
    _CallDrawMenuItem(entry);
}

/* ---- Chunk 222: ChooseTeamsFriendlyInit ---- */

extern uint8_t selTeamsPtr;
extern uint16_t numSelectedTeams;
extern uint16_t viewSelectedTeams;
extern uint32_t chooseTeamsHeader1stPart;
extern uint32_t gameTypeStr;
extern uint8_t *curCountryStr;
extern uint16_t choosingPreset;
extern uint32_t selectedContest;
extern uint8_t copaAmerica[];
extern uint8_t aAllSelectedTea[];

extern uint16_t _GetStringLength(const uint8_t *str);
extern void _SetCurrentEntry(uint16_t entryIndex);

__attribute__((used, externally_visible))
void _ChooseTeamsFriendlyInit(void)
{
    numSelectedTeamsCopy = numSelectedTeams;
    selTeamsUpperBound = numSelectedTeams;

    /* Build teamPointers[] and teamNumbersIndices[] from selTeamsPtr */
    uint8_t *src = &selTeamsPtr;
    uint16_t i;
    for (i = 0; i < selTeamsUpperBound; i++) {
        teamPointers[i] = (uint32_t)src;
        teamNumbersIndices[i] = (uint8_t)i;
        src += 684;
    }

    /* Bubble sort teams alphabetically by name (offset 5 = TEAMFILE_TEAMNAME) */
    if (selTeamsUpperBound >= 2) {
        uint16_t n = selTeamsUpperBound - 2;
        uint16_t swapped;
        do {
            swapped = 0;
            uint8_t *idx = teamNumbersIndices;
            uint16_t j;
            for (j = 0; j <= n; j++) {
                int8_t i1 = (int8_t)idx[0];
                uint8_t *name1 = (uint8_t *)teamPointers[(uint16_t)(int16_t)i1] + TEAMFILE_TEAMNAME;
                int8_t i2 = (int8_t)idx[1];
                uint8_t *name2 = (uint8_t *)teamPointers[(uint16_t)(int16_t)i2] + TEAMFILE_TEAMNAME;

                uint8_t *p1 = name1;
                uint8_t *p2 = name2;
                for (;;) {
                    uint8_t c = *p1++;
                    if (c == 0)
                        break;
                    uint8_t c2 = *p2++;
                    if (c < c2)
                        break;
                    if (c > c2) {
                        /* swap indices */
                        uint8_t tmp = idx[0];
                        idx[0] = idx[1];
                        idx[1] = tmp;
                        swapped = 1;
                        break;
                    }
                }
                idx++;
            }
        } while (swapped);
    }

    /* Build header string */
    uint8_t *headerDst = (uint8_t *)menuEntries[0].fg;
    uint8_t *headerPrefix = (uint8_t *)chooseTeamsHeader1stPart;
    if (headerPrefix != (uint8_t *)0) {
        _StringCopy(headerPrefix, headerDst);
    } else {
        headerDst = _StringCopy(aAllSelectedTea, headerDst);
        uint8_t *gts = (uint8_t *)gameTypeStr;
        if (gts != (uint8_t *)0) {
            headerDst = _StringCopy(gts, headerDst);
        }
        /* Check if country name fits */
        uint16_t hdrLen = _GetStringLength((const uint8_t *)menuEntries[0].fg);
        uint16_t countryLen = _GetStringLength((const uint8_t *)curCountryStr);
        if (hdrLen + countryLen <= 47) {
            *headerDst++ = ' ';
            *headerDst++ = '-';
            *headerDst++ = ' ';
            _StringCopy((const uint8_t *)curCountryStr, headerDst);
        }
    }

    /* Calculate column counts */
    if (selTeamsUpperBound <= 8) {
        viewTeamsColumn1 = 0;
        viewTeamsColumn2 = selTeamsUpperBound;
        viewTeamsColumn3 = 0;
    } else {
        uint16_t base = selTeamsUpperBound / 3;
        uint16_t rem = selTeamsUpperBound % 3;
        viewTeamsColumn1 = base;
        viewTeamsColumn2 = base;
        viewTeamsColumn3 = base;
        if (rem == 1) {
            viewTeamsColumn2++;
        } else if (rem == 2) {
            viewTeamsColumn1++;
            viewTeamsColumn2++;
        }
    }

    _ViewSelTeamsSetColumns();

    /* Handle choosingPreset / copaAmerica */
    if (choosingPreset != 0) {
        if (selectedContest == (uint32_t)copaAmerica) {
            menuEntries[74].isInvisible = 1;
        }
    }

    _SetChooseChangeTeamsEntries();
    _SetTeamEntriesStringsAndColors();

    if (viewSelectedTeams == 2) {
        _SetCurrentEntry(76);
    } else if (viewSelectedTeams == 1) {
        _SetCurrentEntry(74);
    }
}

/* ---- Chunk 225: SelTeamForTacticsInit ---- */

extern uint32_t chooseTeamMenuName;
extern uint16_t numTeamsCountryBuffer;
extern uint16_t selTeamsColumn1;
extern uint16_t selTeamsColumn2;
extern uint16_t selTeamsColumn3;
extern uint8_t indexCountries[];
extern uint32_t dwArrayCountries[];

extern void _SelTeamForTactics_ReflowTeamListLayout(void);

__attribute__((used, externally_visible))
void _SelTeamForTacticsInit(void)
{
    menuEntries[0].fg = chooseTeamMenuName;

    /* Calculate column counts from numTeamsCountryBuffer */
    if (numTeamsCountryBuffer >= 66) {
        selTeamsColumn1 = 22;
        selTeamsColumn2 = 21;
        selTeamsColumn3 = 21;
    } else if (numTeamsCountryBuffer <= 8) {
        selTeamsColumn1 = 0;
        selTeamsColumn2 = numTeamsCountryBuffer;
        selTeamsColumn3 = 0;
    } else {
        uint16_t base = numTeamsCountryBuffer / 3;
        uint16_t rem = numTeamsCountryBuffer % 3;
        selTeamsColumn1 = base;
        selTeamsColumn2 = base;
        selTeamsColumn3 = base;
        if (rem == 1) {
            if (numTeamsCountryBuffer == 64)
                selTeamsColumn1++;
            else
                selTeamsColumn2++;
        } else if (rem == 2) {
            selTeamsColumn1++;
            selTeamsColumn2++;
        }
    }

    _SelTeamForTactics_ReflowTeamListLayout();
    /* bra.w *+4 = NOP */

    /* Build menu entries from indexCountries/dwArrayCountries */
    uint8_t *indices = indexCountries;
    MenuEntry *e = &menuEntries[1]; /* 0x38/56=1 */
    uint16_t d6 = 0;

    for (;;) {
        e->isInvisible = 1;

        if (d6 < numTeamsCountryBuffer) {
            for (;;) {
                uint16_t ord = e->ordinal;
                ord -= 1;
                uint16_t col = ord / 22;
                uint16_t row = ord % 22;

                uint16_t colCount;
                if (col == 0)
                    colCount = selTeamsColumn1;
                else if (col == 1)
                    colCount = selTeamsColumn2;
                else
                    colCount = selTeamsColumn3;

                if (row < colCount) {
                    /* Visible — set team name pointer */
                    e->isInvisible = 0;
                    int8_t idx = (int8_t)*indices++;
                    e->fg = dwArrayCountries[(uint16_t)(int16_t)idx];
                    break;
                }

                e++;
                if (e->ordinal == 67)
                    return;
                e->isInvisible = 1;
            }
        }

        e++;
        if (e->ordinal == 67)
            return;
        d6++;
    }
}

/* ---- Chunk 223: ChooseTeamsFriendlyInitFinish ---- */

extern int16_t topOfStackMark;
extern uint16_t selectTeamsNoLimit;
extern uint8_t aAbort[];
extern uint16_t inFriendlyMenu;
extern uint16_t leagueNumber;
extern uint16_t chooseTeamsFriendly_numTeamsInLeagueList;
extern uint8_t lastLoadedExtNumber;
extern uint8_t teamFileBuffer[];

#define TEAMFILE_LEAGUE 25

__attribute__((used, externally_visible))
void _ChooseTeamsFriendlyInitFinish(void)
{
    /* If topOfStackMark < 0, set abort button */
    if ((int16_t)topOfStackMark < 0) {
        MenuEntry *abortEntry = &menuEntries[56]; /* 0xC40/56=56 */
        abortEntry->fg = (uint32_t)aAbort;
        uint16_t bgc = *(uint16_t *)&abortEntry->bg;
        bgc = (bgc & 0xFFF0) + RED_10;
        *(uint16_t *)&abortEntry->bg = bgc;
    }

    /* If selectTeamsNoLimit, hide entries 1-6 */
    if (selectTeamsNoLimit != 0) {
        menuEntries[1].isInvisible = 1;
        menuEntries[2].isInvisible = 1;
        menuEntries[3].isInvisible = 1;
        menuEntries[4].isInvisible = 1;
        menuEntries[5].isInvisible = 1;
        menuEntries[6].isInvisible = 1;
    }

    /* Build teamsIndex[] and teamsPtrTable[] from teamFileBuffer */
    uint8_t *idx = teamsIndex;
    uint32_t *ptrs = teamsPtrTable;
    uint16_t numTeamsInFile = *(uint16_t *)teamFileBuffer;
    uint8_t *teamRecord = teamFileBuffer + 2;
    uint8_t league = (uint8_t)leagueNumber;
    uint16_t count = 0;
    uint16_t selNum = selTeamNumber;
    uint16_t i;

    for (i = 0; i < numTeamsInFile; i++) {
        if (*(teamRecord + TEAMFILE_LEAGUE) == league) {
            if (inFriendlyMenu != 0 || *(uint16_t *)teamRecord != selNum) {
                *idx++ = (uint8_t)count;
                *ptrs++ = (uint32_t)teamRecord;
                count++;
            }
        }
        teamRecord += 684;
    }
    chooseTeamsFriendly_numTeamsInLeagueList = count;

    /* Bubble sort teamsIndex by team name */
    if (count >= 2) {
        uint16_t n = count - 2;
        uint16_t swapped;
        do {
            swapped = 0;
            uint8_t *si = teamsIndex;
            uint16_t j;
            for (j = 0; j <= n; j++) {
                int8_t i1 = (int8_t)si[0];
                uint8_t *name1 = (uint8_t *)teamsPtrTable[(uint16_t)(int16_t)i1] + TEAMFILE_TEAMNAME;
                int8_t i2 = (int8_t)si[1];
                uint8_t *name2 = (uint8_t *)teamsPtrTable[(uint16_t)(int16_t)i2] + TEAMFILE_TEAMNAME;

                uint8_t *p1 = name1;
                uint8_t *p2 = name2;
                for (;;) {
                    uint8_t c = *p1++;
                    if (c == 0) break;
                    uint8_t c2 = *p2++;
                    if (c < c2) break;
                    if (c > c2) {
                        uint8_t tmp = si[0];
                        si[0] = si[1];
                        si[1] = tmp;
                        swapped = 1;
                        break;
                    }
                }
                si++;
            }
        } while (swapped);
    }

    /* Clear teamsTable (100 bytes) — use volatile to prevent memset optimization */
    {
        volatile uint8_t *tt = (volatile uint8_t *)teamsTable;
        for (i = 0; i < 100; i++)
            tt[i] = 0;
    }

    /* Mark previously selected teams in teamsTable */
    if (numNeededTeams != 1 && numSelectedTeams != 0) {
        uint8_t *selTeam = &selTeamsPtr;
        uint8_t extNum = lastLoadedExtNumber;
        uint8_t lg = (uint8_t)leagueNumber;
        uint16_t ns;

        for (ns = 0; ns < numSelectedTeams; ns++) {
            if (lastLoadedExtNumber == 0x64 ||
                (*(selTeam) == extNum && *(selTeam + 0x19) == lg)) {
                uint8_t ordinal = *(selTeam + 1);
                teamsTable[ordinal] = *(selTeam + 4);
            }
            selTeam += 684;
        }
    }

    /* Calculate column counts */
    if (chooseTeamsFriendly_numTeamsInLeagueList >= 48) {
        chooseTeamsFriendly_leftListNumEntries = 16;
        chooseTeamsFriendly_middleListNumEntries = 16;
        chooseTeamsFriendly_rightListNumEntries = 16;
    } else if (chooseTeamsFriendly_numTeamsInLeagueList <= 8) {
        chooseTeamsFriendly_leftListNumEntries = 0;
        chooseTeamsFriendly_middleListNumEntries = chooseTeamsFriendly_numTeamsInLeagueList;
        chooseTeamsFriendly_rightListNumEntries = 0;
    } else {
        uint16_t base = chooseTeamsFriendly_numTeamsInLeagueList / 3;
        uint16_t rem = chooseTeamsFriendly_numTeamsInLeagueList % 3;
        chooseTeamsFriendly_leftListNumEntries = base;
        chooseTeamsFriendly_middleListNumEntries = base;
        chooseTeamsFriendly_rightListNumEntries = base;
        if (rem == 1) {
            chooseTeamsFriendly_middleListNumEntries++;
        } else if (rem == 2) {
            chooseTeamsFriendly_leftListNumEntries++;
            chooseTeamsFriendly_middleListNumEntries++;
        }
    }

    _ChooseTeamsFriendly_UpdateMenuEntryYLayout();
    _ChooseTeamsSetHeader();
    _CheckNumTeamsAllowed();
    /* bra.w *+4 = NOP (fall through) */

    /* Build menu entries from teamsIndex/teamsPtrTable */
    MenuEntry *e = &menuEntries[7]; /* entry 7 = 0x188/0x38 */
    uint16_t d6 = 0;
    uint16_t d5 = 0;

outer:
    e->isInvisible = 1;

    /* Find next visible entry slot (row < column count) */
    for (;;) {
        uint16_t ord = e->ordinal;
        ord -= 7;
        uint16_t col = ord / 16;
        uint16_t row = ord % 16;

        uint16_t colCount;
        if (col == 0)
            colCount = chooseTeamsFriendly_leftListNumEntries;
        else if (col == 1)
            colCount = chooseTeamsFriendly_middleListNumEntries;
        else
            colCount = chooseTeamsFriendly_rightListNumEntries;

        if (row < colCount)
            break;

        e++;
        if (e->ordinal == 55)
            return;
        e->isInvisible = 1;
    }

    /* Entry is visible — populate it */
    uint16_t defaultColor = selectTeamsNoLimit ? 5 : 9;
    e->isInvisible = 0;

    int8_t tidx = (int8_t)teamsIndex[d5];
    uint8_t *team = (uint8_t *)teamsPtrTable[(uint16_t)(int16_t)tidx];
    uint8_t teamOrd = *(team + 1);
    e->fg = (uint32_t)(team + TEAMFILE_TEAMNAME);

    uint16_t color = defaultColor;
    uint8_t ctrl = teamsTable[teamOrd];
    if (ctrl != 0) {
        color = 10;
        if (ctrl != 1) {
            color = 11;
            if (ctrl != 2)
                color = 13;
        }
    }
    setEntryBgColor(e, color);

    /* Advance: mark remaining entries invisible, increment d5 if more teams */
    for (;;) {
        e++;
        if (e->ordinal == 55)
            return;
        e->isInvisible = 1;
        d6++;
        if (d6 < chooseTeamsFriendly_numTeamsInLeagueList) {
            d5++;
            goto outer;
        }
        /* d6 >= count: continue marking invisible */
    }
}

/* ---- Chunk 224: SetTeamsCoordinates, SelectTeamsReinit, SelectTeamsInit ---- */

extern uint16_t numTeamsColumn1;
extern uint16_t numTeamsColumn2;
extern uint16_t numTeamsColumn3;
extern uint16_t numLeaguesInCountry;
extern uint16_t chooseTeamsNumTeams;
extern uint16_t headerYPlusHeight;
extern uint16_t exitY;
extern uint16_t teamDepthLevel;
extern uint16_t startingTeamIndex;
extern uint16_t teamsLowerBound;
extern uint16_t prevSelectedTeamEntry;
extern uint16_t stayInSelectTeams;
extern uint16_t exitFlag;
extern uint8_t aChooseTeamsFor[];
extern uint8_t aChangeTeamsFor[];

extern void _SetLeagueNames(void);
extern void _SetCountryNames(void);
extern void _CheckNumberOfTeams(void);
extern void _ExitSelectTeams(void);
extern MenuEntry *_CalcMenuEntryAddress(uint16_t ordinal);
extern void _SelectLeaguesCommon(MenuEntry *entry);

__attribute__((used, externally_visible))
void _SetTeamsCoordinates(void)
{
    /* d1 = max of column counts */
    uint16_t d1 = numTeamsColumn1;
    if (d1 < numTeamsColumn2) d1 = numTeamsColumn2;
    if (d1 < numTeamsColumn3) d1 = numTeamsColumn3;

    d1 += numLeaguesInCountry;

    uint16_t topY = menuEntries[0].y + menuEntries[0].dwHeight;
    headerYPlusHeight = topY;
    exitY = menuEntries[88].y; /* 0x1340/56=88 */

    uint16_t totalHeight = d1 * 9;
    int16_t space = (int16_t)(exitY - headerYPlusHeight) - (int16_t)totalHeight;
    int16_t yPos = (space >> 1) + (int16_t)headerYPlusHeight;

    /* Set league entry Y coordinates */
    if (numLeaguesInCountry != 0) {
        /* tst.w chooseTeamsNumTeams is a dead check (followed by unconditional lea) */
        MenuEntry *e = &menuEntries[11]; /* 0x268/56=11 */
        int16_t n = (int16_t)numLeaguesInCountry - 1;
        int16_t j;
        for (j = 0; j <= n; j++) {
            e->y = (uint16_t)yPos;
            yPos += 9;
            e++;
        }
        /* Note: unreachable code at 55337-55348 in asm (after bra.s) — skipped */
    }

    /* Set team entry Y coordinates: 3 columns × 22 entries starting at entry 21 */
    yPos += 2;
    MenuEntry *e = &menuEntries[21]; /* 0x498/56=21 */
    int16_t col;
    for (col = 0; col < 3; col++) {
        int16_t y = yPos;
        int16_t row;
        for (row = 0; row < 22; row++) {
            e->y = (uint16_t)y;
            y += 9;
            e++;
        }
    }
}

__attribute__((used, externally_visible))
void _SelectTeamsReinit(void)
{
    /* Adjust Y of entries 87-90 if too many teams+leagues */
    uint16_t d0 = chooseTeamsNumTeams;
    d0 += 2;
    d0 = d0 / 3;
    d0 += numLeaguesInCountry;
    if (d0 > 18) {
        int16_t yAdj = 10;
        if (d0 != 19)
            yAdj = 15;
        menuEntries[87].y += yAdj; /* 0x1308/56=87 */
        menuEntries[88].y += yAdj; /* 0x1340/56=88 */
        menuEntries[89].y += yAdj; /* 0x1378/56=89 */
        menuEntries[90].y += yAdj; /* 0x13B0/56=90 */
    }

    /* If at top of stack, show "ABORT" button */
    if (teamDepthLevel == (uint16_t)topOfStackMark) {
        MenuEntry *abortEntry = &menuEntries[88]; /* 0x1340/56=88 */
        abortEntry->fg = (uint32_t)aAbort;
        uint16_t bgc = *(uint16_t *)&abortEntry->bg;
        bgc = (bgc & 0xFFF0) + RED_10;
        *(uint16_t *)&abortEntry->bg = bgc;
    }

    /* Build header string */
    uint8_t *headerDst = (uint8_t *)menuEntries[0].fg;
    uint8_t *prefix = (uint8_t *)chooseTeamsHeader1stPart;
    if (prefix != (uint8_t *)0) {
        headerDst = _StringCopy(prefix, headerDst);
    } else {
        uint8_t *label = aChooseTeamsFor;
        if (numSelectedTeams == numNeededTeams)
            label = aChangeTeamsFor;
        headerDst = _StringCopy(label, headerDst);
    }
    uint8_t *gts = (uint8_t *)gameTypeStr;
    if (gts != (uint8_t *)0) {
        headerDst = _StringCopy(gts, headerDst);
    }

    /* Check if country name fits (total <= 47) */
    uint16_t hdrLen = _GetStringLength((const uint8_t *)menuEntries[0].fg);
    uint16_t countryLen = _GetStringLength((const uint8_t *)curCountryStr);
    if (hdrLen + countryLen <= 47) {
        *headerDst++ = ' ';
        *headerDst++ = '-';
        *headerDst++ = ' ';
        _StringCopy((const uint8_t *)curCountryStr, headerDst);
    }

    /* Initialize team display parameters */
    startingTeamIndex = 0;
    teamsLowerBound = 0;

    /* Calculate column counts */
    if (chooseTeamsNumTeams >= 66) {
        numTeamsColumn1 = 22;
        numTeamsColumn2 = 21;
        numTeamsColumn3 = 21;
    } else if (chooseTeamsNumTeams <= 8) {
        numTeamsColumn1 = 0;
        numTeamsColumn2 = chooseTeamsNumTeams;
        numTeamsColumn3 = 0;
    } else {
        uint16_t base = chooseTeamsNumTeams / 3;
        uint16_t rem = chooseTeamsNumTeams % 3;
        numTeamsColumn1 = base;
        numTeamsColumn2 = base;
        numTeamsColumn3 = base;
        if (rem == 1) {
            if (chooseTeamsNumTeams == 64)
                numTeamsColumn1++;
            else
                numTeamsColumn2++;
        } else if (rem == 2) {
            numTeamsColumn1++;
            numTeamsColumn2++;
        }
    }

    _SetTeamsCoordinates();
    _SetLeagueNames();
    _SetCountryNames();

    /* Set cursor to previous selection if valid */
    uint16_t prevEntry = prevSelectedTeamEntry;
    if (prevEntry != 0xFFFF) {
        MenuEntry *e = _CalcMenuEntryAddress(prevEntry);
        if (e->isInvisible != 0) {
            prevEntry = 0xFFFF;
        } else {
            _SetCurrentEntry(prevEntry);
            goto doCheckTeams;
        }
    }
    if (chooseTeamsNumTeams == 0) {
        _SetCurrentEntry(11);
    }
doCheckTeams:
    _CheckNumberOfTeams();
}

/* ---- Chunk 226: ChooseTeamsInit ---- */
extern uint16_t showClubTeams;
extern uint16_t showNationalTeams;
extern uint16_t showOrigCustomTeams;

__attribute__((used, externally_visible))
void _ChooseTeamsInit(void)
{
    /* d7 = half the vertical gap between entry 0 and entry 1 */
    int16_t d7 = (int16_t)(menuEntries[1].y - menuEntries[0].y) >> 1;
    int16_t d6 = 4; /* count of visible categories */

    if (showClubTeams == 0) {
        d6--;
        menuEntries[0].isInvisible = 1;
        menuEntries[1].y -= d7;
        menuEntries[2].y -= d7;
        menuEntries[3].y -= d7;
        _SetCurrentEntry(1);
    }

    if (showNationalTeams == 0) {
        d6--;
        menuEntries[1].isInvisible = 1;
        menuEntries[0].y += d7;
        menuEntries[2].y -= d7;
        menuEntries[3].y -= d7;
        if (showClubTeams == 0)
            _SetCurrentEntry(2);
    }

    if (showOrigCustomTeams == 0) {
        d6 -= 2;
        menuEntries[2].isInvisible = 1;
        menuEntries[3].isInvisible = 1;
        menuEntries[0].y += d7;
        menuEntries[0].y += d7; /* asm adds d7 to y twice (offset 0x16) */
        menuEntries[1].y += d7;
        menuEntries[1].y += d7; /* same: y += 2*d7 total */
    }

    if (d6 == 1) {
        if (showClubTeams != 0) {
            _ClubTeams();
        } else {
            _NationalTeams();
        }
        return;
    }

    /* entry 5 = (menuEntries+MENUENTRY_ALREADYDRAWN+$118) = entry at offset 0x118/56 = 5 */
    MenuEntry *headerEntry = &menuEntries[5];
    char *dst = (char *)headerEntry->fg;
    char *src = (char *)chooseTeamsHeader1stPart;
    if (src != 0) {
        dst = (char *)_StringCopy((const uint8_t *)src, (uint8_t *)dst);
    } else {
        dst = (char *)_StringCopy(aChooseTeamsFor, (uint8_t *)dst);
    }
    src = (char *)gameTypeStr;
    if (src != 0) {
        _StringCopy((const uint8_t *)src, (uint8_t *)dst);
    }
}

__attribute__((used, externally_visible))
void _SelectTeamsInit(void)
{
    _SelectTeamsReinit();
    if (stayInSelectTeams != 0) {
        _SelectLeaguesCommon(&menuEntries[11]); /* entry 11 = 0x268/56 */
    } else {
        if (exitFlag != 0)
            _ExitSelectTeams();
    }
}

/* ================================================================
 * SelectTeamsFinalMenu  (SWOS2.s line 51856)
 *
 * Main team selection state machine. Enumerates competitions for
 * a given country, builds team/league name lists, shows the
 * selectTeamsMenu, and handles navigation, team loading, and
 * competition setup.
 *
 * Asm register convention:
 *   in:  d0.b = mode (0xFF=default, or a nationality/country number)
 * ================================================================ */
__attribute__((used, externally_visible))
void _SelectTeamsFinalMenu(uint16_t mode)
{
    uint16_t d6;            /* country/team-file number, preserved across calls */
    uint16_t d7;            /* current country index byte */
    uint16_t d1;            /* counter for enumeration */
    uint32_t *a5;           /* pointer into competition entry list, advances via (a5)+ */
    uint8_t *compEntry;     /* a1 - current competition entry pointer */
    uint32_t compListAddr;
    uint32_t *compList;
    uint32_t *teamNamesPtr;
    uint32_t *leaguesPtrOut;
    uint16_t *leagueNumsOut;
    uint32_t *countryNamesPtr;
    uint16_t cnCount;
    uint8_t *contest;
    uint16_t lnum;
    uint16_t countryForName;
    uint16_t countryIdx;
    uint8_t *ctEntry;
    uint8_t compType;
    uint16_t divIdx;
    uint8_t nameOff;
    uint8_t *namePtr;
    int16_t off16;
    uint8_t *nameTable;
    uint8_t *cPtr;
    uint8_t cByte;
    uint16_t cidx;
    uint16_t countryByte;

    leagueNumber = 0;

    /* Initialize customsLeague: offset 9 = comp.divisionsCount, offset 0xD = divisions[0].teams */
    customsLeague[9] = 3;
    customsLeague[0xD] = 20;

    if (teamsType == EDITED_CUSTOM_TEAMS) {
        if (_Asset_PromptInsertSaveDiskDf0(aEditedCustomTe_0) != 0)
            goto exit_clear;
        if (_LoadTeamFile(100) != 0)
            goto exit_clear;
        if (*(uint16_t *)teamFileBuffer == 0x30) {
            customsLeague[9] = 1;
            customsLeague[0xD] = 0x30;
        }
    }

    /* loc_13DF8E: always executed (unconditional in asm) */
    customsLeague[9] = 1;
    customsLeague[0xD] = 0x30;

    exitSelectTeams = 0;
    isCountryNumber = 0;
    isContestContinentalCup = 0;

    if ((uint8_t)mode != 255) {
        d6 = (uint8_t)mode;
        d7 = (uint8_t)mode;
        teamDepthLevel = 2;
        topOfStackMark = 2;
        goto loc_E004;
    }

    /* loc_13DFD4: default mode */
    d6 = 254;
    if (teamsType == EDITED_CUSTOM_TEAMS) {
        d6 = 100;
    } else if (teamsType == ORIGINAL_CUSTOM_TEAMS) {
        d6 = 72;
    }

    /* loc_13DFF6: */
    d7 = (uint8_t)d6;
    topOfStackMark = 0;
    teamDepthLevel = 0;

loc_E004:
    _SetSelectedTeamsStackTop();
    countryStackPtr = countriesStack;

loc_E012:
    /* Main loop: enumerate competitions for country d7 */
    d7 &= 0xFF;
    countryForName = d7;
    if (isCountryNumber != 0)
        countryForName = d6;
    _GetCountryName(countryForName);

    gotCountryName = 0;
    if (teamDepthLevel == 2 && isCountryNumber == 0)
        gotCountryName = 1;

    /* loc_13E046: enumerate competitions */
    numberOfLeagues = 0;
    compListAddr = competitionsTable[d7];
    if (compListAddr == 0) {
        panicReasonCode = 0x8632;
        _PanicHaltLoop();
    }
    compList = (uint32_t *)compListAddr;
    teamNamesPtr = teamNames;
    leaguesPtrOut = leaguesPtrTable;
    leagueNumsOut = leagueNumbersTable;
    d1 = 0;
    a5 = compList;

    /* loc_13E086: iterate through competition entries */
loc_E086:
    compEntry = (uint8_t *)*a5;
    a5++;

    if ((uint32_t)compEntry == 0xFFFFFFFF)
        goto loc_E2D0;

    if ((uint32_t)compEntry == 0xFFFFFFFE) {
        if (showCupsAndOther == 0)
            goto loc_E2D0;
        goto loc_E086;
    }

    /* Career mode continental cup filtering (loc_13E0A6) */
    if (gameType == GAME_TYPE_CAREER) {
        if (compEntry == africanNationsCup) {
            if ((uint8_t)(selTeamNumber >> 8) != CN_AFRICA)
                goto loc_E086;
            goto loc_E114;
        }
        if (compEntry == copaAmerica) {
            if ((uint8_t)(selTeamNumber >> 8) != CN_SOUTH_AMERICA)
                goto loc_E086;
            goto loc_E114;
        }
        if (compEntry == northAmericanCup) {
            if ((uint8_t)(selTeamNumber >> 8) != CN_NORTH_AMERICA)
                goto loc_E086;
            goto loc_E114;
        }
        if (compEntry == asianCup) {
            if ((uint8_t)(selTeamNumber >> 8) != CN_ASIA)
                goto loc_E086;
            goto loc_E114;
        }
        if (compEntry == oceaniaCup) {
            if ((uint8_t)(selTeamNumber >> 8) != CN_OCEANIA)
                goto loc_E086;
            goto loc_E114;
        }
    }

    /* loc_13E120: European Championships check */
    if (compEntry == europeanChampionships) {
        if (gameType == GAME_TYPE_CAREER) {
            if ((uint8_t)(selTeamNumber >> 8) != CN_EUROPE)
                goto loc_E086;
            if (!(someTeamFlags & (1 << 0))) {
                if (!(someTeamFlags & (1 << 1)))
                    goto loc_E086;
            }
        }
    }

    goto skip_continental_check;
loc_E114:
    if (!(someTeamFlags & (1 << 1)))
        goto loc_E086;
skip_continental_check:

    /* loc_13E154: World Cup qualification checks */
    if ((uint32_t)compEntry == 252 ||
        compEntry == northAmericaWorldCupQualification ||
        compEntry == southAmericaWorldCupQualification ||
        compEntry == africaWorldCupQualification ||
        compEntry == asiaWorldCupQualification ||
        compEntry == oceaniaWorldCupQualification) {
        /* loc_13E184: */
        if (gameType != GAME_TYPE_CAREER)
            goto loc_E086;
        if (isNationalSelector == 0)
            goto loc_E086;
        if (!(someTeamFlags & (1 << 6)))
            goto loc_E086;
    }

    /* loc_13E1A6: World Cup check */
    if (compEntry == worldCup) {
        if (gameType == GAME_TYPE_CAREER) {
            if (isNationalSelector == 0)
                goto loc_E086;
            if (!(someTeamFlags & (1 << 7)))
                goto loc_E086;
        }
    }

    /* loc_13E1CE: depth level filtering */
    if (showCupsAndOther == 0 &&
        teamsType != ORIGINAL_CUSTOM_TEAMS &&
        teamsType != EDITED_CUSTOM_TEAMS &&
        teamDepthLevel != 2) {
        goto loc_E086;
    }

    /* loc_13E1F6: entry type dispatch */
    if ((uint32_t)compEntry < 256) {
        /* Country entry: get country name from countriesTable */
        countryIdx = (uint16_t)(uint32_t)compEntry;
        ctEntry = (uint8_t *)countriesTable[countryIdx];
        *teamNamesPtr++ = (uint32_t)(ctEntry + 1);
        *leaguesPtrOut++ = (uint32_t)compEntry;
        *leagueNumsOut++ = 0xFFFF;
        d1++;
        goto loc_E086;
    }

    /* loc_13E21E: competition entry (>= 256) */
    compType = compEntry[1];
    if (compType == 0) {
        /* League: enumerate divisions */
        divIdx = 0;
loc_E246:
        nameOff = compEntry[5];
        if (nameOff == 0) {
            /* Use divisionStringTable */
            namePtr = (uint8_t *)divisionStringTable[divIdx];
        } else {
            /* Name table relative to offset 5 */
            off16 = (int8_t)nameOff;
            nameTable = compEntry + 5 + off16;
            namePtr = (uint8_t *)*(uint32_t *)(nameTable + (divIdx << 3));
        }
        *teamNamesPtr++ = (uint32_t)namePtr;
        *leaguesPtrOut++ = (uint32_t)compEntry;
        *leagueNumsOut++ = divIdx;
        d1++;
        divIdx++;
        if ((uint8_t)divIdx != compEntry[9])
            goto loc_E246;
        numberOfLeagues = (int8_t)compEntry[9];
        goto loc_E086;
    } else if (compType == 1) {
        /* Cup */
        nameOff = compEntry[5];
        if (nameOff == 0) {
            namePtr = (uint8_t *)aCup_0;
        } else {
            off16 = (int8_t)nameOff;
            namePtr = (uint8_t *)*(uint32_t *)(compEntry + 5 + off16);
        }
        *teamNamesPtr++ = (uint32_t)namePtr;
        *leaguesPtrOut++ = (uint32_t)compEntry;
        *leagueNumsOut++ = 0xFFFF;
        d1++;
        goto loc_E086;
    } else if (compType == 2) {
        /* Tournament */
        nameOff = compEntry[5];
        if (nameOff == 0) {
            namePtr = (uint8_t *)aLeague_1;
        } else {
            off16 = (int8_t)nameOff;
            namePtr = (uint8_t *)*(uint32_t *)(compEntry + 5 + off16);
        }
        *teamNamesPtr++ = (uint32_t)namePtr;
        *leaguesPtrOut++ = (uint32_t)compEntry;
        *leagueNumsOut++ = 0xFFFF;
        d1++;
        goto loc_E086;
    } else {
        panicReasonCode = 0x2209;
        _PanicHaltLoop();
    }

loc_E2D0:
    numLeaguesInCountry = d1;

    /* Build country names list */
    countryNamesPtr = countryNames;
    cnCount = 0;

    if (d7 == 254 || (d7 >= CN_EUROPE && d7 <= CN_OCEANIA)) {
        /* loc_13E2F4: enumerate countries from continent byte list */
        cPtr = (uint8_t *)a5;
        for (;;) {
            cByte = *cPtr++;
            if (cByte == 255)
                break;
            cidx = (uint16_t)cByte;
            ctEntry = (uint8_t *)countriesTable[cidx];
            *countryNamesPtr++ = (uint32_t)(ctEntry + 1);
            cnCount++;
        }
    }

    chooseTeamsNumTeams = cnCount;

    /* loc_13E318: prepare menu */
    stayInSelectTeams = 0;
    exitFlag = 0;

    if (showCupsAndOther == 0 && chooseTeamsNumTeams == 0 &&
        numLeaguesInCountry == 1) {
        if (exitSelectTeams != 0) {
            exitFlag = 1;
        } else {
            stayInSelectTeams = 1;
        }
    }

    /* loc_13E35E: show menu */
    _GetSelectedTeamsStackTop();
    _ShowMenu(selectTeamsMenu);

    if (chooseTeamsResult != 0)
        goto exit_return;
    if (abortSelectTeams != 0)
        goto exit_return;

    d6 = d7;

    if (viewSelectedTeams != 0)
        goto loc_E73A;

    /* Check selectedTeamIndex */
    d7 = (uint16_t)selectedTeamIndex;
    if (selectedTeamIndex >= 0)
        goto loc_E3DC;

    /* selectedTeamIndex < 0: check selectedLeagueIndex */
    d7 = (uint16_t)selectedLeagueIndex;
    if (selectedLeagueIndex < 0) {
        /* loc_13E3C4: go back */
        if (teamDepthLevel == 0)
            goto exit_return;
        isCountryNumber = 0;
        d7 = _PopContinent();
        goto loc_E012;
    }

    _PushLastSelectedTeam();

    if (loadTeam != 0)
        goto loc_E6E4;
    if (playSeason != 0)
        goto loc_E428;
    goto loc_E50A;

loc_E3DC:
    _PushLastSelectedTeam();

    if (showCupsAndOther != 0 || playSeason != 0 || teamsType != 0) {
        /* loc_13E3F8: push and navigate deeper */
        _PushContinent(d6);
        d7 = ((uint8_t *)a5)[d7];
        _SetSelectedTeamsStackTop();
        goto loc_E012;
    }

    /* loc_13E408: direct team load */
    countryByte = ((uint8_t *)a5)[d7];
    _GetCountryName(countryByte & 0xFF);
    leagueNumber = 0;
    _LoadTeamFile((uint8_t)countryByte);
    goto loc_E6F6;

loc_E428:
    /* Play season: dispatch by competition type */
    contest = (uint8_t *)leaguesPtrTable[d7];
    selectedContest = (uint32_t)contest;

    if (contest[1] == 0) {
        /* League: loc_13E476 */
        lnum = leagueNumbersTable[d7];
        leagueNumber = lnum;
        contestLeagueNumber = lnum;
        _InitCompetitionTeamSelectionFromTableEntry(lnum, contest);
    } else if (contest[1] == 2) {
        /* Tournament: loc_13E45A */
        leagueNumber = 0;
        contestLeagueNumber = 0;
        _InitCompetitionTeamSelectionFromAltTableHeader(contest);
    } else {
        panicReasonCode = 0x1789;
        _PanicHaltLoop();
    }

    /* loc_13E49E: */
    playWhatStr = (uint32_t)aPlaySeason;
    _MarkSelectedTeamsStackLocation();
    selectTeamsNoLimit = 0;
    playSeason = 0;
    loadTeam = 1;
    chooseTeamsHeader1stPart = 0;
    teamsType = 1;
    gameTypeStr = selectedTeamText;
    viewSelectedTeams = 3;
    _GetCountryName(d6);
    goto loc_E73A;

loc_E4EA:
    /* Country number navigation */
    d7 = (uint16_t)(uint32_t)contest;
    _PushLastSelectedTeam();
    _PushContinent(d6);
    _SetSelectedTeamsStackTop();
    isCountryNumber = 1;
    countryNumber = d6;
    goto loc_E012;

loc_E50A:
    /* Select league/competition: dispatch */
    contest = (uint8_t *)leaguesPtrTable[d7];

    /* Career mode special checks (loc_13E50A) */
    if (gameType == GAME_TYPE_CAREER) {
        if (contest == europeanChampionships) {
            if (someTeamFlags & (1 << 0))
                contest = (uint8_t *)0xFD;
        }
        if (contest == worldCup) {
            if (someTeamFlags & (1 << 6))
                contest = (uint8_t *)0xFC;
        }
    }

    /* loc_13E554: check if country entry (< 256) */
    if ((uint32_t)contest < 256)
        goto loc_E4EA;

    selectedContest = (uint32_t)contest;

    if (contest[1] == 0)
        goto loc_E65A;
    else if (contest[1] == 1)
        goto loc_E5EA;
    else
        goto loc_E57A;

loc_E57A:
    /* Tournament setup */
    if (chooseCompetitionOnly != 0) {
        chooseTeamsResult = 1;
        goto exit_return;
    }
    _InitCompetitionTeamSelectionFromAltTableHeader((uint8_t *)selectedContest);
    playWhatStr = (uint32_t)aPlayTournament;
    _SetTeamsTypeForSelectedContest();
    _IsContestContinentalCup();
    _MarkSelectedTeamsStackLocation();
    selectTeamsNoLimit = 0;
    showCupsAndOther = 0;
    loadTeam = 1;
    chooseTeamsHeader1stPart = 0;
    gameTypeStr = selectedTeamText;
    viewSelectedTeams = 2;
    _GetCountryName(d6);
    goto loc_E73A;

loc_E5EA:
    /* Cup setup */
    if (chooseCompetitionOnly != 0) {
        chooseTeamsResult = 1;
        goto exit_return;
    }
    _InitCompetitionTeamSelectionFromTableHeader((uint8_t *)selectedContest);
    playWhatStr = (uint32_t)aPlayCup;
    _SetTeamsTypeForSelectedContest();
    _IsContestContinentalCup();
    _MarkSelectedTeamsStackLocation();
    selectTeamsNoLimit = 0;
    showCupsAndOther = 0;
    loadTeam = 1;
    chooseTeamsHeader1stPart = 0;
    gameTypeStr = selectedTeamText;
    viewSelectedTeams = 2;
    _GetCountryName(d6);
    goto loc_E73A;

loc_E65A:
    /* League setup */
    lnum = leagueNumbersTable[d7];
    leagueNumber = lnum;
    contestLeagueNumber = lnum;

    if (chooseCompetitionOnly != 0) {
        chooseTeamsResult = 1;
        goto exit_return;
    }
    _InitCompetitionTeamSelectionFromTableEntry(lnum, (uint8_t *)selectedContest);
    playWhatStr = (uint32_t)aPlayLeague;
    _SetTeamsTypeForSelectedContest();
    _IsContestContinentalCup();
    _MarkSelectedTeamsStackLocation();
    selectTeamsNoLimit = 0;
    showCupsAndOther = 0;
    loadTeam = 1;
    chooseTeamsHeader1stPart = 0;
    gameTypeStr = selectedTeamText;
    viewSelectedTeams = 2;
    _GetCountryName(d6);
    goto loc_E73A;

loc_E6E4:
    /* Load team file for selected league */
    leagueNumber = d7;
    if (_LoadTeamFile((uint8_t)d6) != 0)
        goto exit_clear;

loc_E6F6:
    /* Show friendly menu */
    entryText2 = selectedTeamText;
    _ShowMenu(chooseTeamsFriendlyMenu);

    if (viewSelectedTeams != 0)
        goto loc_E73A;
    if (abortSelectTeams != 0)
        goto exit_return;
    if (chooseTeamsResult != 0)
        goto exit_return;

    /* loc_13E734: */
    d7 = d6;
    goto loc_E012;

loc_E73A:
    /* Show friendly menu 2 (with country name if needed) */
    if (isCountryNumber != 0)
        _GetCountryName(countryNumber);
    entryText2 = selectedTeamText;
    _ShowMenu(chooseTeamsFriendlyMenu2);

    if (abortSelectTeams != 0)
        goto exit_return;
    if (chooseTeamsResult != 0)
        goto exit_return;

    if (isCountryNumber != 0) {
        isCountryNumber = 0;
        d7 = _PopContinent();
        if (isContestContinentalCup != 0)
            goto loc_E6F6;
        goto loc_E012;
    }

    /* loc_13E79A: */
    if (isContestContinentalCup != 0)
        goto loc_E6F6;

    /* loc_13E734: */
    d7 = d6;
    goto loc_E012;

exit_clear:
    /* loc_13E7A6: */
    chooseTeamsResult = 0;
exit_return:
    /* locret_13E7AC: */
    return;
}
