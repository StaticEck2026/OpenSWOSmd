/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* diy_competition.c — DIY competition/tournament management, player pricing.
 *
 * Chunk 60: 16 functions ported from SWOS2.s.
 */

#include "diy_competition.h"
#include "auto_skip_debug.h"
#include "endian_helpers.h"
#include "game_types.h"
#include "string_utils.h"
#include "random.h"
#include "deterministic.h"
#include "panic.h"
#include "menu_system.h"
#include "text_render.h"
#include "input_text.h"
#include "match_sim.h"
#include "career_status.h"
#include "career_season.h"
#include "game_init.h"
#include <string.h>   /* memcpy — used by the tournament field-map copies */

/* ---- Constants ---- */
#define DIY_CUP                         1

/* ---- ASM externs (globals defined in assembly) ---- */
/* diyCompetitionStart remains a byte-array-backed DIYCompetitionEx (it's
 * 0x5C8 bytes; standings extend past that). diyFileBufferCopy is now the
 * typed tournament struct (step-3 untangle) — its former companion symbols
 * diyTournamentTeamIds / diyTournamentCompetition are now the named fields
 * diyFileBufferCopy.teamIds / diyFileBufferCopy.header. */
extern DIYTournamentState diyFileBufferCopy;
extern DIYCompetitionEx diyCompetitionStart;
/* The standalone backing buffer behind diyCompetitionStart (7136 B). The
 * league standings table extends past sizeof(DIYCompetitionEx) into it, so
 * the current-stage copies below address it as the byte array to move the
 * full 0x734-byte region without a (false) fortify overread/overflow. */
extern uint8_t diyCompetitionStartBuf[];
extern uint8_t diyTeamShuffleIndices[];
extern uint16_t diyTeamEntryOffsetsByIndex[];
extern uint8_t selTeamsPtr;
extern uint8_t selTeamNameBase;
extern uint16_t selTeamNumber;
extern uint16_t careerNumPlayers;
extern uint8_t currentMatchPlayers[];
extern TeamFileEx careerTeam;
extern uint16_t currentTick;
extern uint16_t randomSeed;
extern uint8_t newRandomSeedCopy;
extern uint8_t viewRandomPhase;
extern uint16_t numSelectedTeams;
extern uint8_t quadrantDistanceFactor[];

/* ---- DIY team-selection identity helpers (step-4 phase 3) ----
 * A DIY team-selection value (stored in DIYTeamStats.teamSelOffset and
 * bracketScratch.teamOffsetTable[]) is now a stable team IDENTITY, not a raw
 * byte offset: -1 == careerTeam, >= 0 == the team's index into the selTeamsPtr
 * array. diy_resolve_team / diy_resolve_team_name turn an identity into the team
 * record / team-name address; diy_team_to_identity is the inverse used at the
 * store sites. careerTeam is reached by a branch, NOT by `&selTeamsPtr +
 * (large negative)`, so it no longer has to live adjacent below selTeamsPtr and
 * can be extracted from the overlay (phase 4). The JSON save was already
 * identity-based (see save_format_json.c), so this is save-format-neutral; only
 * the in-memory representation changed. See docs/CAREERTEAM_STEP4_REWORK.md. */
uint8_t *diy_resolve_team(int32_t teamSel)
{
    if (teamSel < 0)                                          /* -1 = careerTeam */
        return (uint8_t *)&careerTeam;
    return (uint8_t *)((TeamFileEx *)&selTeamsPtr + teamSel); /* selTeamsPtr + index*684 */
}
uint8_t *diy_resolve_team_name(int32_t teamSel)
{
    /* the team-name field sits at the same offset within every team record;
     * selTeamNameBase - selTeamsPtr is that offset, valid for careerTeam too. */
    return diy_resolve_team(teamSel) + (&selTeamNameBase - &selTeamsPtr);
}
/* Inverse: team record pointer -> identity (the value stored in the standings
 * row / cup-bracket slot). careerTeam -> -1; a selected team -> its array index.
 * Also used by the save LOAD reconstruction (save_format_json.c) when it rebuilds
 * standings/brackets from _VerifyTeamLoaded pointers. */
int32_t diy_team_to_identity(const uint8_t *teamPtr)
{
    if (teamPtr == (const uint8_t *)&careerTeam)
        return -1;
    /* selected team: positive difference (above selTeamsPtr), 684-aligned */
    return (int32_t)((teamPtr - (const uint8_t *)&selTeamsPtr) / (int32_t)sizeof(TeamFileEx));
}

/* ASM string constants */
extern uint8_t aDiyTournament[];  /* "DIY TOURNAMENT" */
extern uint8_t aDiyLeague[];      /* "DIY LEAGUE" */
extern uint8_t aDiyCup[];         /* "DIY CUP" */

/* ASM globals for DIY selection menu (Chunk 108) */
extern uint16_t diySelected;

/* ASM globals for validation */
extern volatile uint16_t panicReasonCode;
extern uint16_t teamsLoaded;
extern uint8_t teamFileBuffer[];
extern void _PanicHaltLoop(void) __attribute__((noreturn));
extern void _FlashColor0Delay(uint32_t loopCount, uint16_t color);

/* ASM data tables */
extern void *cupRoundStringTable[];
extern uint8_t diyLeagueSeasonStateTable[];
extern volatile uint8_t diyLeagueScratchBuffer[];
extern uint8_t competitionFileBuffer[];
extern uint8_t competitionContext[];
extern uint8_t tertiaryCompetitionContext[];
extern uint8_t euroCupsCompetitionContext[];

/* ASM globals for cup bracket */
extern uint16_t diyCupTeamIds[];

/* ---- C bridges to asm functions ---- */
extern TeamFileEx *_VerifyTeamLoaded(uint16_t teamNum);
extern void _DIY_RecomputeCompetitionDerivedProgressionTable(void);

/* ---- ASM functions still in asm (called via bridge) ---- */
/* _ShowDiyLeaguesTournamentMenu now ported to C — declared in diy_competition.h */
extern void _ShowDIYCupMenuWithParams(uint16_t arg0, uint16_t arg1, uint16_t arg2);
extern void _Career_AutoAdvanceInternationalCompetitionFixtures(uint8_t *record, uint16_t index);

extern void _SetExitMenuFlag(void);

/* ---- C functions from other modules ---- */
extern void _DIY_AdvanceToNextCompetitionSlot(void);
extern void _SaveSelectedTeams(void);
extern void _RestoreSelectedTeams(void);
extern void _Career_LoadCompetitionSlotStateFromBuffer(uint8_t *buf);
extern void _Career_StoreCompetitionSlotStateToBuffer(uint8_t *buf);
extern void _AppendTeamsToSelectionFromTmdOrTeamFile(uint16_t *out, uint16_t *teamIds, uint16_t count);

/* ---- ASM globals for new functions ---- */
extern uint16_t gameType;
extern uint16_t careerLoadedFlag;
extern uint8_t *diyCompetitionPtr;
extern uint32_t *diyCompetitionTablePtr;
extern uint8_t diyCompetitionContext[];
extern uint8_t *currentCompetitionPtr;
extern uint16_t currentDivisionIndex;
extern uint16_t internationalAutoAdvanceFlag;
extern uint16_t competitionSlotUpdatedFlag;
extern uint16_t primaryCompSlotStateA;
extern uint16_t primaryCompSlotStateB;
extern uint16_t primaryCompSlotStateC;
extern uint16_t diyCompSlotStateA;
extern uint16_t diyCompSlotStateB;
extern uint16_t diyCompSlotStateC;
extern uint8_t teamSelectionKeyCache[];
extern uint16_t teamSelectionKeyCacheCount;
extern uint16_t diyHasGroups;
extern uint8_t aFriendly[];  /* string "FRIENDLY" */
extern uint8_t worldCup[];
extern uint8_t diyPointsForAWin[];

/* ---- Fixture data ---- */
extern uint8_t europeanChampionships[];
extern uint8_t europeanChampionships2[];
extern uint8_t euroChampFixtureData[];
extern const uint8_t *fixtureSchedulePtrTable[];
extern uint16_t diyLeagueTeamIds[];

/* ---- Local data table for InitDIYTournament ---- */
static const uint16_t diyTournamentInitTable[20] = {
    0x0005, 0x0018, 0x0006, 0x0004, 0x0010, 0x0000, 0x0010, 0x0008,
    0x0000, 0x0008, 0x0004, 0x0000, 0x0004, 0x0002, 0x0000, 0x0002,
    0x0001, 0x0001, 0x0001, 0x0001
};

/* ================================================================
 * GetAveragePlayerPrice — average price of 16 players in team file
 * asm: a0=team ptr (from VerifyTeamLoaded), returns d0=avg
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetAveragePlayerPrice(TeamFileEx *team)
{
    uint16_t total = 0;
    for (int16_t i = 0; i < 16; i++) {
        total += (uint8_t)team->players[i].price;
    }
    /* ext.l d0; divu.w #16,d0 — returns quotient in low word */
    return total / 16;
}

/* ================================================================
 * GetLowestAndHighestPlayerPrice — find min/max price in career team
 * asm: returns d4=lowest, d5=highest
 * C: returns packed (lowest << 16) | highest
 * ================================================================ */
__attribute__((used, externally_visible))
uint32_t _GetLowestAndHighestPlayerPrice(void)
{
    uint16_t lowest = 255;
    uint16_t highest = 0;

    for (int16_t i = (int16_t)careerNumPlayers - 1; i >= 0; i--) {
        /* GetPlayerAtIndex with selTeamNumber → remaps via currentMatchPlayers,
         * then GetPlayer returns careerTeam base + remappedIndex * 38.
         * Price is at PLAYERFILE_PRICE(32) + 0x4C(76) from that base. */
        uint8_t fileIdx = currentMatchPlayers[i];
        uint8_t price = (uint8_t)careerTeam.players[fileIdx].price;
        if (price < (uint8_t)lowest)
            lowest = price;
        if (price > (uint8_t)highest)
            highest = price;
    }
    return ((uint32_t)lowest << 16) | highest;
}

/* ================================================================
 * CountNearnessFactor — multiply price by nearness ratio
 * asm: d3=curQuad, d6=dstQuad, d7=price, a2=curQuadDistFactor,
 *      a3=playerPositions, a4=coordToQuad → d7=result
 * C: returns result (was d7)
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _CountNearnessFactor(uint16_t curBallQuadrant, uint16_t dstBallQuadrant,
                              uint16_t price, const uint8_t *curQuadDistFactor,
                              const uint8_t *playerPositions, const uint8_t *coordToQuad)
{
    /* Build destination quadrant distance factor table pointer */
    const uint8_t *dstQuadTable = &quadrantDistanceFactor[dstBallQuadrant * 35];

    uint16_t nearDstCount = 0;  /* d1: count of quadrants near destination */
    uint16_t nearCurCount = 0;  /* d2: count of quadrants near current */

    for (int16_t i = 34; i >= 0; i--) {
        if ((uint16_t)i == curBallQuadrant)
            continue;
        if (curQuadDistFactor[i] < 8)
            continue;
        nearCurCount++;
        uint8_t playerPos = playerPositions[i];
        uint8_t quad = coordToQuad[playerPos];
        if (dstQuadTable[quad] < 4)
            continue;
        nearDstCount++;
    }

    /* mulu.w d1,d7; divu.w d2,d7 — price * nearDstCount / nearCurCount */
    uint32_t result = (uint32_t)price * nearDstCount;
    result = result / nearCurCount;
    return (uint16_t)result;
}

/* ================================================================
 * DIY_ShuffleTeams — Fisher-Yates shuffle of team indices
 * asm: a2=diy buffer, a3=competition slot pointer (slot+0x15A=&record)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_ShuffleTeams(uint8_t *diy, uint8_t *slot)
{
    /* slot-record accessor: the threaded `slot` byte pointer equals
     * bufbase + slotIdx*0x76, and slots[] sits at struct offset 0x15A by
     * design, so `slot + 0x15A` is &diyFileBufferCopy.slots[slotIdx]. */
    DIYTournamentSlot *s = (DIYTournamentSlot *)(slot + offsetof(DIYTournamentState, slots));
    uint8_t *teamIndices = ((DIYTournamentState *)diy)->teamIndexOrder;
    uint16_t count = s->numTeams;
    int16_t i = (int16_t)count - 1;

    while (i >= 0) {
        uint16_t r = _Rand2();
        uint16_t j = (uint16_t)(((uint32_t)r * count) >> 8);
        /* Swap teamIndices[i] and teamIndices[j] */
        uint8_t tmp = teamIndices[i];
        teamIndices[i] = teamIndices[j];
        teamIndices[j] = tmp;
        i--;
    }
}

/* ================================================================
 * DIY_SortAndShuffleTeamsByValue — bubble sort by avg price, then
 * shuffle within groups
 * asm: a2=diy buffer, a3=competition slot pointer (slot+0x15A=&record)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_SortAndShuffleTeamsByValue(uint8_t *diy, uint8_t *slot)
{
    DIYTournamentSlot *s = (DIYTournamentSlot *)(slot + offsetof(DIYTournamentState, slots));
    DIYTournamentState *t = (DIYTournamentState *)diy;
    uint16_t *teamIds = t->teamIds;
    uint8_t *teamIndices = t->teamIndexOrder;
    uint16_t numTeams = s->numTeams;

    /* Bubble sort: sort teamIndices by descending average player price */
    for (int16_t outer = (int16_t)numTeams - 2; outer >= 0; outer--) {
        uint8_t *p = teamIndices;
        for (int16_t inner = outer; inner >= 0; inner--) {
            /* Get avg price for team at p[0]. teamIds (at diy+0x9A =
             * diyTournamentTeamIds) is BE-packed per the 4e5f378
             * convention; use BE_READ_U16 so the value passed to
             * VerifyTeamLoaded is the packed `(country<<8)|ordinal`. */
            int16_t idx0 = (int8_t)p[0];
            uint16_t teamNum0 = BE_READ_U16(&teamIds[idx0]);
            TeamFileEx *team0 = _VerifyTeamLoaded(teamNum0);
            uint16_t price0 = _GetAveragePlayerPrice(team0);

            /* Get avg price for team at p[1] */
            int16_t idx1 = (int8_t)p[1];
            uint16_t teamNum1 = BE_READ_U16(&teamIds[idx1]);
            TeamFileEx *team1 = _VerifyTeamLoaded(teamNum1);
            uint16_t price1 = _GetAveragePlayerPrice(team1);

            /* Swap if price0 < price1 (sort descending) */
            if (price0 < price1) {
                uint8_t tmp = p[0];
                p[0] = p[1];
                p[1] = tmp;
            }
            p++;
        }
    }

    /* Shuffle within groups */
    uint16_t groupSize, groupCount;

    if (s->isLeagueFormat) {
        groupSize = s->teamsPerGroup;
        groupCount = s->groups;
    } else {
        groupSize = 4;
        groupCount = numTeams >> 2;
        if (groupCount <= 2)
            return;
    }

    uint8_t *base = teamIndices;
    for (int16_t g = (int16_t)groupSize - 1; g >= 0; g--) {
        for (int16_t j = (int16_t)groupCount - 1; j >= 0; j--) {
            uint16_t r = _Rand2();
            uint16_t k = (uint16_t)(((uint32_t)r * groupCount) >> 8);
            uint8_t tmp = base[j];
            base[j] = base[k];
            base[k] = tmp;
        }
        base += groupCount;
    }
}

/* ================================================================
 * DIY_ShuffleLeagueTeamOrder — shuffle with synchronized indices
 * asm: no params, operates on diyCompetitionStart
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_ShuffleLeagueTeamOrder(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    uint8_t *shuffleIdx = diyTeamShuffleIndices;
    uint16_t *teamOffsets = comp->matchData.league.teamEntryOffsets;
    uint16_t count = (uint16_t)comp->base.numTeams;

    for (int16_t i = (int16_t)count - 1; i >= 0; i--) {
        uint16_t r = _Rand2();
        uint16_t j = (uint16_t)(((uint32_t)r * count) >> 8);

        /* Swap shuffle indices */
        uint8_t tmpIdx = shuffleIdx[i];
        shuffleIdx[i] = shuffleIdx[j];
        shuffleIdx[j] = tmpIdx;

        /* Swap team offsets (word-sized, use i*2 and j*2) */
        uint16_t tmpOff = teamOffsets[i];
        teamOffsets[i] = teamOffsets[j];
        teamOffsets[j] = tmpOff;
    }
}

/* ================================================================
 * DIY_CountUnselectedTeams — count teams without bit 1 set
 * asm: no params, sets diyCompetitionStart+0x32E, returns Z flag
 * C: returns count (callers check != 0)
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _DIY_CountUnselectedTeams(void)
{
    DIYCompetitionEx *compEx = &diyCompetitionStart;
    DIYCompetition *comp = &compEx->base;
    compEx->bracketScratch.secondLegCounter = 0;

    uint8_t *flags = compEx->matchData.cup.cupRoundResults;
    uint16_t numTeams = (uint16_t)comp->totalMatchesInRound;

    for (int16_t i = (int16_t)numTeams - 1; i >= 0; i--) {
        if (!(flags[i] & 2)) {
            compEx->bracketScratch.secondLegCounter += 1;
        }
    }

    return compEx->bracketScratch.secondLegCounter;
}

/* Helper: clear league table stats for 64 entries.
 * Iterates DIYTeamStats rows starting at base+0x2B4; teamSelOffset (the
 * 4-byte head of each row) is intentionally NOT cleared — only the 7
 * stats words. The 64-row × 18-byte span runs from 0x2B4 to 0x734
 * (the end of audioInstrumentBank's address range — intentional
 * shared-memory overlay; see DIYCOMPETITIONEX_AUDIT.md). */
static void clearLeagueTableStats(uint8_t *base)
{
    DIYTeamStats *row = (DIYTeamStats *)(base + DIY_STANDINGS_OFFSET);
    for (int16_t i = 0x3F; i >= 0; i--) {
        row->gamesPlayed  = 0;
        row->wins         = 0;
        row->draws        = 0;
        row->losses       = 0;
        row->goalsFor     = 0;
        row->goalsAgainst = 0;
        row->points       = 0;
        row++;
    }
}

/* Helper: initialize team shuffle indices and entry offsets */
static void initTeamShuffleAndOffsets(void)
{
    uint8_t *shuffleIdx = diyTeamShuffleIndices;
    uint16_t *entryOffsets = diyTeamEntryOffsetsByIndex;
    uint8_t idx = 0;
    uint16_t offset = 0;
    for (int16_t i = 0x3F; i >= 0; i--) {
        *shuffleIdx++ = idx++;
        *entryOffsets++ = offset;
        offset += 0x12;
    }
}

/* ================================================================
 * DIY_InitCompetitionDefaults — initialize diyCompetitionStart
 * with league defaults
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_InitCompetitionDefaults(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a2 = &comp->base;
    uint8_t *a2raw = (uint8_t *)a2;

    a2->competitionType = 0;             /* competition type = 0 (league) */
    *(uint16_t *)&a2->compId = 0;       /* 0x2C: compId + slotId as word */
    a2->isActive = 0;                   /* 0x02 */
    a2->matchStarted = 0;              /* 0x44 */
    a2->careerTeamInvolved = 0;              /* 0x46 */
    a2->resultCode = (int16_t)0xFFFF;    /* 0x3A */
    a2->substitutesCount = 1;            /* 0x3C */
    comp->matchData.league.hasGroupsDiy = 0;
    a2->slotStateA = 3;              /* 0x3E */
    a2->slotStateB = 2;              /* 0x40 */
    a2->slotStateC = 8;              /* 0x42 */
    a2->field_2E = 0xFF;                 /* 0x2E */
    a2->hasGroups = 0;                   /* 0x48 */

    /* Copy "DIY LEAGUE" string to offset 4 */
    _StringCopy(aDiyLeague, a2raw + 4);

    a2->startMonth = 0x38;               /* start month */
    a2->endMonth = 0x20;                 /* end month */
    a2->pitchTypeOrSeason = 1;           /* 0x34 */
    a2->pitchType = (int16_t)0xFFFF;     /* 0x36 */
    a2->numTeams = 2;                    /* 0x32 */
    a2->playEachTeam = 1;               /* 0x5E */
    comp->pointsForAWin = 2;
    comp->matchData.league.leagueMinSubstitutes = 3;
    comp->matchData.league.leagueMaxSubstitutes = 5;
    a2->matchPairsPerRound = 1;                   /* 0x50 */
    a2->subRoundCounter = 0;             /* 0x5C */
    a2->roundComplete = 0;               /* 0x54 */
    a2->competitionFinished = 0;               /* 0x56 */
    a2->currentRoundIndex = 0;                    /* 0x58 */
    a2->numRounds = 0;                   /* 0x5A */

    /* Clear league table stats for 64 entries */
    clearLeagueTableStats(a2raw);

    /* Initialize team shuffle indices and entry offsets */
    initTeamShuffleAndOffsets();
}

/* ================================================================
 * DIY_ResetCompetitionProgressAndShuffle — reset progress, reseed,
 * and shuffle league team order
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_ResetCompetitionProgressAndShuffle(void)
{
    /* Reseed random from current tick */
#ifdef DETERMINISTIC_MODE
    _InitDeterministicMode();
#else
    randomSeed = currentTick;
    newRandomSeedCopy = (uint8_t)(currentTick >> 8);
#endif

    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a2 = &comp->base;
    uint8_t *a2raw = (uint8_t *)a2;
    a2->matchStarted = 0;              /* 0x44 */
    a2->careerTeamInvolved = 0;              /* 0x46 */
    a2->resultCode = (int16_t)0xFFFF;    /* 0x3A */
    comp->matchesPlayedCount = 0;
    a2->subRoundCounter = 0;             /* 0x5C */
    comp->leagueSectionState.sectionMatchesProgress = 0;
    comp->leagueSectionState.halfGroupsCounter = 0;
    comp->leagueMatchProgress = 0;
    comp->leagueCurrentSlot = 0;
    /* Copy reset value to live counter (BSS_00_2DF5E totalMatchesPerSection) */
    comp->matchData.league.totalMatchesPerSectionLive = comp->matchData.league.totalMatchesPerSection;
    comp->minSubstitutes = (uint16_t)a2->playEachTeam;  /* repurposed as section counter */

    /* Clear league table stats for 64 entries */
    clearLeagueTableStats(a2raw);

    /* Reinitialize team shuffle indices and entry offsets */
    initTeamShuffleAndOffsets();

    /* Set random phase from start month */
    a2->viewRandomPhase = (uint8_t)a2->startMonth;
    viewRandomPhase = (uint8_t)a2->startMonth;

    /* Shuffle league team order */
    _DIY_ShuffleLeagueTeamOrder();
}

/* ================================================================
 * DIY_CacheSelectedTeamPointers — build pointer cache from selected
 * team indices
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_CacheSelectedTeamPointers(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a2 = &comp->base;
    /* teamIndices is the league shuffle-indices region (first 14 bytes of
     * shuffleAndPad[64]). Access via the typed field so the address is
     * derived from the struct layout, not the historical Amiga
     * byte-offset `a2raw + 0xEE` which is wrong by +8 on 64-bit. */
    uint8_t *teamIndices = comp->matchData.league.shuffleAndPad;
    uint16_t *teamIds = comp->matchData.league.leagueTeamIds;
    uint16_t count = (uint16_t)a2->numTeams;

    for (int16_t i = (int16_t)count - 1; i >= 0; i--) {
        int8_t idx = (int8_t)teamIndices[i];
        /* leagueTeamIds stores BE-packed `{country, ordinal}` byte
         * pairs to match the asm-faithful layout (see 4e5f378). */
        uint16_t teamNum = BE_READ_U16(&teamIds[(uint16_t)(int16_t)idx]);
        TeamFileEx *teamPtr = _VerifyTeamLoaded(teamNum);

        /* Store the team IDENTITY (step-4 phase 3): -1 for careerTeam, else the
         * selTeamsPtr array index. Standings table overlays cupTeamIds — use the
         * typed helper instead of the Amiga byte offsets (entry + 0x2B4). */
        uint16_t entryIdx = (uint16_t)(int16_t)idx;
        int32_t off = diy_team_to_identity((uint8_t *)teamPtr);
#ifdef SWOS_DEBUG_AUTOSKIP
        SWOS_AUTOSKIP_LOG("[bug8w] CacheTeamPtrs i=%d idx=%d entryIdx=%u teamNum=0x%04X "
                          "teamPtr=%p selTeamsPtr=%p off=%d numTeams=%u",
                          (int)i, (int)idx, (unsigned)entryIdx,
                          (unsigned)teamNum, (void *)teamPtr,
                          (void *)&selTeamsPtr, (int)off,
                          (unsigned)count);
#endif
        diy_standings_row(comp, entryIdx)->teamSelOffset = off;
    }

}

/* ================================================================
 * DIY_SortLeagueTableByStandings — multi-criteria bubble sort
 * Sort by: points > goal diff > goals scored > alphabetical name
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_SortLeagueTableByStandings(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a4s = &comp->base;
    uint8_t *a4 = (uint8_t *)a4s;
    uint16_t numGroups = (uint16_t)a4s->matchPairsPerRound;
    uint16_t teamsPerGroup = (uint16_t)a4s->totalMatchesInRound;
    uint16_t entryBase = 0;  /* word index into teamEntryOffsets */

    for (int16_t g = (int16_t)numGroups - 1; g >= 0; g--) {
        uint16_t *p1 = comp->matchData.league.teamEntryOffsets + entryBase;

        for (int16_t pass = (int16_t)teamsPerGroup - 2; pass >= 0; pass--) {
            uint16_t *p2 = p1 + 1;

            for (int16_t j = pass; j >= 0; j--) {
                uint8_t *teamA = a4 + *p1;
                uint8_t *teamB = a4 + *p2;
                DIYTeamStats *rowA = (DIYTeamStats *)(teamA + DIY_STANDINGS_OFFSET);
                DIYTeamStats *rowB = (DIYTeamStats *)(teamB + DIY_STANDINGS_OFFSET);

                /* Compare points — higher is better */
                if (rowA->points > rowB->points) goto no_swap;
                if (rowA->points < rowB->points) goto do_swap;

                /* Points equal — compare goal difference */
                int16_t gdA = (int16_t)rowA->goalsFor - (int16_t)rowA->goalsAgainst;
                int16_t gdB = (int16_t)rowB->goalsFor - (int16_t)rowB->goalsAgainst;
                if (gdA > gdB) goto no_swap;
                if (gdA < gdB) goto do_swap;

                /* Goal diff equal — compare goals scored */
                if (rowA->goalsFor > rowB->goalsFor) goto no_swap;
                if (rowA->goalsFor < rowB->goalsFor) goto do_swap;

                /* Goals scored equal — alphabetical comparison */
                {
                    uint8_t *nameA = diy_resolve_team_name(rowA->teamSelOffset);
                    uint8_t *nameB = diy_resolve_team_name(rowB->teamSelOffset);
                    for (;;) {
                        uint8_t cA = *nameA++;
                        if (cA == 0) goto no_swap;
                        uint8_t cB = *nameB++;
                        if (cA < cB) goto no_swap;
                        if (cA > cB) goto do_swap;
                    }
                }

            do_swap:
                {
                    uint16_t tmpOff = (uint16_t)(teamB - a4);
                    *p1 = tmpOff;
                    tmpOff = (uint16_t)(teamA - a4);
                    *p2 = tmpOff;
                }
            no_swap:
                p2++;
            }
            p1++;  /* advance to next position after each pass */
        }
        entryBase += teamsPerGroup;
    }
}

/* ================================================================
 * DIY_ComputeAverageTeamStrengthTier
 * asm: d0=count of teams, a0=team ID table (word array)
 * returns d0=avgSkill, d1=tier (clamped 0-7)
 * C: returns packed (avgSkill << 16) | tier
 * ================================================================ */
__attribute__((used, externally_visible))
uint32_t _DIY_ComputeAverageTeamStrengthTier(uint16_t count, uint8_t *table)
{
    uint16_t totalPlayers = count * 16;
    uint32_t totalSkill = 0;

    for (int16_t t = (int16_t)count - 1; t >= 0; t--) {
        /* table holds BE-packed `{country, ordinal}` byte pairs — the
         * asm-faithful format used by static dc.b competition data and
         * runtime writers that mirror m68k `move.w` semantics (16-bit
         * BE stores). Asm `move.w (a0,d0.w),d0` returns the packed
         * `(country<<8)|ordinal` id; host-native `teamIds[t]` flips the
         * bytes on LE. Use BE_READ_U16 for the m68k-compatible read.
         *
         * (Earlier revert was wrong: the LE-byte tables that surfaced
         * via Playaveli's intermediate crash were produced by a
         * separate writer-side bug; this read site needs to stay
         * asm-faithful — that's what the live data in europeanChampi
         * 2Copy.unk1 → diyTournamentTeamIds copy uses.) */
        uint16_t teamId = BE_READ_U16(table + (uint16_t)t * 2);
        TeamFileEx *tf = _VerifyTeamLoaded(teamId);
        /* Read skill byte at offset 0x6C for each of 16 players, stride 0x26 (38) */
        for (int16_t p = 15; p >= 0; p--) {
            totalSkill += (uint8_t)tf->players[p].price;
        }
    }

    uint16_t avg = (uint16_t)(totalSkill / totalPlayers);
    uint16_t tier = avg >> 2;
    if (tier > 7)
        tier = 7;

    return ((uint32_t)avg << 16) | tier;
}

/* ================================================================
 * InitDIYTournament — initialize tournament data structure
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitDIYTournament(void)
{
    DIYCompetition *a2 = &diyFileBufferCopy.header;

    a2->competitionType = 2;             /* competition type = 2 (tournament) */
    *(uint16_t *)&a2->compId = 2;       /* 0x2C: compId + slotId as word */
    a2->isActive = 0;                   /* 0x02 */
    a2->matchStarted = 0;              /* 0x44 */
    a2->careerTeamInvolved = 0;              /* 0x46 */
    a2->resultCode = (int16_t)0xFFFF;    /* 0x3A */
    a2->substitutesCount = 1;            /* 0x3C */
    a2->slotStateA = 3;              /* 0x3E */
    a2->slotStateB = 2;              /* 0x40 */
    a2->slotStateC = 8;              /* 0x42 */
    a2->field_2E = 0xFF;                 /* 0x2E */
    a2->matchPairsPerRound = 0;                   /* 0x50 */

    /* Copy "DIY TOURNAMENT" string into the competition-name field */
    _StringCopy(aDiyTournament, (uint8_t *)diyFileBufferCopy.header.competitionName);

    a2->startMonth = 0x38;               /* start month */
    a2->endMonth = 0x20;                 /* end month */
    a2->pitchTypeOrSeason = 1;           /* 0x34 */
    a2->pitchType = (int16_t)0xFFFF;     /* 0x36 */
    a2->roundIterator = 3;            /* 0x4C */
    a2->numTeamsInStage = 5;                     /* 0x4E */
    a2->menuState = 0;                     /* 0x4A */

    /* Initialize 6 competition slots from init table.
     * The first slot's _pad_00 doubles as a "doubled cup-bracket"
     * flag during init; clear it before populating. */
    const uint16_t *initTbl = diyTournamentInitTable;
    DIYTournamentSlot *slot = diyFileBufferCopy.slots;
    *(uint16_t *)slot->_pad_00 = 0;

    a2->totalMatchesInRound = (int16_t)*initTbl++;   /* total teams: 0x52 */
    a2->roundComplete = 0;                    /* 0x54 */
    a2->numTeams = (int16_t)*initTbl;        /* numTeams = first entry: 0x32 */

    for (int16_t s = 5; s >= 0; s--) {
        slot->numTeams      = *initTbl++;
        slot->isLeagueFormat = *initTbl;       /* peek without consuming */
        slot->seedingType   = 0;
        slot->doubleCount   = 0;
        slot->groups        = *initTbl++;
        slot->teamsPerGroup = *initTbl++;
        slot->divisionFactor = *initTbl;       /* peek next (not consumed) */
        slot->formatFlag16E = 1;
        slot->initConst170  = 1;
        slot->formatFlag172 = 1;
        slot->initConst168  = 2;
        slot->playEachTeam  = 1;
        slot++;
    }

    /* Initialize the 64 team-index / seed-order entries to identity
     * (0..63). This is the `teamIndexOrder` field — relocated off the
     * header in the step-3 untangle. It used to live at raw byte 0x5A,
     * overlapping the DIYCompetition header tail, which is what corrupted
     * it on 64-bit (the namePtr growth slid competitionFinished onto it).
     * As a named field past the slots it can't collide with the header. */
    uint8_t *indices = diyFileBufferCopy.teamIndexOrder;
    for (int16_t i = 0; i <= 0x3F; i++) {
        indices[i] = (uint8_t)i;
    }
}

/* ================================================================
 * DIY_InitCupBracketProgressionTable — build bracket progression
 * table with month interpolation
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_InitCupBracketProgressionTable(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a0s = &comp->base;
    uint16_t *roundFormats = comp->matchData.cup.roundLegs;       /* parent +0x66 */
    uint16_t *roundLegs = comp->matchData.cup.roundPenalties;     /* parent +0x7E */
    uint16_t numRounds = (uint16_t)a0s->numRounds;

    uint16_t totalMatches = 0;
    uint16_t lastRoundHasExtraMatch = 0;

    for (int16_t r = (int16_t)numRounds - 1; r >= 0; r--) {
        totalMatches += 2;
        if (roundFormats[r] != 2) {
            /* Format != 2: check round legs for extra match */
            lastRoundHasExtraMatch = 1;
            if (roundLegs[r] == 1) {
                lastRoundHasExtraMatch = 0;
                totalMatches--;
            }
        }
    }

    comp->bracketScratch.totalMatches = totalMatches;
    uint16_t matchCount = totalMatches - lastRoundHasExtraMatch;

    uint8_t startMonth = (uint8_t)a0s->startMonth;
    uint8_t endMonth = (uint8_t)a0s->endMonth;
    uint8_t *progTable = comp->bracketScratch.progTable;

    if (matchCount == 1) {
        progTable[0] = startMonth;
    } else {
        /* Ensure endMonth > startMonth for interpolation */
        uint8_t adjEndMonth = endMonth;
        if (adjEndMonth < startMonth)
            adjEndMonth += 0x60;

        uint8_t range = adjEndMonth - startMonth;
        uint16_t steps = matchCount - 1;

        for (uint16_t i = 0; i <= steps; i++) {
            uint16_t val = (uint16_t)i * (uint16_t)range / steps;
            progTable[i] = (uint8_t)(startMonth + val);
        }
    }

    /* If last round has extra match, add entry 2 past last month */
    if (lastRoundHasExtraMatch) {
        uint8_t lastMonth = progTable[matchCount - 1];
        progTable[matchCount] = lastMonth + 2;
    }
}

/* ================================================================
 * DIYTournament_BuildMatchesPerTeamProgressionTable — build
 * matches-per-team progression table for tournament format
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYTournament_BuildMatchesPerTeamProgressionTable(void)
{
    DIYTournamentState *t = &diyFileBufferCopy;
    DIYCompetition *bufs = &t->header;
    uint16_t numSlots = (uint16_t)bufs->totalMatchesInRound;

    /* Sum total matches across all slots */
    uint16_t totalMatches = 0;
    DIYTournamentSlot *slots = t->slots;
    for (int16_t s = 0; s < (int16_t)numSlots; s++) {
        totalMatches += slots[s].matchesPerSlot;
    }

    uint8_t startMonth = (uint8_t)bufs->startMonth;
    uint8_t endMonth = (uint8_t)bufs->endMonth;

    if (totalMatches == 1) {
        slots[0].progressionTable[0] = startMonth;
        return;
    }

    /* Ensure endMonth > startMonth for interpolation */
    uint8_t adjEndMonth = endMonth;
    if (adjEndMonth < startMonth)
        adjEndMonth += 0x60;

    uint8_t range = adjEndMonth - startMonth;
    uint16_t steps = totalMatches - 1;

    /* Walk through slots, filling progression table per-slot */
    int16_t slotIdx = -1;
    uint8_t *progTable = (uint8_t *)0;
    uint16_t slotMatchesRemaining = 0;
    uint16_t matchIdx = 0;

    for (uint16_t i = 0; i <= steps; i++) {
        if (slotMatchesRemaining == 0) {
            slotIdx++;
            slotMatchesRemaining = slots[slotIdx].matchesPerSlot;
            progTable = slots[slotIdx].progressionTable;
        }
        uint16_t val = (uint16_t)matchIdx * (uint16_t)range / steps;
        *progTable++ = (uint8_t)(startMonth + val);
        slotMatchesRemaining--;
        matchIdx++;
    }
}

/* ================================================================
 * DIYTournament_RecalculateMatchesPerTeam — recalculate match counts
 * for all tournament slots, then rebuild progression tables
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYTournament_RecalculateMatchesPerTeam(void)
{
    diyFileBufferCopy.header.isActive = 1;  /* 0x02 */
    /* nullsub_12 call — no-op */
    diyFileBufferCopy.header.currentRoundIndex = 0;              /* 0x58: FIELD_3A + 0x1E */
    diyFileBufferCopy.header.roundComplete = 0;          /* 0x54: FIELD_3A + 0x1A */

    (&diyCompetitionStart.base)->isActive = 0;

    /* Recalculate match counts per slot */
    DIYTournamentSlot *slots = diyFileBufferCopy.slots;
    uint16_t numSlots = (uint16_t)diyFileBufferCopy.header.totalMatchesInRound;

    for (int16_t s = 0; s < (int16_t)numSlots; s++) {
        DIYTournamentSlot *slot = &slots[s];
        if (slot->isLeagueFormat) {
            /* League/grouped: round-robin match count from teamsPerGroup */
            uint16_t teamsPerGroup = slot->teamsPerGroup;
            uint16_t halfTeams = teamsPerGroup >> 1;
            uint16_t matchPairs = slot->teamsPerGroup;
            uint32_t total = (uint32_t)(matchPairs - 1) * matchPairs;
            total >>= 1;
            total = total / halfTeams;
            total *= slot->playEachTeam;
            slot->matchesPerSlot = (uint16_t)total;
        } else {
            /* League: default 2 matches */
            slot->matchesPerSlot = 2;
            if (slot->formatFlag16E == 2) {
                /* skip */
            } else if (slot->formatFlag172 == 1) {
                slot->matchesPerSlot = 1;
            }
        }
    }

    /* Rebuild matches-per-team progression table */
    _DIYTournament_BuildMatchesPerTeamProgressionTable();

    /* Compute average team strength tier */
    uint16_t numTeams = (uint16_t)diyFileBufferCopy.header.numTeams;
    uint32_t result = _DIY_ComputeAverageTeamStrengthTier(numTeams, (uint8_t *)diyFileBufferCopy.teamIds);
    diyFileBufferCopy.header.avgSkill = (uint8_t)(result >> 16);      /* 0x30 */
    diyFileBufferCopy.header.strengthTier = (uint8_t)(result & 0xFF); /* 0x31 */
}

/* ================================================================
 * GetNextCupRoundStringIndex  (SWOS2.s line 29351)
 *
 * Given current round (cur), total rounds (n), and base add value,
 * returns the string table index for the cup round name.
 * Last 3 rounds map to quarter-final(6), semi-final(7), final(8).
 *
 * Asm: d0=cur, d1=n, d2=add, returns d0=index.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetNextCupRoundStringIndex(uint16_t cur, uint16_t n, uint16_t add)
{
    if (cur == (uint16_t)(n - 1))
        return 8;   /* final */
    if (cur == (uint16_t)(n - 2))
        return 7;   /* semi-final */
    if (cur == (uint16_t)(n - 3))
        return 6;   /* quarter-final */
    return cur + add - 1;
}

/* ================================================================
 * GetNextCupRoundString  (SWOS2.s line 29376)
 *
 * Returns a pointer to the cup round name string for the next round.
 * Calls GetNextCupRoundStringIndex and indexes into cupRoundStringTable.
 *
 * Asm: d0=cur, d1=n, d2=add, returns a0=string pointer.
 * ================================================================ */
__attribute__((used, externally_visible))
const char *_GetNextCupRoundString(uint16_t cur, uint16_t n, uint16_t add)
{
    uint16_t idx = _GetNextCupRoundStringIndex(cur, n, add);
    return (const char *)cupRoundStringTable[idx];
}

/* ================================================================
 * GetCupRoundStringIndex  (SWOS2.s line 29385)
 *
 * More complex cup round string index calculation with flag, stage,
 * and opt parameters for different tournament formats.
 *
 * Asm: d0=cur, d1=flag, d2=idx, d3=stage, d4=opt, returns d0=index.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetCupRoundStringIndex(uint16_t cur, uint16_t flag, uint16_t idx,
                                  uint16_t stage, uint16_t opt)
{
    idx--;

    if (flag != 0) {
        if (cur != idx)
            return cur;
        if (flag == 1)
            return 9;   /* final group */
        return 11;      /* final groups */
    }

    /* flag == 0 path */
    if (cur == idx)
        return 8;       /* final */

    idx--;
    if (opt != 0) {
        if (cur == idx)
            return 10;  /* third place play-off */
        idx--;
    }

    if (cur == idx) {
        if (stage == 4)
            return 7;   /* semi-final */
        return cur;
    }

    idx--;
    if (cur == idx) {
        if (stage == 8)
            return 6;   /* quarter-final */
        return cur;
    }

    return cur;
}

/* ================================================================
 * GetCupRoundString  (SWOS2.s line 29437)
 *
 * Returns a pointer to the cup round name string.
 * Calls GetCupRoundStringIndex and indexes into cupRoundStringTable.
 *
 * Asm: d0=cur, d1=flag, d2=idx, d3=stage, d4=opt, returns a0=string.
 * ================================================================ */
__attribute__((used, externally_visible))
const char *_GetCupRoundString(uint16_t cur, uint16_t flag, uint16_t idx,
                                uint16_t stage, uint16_t opt)
{
    uint16_t stringIdx = _GetCupRoundStringIndex(cur, flag, idx, stage, opt);
    return (const char *)cupRoundStringTable[stringIdx];
}

/* ================================================================
 * DIY_LoadLeagueScratchFromTable  (SWOS2.s line 35209)
 *
 * Copies 2000 bytes from diyLeagueSeasonStateTable to
 * diyLeagueScratchBuffer. Uses volatile to prevent GCC from
 * optimizing the copy loop into a memcpy call (no libc available).
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_LoadLeagueScratchFromTable(void)
{
    const volatile uint8_t *src = (const volatile uint8_t *)diyLeagueSeasonStateTable;
    volatile uint8_t *dst = (volatile uint8_t *)diyLeagueScratchBuffer;
    int16_t i;
    for (i = 0x7CF; i >= 0; i--)
        *dst++ = *src++;
}

/* ================================================================
 * DIY_SaveLeagueScratchToTable  (SWOS2.s line 35220)
 *
 * Copies 2000 bytes from diyLeagueScratchBuffer back to
 * diyLeagueSeasonStateTable.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_SaveLeagueScratchToTable(void)
{
    volatile uint8_t *dst = (volatile uint8_t *)diyLeagueSeasonStateTable;
    const volatile uint8_t *src = (const volatile uint8_t *)diyLeagueScratchBuffer;
    int16_t i;
    for (i = 0x7CF; i >= 0; i--)
        *dst++ = *src++;
}

/* ================================================================
 * Helper: word-copy between two buffers.
 * Uses volatile to prevent GCC from converting to memcpy.
 * count = number of words minus 1 (matches dbf convention).
 * ================================================================ */
static void copyWords(volatile uint16_t *dst, const volatile uint16_t *src, int16_t count)
{
    int16_t i;
    for (i = count; i >= 0; i--)
        *dst++ = *src++;
}

/* ================================================================
 * Tournament-buffer <-> competitionFileBuffer field maps.
 *
 * diyFileBufferCopy is now the typed DIYTournamentState struct, but the
 * competitionFileBuffer backup keeps the legacy raw byte layout (it's an
 * overlay inside careerFileBuffer). The four copy entry points below were
 * a single flat 2900-byte word copy that relied on the two buffers being
 * byte-identical; they now field-map: the tournament fields by name, plus
 * the trailing current-stage standings region as a flat block (cfb +0x420
 * == the bytes diyCompetitionStart was adjacent to in the old layout,
 * copied to/from the now-standalone diyCompetitionStart). cfb keeps the
 * old overlap, so on the way IN the header's tail
 * (competitionFinished/currentRoundIndex/numRounds...) picks up team-list
 * bytes from cfb's 0x5A overlap — but those are reset by
 * _DIY_LoadCurrentCompetitionSlot / unused for tournaments, so they self-
 * heal; teamIndexOrder (a real field past the header) always survives.
 * ================================================================ */
#define DIY_CFB_STANDINGS_LEN  0x734u  /* current-stage record + league standings */

/* --- cfb<->tournament copy ---
 * Post-untangle EVERY career competition slot — competitionFileBuffer (slot 0,
 * = primaryCompetitionBuf) AND the four *Context slots — is a standalone
 * CompetitionSlotBuffer, so this is a single flat struct copy: the
 * DIYTournamentState (header tail incl. currentRoundIndex intact — teamIndexOrder
 * at its real 0x41E, no 0x5A overlay) plus the stage snapshot. No magic offsets,
 * no header collision → type-2 tournaments (WC primary, euro-cup quaternary) keep
 * currentRoundIndex and the per-stage cache fills. The legacy raw-offset field-
 * map (0x5A/0x9A/0x11A/0x15A/0x420) retired with the careerFileBuffer overlay it
 * served. */
static void diy_cfb_to_tournament_clean(const uint8_t *c)
{
    const CompetitionSlotBuffer *s = (const CompetitionSlotBuffer *)c;
    memcpy(&diyFileBufferCopy, &s->tourn, sizeof(DIYTournamentState));
    memcpy(diyCompetitionStartBuf, s->stageSnapshot, DIY_CFB_STANDINGS_LEN);
}

static void diy_tournament_to_cfb_clean(uint8_t *c)
{
    CompetitionSlotBuffer *s = (CompetitionSlotBuffer *)c;
    memcpy(&s->tourn, &diyFileBufferCopy, sizeof(DIYTournamentState));
    memcpy(s->stageSnapshot, diyCompetitionStartBuf, DIY_CFB_STANDINGS_LEN);
}

/* slot 0 (competitionFileBuffer = primaryCompetitionBuf) and the four *Context
 * slots are all CompetitionSlotBuffers — one flat copy serves every slot. */
static void diy_cfb_to_tournament(void) { diy_cfb_to_tournament_clean(competitionFileBuffer); }
static void diy_tournament_to_cfb(void) { diy_tournament_to_cfb_clean(competitionFileBuffer); }

/* Exported buffer-parameterised entry points for the career slot store/load
 * (career_teams.c). A type-2 tournament slot can live in competitionFileBuffer
 * OR any *Context buffer; all are CompetitionSlotBuffers now, so one flat copy
 * serves every slot base. */
__attribute__((used, externally_visible))
void _DIY_CfbBufToTournament(const uint8_t *cfb) { diy_cfb_to_tournament_clean(cfb); }

__attribute__((used, externally_visible))
void _DIY_TournamentToCfbBuf(uint8_t *cfb) { diy_tournament_to_cfb_clean(cfb); }

/* SaveDIYTournament / DIY_CopyCompetitionFileBufferToDiyFileBufferCopy
 * (SWOS2.s 32343 / 32365): competitionFileBuffer -> diyFileBufferCopy. */
__attribute__((used, externally_visible))
void _SaveDIYTournament(void) { diy_cfb_to_tournament(); }

__attribute__((used, externally_visible))
void _DIY_CopyCompetitionFileBufferToDiyFileBufferCopy(void) { diy_cfb_to_tournament(); }

/* RestoreDIYTournament / DIY_CopyDiyFileBufferCopyToCompetitionFileBuffer
 * (SWOS2.s 32354 / 32376): diyFileBufferCopy -> competitionFileBuffer. */
__attribute__((used, externally_visible))
void _RestoreDIYTournament(void) { diy_tournament_to_cfb(); }

__attribute__((used, externally_visible))
void _DIY_CopyDiyFileBufferCopyToCompetitionFileBuffer(void) { diy_tournament_to_cfb(); }

/* ================================================================
 * DIY_CopyCompetitionFileBufferToCompetitionStart  (SWOS2.s line 32431)
 * Copies 546 words (0x222) from competitionFileBuffer to diyCompetitionStart.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_CopyCompetitionFileBufferToCompetitionStart(void)
{
    copyWords((volatile uint16_t *)&diyCompetitionStart,
              (const volatile uint16_t *)competitionFileBuffer, 0x221);
}

/* ================================================================
 * DIY_CopyCompetitionStartToCompetitionFileBuffer  (SWOS2.s line 32442)
 * Copies 546 words from diyCompetitionStart to competitionFileBuffer.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_CopyCompetitionStartToCompetitionFileBuffer(void)
{
    copyWords((volatile uint16_t *)competitionFileBuffer,
              (const volatile uint16_t *)&diyCompetitionStart, 0x221);
}

/* ================================================================
 * DIY_CopyCompetitionFileBufferToCompetitionStart_Alt  (SWOS2.s line 32453)
 * Same as DIY_CopyCompetitionFileBufferToCompetitionStart.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_CopyCompetitionFileBufferToCompetitionStart_Alt(void)
{
    copyWords((volatile uint16_t *)&diyCompetitionStart,
              (const volatile uint16_t *)competitionFileBuffer, 0x221);
}

/* ================================================================
 * DIY_CopyCompetitionStartToCompetitionFileBuffer_Alt  (SWOS2.s line 32464)
 * Same as DIY_CopyCompetitionStartToCompetitionFileBuffer.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_CopyCompetitionStartToCompetitionFileBuffer_Alt(void)
{
    copyWords((volatile uint16_t *)competitionFileBuffer,
              (const volatile uint16_t *)&diyCompetitionStart, 0x221);
}

/* ================================================================
 * SaveDIYLeague  (SWOS2.s line 33301)
 * Copies 922 words from competitionFileBuffer to diyCompetitionStart.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SaveDIYLeague(void)
{
    copyWords((volatile uint16_t *)&diyCompetitionStart,
              (const volatile uint16_t *)competitionFileBuffer, 921);
}

/* ================================================================
 * RestoreDIYLeague  (SWOS2.s line 33312)
 * Copies 922 words from diyCompetitionStart to competitionFileBuffer.
 * ================================================================ */
__attribute__((used, externally_visible))
void _RestoreDIYLeague(void)
{
    copyWords((volatile uint16_t *)competitionFileBuffer,
              (const volatile uint16_t *)&diyCompetitionStart, 921);
}

/* ================================================================
 * DIY_SaveCompetitionFileBufferToDIYCompetition  (SWOS2.s line 33323)
 * Copies 922 words (0x39A) from competitionFileBuffer to diyCompetitionStart.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_SaveCompetitionFileBufferToDIYCompetition(void)
{
    copyWords((volatile uint16_t *)&diyCompetitionStart,
              (const volatile uint16_t *)competitionFileBuffer, 0x399);
}

/* ================================================================
 * DIY_RestoreCompetitionFileBufferFromDIYCompetition  (SWOS2.s line 33334)
 * Copies 922 words from diyCompetitionStart to competitionFileBuffer.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_RestoreCompetitionFileBufferFromDIYCompetition(void)
{
    copyWords((volatile uint16_t *)competitionFileBuffer,
              (const volatile uint16_t *)&diyCompetitionStart, 0x399);
}

/* ================================================================
 * InitDIYCup  (SWOS2.s line 32419)
 *
 * Initialize diyCompetitionStart for DIY Cup mode.
 * Sets competition type, default fields, "DIY CUP" name,
 * 6 round config slots with legs=1, and sequential team indices.
 *
 * Asm: no params, no return value.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitDIYCup(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a2 = &comp->base;
    uint8_t *a2raw = (uint8_t *)a2;

    a2->competitionType = DIY_CUP;       /* competition type = cup */
    *(uint16_t *)&a2->compId = 1;       /* 0x2C: compId + slotId as word */
    a2->menuState = 0;                     /* 0x4A */
    a2->subRoundCounter = 1;             /* 0x5C */
    a2->isActive = 0;                   /* 0x02 */
    a2->matchStarted = 0;              /* 0x44 */
    a2->careerTeamInvolved = 0;              /* 0x46 */
    a2->resultCode = (int16_t)0xFFFF;    /* 0x3A */
    a2->substitutesCount = 1;            /* 0x3C */
    a2->slotStateA = 3;              /* 0x3E */
    a2->slotStateB = 2;              /* 0x40 */
    a2->slotStateC = 8;              /* 0x42 */
    a2->field_2E = 0xFF;                 /* 0x2E */
    a2->hasGroups = 0;                   /* 0x48 */

    /* Copy "DIY CUP" string to offset 4 */
    _StringCopy(aDiyCup, a2raw + 4);

    a2->startMonth = 0x38;               /* start month */
    a2->endMonth = 0x20;                 /* end month */
    a2->pitchTypeOrSeason = 1;           /* 0x34 */
    a2->pitchType = (int16_t)0xFFFF;     /* 0x36 */
    a2->numTeams = 0x10;                 /* numTeams = 16 */
    a2->numRounds = 4;                   /* 0x5A */
    a2->playEachTeam = 0;               /* 0x5E */
    comp->minSubstitutes = 3;
    comp->pointsForAWin = 5;
    comp->matchData.cup.currentMatchIndex = 0;

    /* Initialize 6 round config slots: legs=1 for each (roundLegs[i] /
     * roundExtraTime[i] / roundPenalties[i]) for i=0..5. */
    for (int16_t s = 0; s <= 5; s++) {
        comp->matchData.cup.roundLegs[s] = 1;
        comp->matchData.cup.roundExtraTime[s] = 1;
        comp->matchData.cup.roundPenalties[s] = 1;
    }

    /* Initialize 64 team indices (0..63) at teamGridPairings */
    uint8_t *indices = ((DIYCompetitionEx *)a2raw)->teamGridPairings;
    uint8_t idx = 0;
    for (int16_t i = 0x3F; i >= 0; i--) {
        *indices++ = idx++;
    }
}

/* ================================================================
 * DIY_ShuffleTeamOrderList  (SWOS2.s line 32626)
 *
 * Initialize sequential order list at diyCompetitionStart+0x30E,
 * then Fisher-Yates shuffle it using Rand2.
 * Count = word at offset 0x50 of diyCompetitionStart.
 *
 * Asm: no params, no return value.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_ShuffleTeamOrderList(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a1s = &comp->base;
    uint8_t *orderList = comp->teamShuffleOrder;
    uint16_t count = (uint16_t)a1s->matchPairsPerRound;

    /* Initialize sequential order: 0, 1, 2, ..., count-1 */
    uint8_t val = 0;
    for (int16_t i = (int16_t)count - 1; i >= 0; i--) {
        *orderList++ = val++;
    }

    /* Fisher-Yates shuffle */
    orderList = comp->teamShuffleOrder;
    for (int16_t j = (int16_t)count - 1; j >= 0; j--) {
        uint16_t r = _Rand2();
        uint16_t k = (uint16_t)(((uint32_t)r * count) >> 8);
        uint8_t tmp = orderList[j];
        orderList[j] = orderList[k];
        orderList[k] = tmp;
    }
}

/* ================================================================
 * DIY_ShuffleTeamGridPairs  (SWOS2.s line 32986)
 *
 * Fisher-Yates shuffle of team grid pairs at offset 0x2CE.
 * Number of entries = count * 2, where count = word at offset 0x50.
 *
 * Asm: no params, no return value.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_ShuffleTeamGridPairs(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a2s = &comp->base;
    uint8_t *gridPairs = comp->teamGridPairings;
    uint16_t count = (uint16_t)a2s->matchPairsPerRound;
    uint16_t numEntries = count * 2;

    for (int16_t i = (int16_t)numEntries - 1; i >= 0; i--) {
        uint16_t r = _Rand2();
        uint16_t j = (uint16_t)(((uint32_t)r * numEntries) >> 8);
        uint8_t tmp = gridPairs[i];
        gridPairs[i] = gridPairs[j];
        gridPairs[j] = tmp;
    }
}

/* ================================================================
 * DIY_SyncProgressToDiyFileBufferCopy  (SWOS2.s line 33235)
 *
 * If word at diyCompetitionStart+0x48 is non-zero, copy two
 * progress words (offsets 0x44, 0x46) from diyCompetitionStart
 * to diyFileBufferCopy.
 *
 * Also used for DIY_SyncProgressToDiyFileBufferCopy_2 (line 33694)
 * which is an identical duplicate.
 *
 * Asm: no params, no return value.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_SyncProgressToDiyFileBufferCopy(void)
{
    if ((&diyCompetitionStart.base)->hasGroups == 0)
        return;
    diyFileBufferCopy.header.matchStarted = (&diyCompetitionStart.base)->matchStarted;
    diyFileBufferCopy.header.careerTeamInvolved = (&diyCompetitionStart.base)->careerTeamInvolved;
}

/* ================================================================
 * DIY_ValidateLeagueScratchBuffer  (SWOS2.s line 35077)
 *
 * Checksum validation of diyLeagueScratchBuffer.
 * Part 1: sum all 2000 bytes — if non-zero, panic with code 0x4444.
 * Part 2: if teamsLoaded, read count and offset from teamFileBuffer,
 *          sum that portion — if non-zero, panic with code 0x5555.
 *
 * Asm: no params, no return value. Saves/restores all registers.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_ValidateLeagueScratchBuffer(void)
{
    /* Part 1: checksum entire 2000-byte scratch buffer.
     * Use volatile to prevent LTO from optimizing the checksum loop. */
    uint8_t checksum = 0;
    volatile uint8_t *p = diyLeagueScratchBuffer;
    for (int16_t i = 0x7CF; i >= 0; i--) {
        checksum += *p++;
    }
    if (checksum != 0) {
        panicReasonCode = 0x4444;
        _PanicHaltLoop();
    }

    /* Part 2: validate loaded team portion */
    if (teamsLoaded == 0)
        return;

    volatile uint16_t *tfb = (volatile uint16_t *)teamFileBuffer;
    uint16_t count = tfb[0];
    uint16_t offset = tfb[2];  /* word at offset 4 = index 2 */
    volatile uint8_t *teamData = diyLeagueScratchBuffer + offset;

    checksum = 0;
    for (int16_t i = (int16_t)(count - 1); i >= 0; i--) {
        checksum += *teamData++;
    }
    if (checksum != 0) {
        panicReasonCode = 0x5555;
        _PanicHaltLoop();
    }
}

/* ================================================================
 * DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs  (SWOS2.s line 32826)
 *
 * Separates team grid pairs into two halves (home/away) based on
 * flags byte bit 0, using a temporary buffer, then writes them
 * back interleaved.
 *
 * Asm: no params, no return value.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a6s = &comp->base;
    uint16_t count = (uint16_t)a6s->matchPairsPerRound;
    uint8_t temp[64];
    uint8_t *lowHalf = temp;
    uint8_t *highHalf = temp + count;
    uint8_t *grid = comp->teamGridPairings;
    uint8_t *flags = comp->matchData.cup.cupRoundResults;

    SWOS_DBG("[bug23-swap] ENTER count=%u grid[0..7]="
                    "%02X %02X %02X %02X %02X %02X %02X %02X "
                    "flags[0..3]=%02X %02X %02X %02X\n",
            (unsigned)count,
            grid[0], grid[1], grid[2], grid[3],
            grid[4], grid[5], grid[6], grid[7],
            flags[0], flags[1], flags[2], flags[3]);

    for (int16_t i = (int16_t)count - 1; i >= 0; i--) {
        if (*flags++ & 1) {
            /* Swapped: team1→high, team2→low */
            *highHalf++ = *grid++;
            *lowHalf++ = *grid++;
        } else {
            /* Normal: team1→low, team2→high */
            *lowHalf++ = *grid++;
            *highHalf++ = *grid++;
        }
    }

    /* Copy temp back to grid (volatile to prevent memcpy optimization) */
    volatile uint8_t *vsrc = temp;
    volatile uint8_t *vdst = comp->teamGridPairings;
    uint16_t total = count * 2;
    for (int16_t j = (int16_t)total - 1; j >= 0; j--) {
        *vdst++ = *vsrc++;
    }

    SWOS_DBG("[bug23-swap] EXIT  grid[0..7]="
                    "%02X %02X %02X %02X %02X %02X %02X %02X\n",
            comp->teamGridPairings[0], comp->teamGridPairings[1],
            comp->teamGridPairings[2], comp->teamGridPairings[3],
            comp->teamGridPairings[4], comp->teamGridPairings[5],
            comp->teamGridPairings[6], comp->teamGridPairings[7]);
}

/* ================================================================
 * DIY_ReorderTeamGridByOrderList  (SWOS2.s line 32795)
 *
 * Apply home/away swap flags, then reorder team grid entries
 * according to the order list at offset 0x30E.
 *
 * Asm: no params, no return value.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_ReorderTeamGridByOrderList(void)
{
    _DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs();

    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a0s = &comp->base;
    uint8_t *grid = comp->teamGridPairings;
    uint16_t count = (uint16_t)a0s->matchPairsPerRound;

    /* Copy grid[0..count-1] to temp (volatile to prevent memcpy optimization) */
    uint8_t temp[64];
    volatile uint8_t *vsrc = grid;
    volatile uint8_t *vdst = temp;
    for (int16_t i = (int16_t)count - 1; i >= 0; i--) {
        *vdst++ = *vsrc++;
    }

    /* Reorder: grid[i] = temp[orderList[i]] */
    uint8_t *orderList = comp->teamShuffleOrder;
    uint8_t *dst = comp->teamGridPairings;
    for (int16_t j = (int16_t)count - 1; j >= 0; j--) {
        uint16_t idx = *orderList++;
        idx &= 0xFF;
        *dst++ = temp[idx];
    }
}

/* ================================================================
 * Static helper: decrement missing team count if team not loaded
 * (was DIY_DecrementMissingTeamCountIfNotLoaded, SWOS2.s line 32911)
 *
 * Reads team number from teamIds[gridByte], calls VerifyTeamLoaded,
 * decrements counter at comp+0x4C if team category byte != 1.
 * ================================================================ */
static void decrementMissingIfNotLoaded(uint8_t *comp, uint16_t *teamIds, uint16_t gridByte)
{
    DIYCompetition *compS = (DIYCompetition *)comp;
    gridByte &= 0xFF;
    /* teamIds is cupTeamIds — BE-packed per 4e5f378. */
    uint16_t teamNum = BE_READ_U16(&teamIds[gridByte]);
    TeamFileEx *teamPtr = _VerifyTeamLoaded(teamNum);
    if (teamPtr->team.teamControls != 1) {
        compS->roundIterator -= 1;
    }
}

/* ================================================================
 * DIY_RecountMissingTeamsInTeamGrid  (SWOS2.s line 32864)
 *
 * Count teams that failed to load in the current cup bracket.
 * Two paths: tournament slot check (offset 0x48 != 0) or general
 * loop through all team grid pairs.
 *
 * Asm: no params, no return value.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_RecountMissingTeamsInTeamGrid(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a6s = &comp->base;
    uint8_t *a6 = (uint8_t *)a6s;
    uint16_t *teamIds = comp->cupTeamIds;
    uint8_t *grid = comp->teamGridPairings;
    uint8_t *flags = comp->matchData.cup.cupRoundResults;  /* parent 0x8A */

    if (a6s->hasGroups != 0) {
        /* Tournament slot path */
        uint8_t *slot = (uint8_t *)&diyFileBufferCopy;
        uint16_t slotIdx = (uint16_t)diyFileBufferCopy.header.roundComplete;
        slot += (uint32_t)slotIdx * 0x76;
        DIYTournamentSlot *s = (DIYTournamentSlot *)(slot + offsetof(DIYTournamentState, slots));

        if (s->doubleCount != 0) {
            /* Slot has doubled-count flag set */
            if (!(flags[0] & 2))
                return;
            decrementMissingIfNotLoaded(a6, teamIds, (uint8_t)grid[0]);
            decrementMissingIfNotLoaded(a6, teamIds, (uint8_t)grid[1]);
            return;
        }
        if (s->divisionFactor == 1)
            goto mainLoop;
        /* +0x1D2 from slot byte base = +0x78 from DIYTournamentSlot start
         * = next slot's doubleCount (0x76 stride + +0x02). */
        if (s[1].doubleCount == 0)
            goto mainLoop;
        return;
    }

mainLoop:
    {
        uint16_t count = (uint16_t)a6s->totalMatchesInRound;
        for (int16_t i = (int16_t)count - 1; i >= 0; i--) {
            if (flags[0] & 2) {
                uint8_t d0;
                if (flags[0] & 1) {
                    d0 = grid[0];  /* home team */
                } else {
                    d0 = grid[1];  /* away team */
                }
                decrementMissingIfNotLoaded(a6, teamIds, d0);
            }
            flags++;
            grid += 2;
        }
    }
}

/* ================================================================
 * DIY_BuildTeamSelectionPointerCache  (SWOS2.s line 32544)
 *
 * Two parts:
 * 1) Build relative pointer cache (team ptr - selTeamsPtr) at
 *    offset 0x344 + idx*4 for each team in the grid.
 * 2) Count non-national teams (category byte != 1) into offset 0x4C.
 *
 * Asm: no params, no return value.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_BuildTeamSelectionPointerCache(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a2s = &comp->base;
    uint8_t *a2 = (uint8_t *)a2s;
    uint8_t *gridPtr = comp->teamGridPairings;
    uint16_t *teamIds = comp->cupTeamIds;
    uint16_t count = (uint16_t)a2s->numTeams;

    /* Part 1: build relative pointer cache (teamOffsetTable inside
     * bracketScratch). cupTeamIds holds BE-packed `{country, ordinal}`
     * byte pairs (4e5f378 convention) — read via BE_READ_U16. */
    for (int16_t i = (int16_t)count - 1; i >= 0; i--) {
        int16_t gridIdx = (int8_t)*gridPtr++;   /* ext.w d6: sign-extend */
        uint16_t teamNum = BE_READ_U16(&teamIds[gridIdx]);
        TeamFileEx *teamPtr = _VerifyTeamLoaded(teamNum);
        /* Store the team IDENTITY (step-4 phase 3): -1 for careerTeam, else the
         * selTeamsPtr array index. Replaces the old signed byte offset — careerTeam
         * is now resolved by a branch, not `&selTeamsPtr + (negative)`, so it need
         * not live below selTeamsPtr. */
        int32_t relOff = diy_team_to_identity((uint8_t *)teamPtr);
        comp->bracketScratch.teamOffsetTable[(uint16_t)gridIdx] = relOff;
    }

    /* Part 2: count non-national teams */
    a2s->roundIterator = 0;
    gridPtr = comp->teamGridPairings;
    teamIds = comp->cupTeamIds;
    count = (uint16_t)a2s->numTeams;

    for (int16_t j = (int16_t)count - 1; j >= 0; j--) {
        uint16_t gridByte = (uint8_t)*gridPtr++;
        uint16_t teamNum = BE_READ_U16(&teamIds[gridByte]);
        TeamFileEx *teamPtr = _VerifyTeamLoaded(teamNum);
        if (teamPtr->team.teamControls != 1) {
            a2s->roundIterator += 1;
        }
    }
}

/* ================================================================
 * DIY_InitCupBracketState  (SWOS2.s line 32425)
 *
 * Initialize the cup bracket state: calculate rounds from team count,
 * clear flags and counters, set random phase, compute team strength.
 *
 * Calls: DIY_BuildTeamSelectionPointerCache,
 *        DIY_InitCupBracketProgressionTable,
 *        DIY_ComputeAverageTeamStrengthTier
 *
 * Asm: no params, no return value.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_InitCupBracketState(void)
{
    _DIY_BuildTeamSelectionPointerCache();

    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a2s = &comp->base;
    uint8_t *a2 = (uint8_t *)a2s;
    uint16_t halfTeamsVal = (uint16_t)a2s->numTeams >> 1;
    a2s->matchPairsPerRound = (int16_t)halfTeamsVal;
    a2s->totalMatchesInRound = (int16_t)halfTeamsVal;

    /* Count number of rounds (log2 of halfTeams) */
    uint16_t numRoundsVal = 0;
    uint16_t temp = (uint16_t)a2s->numTeams >> 1;
    while (temp != 0) {
        numRoundsVal++;
        temp >>= 1;
    }
    a2s->numRounds = (int16_t)numRoundsVal;

    a2s->roundComplete = 0;
    a2s->currentRoundIndex = 0;
    a2s->isActive = 1;
    a2s->competitionFinished = 0;
    comp->matchData.cup.competitionFormat = 0;

    /* Get round format for current round (round 0): from cup roundLegs[currentRoundIndex] */
    uint16_t roundIdx = (uint16_t)a2s->currentRoundIndex;  /* 0 */
    comp->matchData.cup.roundFormat = comp->matchData.cup.roundLegs[roundIdx];

    /* If only 1 team pair and replay mode != 2, clear replay */
    if (a2s->matchPairsPerRound == 1) {
        if (comp->matchData.cup.roundFormat != 2) {
            comp->matchData.cup.roundFormat = 0;
        }
    }

    /* Clear counters */
    comp->bracketScratch.groupSectionFlag = 0;
    comp->bracketScratch.secondLegCounter = 0;
    comp->bracketScratch.curMonthIdx = 0;
    comp->bracketScratch.matchCount = 0;

    /* Clear 32 bytes of flags at offset 0x8A (volatile to prevent memset optimization) */
    volatile uint8_t *flagPtr = comp->matchData.cup.cupRoundResults;
    for (int16_t i = 0x1F; i >= 0; i--) {
        *flagPtr++ = 0;
    }

    /* Set random phase from start month */
    a2s->viewRandomPhase = (uint8_t)a2s->startMonth;
    viewRandomPhase = a2s->viewRandomPhase;

    /* Build bracket progression table */
    _DIY_InitCupBracketProgressionTable();

    /* Compute average team strength tier */
    uint16_t numTeams = (uint16_t)(&diyCompetitionStart.base)->numTeams;
    uint32_t result = _DIY_ComputeAverageTeamStrengthTier(numTeams, (uint8_t *)diyCupTeamIds);
    (&diyCompetitionStart.base)->avgSkill = (uint8_t)(result >> 16);
    (&diyCompetitionStart.base)->strengthTier = (uint8_t)(result & 0xFF);
}

/* ================================================================
 * DIY_RecomputeCompetitionDerivedFields  (SWOS2.s line 33006)
 *
 * Computes derived fields from the competition structure: rounds,
 * fixture counts, random phase, progression table, strength tier.
 *
 * Asm: no params, no return.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_RecomputeCompetitionDerivedFields(void)
{
    _DIY_CacheSelectedTeamPointers();
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a2s = &comp->base;
    uint8_t *a2 = (uint8_t *)a2s;

    /* numTeams / matchesPerFixtureRound = groupsPerRound */
    int16_t numTeams = a2s->numTeams;
    uint16_t matchesPerRound = (uint16_t)a2s->matchPairsPerRound;
    uint16_t groupsPerRound = (uint16_t)((uint32_t)(int32_t)numTeams / matchesPerRound);
    a2s->totalMatchesInRound = (int16_t)groupsPerRound;

    comp->leagueSectionState.sectionMatchesProgress = 0;

    uint16_t halfGroups = groupsPerRound >> 1;
    comp->leagueSectionState.halfGroupCount = halfGroups;

    uint16_t matchesPerHalf = halfGroups * matchesPerRound;
    comp->leagueSectionState.matchesPerHalf = matchesPerHalf;

    comp->leagueSectionState.halfGroupsCounter = 0;
    comp->leagueMatchProgress = 0;
    comp->leagueCurrentSlot = 0;

    /* totalMatchesPerSection = (groupsPerRound - 1) * groupsPerRound / 2 */
    uint16_t totalMatchesPerSection = (uint16_t)(((uint16_t)(groupsPerRound - 1) * groupsPerRound) >> 1);
    comp->matchData.league.totalMatchesPerSection = totalMatchesPerSection;
    comp->matchData.league.totalMatchesPerSectionLive = totalMatchesPerSection;

    /* matchesPerSeason = totalMatchesPerSection / halfGroups * playEachTeam */
    int16_t totalSigned = (int16_t)totalMatchesPerSection;
    uint16_t matchesPerSeason = (uint16_t)((uint32_t)(int32_t)totalSigned / halfGroups);
    matchesPerSeason *= (uint16_t)a2s->playEachTeam;
    comp->leagueTotalMatches = matchesPerSeason;

    comp->minSubstitutes = (uint16_t)a2s->playEachTeam;  /* repurposed as section-progression counter */
    comp->matchesPlayedCount = 0;
    a2s->isActive = 1;

    a2s->viewRandomPhase = (uint8_t)a2s->startMonth;
    viewRandomPhase = a2s->viewRandomPhase;

    _DIY_RecomputeCompetitionDerivedProgressionTable();

    uint16_t teamCount = (uint16_t)comp->base.numTeams;
    uint32_t result = _DIY_ComputeAverageTeamStrengthTier(teamCount, (uint8_t *)diyLeagueTeamIds);
    comp->base.avgSkill = (uint8_t)(result >> 16);
    comp->base.strengthTier = (uint8_t)(result & 0xFF);
}

/* ================================================================
 * getCurrentFixtureTeamPtrs  (SWOS2.s line 33148)
 *
 * Determines the two team competition record pointers for the
 * current fixture. Uses European Championship fixture data or
 * the general fixture schedule table.
 *
 * Returns pointers via output params.
 * ================================================================ */
static void getCurrentFixtureTeamPtrs(uint8_t **team1Out, uint8_t **team2Out)
{
    DIYCompetitionEx *compEx = &diyCompetitionStart;
    DIYCompetition *a0s = &compEx->base;
    uint8_t *a0 = (uint8_t *)a0s;
    uint16_t d5 = (uint16_t)a0s->subRoundCounter;
    d5 = (uint16_t)(d5 * (uint16_t)a0s->totalMatchesInRound);
    /* fixturePositions = shuffleAndPad[d5..]. Original asm uses the raw
     * byte offset 0xEE (parent-relative) but on 64-bit DIYCompetition
     * grew by +8 from the namePtr pointer-width change (see size_check
     * at game_types.h:599), so the raw offset reads 8 bytes too early —
     * inside cupRoundResults[100], producing garbage like 56/74/92/110
     * for the team grid lookup. Use the typed field so the address is
     * bitness-correct on both 32 and 64-bit. */
    uint8_t *fixturePositions = compEx->matchData.league.shuffleAndPad + d5;

    /* Select fixture schedule: EC fixture data or general table */
    uint8_t *schedule;
    uint8_t compId = a0s->field_2E;
    if (compId == europeanChampionships[0] || compId == europeanChampionships2[0]) {
        schedule = euroChampFixtureData;
    } else {
        uint16_t groups = (uint16_t)a0s->totalMatchesInRound;
        schedule = (uint8_t *)fixtureSchedulePtrTable[groups - 2];  /* drop const for schedule += writes below */
    }

    /* Advance to current round fixture pair */
    uint16_t roundIdx = diyCompetitionStart.leagueSectionState.sectionMatchesProgress;
    schedule += roundIdx * 2;

    /* Team A: position index → grid index → competition record */
    uint16_t posA = schedule[0];
    int16_t gridIdxA = (int8_t)fixturePositions[posA];
    uint32_t offsetA = (uint16_t)gridIdxA * 18u;
    uint8_t *team1 = a0 + offsetA;

    /* Team B */
    uint16_t posB = schedule[1];
    int16_t gridIdxB = (int8_t)fixturePositions[posB];
    uint32_t offsetB = (uint16_t)gridIdxB * 18u;
    uint8_t *team2 = a0 + offsetB;

    /* Home/away swap if flag bit 0 is set. Asm: btst #0,$61(a0) with
     * a0=diyCompetitionStart. On Amiga BE, byte +0x61 is the LSB of
     * `minSubstitutes` (uint16 at parent +0x60). Bit 0 of the LSB == bit
     * 0 of the uint16. Read the typed field directly so the test is
     * bitness- and endian-agnostic — `0x61` is a 32-bit raw offset and
     * lands inside DIYCompetition padding on 64-bit (same trap that broke
     * the fixturePositions read above). */
    uint8_t swapFlag = (uint8_t)(compEx->minSubstitutes & 1);
    if (swapFlag) {  /* extended */
        *team1Out = team2;
        *team2Out = team1;
    } else {
        *team1Out = team1;
        *team2Out = team2;
    }

    /* [bug20-getfx] Always-on first-N trace of grid/schedule lookup. */
    {
        static int s_gfCalls = 0;
        if (s_gfCalls < 30) {
            s_gfCalls++;
            SWOS_DBG("[bug20-getfx] #%d subRound=%d "
                            "matchesInRound=%d sectionProg=%u "
                            "d5(grid stride)=%u "
                            "fxPositionsOff=0x%lx "
                            "schedule[0..3]=%02X %02X %02X %02X "
                            "posA=%u posB=%u gridA=%d gridB=%d "
                            "swap=%u\n",
                            s_gfCalls,
                            (int)a0s->subRoundCounter,
                            (int)a0s->totalMatchesInRound,
                            (unsigned)roundIdx,
                            (unsigned)d5,
                            (unsigned long)(fixturePositions - a0),
                            (unsigned)schedule[0], (unsigned)schedule[1],
                            (unsigned)schedule[2], (unsigned)schedule[3],
                            (unsigned)posA, (unsigned)posB,
                            (int)gridIdxA, (int)gridIdxB,
                            (unsigned)swapFlag);
            SWOS_DBG_FLUSH();
        }
    }

#ifdef SWOS_DEBUG_AUTOSKIP
    {
        DIYTeamStats *r1 = (DIYTeamStats *)(*team1Out + DIY_STANDINGS_OFFSET);
        DIYTeamStats *r2 = (DIYTeamStats *)(*team2Out + DIY_STANDINGS_OFFSET);
        int32_t selOff1 = (int32_t)r1->teamSelOffset;
        int32_t selOff2 = (int32_t)r2->teamSelOffset;
        uint16_t t1Id = 0, t2Id = 0;
        uint8_t t1Ctrl = 0xFF, t2Ctrl = 0xFF;
        uint8_t *sel1Ptr = diy_resolve_team(selOff1);
        uint8_t *sel2Ptr = diy_resolve_team(selOff2);
        const int32_t maxPositive = 64 * 684;
        const int32_t maxNegative = -1 * 1024 * 1024;
        int selOk1 = (selOff1 >= maxNegative && selOff1 <= maxPositive);
        int selOk2 = (selOff2 >= maxNegative && selOff2 <= maxPositive);
        if (selOk1) { t1Id = BE_READ_U16(sel1Ptr); t1Ctrl = sel1Ptr[4]; }
        if (selOk2) { t2Id = BE_READ_U16(sel2Ptr); t2Ctrl = sel2Ptr[4]; }
        SWOS_AUTOSKIP_LOG("[bug20-fixture] GetFixturePtrs compId=0x%04X subRound=%d "
                          "matchesInRound=%d sectionProgress=%u posA=%u posB=%u "
                          "gridA=%d gridB=%d swap=%u "
                          "team1Off=0x%lx team2Off=0x%lx "
                          "selOff1=%ld selOff2=%ld selOk1=%d selOk2=%d "
                          "t1Id=0x%04X t1Ctrl=%u t2Id=0x%04X t2Ctrl=%u",
                          (unsigned)*(uint16_t *)&a0s->compId,
                          (int)a0s->subRoundCounter,
                          (int)a0s->totalMatchesInRound,
                          (unsigned)roundIdx,
                          (unsigned)posA, (unsigned)posB,
                          (int)gridIdxA, (int)gridIdxB,
                          (unsigned)swapFlag,
                          (unsigned long)(*team1Out - a0),
                          (unsigned long)(*team2Out - a0),
                          (long)selOff1, (long)selOff2,
                          selOk1, selOk2,
                          (unsigned)t1Id, (unsigned)t1Ctrl,
                          (unsigned)t2Id, (unsigned)t2Ctrl);
    }
#endif
}

/* Globals for asm bridge — written by _DIY_GetCurrentFixtureTeamPtrs,
 * read by the asm stub to set a1/a2 registers. */
__attribute__((used)) uint8_t *fixtureTeam1CompPtr;
__attribute__((used)) uint8_t *fixtureTeam2CompPtr;

__attribute__((used, externally_visible))
void _DIY_GetCurrentFixtureTeamPtrs(void)
{
    getCurrentFixtureTeamPtrs(&fixtureTeam1CompPtr, &fixtureTeam2CompPtr);
}

/* ================================================================
 * DIY_AdvanceLeagueProgressAfterMatch  (SWOS2.s line 33193)
 *
 * Updates match counters, fixture/round progression, and random
 * phase after a league match.
 *
 * Asm: a1=team1 comp record, a2=team2 comp record.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_AdvanceLeagueProgressAfterMatch(uint8_t *team1, uint8_t *team2)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a0s = &comp->base;
    uint8_t *a0 = (uint8_t *)a0s;
    DIYTeamStats *row1 = (DIYTeamStats *)(team1 + DIY_STANDINGS_OFFSET);
    DIYTeamStats *row2 = (DIYTeamStats *)(team2 + DIY_STANDINGS_OFFSET);

    /* Increment matches played for both teams */
    row1->gamesPlayed += 1;
    row2->gamesPlayed += 1;

    /* Reset match counter if phase-change flag set */
    if (comp->leagueCurrentSlot) {
        comp->leagueCurrentSlot = 0;
        comp->leagueSectionState.halfGroupsCounter = 0;
    }

    comp->leagueSectionState.halfGroupsCounter += 1;
    a0s->subRoundCounter += 1;

    if ((uint16_t)a0s->subRoundCounter != (uint16_t)a0s->matchPairsPerRound)
        return;

    /* All fixtures in this round done */
    a0s->subRoundCounter = 0;
    comp->leagueSectionState.sectionMatchesProgress += 1;
    comp->leagueCurrentSlot = 0;

    if (comp->leagueSectionState.halfGroupsCounter == comp->leagueSectionState.matchesPerHalf) {
        /* Phase boundary reached */
        comp->leagueCurrentSlot = 1;
        comp->leagueMatchProgress += 1;
        uint16_t phaseIdx = comp->leagueMatchProgress;
        a0s->viewRandomPhase = comp->leagueSectionState.phaseStartMonths[phaseIdx];
        viewRandomPhase = a0s->viewRandomPhase;
#ifdef SWOS_DEBUG_AUTOSKIP
        SWOS_AUTOSKIP_LOG("[bug3-league]   PhaseBoundary compId=0x%04X new_leagueMatchProgress=%u "
                          "new_viewRandomPhase=%u(phaseStartMonths[%u]) "
                          "sectionsLeft(minSubs)=%u totalMatchesPerSectionLive=%d",
                          (unsigned)*(uint16_t *)&a0s->compId,
                          (unsigned)comp->leagueMatchProgress,
                          (unsigned)a0s->viewRandomPhase, (unsigned)phaseIdx,
                          (unsigned)comp->minSubstitutes,
                          (int)comp->matchData.league.totalMatchesPerSectionLive);
#endif
    }

    comp->matchData.league.totalMatchesPerSectionLive -= 1;
    if (comp->matchData.league.totalMatchesPerSectionLive != 0)
        return;

    /* minSubstitutes repurposed as section-progression counter */
    comp->minSubstitutes -= 1;
#ifdef SWOS_DEBUG_AUTOSKIP
    SWOS_AUTOSKIP_LOG("[bug3-league]   SectionEnd compId=0x%04X "
                      "sectionsLeft(minSubs)=%u (decremented from prev+1)",
                      (unsigned)*(uint16_t *)&a0s->compId, (unsigned)comp->minSubstitutes);
#endif
    if (comp->minSubstitutes == 0)
        return;

    /* Reset for next section */
    comp->matchData.league.totalMatchesPerSectionLive = comp->matchData.league.totalMatchesPerSection;
    comp->leagueSectionState.sectionMatchesProgress = 0;
}

/* ================================================================
 * DIY_GetCurrentFixtureTeamClass  (SWOS2.s line 33231)
 *
 * Determines the match class for the current fixture based on
 * team classes. Class 1 defers to the other team; otherwise
 * returns the dominant class or 2 if neither is 1.
 *
 * Asm: no params, returns d0.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _DIY_GetCurrentFixtureTeamClass(void)
{
    uint8_t *team1, *team2;
    getCurrentFixtureTeamPtrs(&team1, &team2);

    /* Follow pointer at DIYTeamStats teamSelOffset (+0x2B4) + selTeamsPtr base */
    uint8_t *sel1 = diy_resolve_team(((DIYTeamStats *)(team1 + DIY_STANDINGS_OFFSET))->teamSelOffset);
    uint8_t *sel2 = diy_resolve_team(((DIYTeamStats *)(team2 + DIY_STANDINGS_OFFSET))->teamSelOffset);

    int16_t class1 = (int8_t)sel1[4];
    int16_t class2 = (int8_t)sel2[4];

    if (class1 == class2)
        return (uint16_t)class1;
    if (class2 == 1)
        return (uint16_t)class1;
    /* exg d0,d1 */
    if (class1 == 1)
        return (uint16_t)class2;
    return 2;
}

/* ================================================================
 * DIY_BuildQualifiedTeamListFromSortedTable  (SWOS2.s lines 33519, 33630)
 *
 * Builds a qualified team list from the sorted league table.
 * Both EC (line 33519) and WC (line 33630) variants have
 * identical logic — they call DIY_SortLeagueTableByStandings,
 * then iterate the sorted index array at offset 0x6E, dividing
 * each entry by 0x12 (18) to get a team slot index, and looking
 * up the team ID from offset 0x12E.
 *
 * Asm: d7 = count, a5 = output buffer
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_BuildQualifiedTeamListFromSortedTable(uint16_t count, uint8_t *out)
{
    _DIY_SortLeagueTableByStandings();

    DIYCompetitionEx *comp = &diyCompetitionStart;
    uint16_t *sortedIndices = comp->matchData.league.teamEntryOffsets;
    uint16_t *teamIds = comp->matchData.league.leagueTeamIds;
    uint16_t *dst = (uint16_t *)out;

    for (int16_t i = (int16_t)(count - 1); i >= 0; i--) {
        /* ext.l d0; divu.w #$12,d0 — sign-extend word, unsigned divide by 18 */
        int16_t val = (int16_t)*sortedIndices++;
        uint16_t slotIdx = (uint16_t)((uint32_t)(int32_t)val / 18);
        *dst++ = teamIds[slotIdx];
    }
}

/* ================================================================
 * DIY_BuildQualifiedTeamListFromDiyFileBuffer  (SWOS2.s line 33654)
 *
 * Builds a qualified team list from the DIY file buffer copy.
 * First calls DIY_SyncCompetitionTeamGridOrBuildBracket, then
 * reads byte indices from offset 0x5A of diyFileBufferCopy,
 * sign-extends each to a word, doubles it, and looks up team IDs
 * from offset 0x9A.
 *
 * Asm: d7 = count, a5 = output buffer
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_BuildQualifiedTeamListFromDiyFileBuffer(uint16_t count, uint8_t *out)
{
    _DIY_SyncCompetitionTeamGridOrBuildBracket();

    DIYTournamentState *t = &diyFileBufferCopy;
    uint8_t *indices = t->teamIndexOrder;  /* 0x5A (relocated) */
    uint16_t *teamIds = t->teamIds;        /* 0x9A */
    uint16_t *dst = (uint16_t *)out;

    for (int16_t i = (int16_t)(count - 1); i >= 0; i--) {
        /* move.b (a0)+,d0; ext.w d0; lsl.w #1,d0; move.w (a1,d0.w),(a5)+ */
        int8_t idx = (int8_t)*indices++;
        int16_t wordIdx = (int16_t)idx;
        *dst++ = teamIds[wordIdx];
    }
}

/* ================================================================
 * DIY_SyncCompetitionTeamGridOrBuildBracket  (SWOS2.s line 33673)
 *
 * Conditionally builds a seeded bracket or copies the team grid.
 * Reads diyFileBufferCopy + offset 0x54 (word) to compute a
 * pointer: a3 = diyFileBufferCopy + val * 0x76.
 * If a3[0x15E] != 0, calls DIY_BuildSeededBracketAndUpdateScores;
 * otherwise calls DIY_CopyTeamGridFromCompToDIY.
 * Both asm functions take a2, a3, a4, a5 as params.
 *
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_SyncCompetitionTeamGridOrBuildBracket(void)
{
    uint8_t *a2 = (uint8_t *)&diyFileBufferCopy;
    uint16_t offset = (uint16_t)diyFileBufferCopy.header.roundComplete;
    uint8_t *a3 = a2 + (uint32_t)offset * 0x76;
    DIYTournamentSlot *a3Slot = (DIYTournamentSlot *)(a3 + offsetof(DIYTournamentState, slots));
    uint8_t *a4 = (uint8_t *)&diyCompetitionStart.base;
    uint8_t *a5 = (uint8_t *)&diyCompetitionStart.base;

    if (a3Slot->isLeagueFormat != 0) {
        _DIY_BuildSeededBracketAndUpdateScores(a2, a3, a4, a5);
    } else {
        _DIY_CopyTeamGridFromCompToDIY(a2, a3, a4, a5);
    }
}

/* ================================================================
 * DIY_CopyCompetitionTeamIndexTable  (SWOS2.s line 34155)
 *
 * Copies team index words from diyCompetitionStart offset 0x1AE
 * to the destination. Count is the sum of words at offsets 0x56
 * and 0x5A. If count is 0, does nothing.
 *
 * Asm: a3 = dst, advances a3 in-place via `move.w (a0)+,(a3)+`.
 * The caller (Career_ProcessPrimaryCompetitionForNextSeason) sets
 * a3 = diyQualifiedTeamIndices ONCE before its per-division loop, so
 * each call APPENDS to the cumulative buffer rather than overwriting
 * from base. Without this advancement, the qualification-entry loop
 * later only sees div N-1's promotion list (the last write), and
 * teams from earlier divisions go missing — leading to scratch
 * imbalance in _DIY_ValidateLeagueScratchBuffer.
 *
 * Return the new cursor so the caller can chain calls.
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_DIY_CopyCompetitionTeamIndexTable(uint8_t *dst)
{
    DIYCompetition *comps = &diyCompetitionStart.base;
    uint8_t *comp = (uint8_t *)comps;
    uint16_t count = (uint16_t)comps->competitionFinished + (uint16_t)comps->numRounds;
    if (count == 0)
        return dst;

    /* +0x1AE is parent offset = offset 0x60 within _bss_2E0AE (which
     * starts at parent +0x14E). Use typed field so 64-bit pointer
     * growth in DIYCompetition base doesn't shift the read. See
     * project_raw_offset_64bit_shift_trap.md. */
    DIYCompetitionEx *_compEx_idx_table = &diyCompetitionStart;
    uint16_t *src = (uint16_t *)(_compEx_idx_table->_bss_2E0AE + 0x60);  /* extended */
    uint16_t *d = (uint16_t *)dst;
    for (int16_t i = (int16_t)(count - 1); i >= 0; i--) {
        *d++ = *src++;
    }
    return (uint8_t *)d;
}

/* ================================================================
 * DIY_ApplyPromotionRelegationWeightsToScratch  (SWOS2.s line 34242)
 *
 * Applies promotion/relegation weights to the league scratch buffer.
 * First calls DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs.
 * Then iterates numTeams times: reads team index byte from
 * diyCompetitionStart+0x2CE, looks up weight from weights table,
 * verifies team is loaded, and adds weight to scratch buffer at
 * the team's country index. If the team is careerTeam, sets the
 * promoted/relegated animation flag.
 *
 * Asm: a2 = weights table. The weights table is a 2D array
 * (numTeams rows × variable columns); a2 advances by numTeams
 * bytes each iteration.
 *
 * ================================================================ */
extern uint16_t playPromotedAnimation;
extern uint16_t playRelegatedAnimation;

__attribute__((used, externally_visible))
void _DIY_ApplyPromotionRelegationWeightsToScratch(const uint8_t *weights)
{
    _DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs();

    volatile uint8_t *scratch = diyLeagueScratchBuffer;
    DIYCompetition *comps = &diyCompetitionStart.base;
    uint8_t *comp = (uint8_t *)comps;
    uint16_t numTeams = (uint16_t)comps->numTeams;
    /* +0x2CE = teamGridPairings[64]; use typed field for bitness safety. */
    uint8_t *teamData = ((DIYCompetitionEx *)comp)->teamGridPairings;
    const uint8_t *w = weights;

    for (int16_t i = (int16_t)(numTeams - 1); i >= 0; i--) {
        int8_t rawIdx = (int8_t)*teamData++;
        int16_t idx = (int16_t)rawIdx;
        int8_t weight = (int8_t)w[idx];

        if (weight != 0) {
            /* Look up team ID from cup team IDs (BE-packed per 4e5f378). */
            uint16_t teamId = BE_READ_U16(&diyCupTeamIds[idx]);
            /* VerifyTeamLoaded returns the team pointer */
            TeamFileEx *team = _VerifyTeamLoaded(teamId);
            /* Add weight to scratch at country number index */
            uint16_t countryNum = team->team.globalTeamNumber;
            scratch[countryNum] += (uint8_t)weight;
            /* Check if this is the career team */
            if (team == &careerTeam) {
                if (weight < 0) {
                    playPromotedAnimation = 1;
                } else {
                    playRelegatedAnimation = 1;
                }
            }
        }

        /* Advance weights pointer by numTeams */
        w += numTeams;
    }
}

/* ================================================================
 * DIY_UpdatePromotionRelegationScratchCounts  (SWOS2.s line 34325)
 *
 * Updates the league scratch buffer with promotion/relegation
 * counts. Only processes if competition type at offset 0 is 0
 * (league). Reads promoted team count from offset 0x54 and
 * relegated count from offset 0x58.
 *
 * For promoted teams: iterates indices at offset 0x6E forwards,
 * reads competition entry, follows pointer at offset 0x2B4 to
 * get team data in selTeamsPtr, reads country number, and
 * decrements scratch count. Sets playPromotedAnimation if
 * career team.
 *
 * For relegated teams: iterates indices backwards from the end
 * of the sorted table, increments scratch count.
 * Sets playRelegatedAnimation if career team.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_UpdatePromotionRelegationScratchCounts(void)
{
    DIYCompetitionEx *compEx = &diyCompetitionStart;
    DIYCompetition *comps = &compEx->base;
    uint8_t *comp = (uint8_t *)comps;
    /* Only process for league competitions (type == 0) */
    if (comps->competitionType != 0)
        return;

    volatile uint8_t *scratch = diyLeagueScratchBuffer;

    /* Process promoted teams (from top of table) */
    uint16_t promotedCount = (uint16_t)comps->roundComplete;
    if (promotedCount != 0) {
        uint16_t *indices = compEx->matchData.league.teamEntryOffsets;
        for (int16_t i = (int16_t)(promotedCount - 1); i >= 0; i--) {
            uint16_t entryOff = *indices++;
            uint8_t *entry = comp + entryOff;
            /* Follow DIYTeamStats teamSelOffset (+0x2B4) and add selTeamsPtr base */
            /* teamSelOffset is int32_t — careerTeam's offset is negative; keep
         * signed so 64-bit pointer arithmetic sign-extends correctly
         * (see SetupCurrentFixtureTeamNames for the bug story). */
        int32_t selOff = ((DIYTeamStats *)(entry + DIY_STANDINGS_OFFSET))->teamSelOffset;
            TeamFileEx *team = (TeamFileEx *)diy_resolve_team(selOff);
            /* "countryNum" name predates the audit — the slot at byte 2
             * is TeamFile.globalTeamNumber, used as a scratch index. */
            uint16_t globalNo = (uint16_t)team->team.globalTeamNumber;
            scratch[globalNo]--;
            /* Check if career team */
            if (team == &careerTeam) {
                playPromotedAnimation = 1;
            }
        }
    }

    /* Process relegated teams (from bottom of table) */
    uint16_t relegatedCount = (uint16_t)comps->currentRoundIndex;
    if (relegatedCount == 0)
        return;

    uint16_t numTeams = (uint16_t)comps->numTeams;
    uint16_t *bottomIndices = compEx->matchData.league.teamEntryOffsets + numTeams;
    for (int16_t i = (int16_t)(relegatedCount - 1); i >= 0; i--) {
        uint16_t entryOff = *(--bottomIndices);
        uint8_t *entry = comp + entryOff;
        /* teamSelOffset is int32_t — careerTeam's offset is negative; keep
         * signed so 64-bit pointer arithmetic sign-extends correctly
         * (see SetupCurrentFixtureTeamNames for the bug story). */
        int32_t selOff = ((DIYTeamStats *)(entry + DIY_STANDINGS_OFFSET))->teamSelOffset;
        TeamFileEx *team = (TeamFileEx *)diy_resolve_team(selOff);
        uint16_t globalNo = (uint16_t)team->team.globalTeamNumber;
        scratch[globalNo]++;
        if (team == &careerTeam) {
            playRelegatedAnimation = 1;
        }
    }
}

/* ================================================================
 * Career_IsTeamIdNotInListOrReturnSentinel  (SWOS2.s line 98111)
 *
 * Searches diyCupTeamIds for teamId up to endPtr. If found, returns
 * the team ID (low 16 bits). If not found, returns teamId | 0x10000
 * (bit 16 set as sentinel). Asm stub uses btst #16 to set Z flag.
 * ================================================================ */
__attribute__((used, externally_visible))
uint32_t _Career_IsTeamIdNotInListOrReturnSentinel(uint16_t teamId, uint16_t *endPtr)
{
    uint16_t *ptr = diyCupTeamIds;
    while (ptr != endPtr) {
        /* diyCupTeamIds stores BE-packed byte pairs (4e5f378). */
        if (BE_READ_U16(ptr) == teamId)
            return (uint32_t)teamId;  /* found */
        ptr++;
    }
    return (uint32_t)teamId | 0x10000u;  /* not found */
}

/* ================================================================
 * Career_IsTeamIdNotInListOrReturnSentinel2  (SWOS2.s line 98202)
 *
 * Same as above but searches diyTournamentTeamIds instead.
 * ================================================================ */
__attribute__((used, externally_visible))
uint32_t _Career_IsTeamIdNotInListOrReturnSentinel2(uint16_t teamId, uint16_t *endPtr)
{
    uint16_t *ptr = diyFileBufferCopy.teamIds;
    while (ptr != endPtr) {
        /* diyFileBufferCopy.teamIds stores BE-packed byte pairs (4e5f378). */
        if (BE_READ_U16(ptr) == teamId)
            return (uint32_t)teamId;  /* found */
        ptr++;
    }
    return (uint32_t)teamId | 0x10000u;  /* not found */
}


/* ================================================================
 * Chunk 96 — DIY_EditCompetitionSlot, DIY_UpdateCompetitionProgress,
 *            DIY_ReorderTeamsBySelectionFlags, DIY_RebuildCaches,
 *            DIY_UpdateCompetitionProgressOrAdvanceSlot + sub-funcs,
 *            DIY_InitCupFromTeamGridAndValidateTeams,
 *            DIY_ResetAndLoadCurrentCompetitionSlot
 * ================================================================ */

/* Helper macros for reading/writing fields at byte offsets in raw buffers.
 * Used for extended fields (offset >= 0x60) beyond the typed DIYCompetition base.
 * Base-struct fields (0x00-0x5F) use proper struct access via DIYCompetition. */
#define RD16(base, off) (*(int16_t *)((uint8_t *)(base) + (off)))
#define RD16U(base, off) (*(uint16_t *)((uint8_t *)(base) + (off)))
#define WR16(base, off, val) (*(int16_t *)((uint8_t *)(base) + (off)) = (int16_t)(val))
#define RD32U(base, off) (*(uint32_t *)((uint8_t *)(base) + (off)))
#define RD8U(base, off) (*(uint8_t *)((uint8_t *)(base) + (off)))
#define WR8U(base, off, val) (*(uint8_t *)((uint8_t *)(base) + (off)) = (uint8_t)(val))

/* Slot structure offsets (0x76 = 118 bytes per slot):
 * +0x15E: slot active flag
 * +0x160: first leg flag
 * +0x162: teams per match
 * +0x164: fixture type
 * +0x16A: groups per round
 * +0x16C: teams per group
 * +0x1D2: home/away swap
 */

/* ================================================================
 * DIY_EditCompetitionSlot (SWOS2.s line 31502)
 *
 * Takes 3 register params (d0, d1, d2) and routes to either
 * ShowDiyLeaguesTournamentMenu or ShowDIYCupMenuWithParams depending
 * on the competition type. Copies results back to diyFileBufferCopy.
 * For career/contest modes, may loop via DIY_AdvanceToNextCompetitionSlot.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_EditCompetitionSlot(uint16_t paramA, uint16_t paramB, uint16_t paramC)
{
    /* Store params into local data (asm used diyEditParamA/B/C) */
    uint16_t editA = paramA;
    uint16_t editB = paramB;
    uint16_t editC = paramC;

    DIYCompetition *bufCopy = &diyFileBufferCopy.header;
    DIYCompetition *comp = &diyCompetitionStart.base;
    uint8_t *bufCopyRaw = (uint8_t *)&diyFileBufferCopy;
    uint8_t *compRaw = (uint8_t *)comp;

#ifdef SWOS_DEBUG_AUTOSKIP
    /* Diagnostic for the WC94TEST.PRE round-trip: dump the head of both
     * competitionFileBuffer (what the .PRE was loaded into / saved from)
     * and diyFileBufferCopy (the active runtime state), at slot 0 + slot 1
     * head + the slot-1 tournament-fields overlay region. If these
     * diverge after _SaveDIYTournament has run we know the 1450-word copy
     * is the bug. */
    {
        extern uint8_t competitionFileBuffer[];
        const uint8_t *cfb = competitionFileBuffer;
        const uint8_t *dfb = (const uint8_t *)&diyFileBufferCopy;
        SWOS_AUTOSKIP_LOG("EditCompSlot: paramA=%u paramB=%u paramC=%u slotIdx(buf)=%u",
                          (unsigned)paramA, (unsigned)paramB, (unsigned)paramC,
                          (unsigned)bufCopy->roundComplete);
        SWOS_AUTOSKIP_LOG("EditCompSlot: cfb[0x00..0x1F]=%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                          cfb[0x00], cfb[0x01], cfb[0x02], cfb[0x03], cfb[0x04], cfb[0x05], cfb[0x06], cfb[0x07],
                          cfb[0x08], cfb[0x09], cfb[0x0A], cfb[0x0B], cfb[0x0C], cfb[0x0D], cfb[0x0E], cfb[0x0F],
                          cfb[0x10], cfb[0x11], cfb[0x12], cfb[0x13], cfb[0x14], cfb[0x15], cfb[0x16], cfb[0x17],
                          cfb[0x18], cfb[0x19], cfb[0x1A], cfb[0x1B], cfb[0x1C], cfb[0x1D], cfb[0x1E], cfb[0x1F]);
        SWOS_AUTOSKIP_LOG("EditCompSlot: dfb[0x00..0x1F]=%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                          dfb[0x00], dfb[0x01], dfb[0x02], dfb[0x03], dfb[0x04], dfb[0x05], dfb[0x06], dfb[0x07],
                          dfb[0x08], dfb[0x09], dfb[0x0A], dfb[0x0B], dfb[0x0C], dfb[0x0D], dfb[0x0E], dfb[0x0F],
                          dfb[0x10], dfb[0x11], dfb[0x12], dfb[0x13], dfb[0x14], dfb[0x15], dfb[0x16], dfb[0x17],
                          dfb[0x18], dfb[0x19], dfb[0x1A], dfb[0x1B], dfb[0x1C], dfb[0x1D], dfb[0x1E], dfb[0x1F]);
        /* Slot 1 head (offset 0x76) and slot-1 tournament fields (offset 0x76+0x15A=0x1D0) */
        SWOS_AUTOSKIP_LOG("EditCompSlot: cfb[0x76..0x95]=%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                          cfb[0x76], cfb[0x77], cfb[0x78], cfb[0x79], cfb[0x7A], cfb[0x7B], cfb[0x7C], cfb[0x7D],
                          cfb[0x7E], cfb[0x7F], cfb[0x80], cfb[0x81], cfb[0x82], cfb[0x83], cfb[0x84], cfb[0x85],
                          cfb[0x86], cfb[0x87], cfb[0x88], cfb[0x89], cfb[0x8A], cfb[0x8B], cfb[0x8C], cfb[0x8D],
                          cfb[0x8E], cfb[0x8F], cfb[0x90], cfb[0x91], cfb[0x92], cfb[0x93], cfb[0x94], cfb[0x95]);
        SWOS_AUTOSKIP_LOG("EditCompSlot: dfb[0x76..0x95]=%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                          dfb[0x76], dfb[0x77], dfb[0x78], dfb[0x79], dfb[0x7A], dfb[0x7B], dfb[0x7C], dfb[0x7D],
                          dfb[0x7E], dfb[0x7F], dfb[0x80], dfb[0x81], dfb[0x82], dfb[0x83], dfb[0x84], dfb[0x85],
                          dfb[0x86], dfb[0x87], dfb[0x88], dfb[0x89], dfb[0x8A], dfb[0x8B], dfb[0x8C], dfb[0x8D],
                          dfb[0x8E], dfb[0x8F], dfb[0x90], dfb[0x91], dfb[0x92], dfb[0x93], dfb[0x94], dfb[0x95]);
        SWOS_AUTOSKIP_LOG("EditCompSlot: cfb[0x1D0..0x1EF]=%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                          cfb[0x1D0], cfb[0x1D1], cfb[0x1D2], cfb[0x1D3], cfb[0x1D4], cfb[0x1D5], cfb[0x1D6], cfb[0x1D7],
                          cfb[0x1D8], cfb[0x1D9], cfb[0x1DA], cfb[0x1DB], cfb[0x1DC], cfb[0x1DD], cfb[0x1DE], cfb[0x1DF],
                          cfb[0x1E0], cfb[0x1E1], cfb[0x1E2], cfb[0x1E3], cfb[0x1E4], cfb[0x1E5], cfb[0x1E6], cfb[0x1E7],
                          cfb[0x1E8], cfb[0x1E9], cfb[0x1EA], cfb[0x1EB], cfb[0x1EC], cfb[0x1ED], cfb[0x1EE], cfb[0x1EF]);
        SWOS_AUTOSKIP_LOG("EditCompSlot: dfb[0x1D0..0x1EF]=%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                          dfb[0x1D0], dfb[0x1D1], dfb[0x1D2], dfb[0x1D3], dfb[0x1D4], dfb[0x1D5], dfb[0x1D6], dfb[0x1D7],
                          dfb[0x1D8], dfb[0x1D9], dfb[0x1DA], dfb[0x1DB], dfb[0x1DC], dfb[0x1DD], dfb[0x1DE], dfb[0x1DF],
                          dfb[0x1E0], dfb[0x1E1], dfb[0x1E2], dfb[0x1E3], dfb[0x1E4], dfb[0x1E5], dfb[0x1E6], dfb[0x1E7],
                          dfb[0x1E8], dfb[0x1E9], dfb[0x1EA], dfb[0x1EB], dfb[0x1EC], dfb[0x1ED], dfb[0x1EE], dfb[0x1EF]);
    }
#endif

restart_loop:
    ;
    /* a3 = bufCopy + bufCopy[0x54] * 0x76 (current slot base) */
    uint16_t slotIdx = (uint16_t)bufCopy->roundComplete;
    uint8_t *slot = bufCopyRaw + (uint16_t)(slotIdx * 0x76);
    DIYTournamentSlot *s = (DIYTournamentSlot *)(slot + offsetof(DIYTournamentState, slots));

    /* Propagate tournament-identity fields from bufCopy (where
     * _InitDIYLeagueFromRecord wrote them) to comp before showing the
     * menu. The downstream menu init (_DIYLeagueTournamentOnRestore
     * for the play menu, the design-menu header) reads the title from
     * `comp + 4` (= diyCompetitionStart.base.competitionName), but
     * the PRESET / season entry points hit this function without
     * going through `_DIY_LoadCurrentCompetitionSlot` first, so the
     * name lives only in bufCopy and the title bar comes up blank
     * for World Cup PRESET / similar tournament-mode menus.
     *
     * The DIY-tournament play entry point at diy_competition.c:7699
     * does call `_DIY_LoadCurrentCompetitionSlot` which copies these
     * fields; doing the same copy here ensures every caller's title
     * is consistent. Bulk-copy `field_2` (offset 2..0x31, 48 bytes)
     * mirrors what LoadSlot does for the name + namePtr + compId +
     * surrounding identity bytes. */
    {
        uint8_t *bufRaw = (uint8_t *)bufCopy;
        for (uint16_t k = 2; k <= 0x31; k++)
            compRaw[k] = bufRaw[k];
    }

    /* Check if slot is cup (isLeagueFormat=0) — cup path takes this branch */
    if (s->isLeagueFormat == 0) {
        /* Cup path (slot not active / cup-type competition) */
        /* a4=comp, a5=comp */
        comp->hasGroups = 1;  /* DIY_COMPETITION_FIELD_3A+$E(a4) = 1 */

        /* Check if current round is last */
        uint16_t curRound = (uint16_t)bufCopy->roundComplete;
        if ((uint16_t)(curRound + 1) == RD16U(bufCopyRaw, 0x72)) {
            comp->hasGroups = 2;
        }

        /* Copy fields from bufCopy to comp */
        comp->roundIterator = bufCopy->hasGroups;  /* field_3A+$12(a4) = field_3A+$E(a2) */
        comp->slotStateB = bufCopy->slotStateB;  /* field_3A+6 */
        comp->slotStateA = bufCopy->slotStateA;  /* field_3A+4 */
        comp->slotStateC = bufCopy->slotStateC;  /* field_3A+8 */

        /* Call ShowDIYCupMenuWithParams */
        _ShowDIYCupMenuWithParams(editA, editB, editC);

        /* Copy results back from comp to bufCopy */
        bufCopy->hasGroups = comp->roundIterator;  /* field_3A+$E(a2) = field_3A+$12(a4) */
        bufCopy->resultCode = comp->resultCode;  /* field_3A+0 */
        bufCopy->matchStarted = comp->matchStarted;  /* field_3A+$A */
        bufCopy->careerTeamInvolved = comp->careerTeamInvolved;  /* field_3A+$C */
        bufCopy->slotStateB = comp->slotStateB;  /* field_3A+6 */
        bufCopy->slotStateA = comp->slotStateA;  /* field_3A+4 */
        bufCopy->slotStateC = comp->slotStateC;  /* field_3A+8 */

        /* sne — set byte if nonzero: if comp's finishedFlag != 0, set bufCopy's to 0xFF, else 0 */
        if (comp->competitionFinished != 0) {
            *(uint8_t *)&bufCopy->competitionFinished = 0xFF;
        } else {
            *(uint8_t *)&bufCopy->competitionFinished = 0x00;
        }

        /* Check game type */
        if (gameType == 4 || gameType == 3) {
            /* For career/contest: check if round is not yet started */
            if (comp->roundComplete != 0) {
                _DIY_UpdateCompetitionProgress();
            }
            return;
        }

        /* Non-career path: check hasGroups */
        if (comp->hasGroups < 0)
            return;

        _DIY_AdvanceToNextCompetitionSlot();

        if (bufCopy->currentRoundIndex != 0)  /* exit flag */
            return;
        goto restart_loop;
    }

    /* League/tournament path (slot IS active) */
    /* a5=comp */
    comp->hasGroups = 1;  /* DIY_COMPETITION_FIELD_3A+$E(a5) */

    /* Check if last round */
    uint16_t round = (uint16_t)bufCopy->roundComplete;
    if ((uint16_t)(round + 1) == (uint16_t)bufCopy->totalMatchesInRound) {
        comp->hasGroups = 2;
    }

    /* Copy fields from bufCopy to comp */
    comp->slotStateB = bufCopy->slotStateB;  /* field_3A+6 */
    comp->slotStateA = bufCopy->slotStateA;  /* field_3A+4 */
    comp->slotStateC = bufCopy->slotStateC;  /* field_3A+8 */

    /* Call ShowDiyLeaguesTournamentMenu */
    _ShowDiyLeaguesTournamentMenu(editA, editB, editC);

    /* Copy results back */
    bufCopy->resultCode = comp->resultCode;
    bufCopy->matchStarted = comp->matchStarted;
    bufCopy->careerTeamInvolved = comp->careerTeamInvolved;
    bufCopy->slotStateB = comp->slotStateB;
    bufCopy->slotStateA = comp->slotStateA;
    bufCopy->slotStateC = comp->slotStateC;

    /* seq — set byte if zero: if minSubstitutes (parent +0x60) == 0, set byte to 0xFF */
    if (diyCompetitionStart.minSubstitutes == 0) {
        *(uint8_t *)&bufCopy->competitionFinished = 0xFF;
    } else {
        *(uint8_t *)&bufCopy->competitionFinished = 0x00;
    }

    /* Check game type */
    if (gameType == 4 || gameType == 3) {
        return;
    }

    /* Non-career: check hasGroups */
    if (comp->hasGroups < 0)
        return;

    _DIY_AdvanceToNextCompetitionSlot();

    if (bufCopy->currentRoundIndex != 0)
        return;
    goto restart_loop;
}

/* ================================================================
 * DIY_ReorderTeamsBySelectionFlags (SWOS2.s line 32502)
 *
 * Reorders teams in diyCompetitionStart by selection flags:
 * swaps unselected teams (bit 1 clear in selection flags) to the
 * front of the team grid, maintaining order list consistency.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_ReorderTeamsBySelectionFlags(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a0s = &comp->base;
    uint8_t *teamGrid = comp->teamGridPairings;                    /* a3 */
    uint8_t *selFlags = comp->matchData.cup.cupRoundResults;       /* a2 */
    /* Three parallel uint16 score arrays held at:
     *   scoreTab1     (parent +0xCA): cupRoundResults+0x40 (knockout score region)
     *   aggregateBuf  (parent +0x14E): _bss_2E0AE base (second-leg agg score)
     *   penaltyBuf    (parent +0x1CE): leagueSectionState.phaseStartMonths (penalty score)
     */
    int16_t *scoreTab1    = (int16_t *)&comp->matchData.cup.cupRoundResults[0x40];
    int16_t *aggregateBuf = (int16_t *)&comp->_bss_2E0AE[0];
    int16_t *penaltyBuf   = (int16_t *)&comp->leagueSectionState.phaseStartMonths[0];
    int16_t d1 = 0;
    int16_t d2 = 0;

    while (d1 < a0s->totalMatchesInRound) {
        /* Check bit 1 of selection flags */
        if (selFlags[d1] & 0x02) {
            /* Bit 1 set — skip (already selected), just increment d2 */
            d1++;
            d2++;
            continue;
        }

        /* Swap entries at d1 and d2 in all parallel arrays */
        {
            int16_t tmp;
            int16_t *teamGridW = (int16_t *)teamGrid;

            tmp              = teamGridW[d1];
            teamGridW[d1]    = teamGridW[d2];
            teamGridW[d2]    = tmp;

            /* scoreTab1 at word offsets */
            tmp = scoreTab1[d1];
            scoreTab1[d1] = scoreTab1[d2];
            scoreTab1[d2] = tmp;

            /* aggregateBuf at word offsets */
            tmp = aggregateBuf[d1];
            aggregateBuf[d1] = aggregateBuf[d2];
            aggregateBuf[d2] = tmp;

            /* penaltyBuf at word offsets */
            tmp = penaltyBuf[d1];
            penaltyBuf[d1] = penaltyBuf[d2];
            penaltyBuf[d2] = tmp;
        }

        /* Selection flags (byte entries) */
        {
            uint8_t tmp = selFlags[d1];
            selFlags[d1] = selFlags[d2];
            selFlags[d2] = tmp;
        }

        /* Update order list if not has-groups mode */
        if (a0s->hasGroups == 0) {
            uint8_t *orderList = comp->teamShuffleOrder;  /* a4 */
            /* Find d1 in order list */
            uint8_t *p1 = orderList;
            while (*p1 != (uint8_t)d1)
                p1++;
            /* Find d2 in order list */
            uint8_t *p2 = orderList;
            while (*p2 != (uint8_t)d2)
                p2++;
            /* Swap */
            *p2 = (uint8_t)d1;
            *p1 = (uint8_t)d2;
        }

        d2++;
        d1++;
    }
}

/* ================================================================
 * DIY_UpdateCompetitionProgress (SWOS2.s line 32385)
 *
 * Advances competition progress after a round is complete:
 * - For cup rounds: reorders team grid, advances round counter
 * - For 2-leg cup rounds: swaps home/away and rotates scores
 * - For group stage with second legs: similar reorder
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_UpdateCompetitionProgress(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a0s = &comp->base;

#ifdef SWOS_DEBUG_AUTOSKIP
    SWOS_AUTOSKIP_LOG("[bug4-cup] UpdateCompetitionProgress ENTER compId=0x%04X "
                      "currentRoundIdx=%d/%d competitionFormat=%d roundFormat=%d "
                      "secondLegCounter=%d competitionFinished=%d",
                      (unsigned)*(uint16_t *)&a0s->compId,
                      (int)a0s->currentRoundIndex, (int)a0s->numRounds,
                      (int)comp->matchData.cup.competitionFormat,
                      (int)comp->matchData.cup.roundFormat,
                      (int)comp->bracketScratch.secondLegCounter,
                      (int)a0s->competitionFinished);
#endif

    /* Check finished flag — field_3A+$1C = offset 0x56 */
    if (a0s->competitionFinished != 0) {
#ifdef SWOS_DEBUG_AUTOSKIP
        SWOS_AUTOSKIP_LOG("[bug4-cup] UpdateCompetitionProgress EXIT early "
                          "(competitionFinished=1) compId=0x%04X",
                          (unsigned)*(uint16_t *)&a0s->compId);
#endif
        return;
    }

    /* Check competition flow:
     * If competitionFormat == 1 → cup knock-out path
     * Else if roundFormat == 2 → 2-leg path
     */
    if (comp->matchData.cup.competitionFormat == 1) {
        /* Fall through to knockout path */
    } else if (comp->matchData.cup.roundFormat == 2) {
        /* 2-leg cup path */
        goto two_leg_path;
    }

    /* Knockout / main path */
    if (comp->bracketScratch.secondLegCounter != 0) {
        /* Second leg path */
        goto second_leg_path;
    }

    {
        /* [bug23-cup] Before advance — log grid + key state. The bug:
         * after the semi-final, winners get scheduled for BOTH the
         * final and the 3rd-place play-off. We need to see what's in
         * the team grid + slot.doubleCount at this point. */
        uint8_t *grid = comp->teamGridPairings;
        uint8_t *flags = comp->matchData.cup.cupRoundResults;
        uint16_t mppr = (uint16_t)a0s->matchPairsPerRound;
        /* Tournament slot for current round (if any) */
        uint16_t slotIdx = (uint16_t)diyFileBufferCopy.header.roundComplete;
        DIYTournamentSlot *cur_s = &diyFileBufferCopy.slots[slotIdx];
        DIYTournamentSlot *nxt_s = cur_s + 1;
        SWOS_DBG("[bug23-cup] BEFORE: round=%d mppr=%u grid[0..7]="
                        "%02X %02X %02X %02X %02X %02X %02X %02X "
                        "flags[0..3]=%02X %02X %02X %02X "
                        "slot.doubleCount=cur=%u next=%u\n",
                (int)a0s->currentRoundIndex, (unsigned)mppr,
                grid[0], grid[1], grid[2], grid[3],
                grid[4], grid[5], grid[6], grid[7],
                flags[0], flags[1], flags[2], flags[3],
                (unsigned)cur_s->doubleCount, (unsigned)nxt_s->doubleCount);
    }

    /* Reorder team grid by order list */
    _DIY_ReorderTeamGridByOrderList();

    {
        uint8_t *grid = comp->teamGridPairings;
        SWOS_DBG("[bug23-cup] AFTER reorder: grid[0..7]="
                        "%02X %02X %02X %02X %02X %02X %02X %02X\n",
                grid[0], grid[1], grid[2], grid[3],
                grid[4], grid[5], grid[6], grid[7]);
    }

    /* Check if sub-round counter needs extra increment.
     * ASM: cmpi.w #2,DIY_COMPETITION_FIELD_2+$14A(a0) = offset $14C (round format type). */
    if (comp->matchData.cup.roundFormat != 2 && comp->bracketScratch.groupSectionFlag == 0) {
        int16_t roundIdx = a0s->currentRoundIndex;
        if (comp->matchData.cup.roundPenalties[roundIdx] != 1) {
            /* Add extra match counter */
            comp->bracketScratch.curMonthIdx += 1;
        }
    }

    /* Advance counters */
    comp->bracketScratch.curMonthIdx += 1;
    a0s->currentRoundIndex += 1;

    /* Halve round size */
    {
        int16_t half = (int16_t)((uint16_t)a0s->matchPairsPerRound >> 1);
        a0s->matchPairsPerRound = half;
        a0s->totalMatchesInRound = half;
        SWOS_DBG("[bug23-cup] HALVED: new mppr=%d new round=%d\n",
                (int)half, (int)a0s->currentRoundIndex);
    }

    /* Reset fixture index and set round type */
    comp->matchData.cup.currentMatchIndex = 0;
    {
        int16_t rndIdx = a0s->currentRoundIndex;
        comp->matchData.cup.roundFormat = comp->matchData.cup.roundLegs[rndIdx];
    }

    /* If only 1 team left and round type != 2, clear round type */
    if (a0s->matchPairsPerRound == 1 && comp->matchData.cup.roundFormat != 2) {
        comp->matchData.cup.roundFormat = 0;
    }

    /* Clear group/second-leg flags */
    comp->bracketScratch.groupSectionFlag = 0;
    comp->bracketScratch.secondLegCounter = 0;
    a0s->roundComplete = 0;
    comp->matchData.cup.competitionFormat = 0;

    /* Clear selection flags: fill 0x8A..0x8A+0x1F with 0 (longword stores to avoid memset) */
    {
        uint32_t *p = (uint32_t *)comp->matchData.cup.cupRoundResults;
        p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0;
        p[4] = 0; p[5] = 0; p[6] = 0; p[7] = 0;
    }

    goto final_check;

two_leg_path:
    /* 2-leg round: advance sub-round and fixture, rotate scores */
    comp->bracketScratch.curMonthIdx += 1;
    comp->matchData.cup.competitionFormat += 1;
    comp->matchData.cup.currentMatchIndex = 0;
    a0s->roundComplete = 0;

    {
        /* a1: scoreTab1 (parent +0xCA = cupRoundResults+0x40 score region)
         * a2: BSS_00_2E0AE — second-leg score buffer, kept raw pending naming */
        uint8_t *scores  = (uint8_t *)&comp->matchData.cup.cupRoundResults[0x40];
        /* parent +0x14E = _bss_2E0AE start; use typed field. */
        uint8_t *scores2 = comp->_bss_2E0AE;
        uint8_t *grid    = comp->teamGridPairings;
        uint8_t *flags   = comp->matchData.cup.cupRoundResults;
        int16_t count    = a0s->totalMatchesInRound - 1;

        uint16_t *gridW    = (uint16_t *)grid;
        uint16_t *scoresW  = (uint16_t *)scores;
        uint16_t *scores2W = (uint16_t *)scores2;
        for (int16_t i = 0; i <= count; i++) {
            flags[i] = 0x20;  /* ' ' */
            /* ror.w #8: swap high/low bytes of the team-grid word. */
            uint16_t gridVal = gridW[i];
            gridW[i] = (uint16_t)((gridVal >> 8) | (gridVal << 8));
            /* same for the score word (read source, write swapped to
             * scores2 sibling). */
            uint16_t sVal = scoresW[i];
            scores2W[i] = (uint16_t)((sVal >> 8) | (sVal << 8));
        }
    }
    goto final_check;

second_leg_path:
    /* Group stage second leg */
    _DIY_ReorderTeamsBySelectionFlags();

    /* Check counters for extra match increment */
    if (comp->bracketScratch.groupSectionFlag == 0 && comp->matchData.cup.roundFormat != 2) {
        comp->bracketScratch.curMonthIdx += 1;
    }

    /* Set teams per round from second leg count */
    a0s->totalMatchesInRound = (int16_t)comp->bracketScratch.secondLegCounter;

    /* If round type was 2, clear it */
    if (comp->matchData.cup.roundFormat == 2) {
        comp->matchData.cup.roundFormat = 0;
    }

    comp->matchData.cup.competitionFormat = 0;
    comp->bracketScratch.groupSectionFlag += 1;
    comp->matchData.cup.currentMatchIndex = 0;
    a0s->roundComplete = 0;

    {
        uint8_t *grid = comp->teamGridPairings;
        int16_t count = a0s->totalMatchesInRound;
        uint8_t d1val = 0;

        if (comp->bracketScratch.groupSectionFlag != 0) {
            d1val |= 0x40;  /* bset #6 */
        }

        uint8_t  *flags = comp->matchData.cup.cupRoundResults;
        uint16_t *gridW = (uint16_t *)grid;
        for (int16_t i = 0; i < count; i++) {
            flags[i] = d1val;
            /* ror.w #8 on grid */
            uint16_t gridVal = gridW[i];
            gridW[i] = (uint16_t)((gridVal >> 8) | (gridVal << 8));
        }
    }

final_check:
    /* Check if career mode and not standalone — jump to career update */
    if (careerLoadedFlag != 0)
        return;
    if (gameType == 4 || gameType == 3) {
        /* These game types need additional processing via
         * DIY_UpdateCompetitionProgressOrAdvanceSlot loc_12D36A path.
         * This is handled by the caller chain in career mode. */
        _DIY_RecordCupBracketProgress();
        return;
    }
    /* For non-career modes, return */
}

/* Forward declare internal helper */
static void diy_ResetCompetitionStatsAndProgress(void);

/* ================================================================
 * DIY_RecordCupBracketProgress (SWOS2.s loc_12D36A within
 *   DIY_UpdateCompetitionProgressOrAdvanceSlot)
 *
 * Processes cup bracket results: finds team selection key cache entry,
 * iterates through fixture pairs, records results for non-human teams.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_RecordCupBracketProgress(void)
{
    int16_t d7 = 0;

outer_loop:
    ;
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a2s = &comp->base;
    uint8_t *a2 = (uint8_t *)a2s;

    /* Check promotion count (0x56) — if nonzero, done */
    if (a2s->competitionFinished != 0)
        return;
    /* Check sub-round counter (0x14A) */
    if (comp->matchData.cup.competitionFormat == 1)
        return;

    /* Get round index and competition ID */
    int16_t roundIdx = a2s->currentRoundIndex;
    if (a2s->hasGroups != 0) {
        /* Use diyFileBufferCopy round index */
        roundIdx = diyFileBufferCopy.header.roundComplete;
    }
    /* Read low byte of (compId, slotId) word at +0x2C — same fix pattern
     * as _DIY_RecordLeagueFixtureResultAndAdvanceProgress: a2s->slotId
     * on SDL LE is always 0 because the slot index got written into the
     * compId byte instead. *(uint16_t *) gives the same value on both
     * hosts. Without this, cup-bracket progress for CL/UEFA/CWC reads
     * the wrong cache entry on SDL. */
    uint8_t compId = (uint8_t)*(uint16_t *)&a2s->compId;

    /* Search teamSelectionKeyCache for matching entry */
    uint8_t *cacheEntry = teamSelectionKeyCache;
    int16_t cacheCount = teamSelectionKeyCacheCount - 1;

    while (cacheCount >= 0) {
        if (cacheEntry[0] == compId) {
            if (cacheEntry[7] == (uint8_t)roundIdx)
                goto found_entry;
        }
        /* Advance by stride at offset 2 */
        cacheEntry += *(uint16_t *)(cacheEntry + 2);
        cacheCount--;
    }
    /* Not found — panic */
    panicReasonCode = 0x6666;
    _PanicHaltLoop();
    return;  /* unreachable */

found_entry:
    ;
    /* Check d7 state for second pass */
    if (d7 != 0) {
        if (cacheEntry[8] != 2)
            goto try_next;
        goto process_fixtures;
    }

    /* First pass checks */
    if (cacheEntry[8] == 1)
        goto process_fixtures;

    if (comp->bracketScratch.groupSectionFlag == 0) {
        if (cacheEntry[8] == 3)
            goto try_next;
    } else {
        if (cacheEntry[8] != 3)
            goto try_next;
        cacheEntry[4] = 0;
    }

process_fixtures:
    ;
    uint8_t *a5 = cacheEntry;  /* result write pointer */
    cacheEntry[5] = 0;
    cacheEntry[6] = 0;

    uint8_t *teamGrid = comp->teamGridPairings;  /* a1 points here */
    int16_t numTeams = a2s->totalMatchesInRound;

    for (int16_t i = 0; i < numTeams; i++) {
        /* Read two team indices from grid */
        int8_t idx1 = (int8_t)*teamGrid++;
        uint8_t *team1 = diy_resolve_team(comp->bracketScratch.teamOffsetTable[(int16_t)idx1]);
        uint16_t teamId1 = *(uint16_t *)team1;

        int8_t idx2 = (int8_t)*teamGrid++;
        uint8_t *team2 = diy_resolve_team(comp->bracketScratch.teamOffsetTable[(int16_t)idx2]);
        uint16_t teamId2 = *(uint16_t *)team2;

        /* If d7 > 0 (second pass), swap teams */
        if (cacheEntry[8] == 2) {
            uint16_t tmp16 = teamId1;
            teamId1 = teamId2;
            teamId2 = tmp16;
            uint8_t *tmpP = team1;
            team1 = team2;
            team2 = tmpP;
        }

        /* Check if team1 is human-controlled (category == 1) */
        if (team1[4] == 1) {
            /* team2 must also be human for pair to be skipped */
            if (team2[4] == 1)
                goto next_fixture;

            /* Check competition name for FRIENDLY or playEachTeam */
            if (comp->matchData.cup.roundFormat == 0) {
                a5[0x0A] = 0;
            } else {
                if (a2s->namePtr == aFriendly ||
                    a2s->playEachTeam == 1) {
                    /* FRIENDLY or playEachTeam == 1 */
                    a5[0x0A] = 2;
                } else {
                    a5[0x0A] = 0;
                }
            }
        } else {
            /* Non-human team1 */
            if (comp->matchData.cup.roundFormat == 0) {
                a5[0x0A] = 0;
            } else {
                if (a2s->namePtr == aFriendly ||
                    a2s->playEachTeam == 1) {
                    a5[0x0A] = 1;
                } else {
                    a5[0x0A] = 0;
                }
            }
        }

        cacheEntry[5]++;
        cacheEntry[6]++;
        a5[0x0B] = 0;
        *(uint16_t *)(a5 + 0x0C) = teamId1;  /* asm: move.w (a3),$C(a5) = first team */
        *(uint16_t *)(a5 + 0x0E) = teamId2;  /* asm: move.w (a0),$E(a5) = second team */
        a5 += 0x0C;

next_fixture:
        ;
    }

    /* Check if any fixtures recorded */
    if (cacheEntry[5] == 0) {
        if (a2s->roundIterator != 0) {
            cacheEntry[6] = 0xFF;
        }
    }

    d7 = 1;
    if (cacheEntry[8] == 1)
        goto outer_loop;

    return;

try_next:
    cacheEntry += *(uint16_t *)(cacheEntry + 2);
    cacheCount--;
    if (cacheCount >= 0) {
        if (cacheEntry[0] == compId) {
            if (cacheEntry[7] == (uint8_t)roundIdx)
                goto found_entry;
        }
        goto try_next;
    }
    panicReasonCode = 0x6666;
    _PanicHaltLoop();
}

/* ================================================================
 * DIY_PostLoadCompetitionDispatch (SWOS2.s loc_12D4B0)
 *
 * Career-mode post-load processing for hybrid competition slots.
 * Checks diyFileBufferCopy exitFlag, computes current slot from
 * currentRound, then dispatches to cup bracket or league+reset path.
 * ================================================================ */
void _DIY_PostLoadCompetitionDispatch(void)
{
    DIYCompetition *fbCopy = &diyFileBufferCopy.header;
    if (fbCopy->currentRoundIndex != 0)
        return;

    uint16_t currentRound = fbCopy->roundComplete;
    DIYTournamentSlot *slot = &diyFileBufferCopy.slots[currentRound];

    if (slot->isLeagueFormat == 0) {
        /* Cup path */
        _DIY_RecordCupBracketProgress();
    } else {
        /* League path then reset stats */
        _DIY_RecordLeagueFixtureResultAndAdvanceProgress();
        diy_ResetCompetitionStatsAndProgress();
    }
}

/* ================================================================
 * DIY_UpdateCompetitionProgressOrAdvanceSlot (SWOS2.s line 35425)
 *
 * Dispatches by competition type:
 *   0 = league → DIY_RecordLeagueFixtureResultAndAdvanceProgress
 *   1 = cup → loc_12D36A (cup bracket progress)
 *   else → hybrid (type=2 mixed)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_UpdateCompetitionProgressOrAdvanceSlot(uint8_t *blk)
{
    int16_t type = *(int16_t *)blk;
#ifdef SWOS_DEBUG_AUTOSKIP
    {
        DIYCompetitionEx *bex = &diyCompetitionStart;
        SWOS_AUTOSKIP_LOG("[bug20-update] UpdateCompetitionProgressOrAdvanceSlot ENTER "
                          "blk=%p type=%d compId=0x%04X minSubstitutes=%u "
                          "leagueMatchProgress=%u leagueCurrentSlot=%u "
                          "totalMatchesPerSectionLive=%d hasGroups=%d",
                          (void *)blk, (int)type,
                          (unsigned)*(uint16_t *)&bex->base.compId,
                          (unsigned)bex->minSubstitutes,
                          (unsigned)bex->leagueMatchProgress,
                          (unsigned)bex->leagueCurrentSlot,
                          (int)bex->matchData.league.totalMatchesPerSectionLive,
                          (int)bex->base.hasGroups);
    }
#endif

    if (type == 0) {
        _DIY_RecordLeagueFixtureResultAndAdvanceProgress();
    } else if (type == 1) {
        _DIY_RecordCupBracketProgress();
    } else {
        /* type == 2: hybrid */
        DIYCompetition *fbCopyS = &diyFileBufferCopy.header;
        uint8_t *fbCopy = (uint8_t *)&diyFileBufferCopy;
        if (fbCopyS->currentRoundIndex != 0)
            return;
        uint16_t pitchType2 = (uint16_t)fbCopyS->roundComplete;
        uint8_t *slot = fbCopy + (uint16_t)(pitchType2 * 0x76);
        DIYTournamentSlot *s = (DIYTournamentSlot *)(slot + offsetof(DIYTournamentState, slots));
        if (s->isLeagueFormat == 0) {
            /* Cup path */
            _DIY_RecordCupBracketProgress();
        } else {
            /* League path then reset */
            _DIY_RecordLeagueFixtureResultAndAdvanceProgress();
            diy_ResetCompetitionStatsAndProgress();
        }
    }
}

/* ================================================================
 * DIY_RecordLeagueFixtureResultAndAdvanceProgress (SWOS2.s line 35434)
 *
 * Processes recorded league fixture results via team selection key cache.
 * Advances league progress after each match.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_RecordLeagueFixtureResultAndAdvanceProgress(void)
{
    DIYCompetitionEx *a6Ex = &diyCompetitionStart;
    DIYCompetition *a6s = &a6Ex->base;
    uint8_t *a6 = (uint8_t *)a6s;

    /* [bug20-record] Always-on writer entry trace during bring-up.
     * First-N limited so it doesn't flood. Pair with [bug20-fixture]
     * (getCurrentFixtureTeamPtrs) to see fixture lookup chain. */
    {
        static int s_recCalls = 0;
        if (s_recCalls < 20) {
            s_recCalls++;
            SWOS_DBG("[bug20-record] #%d ENTER compId=0x%04X "
                            "minSubs=%u playEachTeam=%u "
                            "matchPairsPerRound=%d totalMatchesInRound=%d "
                            "sectionMatchesProgress=%u halfGroupsCounter=%u "
                            "matchesPerHalf=%u leagueCurrentSlot=%u "
                            "leagueMatchProgress=%u "
                            "totalMatchesPerSectionLive=%d "
                            "subRoundCounter=%d numTeams=%d "
                            "leagueTotalMatches=%u\n",
                            s_recCalls,
                            (unsigned)*(uint16_t *)&a6s->compId,
                            (unsigned)a6Ex->minSubstitutes,
                            (unsigned)a6s->playEachTeam,
                            (int)a6s->matchPairsPerRound,
                            (int)a6s->totalMatchesInRound,
                            (unsigned)a6Ex->leagueSectionState.sectionMatchesProgress,
                            (unsigned)a6Ex->leagueSectionState.halfGroupsCounter,
                            (unsigned)a6Ex->leagueSectionState.matchesPerHalf,
                            (unsigned)a6Ex->leagueCurrentSlot,
                            (unsigned)a6Ex->leagueMatchProgress,
                            (int)a6Ex->matchData.league.totalMatchesPerSectionLive,
                            (int)a6s->subRoundCounter,
                            (int)a6s->numTeams,
                            (unsigned)a6Ex->leagueTotalMatches);
            SWOS_DBG_FLUSH();
        }
    }

    /* Check if there are results to process (minSubstitutes repurposed as section counter) */
    if (a6Ex->minSubstitutes == 0)
        return;

    /* Find matching cache entry */
    uint8_t *cache = teamSelectionKeyCache;
    int16_t cacheCount = teamSelectionKeyCacheCount - 1;
    int16_t curRound = diyFileBufferCopy.header.roundComplete;
    /* Asm: move.w $2C(a6),d0 / cmp.b (a4),d0 — read the (compId, slotId)
     * word at +0x2C, then compare its low byte to the cache header's
     * teamSlot byte. On Amiga BE the word's low byte is the slotId
     * field at +0x2D (which the cache builder also reads as
     * *(uint16_t *)&compId via the same low-byte-of-word path). On SDL
     * LE the word's low byte is the compId field at +0x2C — different
     * memory location, but the same VALUE because *(uint16_t *) on LE
     * gives the same number that was written. The previous version
     * read `a6s->slotId` directly which on SDL LE is always zero (slot
     * value got written into compId byte instead), so cache lookups
     * for any slot > 0 silently matched slot 0's cache entries —
     * causing Juventus's CL fixtures to overwrite Serie A's. */
    uint8_t compId = (uint8_t)*(uint16_t *)&a6s->compId;

    while (cacheCount >= 0) {
        if (cache[0] == compId) {
            if (a6s->hasGroups == 0 || cache[7] == (uint8_t)curRound)
                goto found;
        }
        cache += *(uint16_t *)(cache + 2);
        cacheCount--;
    }
    panicReasonCode = 0x7777;
    _PanicHaltLoop();
    return;

found:
    ;
    uint8_t *resultPtr = cache;  /* a3 */

process_next:
    /* Get current fixture team pointers */
    _DIY_GetCurrentFixtureTeamPtrs();
    extern uint8_t *fixtureTeam1CompPtr;
    extern uint8_t *fixtureTeam2CompPtr;
    uint8_t *team1Comp = fixtureTeam1CompPtr;
    uint8_t *team2Comp = fixtureTeam2CompPtr;

    /* Advance league progress */
    _DIY_AdvanceLeagueProgressAfterMatch(team1Comp, team2Comp);

    /* Get selected team pointers from DIYTeamStats teamSelOffset (+0x2B4) */
    uint8_t *sel1 = diy_resolve_team(((DIYTeamStats *)(team1Comp + DIY_STANDINGS_OFFSET))->teamSelOffset);
    uint8_t *sel2 = diy_resolve_team(((DIYTeamStats *)(team2Comp + DIY_STANDINGS_OFFSET))->teamSelOffset);

    /* [bug20-fixture] Always-on per-iteration trace. First-N limited. */
    {
        static int s_fxCalls = 0;
        if (s_fxCalls < 30) {
            s_fxCalls++;
            uint8_t *a0 = (uint8_t *)a6s;
            SWOS_DBG("[bug20-fixture] #%d compId=0x%04X "
                            "subRound=%d sectionProgress=%u "
                            "totalMatchesInRound=%d matchPairsPerRound=%d "
                            "team1Off=0x%lx team2Off=0x%lx "
                            "t1=0x%04X ctrl1=%u t2=0x%04X ctrl2=%u "
                            "resultPtrOff=0x%lx\n",
                            s_fxCalls,
                            (unsigned)*(uint16_t *)&a6s->compId,
                            (int)a6s->subRoundCounter,
                            (unsigned)a6Ex->leagueSectionState.sectionMatchesProgress,
                            (int)a6s->totalMatchesInRound,
                            (int)a6s->matchPairsPerRound,
                            (unsigned long)(team1Comp - a0),
                            (unsigned long)(team2Comp - a0),
                            (unsigned)BE_READ_U16(sel1), (unsigned)sel1[4],
                            (unsigned)BE_READ_U16(sel2), (unsigned)sel2[4],
                            (unsigned long)(resultPtr - cache));
            SWOS_DBG_FLUSH();
        }
    }

    /* Check if team1 is human-controlled (category == 1) and write
     * the home/away marker to fixture[+0x0A].
     *
     * Asm (SWOS2.s line 59780-59800) logic:
     *   - FRIENDLY                   → always set marker (1 or 2)
     *   - else playEachTeam == 1     → clear marker (single-leg league:
     *                                  no H/A indicator)
     *   - else                       → set marker
     *
     * The previous SDL translation had the playEachTeam check inverted
     * (`== 1` instead of `!= 1`), which CLEARED the marker for normal
     * league competitions where playEachTeam is 2 (home + away). That's
     * why the career main screen showed (H)/(A) only on Coppa /
     * European Cup rows and never on Serie A — tester report. */
    if (sel1[4] == 1) {
        if (sel2[4] == 1) {
            /* Both human — skip recording */
#ifdef SWOS_DEBUG_AUTOSKIP
            SWOS_AUTOSKIP_LOG("[bug20-fixture]   SKIP both-human compId=0x%04X "
                              "t1=0x%04X t2=0x%04X",
                              (unsigned)*(uint16_t *)&a6s->compId,
                              (unsigned)BE_READ_U16(sel1),
                              (unsigned)BE_READ_U16(sel2));
#endif
            goto check_continue;
        }
        if (a6s->namePtr == aFriendly || a6s->playEachTeam != 1) {
            resultPtr[0x0A] = 2;
        } else {
            resultPtr[0x0A] = 0;
        }
    } else {
        if (a6s->namePtr == aFriendly || a6s->playEachTeam != 1) {
            resultPtr[0x0A] = 1;
        } else {
            resultPtr[0x0A] = 0;
        }
    }

    cache[5]++;
    cache[6]++;
    resultPtr[0x0B] = 0;
    /* Asm: `move.w (a1),$C(a3)` / `move.w (a2),$E(a3)` — reads each
     * team's leading `{country, ordinal}` bytes as a single BE uint16
     * and stores it back to the cache. CareerGameListCommon later
     * reads these slots with BE_READ_U16. Preserve the BE byte order
     * across the host so the read decodes to the same `(country<<8)|ord`
     * the original Amiga produced. */
    BE_WRITE_U16(resultPtr + 0x0C, BE_READ_U16(sel1));
    BE_WRITE_U16(resultPtr + 0x0E, BE_READ_U16(sel2));
#ifdef SWOS_DEBUG_AUTOSKIP
    SWOS_AUTOSKIP_LOG("[bug20-fixture]   WROTE compId=0x%04X resultOff=0x%lx "
                      "t1=0x%04X t2=0x%04X marker=%u",
                      (unsigned)*(uint16_t *)&a6s->compId,
                      (unsigned long)(resultPtr - cache),
                      (unsigned)BE_READ_U16(sel1),
                      (unsigned)BE_READ_U16(sel2),
                      (unsigned)resultPtr[0x0A]);
#endif
    resultPtr += 0x0C;

check_continue:
    /* Check if league has more fixtures in this round */
    if (a6Ex->leagueCurrentSlot == 0)
        goto process_next;
    if (a6Ex->minSubstitutes != 0) {
        /* Asm (SWOS2.s loc_12D2AA): `adda.w 2(a4),a4; dbf d7,loc_12D29A`
         * — walks subsequent cache entries one-by-one until a matching
         * entry is found, panicking 0x7777 only if all entries are
         * exhausted with no match. Our previous SDL translation only
         * advanced ONE entry and exited silently on miss, which broke
         * multi-round processing for hasGroups (Champions Cup group
         * stage): after round 0's cache entries were exhausted, the
         * walker stopped at round 1's first entry (cache[7]!=curRound)
         * and exited, leaving rounds 2+ unprocessed. Tester report:
         * only the first CL group match showed on the career main
         * screen — schedule "jumps from the match before to the match
         * after" because rounds 2-6 had team IDs zero. Loop until match
         * found or cache exhausted, matching the asm. */
        for (;;) {
            cache += *(uint16_t *)(cache + 2);
            cacheCount--;
            if (cacheCount < 0)
                return;  /* Asm panics here; in our path the wider career
                          * flow continues, so exit silently. */
            if (cache[0] != compId)
                continue;
            if (a6s->hasGroups != 0 && cache[7] != (uint8_t)curRound)
                continue;
            resultPtr = cache;
            goto process_next;
        }
    }
}

/* ================================================================
 * diy_ResetCompetitionStatsAndProgress (static, from SWOS2.s loc_12AB5C)
 *
 * Resets all team statistics in diyCompetitionStart for a new round.
 * ================================================================ */
static void diy_ResetCompetitionStatsAndProgress(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a2s = &comp->base;
    uint8_t *a2 = (uint8_t *)a2s;
    a2s->viewRandomPhase = (uint8_t)a2s->startMonth;
    comp->matchesPlayedCount = 0;
    a2s->subRoundCounter = 0;
    comp->leagueSectionState.sectionMatchesProgress = 0;
    comp->leagueSectionState.halfGroupsCounter = 0;
    comp->leagueMatchProgress = 0;
    comp->leagueCurrentSlot = 0;
    comp->matchData.league.totalMatchesPerSectionLive = comp->matchData.league.totalMatchesPerSection;
    comp->minSubstitutes = (uint16_t)a2s->playEachTeam;

    /* Reset per-team stats: 64 entries (DIYTeamStats rows from base+0x2B4) */
    {
        DIYTeamStats *row = (DIYTeamStats *)(a2 + DIY_STANDINGS_OFFSET);
        for (int16_t i = 0; i <= 0x3F; i++) {
            row->gamesPlayed  = 0;
            row->wins         = 0;
            row->draws        = 0;
            row->losses       = 0;
            row->goalsFor     = 0;
            row->goalsAgainst = 0;
            row->points       = 0;
            row++;
        }
    }
}

/* ================================================================
 * _DIY_RebuildCompetitionAndTeamSelectionCaches (SWOS2.s line 35046)
 *
 * Rebuilds competition team selection caches from diyCompetitionContext.
 * For each competition entry, either copies from competitionFileBuffer
 * or auto-advances international fixtures. Then rebuilds key cache.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_RebuildCompetitionAndTeamSelectionCaches(void)
{
    if (diyCompetitionPtr == 0)
        return;

    uint32_t *tablePtr = diyCompetitionTablePtr;
    /* competitionTeamIdScratchList4 is a separate small scratch buffer (asm data label). */
    extern uint16_t competitionTeamIdScratchList4[];
    uint16_t *a5 = competitionTeamIdScratchList4;

    DIYCompetition *ctxS = (DIYCompetition *)diyCompetitionContext;
    uint8_t *ctx = (uint8_t *)ctxS;
    int16_t count = ctxS->numTeams - 1;

    for (int16_t i = count; i >= 0; i--) {
        /* `tablePtr` walks {ptr, int} pairs at 4-byte stride. On 64-bit
         * tablePtr[0] is a 32-bit value we need to interpret as a
         * pointer — double-cast through uintptr_t to silence the
         * Wint-to-pointer-cast warning while preserving the zero-
         * extended low-32 address (matches the equality-compare against
         * currentCompetitionPtr, which has the same source/encoding). */
        uint8_t *compPtr = (uint8_t *)(uintptr_t)tablePtr[0];
        uint32_t divIndex = tablePtr[1];
        tablePtr += 2;

        if (compPtr == currentCompetitionPtr &&
            (uint16_t)divIndex == currentDivisionIndex) {
            /* Current competition — copy from competitionFileBuffer */
            /* Career_CopyCompetitionTeamByIndex inline:
             * reads divIndex from tablePtr stream, looks up team ID at
             * competition+0x1AE + idx*2 */
            uint32_t idx = *(uint32_t *)((uint8_t *)tablePtr - 4);  /* re-read d0 */
            (void)idx;
            /* +0x1AE is inside _bss_2E0AE in DIYCompetitionEx (BSS_00_2E0AE
             * region holds an array of qualified team IDs in
             * competitionFileBuffer's view). divIndex indexes a uint16
             * array starting at offset 0x1AE - 0x14E = 0x60 inside
             * the _bss_2E0AE block. */
            DIYCompetitionEx *destEx = (DIYCompetitionEx *)competitionFileBuffer;
            uint16_t teamId = *(uint16_t *)&destEx->_bss_2E0AE[0x60 + (uint16_t)divIndex * 2];
            *a5++ = teamId;
        } else {
            /* Not current — auto-advance international fixtures */
            internationalAutoAdvanceFlag = 1;
            _SaveSelectedTeams();
            _Career_AutoAdvanceInternationalCompetitionFixtures(compPtr, (uint16_t)divIndex);
            _RestoreSelectedTeams();
            internationalAutoAdvanceFlag = 0;

            /* Copy from diyCompetitionStart's _bss_2E0AE qualified-teams array */
            uint16_t teamId = *(uint16_t *)&diyCompetitionStart._bss_2E0AE[0x60 + (uint16_t)divIndex * 2];
            *a5++ = teamId;
        }
    }

    /* Append teams to selection */
    a5 = competitionTeamIdScratchList4;
    uint16_t *teamIdDest = ((DIYCompetitionEx *)ctx)->cupTeamIds;
    int16_t teamCount = ctxS->numTeams;
    _AppendTeamsToSelectionFromTmdOrTeamFile(teamIdDest, a5, teamCount);

    /* Load competition state, build cache, advance, store */
    _Career_LoadCompetitionSlotStateFromBuffer(ctx);

    extern void _DIY_BuildTeamSelectionPointerCache(void);
    _DIY_BuildTeamSelectionPointerCache();

    _DIY_UpdateCompetitionProgressOrAdvanceSlot((uint8_t *)&diyCompetitionStart);

    _Career_StoreCompetitionSlotStateToBuffer(ctx);

    /* Set updated flag and copy state */
    competitionSlotUpdatedFlag = 1;
    diyCompSlotStateA = primaryCompSlotStateA;
    diyCompSlotStateB = primaryCompSlotStateB;
    diyCompSlotStateC = primaryCompSlotStateC;
}

/* ================================================================
 * Chunk 97 — DIY Competition Setup Functions
 * ================================================================ */

/* ASM data tables in SWOS2.s */
extern uint8_t diyShuffleOrder8Teams[];    /* 8 bytes: {1,4,5,0,2,7,6,3} */
extern uint8_t diyShuffleOrder4Teams[];    /* 4 bytes: {0,3,1,2} */
extern uint8_t diyTournamentShuffleOrder8[]; /* 8 bytes: {6,1,5,4,2,3,7,0} */
extern uint8_t diyLeagueTeamShuffleMap[];   /* 16 bytes: signed index map */
extern uint8_t diyGroupPatternLookup[];     /* 76 bytes: bitmask→division ordering */
extern uint8_t diyTeamShuffleScratch[];     /* 64 bytes: scratch buffer */

/* sub_129960 in asm — its machine code bytes {0x02,0x01,0x00,0x00} are used
 * as a 4-byte shuffle order table for 4-team comp-specific lists (3-group case).
 * We define the actual data here. */
static const uint8_t diyCompSpecificShuffle4[4] = {0x02, 0x01, 0x00, 0x00};

/* ================================================================
 * DIY_BuildCompSpecificTeamShuffle — reorder teams for comp-specific
 * bracket seeding (shuffle variant used by BuildTeamGridAndIndexMaps)
 *
 * asm: a2=diy buffer. Writes to diyTeamShuffleScratch.
 * ================================================================ */
static void buildCompSpecificTeamShuffle(uint8_t *diy)
{
    DIYCompetition *diyS = (DIYCompetition *)diy;
    uint16_t field54 = (uint16_t)diyS->roundComplete;
    uint8_t *src = ((DIYTournamentState *)diy)->teamIndexOrder;
    uint8_t *dst = diyTeamShuffleScratch;

    if (field54 == 0) {
        /* Case 0: copy 16 teams straight */
        for (int16_t i = 15; i >= 0; i--)
            *dst++ = *src++;
    } else if (field54 == 1) {
        /* Case 1: 8 teams, use diyShuffleOrder8Teams */
        for (int16_t i = 0; i < 8; i++) {
            uint16_t idx = (uint8_t)diyShuffleOrder8Teams[i];
            *dst++ = src[idx];
        }
    } else if (field54 == 2) {
        /* Case 2: 4 teams, use diyShuffleOrder4Teams */
        for (int16_t i = 0; i < 4; i++) {
            uint16_t idx = (uint8_t)diyShuffleOrder4Teams[i];
            *dst++ = src[idx];
        }
    } else {
        /* Case 3+: 2 teams, copy first word */
        *(uint16_t *)dst = *(uint16_t *)src;
    }
}

/* ================================================================
 * DIY_BuildCompSpecificTeamList — reorder teams for comp-specific
 * bracket seeding (list variant used by ShuffleTeamsAndBuildCompTeamList)
 *
 * asm: a2=diy buffer. Writes to diyTeamShuffleScratch.
 * ================================================================ */
static void buildCompSpecificTeamList(uint8_t *diy)
{
    DIYCompetition *diyS = (DIYCompetition *)diy;
    uint16_t field54 = (uint16_t)diyS->roundComplete;
    uint8_t *src = ((DIYTournamentState *)diy)->teamIndexOrder;
    uint8_t *dst = diyTeamShuffleScratch;
#ifdef SWOS_DEBUG_AUTOSKIP
    SWOS_AUTOSKIP_LOG("buildCompSpecificTeamList: ENTER field54=%u compType=%d numTeams=%d "
                      "compId=%u slotId=%u namePtr=0x%lx",
                      (unsigned)field54, (int)diyS->competitionType,
                      (int)diyS->numTeams, (unsigned)diyS->compId,
                      (unsigned)diyS->slotId, (unsigned long)diyS->namePtr);
    SWOS_AUTOSKIP_LOG("buildCompSpecificTeamList: src[0..15]=%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                      src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7],
                      src[8], src[9], src[10], src[11], src[12], src[13], src[14], src[15]);
    SWOS_AUTOSKIP_LOG("buildCompSpecificTeamList: divTbl(0x66)[0..15]=%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                      diy[0x66], diy[0x67], diy[0x68], diy[0x69],
                      diy[0x6A], diy[0x6B], diy[0x6C], diy[0x6D],
                      diy[0x6E], diy[0x6F], diy[0x70], diy[0x71],
                      diy[0x72], diy[0x73], diy[0x74], diy[0x75]);
    SWOS_AUTOSKIP_LOG("buildCompSpecificTeamList: groupMap(0x11A)[0..15]=%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                      diy[0x11A], diy[0x11B], diy[0x11C], diy[0x11D],
                      diy[0x11E], diy[0x11F], diy[0x120], diy[0x121],
                      diy[0x122], diy[0x123], diy[0x124], diy[0x125],
                      diy[0x126], diy[0x127], diy[0x128], diy[0x129]);
#endif

    if (field54 == 0) {
        /* Case 0: copy 24 teams straight */
        for (int16_t i = 23; i >= 0; i--)
            *dst++ = *src++;
    } else if (field54 == 1) {
        /* Case 1: 16-team league pattern with group lookup */
        uint8_t *groupMap = ((DIYTournamentState *)diy)->groupMap;
        uint8_t *divTable = &((DIYTournamentState *)diy)->teamIndexOrder[0xC];
        uint16_t d1 = 0;
        int16_t d2 = 3;
        /* Build group bitmask from first 4 division entries */
        for (; d2 >= 0; d2--) {
            uint16_t idx = (uint8_t)(*divTable++);
            uint8_t grp = groupMap[idx];
            int16_t grpExt = (int8_t)grp;
            d1 |= (1 << grpExt);
        }
#ifdef SWOS_DEBUG_AUTOSKIP
        SWOS_AUTOSKIP_LOG("buildCompSpecificTeamList: case1 d1=0x%02x (looking up in diyGroupPatternLookup)",
                          (unsigned)d1);
#endif
        /* Search diyGroupPatternLookup for matching bitmask */
        uint8_t *lookup = diyGroupPatternLookup;
        for (;;) {
            if ((int8_t)*lookup < 0) {
                /* End of table — panic */
#ifdef SWOS_DEBUG_AUTOSKIP
                SWOS_AUTOSKIP_LOG("buildCompSpecificTeamList: PANIC 0x8034 — d1=0x%02x not found in diyGroupPatternLookup",
                                  (unsigned)d1);
#endif
                panicReasonCode = 0x8034;
                extern void _PanicHaltLoop(void);
                _PanicHaltLoop();
            }
            if (*lookup == (uint8_t)d1) {
                lookup++; /* skip matched byte */
                break;
            }
            lookup += 5; /* skip bitmask + 4 order bytes */
        }
        /* Build shuffled list using diyLeagueTeamShuffleMap + pattern lookup */
        uint8_t *teamSrc = ((DIYTournamentState *)diy)->teamIndexOrder;
        dst = diyTeamShuffleScratch;
        uint8_t *map = diyLeagueTeamShuffleMap;
        for (int16_t i = 15; i >= 0; i--) {
            int8_t mapVal = (int8_t)*map++;
            uint16_t idx;
            if (mapVal < 0) {
                /* Negative: read from pattern lookup, add 12 */
                idx = (uint8_t)(*lookup++) + 12;
            } else {
                idx = (uint8_t)mapVal;
            }
            *dst++ = teamSrc[(int16_t)(int8_t)idx];
        }
    } else if (field54 == 2) {
        /* Case 2: 8-team tournament shuffle */
        for (int16_t i = 0; i < 8; i++) {
            uint16_t idx = (uint8_t)diyTournamentShuffleOrder8[i];
            *dst++ = src[idx];
        }
    } else if (field54 == 3) {
        /* Case 3: 4-team pattern using sub_129960 bytes as data */
        for (int16_t i = 0; i < 4; i++) {
            uint16_t idx = diyCompSpecificShuffle4[i];
            *dst++ = src[idx];
        }
    } else {
        /* Case 4+: 2 teams, copy first word */
        *(uint16_t *)dst = *(uint16_t *)src;
    }
}

/* ================================================================
 * DIY_AdjustWorldCupBracketScores — adjust seeding scores for
 * World Cup format
 *
 * asm: a2=diy, a3=slot, a5=comp (diyCompetitionStart)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_AdjustWorldCupBracketScores(uint8_t *diy, uint8_t *slot, uint8_t *comp)
{
    DIYCompetition *diyS = (DIYCompetition *)diy;
    DIYCompetition *compS = (DIYCompetition *)comp;
    DIYTournamentSlot *s = (DIYTournamentSlot *)(slot + offsetof(DIYTournamentState, slots));

    if (diyS->roundComplete != 0)
        return;

    int32_t d3ext = (int16_t)s->divisionFactor;
    uint16_t quotient = (uint16_t)((uint32_t)(uint16_t)d3ext / s->groups);

    DIYCompetitionEx *compEx = (DIYCompetitionEx *)comp;
    uint16_t *tableOffsets = compEx->matchData.league.teamEntryOffsets;
    int16_t d4 = 0x2328;
    int16_t d1 = 0;

    uint16_t numGroups = (uint16_t)compS->matchPairsPerRound;
    uint16_t teamsInGroup = (uint16_t)compS->totalMatchesInRound;

    for (; d1 < (int16_t)numGroups; d1++) {
        int16_t d2 = 0;
        uint16_t *a1 = tableOffsets;
        for (; d2 < (int16_t)teamsInGroup; d2++) {
            uint16_t offset = *a1++;
            DIYTeamStats *row = (DIYTeamStats *)(comp + offset + DIY_STANDINGS_OFFSET);
            if (d2 == quotient) {
                row->points += 0x3E8;  /* exact match bonus */
            } else if (d2 < quotient) {
                row->points += (uint16_t)d4;  /* decreasing bonus */
                d4 -= 100;
            }
            /* d2 > quotient: no bonus */
        }
        tableOffsets += teamsInGroup;
    }

    /* Reset to 1 group, all teams in single group */
    compS->matchPairsPerRound = 1;
    compS->totalMatchesInRound = compS->numTeams;

    /* Sort by standings */
    _DIY_SortLeagueTableByStandings();

    /* Apply group position bonuses */
    uint8_t *groupMap = ((DIYTournamentState *)diy)->groupMap;
    int16_t d2count = 3;
    /* +0x86 = sorted offsets array, past teamEntryOffsets[14]; reads 4 entries
     * walking into matchData.raw[0x22..0x29]. Original asm aliases the same
     * memory via `comp + 0x86`. */
    uint16_t *a1sorted = (uint16_t *)&compEx->matchData.raw[0x86 - 0x64];

    for (; d2count >= 0; d2count--) {
        uint16_t offset = *a1sorted++;
        DIYTeamStats *row = (DIYTeamStats *)(comp + offset + DIY_STANDINGS_OFFSET);
        int32_t offExt = (int32_t)offset;
        uint16_t entryIdx = (uint16_t)((uint32_t)(uint16_t)offExt / 0x12);
        int8_t grpVal = (int8_t)groupMap[entryIdx];
        int16_t grpExt = (int16_t)grpVal;
        int16_t bonus = (6 - grpExt) * 100;
        row->points += (uint16_t)bonus;
    }

    /* Sort again */
    _DIY_SortLeagueTableByStandings();
}

/* ================================================================
 * DIY_BuildSeededBracketAndUpdateScores — build seeded bracket with
 * scoring for team strength and group position
 *
 * asm: a2=diy, a3=slot, a4=comp(unused in body), a5=comp
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_BuildSeededBracketAndUpdateScores(uint8_t *diy, uint8_t *slot,
                                             uint8_t *comp4, uint8_t *comp)
{
    (void)comp4; /* a4 not used in this function's body */
    DIYCompetition *diyS = (DIYCompetition *)diy;
    DIYCompetitionEx *compEx = (DIYCompetitionEx *)comp;
    DIYCompetition *compS = &compEx->base;
    DIYTournamentSlot *s = (DIYTournamentSlot *)(slot + offsetof(DIYTournamentState, slots));

    /* Check if world cup competition */
    uint8_t compNum = diyS->field_2E;
#ifdef SWOS_DEBUG_AUTOSKIP
    {
        const uint8_t *teamList = ((DIYTournamentState *)diy)->teamIndexOrder;
        SWOS_AUTOSKIP_LOG("BuildSeededBracket: ENTER compNum=0x%02x WC?=%d numTeams=%d roundComplete=%d",
                          (unsigned)compNum, (int)(compNum == worldCup[0]),
                          (int)compS->numTeams, (int)diyS->roundComplete);
        SWOS_AUTOSKIP_LOG("BuildSeededBracket: teamList(0x5A)[0..15] BEFORE=%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                          teamList[0], teamList[1], teamList[2], teamList[3],
                          teamList[4], teamList[5], teamList[6], teamList[7],
                          teamList[8], teamList[9], teamList[10], teamList[11],
                          teamList[12], teamList[13], teamList[14], teamList[15]);
    }
#endif
    if (compNum == worldCup[0]) {
        _DIY_AdjustWorldCupBracketScores(diy, slot, comp);
        goto copy_back;
    }

    {
        int32_t d3ext = (int16_t)s->divisionFactor;
        uint16_t teamsPerGroup = s->groups;
        uint16_t d3quot = (uint16_t)((uint32_t)(uint16_t)d3ext / teamsPerGroup);
        uint16_t d3rem;
        {
            int32_t fullDiv = (int32_t)(int16_t)s->divisionFactor;
            uint16_t q = (uint16_t)((uint32_t)(uint16_t)fullDiv / teamsPerGroup);
            d3rem = (uint16_t)((uint32_t)(uint16_t)fullDiv % teamsPerGroup);
            d3quot = q;
        }

        uint16_t *tableOffsets = compEx->matchData.league.teamEntryOffsets;
        int16_t d5 = 0x7D00;
        uint16_t numGroups = (uint16_t)compS->matchPairsPerRound;
        uint16_t teamsInGroup = (uint16_t)compS->totalMatchesInRound;

        for (int16_t d1 = (int16_t)numGroups - 1; d1 >= 0; d1--) {
            int16_t d4 = d5;
            int16_t d2 = 0;
            for (; d2 < (int16_t)teamsInGroup; d2++) {
                uint16_t offset = *tableOffsets++;
                DIYTeamStats *row = (DIYTeamStats *)(comp + offset + DIY_STANDINGS_OFFSET);
                if (d2 == d3quot) {
                    row->points += 0x7D0;  /* exact match bonus */
                } else if (d2 < d3quot) {
                    row->points += (uint16_t)d4;  /* decreasing bonus */
                    d4 -= 0x3E8;
                }
                /* Above: no bonus */
            }
            d5 -= 100;
        }

        /* Reset to 1 group, all teams */
        compS->matchPairsPerRound = 1;
        compS->totalMatchesInRound = compS->numTeams;

        /* Sort */
        _DIY_SortLeagueTableByStandings();

        /* Apply group-position bonuses if there's a remainder */
        uint8_t *groupMap = ((DIYTournamentState *)diy)->groupMap;
        if (d3rem != 0) {
            uint16_t skipEntries = d3quot * teamsPerGroup;
            uint16_t *a1 = compEx->matchData.league.teamEntryOffsets + skipEntries;
            uint16_t entryStride = 0x12 * s->teamsPerGroup;

            for (int16_t i = (int16_t)d3rem - 1; i >= 0; i--) {
                uint16_t offset = *a1++;
                DIYTeamStats *row = (DIYTeamStats *)(comp + offset + DIY_STANDINGS_OFFSET);
                int32_t offExt = (int32_t)offset;
                uint16_t entryIdx = (uint16_t)((uint32_t)(uint16_t)offExt / 0x12);
                int8_t grpVal = (int8_t)groupMap[entryIdx];
                int16_t grpExt = (int16_t)grpVal;
                int16_t bonus = (8 - grpExt) * 25;
                row->points += (uint16_t)bonus;
            }

            /* Sort again */
            _DIY_SortLeagueTableByStandings();
        }
    }

copy_back:
    {
        /* Copy sorted offsets to team order, then to team list.
         * Raw `comp + 0xEE` is the 32-bit byte offset to the shuffle
         * grid; on 64-bit DIYCompetition grew by +8 so the raw offset
         * lands 8 bytes too early (inside cupRoundResults). Use the
         * typed field. See project_raw_offset_64bit_shift_trap.md. */
        uint16_t *sortedOffsets = compEx->matchData.league.teamEntryOffsets;
        uint8_t *teamOrder = compEx->matchData.league.shuffleAndPad;
        uint16_t numTeams = (uint16_t)compS->numTeams;

        for (int16_t i = (int16_t)numTeams - 1; i >= 0; i--) {
            uint16_t offset = *sortedOffsets++;
            int32_t offExt = (int32_t)offset;
            uint8_t idx = (uint8_t)((uint32_t)(uint16_t)offExt / 0x12);
            *teamOrder++ = idx;
        }

        /* Copy team order to diy team list */
        uint8_t *orderSrc = compEx->matchData.league.shuffleAndPad;
        uint8_t *diyDst = ((DIYTournamentState *)diy)->teamIndexOrder;  /* `diy` is the diyFileBufferCopy tournament struct */
        for (int16_t i = (int16_t)numTeams - 1; i >= 0; i--)
            *diyDst++ = *orderSrc++;

        /* Check if pair swap needed */
        if (s->divisionFactor == 1) return;
        /* slot+0x1D2 = next-slot doubleCount (0x76 stride + +0x02 within slot) */
        if (s[1].doubleCount == 0) return;

        /* Swap first pair in diy team list */
        uint8_t *pairStart = ((DIYTournamentState *)diy)->teamIndexOrder;
        uint8_t tmp = pairStart[0];
        pairStart[0] = pairStart[1];
        pairStart[1] = tmp;
    }
#ifdef SWOS_DEBUG_AUTOSKIP
    {
        const uint8_t *teamList = ((DIYTournamentState *)diy)->teamIndexOrder;
        SWOS_AUTOSKIP_LOG("BuildSeededBracket: teamList(0x5A)[0..15] AFTER=%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                          teamList[0], teamList[1], teamList[2], teamList[3],
                          teamList[4], teamList[5], teamList[6], teamList[7],
                          teamList[8], teamList[9], teamList[10], teamList[11],
                          teamList[12], teamList[13], teamList[14], teamList[15]);
    }
#endif
}

/* ================================================================
 * DIY_CopyTeamGridFromCompToDIY — copy team grid from comp to diy
 * after applying home/away swap flags and world cup adjustments
 *
 * asm: a2=diy, a3=slot, a4=comp(diyCompetitionStart), a5=unused
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_CopyTeamGridFromCompToDIY(uint8_t *diy, uint8_t *slot,
                                     uint8_t *comp, uint8_t *comp5)
{
    (void)comp5;
    DIYCompetition *diyS = (DIYCompetition *)diy;
    DIYCompetition *compS = (DIYCompetition *)comp;
    DIYTournamentSlot *s = (DIYTournamentSlot *)(slot + offsetof(DIYTournamentState, slots));

    uint16_t legs = s->divisionFactor;
    uint16_t field58 = (uint16_t)diyS->currentRoundIndex;

    if (!(legs == 1 && field58 == 0)) {
        _DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs();

        /* Check if pair swap needed */
        if (s->doubleCount != 0) {
            goto do_pair_swap;
        }
        if (legs == 1) goto check_worldcup;
        /* slot+0x1D2 = next-slot doubleCount (0x76 stride + +0x02 within slot) */
        if (s[1].doubleCount == 0) goto check_worldcup;

    do_pair_swap:
        {
            uint8_t *grid = ((DIYCompetitionEx *)comp)->teamGridPairings;
            uint8_t tmp = grid[0];
            grid[0] = grid[1];
            grid[1] = tmp;
        }
    }

check_worldcup:
    {
        uint8_t compNum = diyS->field_2E;
        if (compNum == worldCup[0]) {
            _DIY_AdjustWorldCupBracketScores(diy, slot, comp);
        }
    }

    /* Copy team grid from comp to diy. Raw `comp + 0x2CE` is the 32-bit
     * byte offset to teamGridPairings[64]; on 64-bit DIYCompetition grew
     * by +8 so this would land in cupTeamIds. Use the typed field. */
    {
        uint8_t *gridSrc = ((DIYCompetitionEx *)comp)->teamGridPairings;
        uint8_t *gridDst = ((DIYTournamentState *)diy)->teamIndexOrder;  /* `diy` is the diyFileBufferCopy tournament struct */
        uint16_t count = (uint16_t)compS->numTeams;
        if (s->doubleCount != 0)
            count <<= 1;
        for (int16_t i = (int16_t)count - 1; i >= 0; i--)
            *gridDst++ = *gridSrc++;
    }
}

/* ================================================================
 * DIY_BuildTeamGridAndIndexMaps — build team grid with seeded ordering,
 * then build team index and group maps
 *
 * asm: a2=diy, a3=slot, a4=unused, a5=comp(diyCompetitionStart)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_BuildTeamGridAndIndexMaps(uint8_t *diy, uint8_t *slot,
                                     uint8_t *comp4, uint8_t *comp)
{
    (void)comp4;
    DIYCompetition *diyS = (DIYCompetition *)diy;
    DIYTournamentSlot *s = (DIYTournamentSlot *)(slot + offsetof(DIYTournamentState, slots));

    uint8_t compNum = diyS->field_2E;

    /* Check for world cup — use comp-specific list directly */
    if (compNum == worldCup[0]) {
        buildCompSpecificTeamList(diy);
        goto copy_and_index;
    }

    /* Check for European championships — use comp-specific shuffle */
    if (compNum == europeanChampionships[0] ||
        compNum == europeanChampionships2[0]) {
        buildCompSpecificTeamShuffle(diy);
        goto copy_and_index;
    }

    /* Standard path */
    {
        uint16_t field54 = (uint16_t)diyS->roundComplete;
        uint16_t field50 = (uint16_t)diyS->matchPairsPerRound;

        if (field54 == 0 && field50 != 0)
            goto build_index_maps;

        if (s->seedingType == 0) {
            _DIY_ShuffleTeams(diy, slot);
            goto build_index_maps;
        }

        /* Seeded bracket */
        int16_t d5 = -1;
        /* slot+0xE8 = previous slot's isLeagueFormat (0x76 stride peek-back) */
        if (field54 != 0 && s[-1].isLeagueFormat != 0) {
            d5 = 0;
        } else {
            _DIY_SortAndShuffleTeamsByValue(diy, slot);
        }

        /* Interleave teams into groups */
        {
            uint8_t *srcTeams = ((DIYTournamentState *)diy)->teamIndexOrder;
            uint8_t *dst = diyTeamShuffleScratch;

            if (d5 >= 0) {
                /* slot+0xF4 = previous slot's groups field */
                d5 = (int16_t)s[-1].groups;
            }

            uint16_t numGroupSlots = s->teamsPerGroup;
            uint16_t numGroups = s->groups;
            uint16_t numTeams = s->numTeams;

            int16_t d4 = 0;
            int16_t d1 = 0;

            for (int16_t g = (int16_t)numGroupSlots - 1; g >= 0; g--) {
                if (d5 >= 0 && d4 == 0) {
                    d1 += (int16_t)numGroupSlots;
                }

                for (int16_t grp = (int16_t)numGroups - 1; grp >= 0; grp--) {
                    /* Wrap d1 if it exceeds numTeams */
                    while (d1 >= (int16_t)numTeams)
                        d1 -= (int16_t)numTeams;

                    dst[d1] = *srcTeams++;
                    d4++;
                    if (d4 == d5)
                        d4 = 0;
                    d1 += (int16_t)numGroupSlots;
                }
                d1++;  /* asm: addq.w #1,d1 after inner loop */
            }
        }

    copy_and_index:
        /* Copy scratch back to diy team list */
        {
            uint8_t *src = diyTeamShuffleScratch;
            uint8_t *dst = ((DIYTournamentState *)diy)->teamIndexOrder;
            uint16_t numTeams = s->numTeams;
            for (int16_t i = (int16_t)numTeams - 1; i >= 0; i--)
                *dst++ = *src++;
        }
    }

build_index_maps:
    {
        /* Build team order and offset maps. Same 64-bit raw-offset trap
         * as elsewhere — use the typed shuffleAndPad field instead of
         * `comp + 0xEE`. */
        DIYCompetitionEx *compEx = (DIYCompetitionEx *)comp;
        uint8_t *teamList = ((DIYTournamentState *)diy)->teamIndexOrder;
        uint8_t *teamOrder = compEx->matchData.league.shuffleAndPad;
        uint16_t *teamOffsets = compEx->matchData.league.teamEntryOffsets;
        uint16_t numTeams = s->numTeams;

        for (int16_t i = (int16_t)numTeams - 1; i >= 0; i--) {
            uint8_t idx = *teamList++;
            *teamOrder++ = idx;
            int16_t idxExt = (int8_t)idx;
            *teamOffsets++ = (uint16_t)(idxExt * 0x12);
        }

        /* Build group map: for each team, store which group slot it's in */
        teamList = ((DIYTournamentState *)diy)->teamIndexOrder;
        uint8_t *groupMap = ((DIYTournamentState *)diy)->groupMap;
        uint16_t numGroupSlots = s->teamsPerGroup;
        uint16_t numGroups = s->groups;
#ifdef SWOS_DEBUG_AUTOSKIP
        SWOS_AUTOSKIP_LOG("LoadSlot: populate groupMap: numGroups=%u teamsPerGroup=%u numTeams=%u",
                          (unsigned)numGroups, (unsigned)numGroupSlots,
                          (unsigned)s->numTeams);
        SWOS_AUTOSKIP_LOG("LoadSlot: teamList(0x5A)[0..15]=%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                          teamList[0], teamList[1], teamList[2], teamList[3],
                          teamList[4], teamList[5], teamList[6], teamList[7],
                          teamList[8], teamList[9], teamList[10], teamList[11],
                          teamList[12], teamList[13], teamList[14], teamList[15]);
#endif

        uint8_t grpIdx = 0;
        for (uint16_t g = 0; g < numGroups; g++) {
            for (int16_t ss = (int16_t)numGroupSlots - 1; ss >= 0; ss--) {
                uint16_t teamIdx = (uint8_t)*teamList++;
                groupMap[teamIdx] = grpIdx;
            }
            grpIdx++;
        }
    }
}

/* ================================================================
 * DIY_ShuffleTeamsAndBuildCompTeamList — shuffle or seed teams, then
 * copy to competition team list and verify all loaded
 *
 * asm: a2=diy, a3=slot, a4=comp(diyCompetitionStart), a5=unused
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_ShuffleTeamsAndBuildCompTeamList(uint8_t *diy, uint8_t *slot,
                                            uint8_t *comp, uint8_t *comp5)
{
    (void)comp5;
    DIYCompetition *diyS = (DIYCompetition *)diy;
    DIYTournamentSlot *s = (DIYTournamentSlot *)(slot + offsetof(DIYTournamentState, slots));

    uint8_t compNum = diyS->field_2E;

    /* Check for world cup — use comp-specific list */
    if (compNum == worldCup[0]) {
        buildCompSpecificTeamList(diy);
        goto copy_to_comp;
    }

    /* Check for European championships — use comp-specific shuffle */
    if (compNum == europeanChampionships[0] ||
        compNum == europeanChampionships2[0]) {
        buildCompSpecificTeamShuffle(diy);
        goto copy_to_comp;
    }

    /* Standard shuffle path */
    {
        uint16_t field54 = (uint16_t)diyS->roundComplete;
        uint16_t field50 = (uint16_t)diyS->matchPairsPerRound;

        if (field54 != 0 || field50 == 0) {
            if (s->seedingType == 0) {
                _DIY_ShuffleTeams(diy, slot);
                goto build_comp_list;
            }

            /* Seeded: check if sort needed.
             * slot+0xE8/+0xEA = previous slot's isLeagueFormat / seedingType */
            if (field54 != 0) {
                if (s[-1].isLeagueFormat != 0 || s[-1].seedingType != 0)
                    goto after_sort;
            }
            _DIY_SortAndShuffleTeamsByValue(diy, slot);

        after_sort:
            /* Random home/away swap for each pair */
            {
                uint8_t *teamList = ((DIYTournamentState *)diy)->teamIndexOrder;
                uint16_t numTeams = s->numTeams;
                uint8_t *lo = teamList;
                uint8_t *hi = teamList + numTeams;
                uint8_t *dst = diyTeamShuffleScratch;
                int16_t pairs = (int16_t)(numTeams >> 1) - 1;

                for (; pairs >= 0; pairs--) {
                    uint16_t r = _Rand2();
                    if (r & 1) {
                        *dst++ = *--hi;
                        *dst++ = *lo++;
                    } else {
                        *dst++ = *lo++;
                        *dst++ = *--hi;
                    }
                }
            }

        copy_to_comp:
            /* Copy from scratch to diy team list */
            {
                uint8_t *src = diyTeamShuffleScratch;
                uint8_t *dst = ((DIYTournamentState *)diy)->teamIndexOrder;
                uint16_t numTeams = s->numTeams;
                for (int16_t i = (int16_t)numTeams - 1; i >= 0; i--)
                    *dst++ = *src++;
            }
        }
    }

build_comp_list:
    {
        /* Copy team list to competition order list. Raw `comp + 0x2CE`
         * needs the +8 64-bit shift compensation — use the typed field
         * (teamGridPairings[64] is at parent +0x2CE). */
        uint8_t *teamList = ((DIYTournamentState *)diy)->teamIndexOrder;
        uint8_t *compOrder = ((DIYCompetitionEx *)comp)->teamGridPairings;
        uint16_t numTeams = s->numTeams;
        uint16_t copyCount = numTeams;

        if (s->doubleCount != 0)
            copyCount <<= 1;

        for (int16_t i = (int16_t)copyCount - 1; i >= 0; i--)
            *compOrder++ = *teamList++;

        /* Verify all teams loaded and count missing */
        uint16_t *teamIds = ((DIYTournamentState *)diy)->teamIds;
        diyS->hasGroups = 0;
        teamList = ((DIYTournamentState *)diy)->teamIndexOrder;
        int16_t count = (int16_t)copyCount - 1;

        for (; count >= 0; count--) {
            uint16_t idx = (uint8_t)*teamList++;
            /* teamIds (diy+0x9A = diyTournamentTeamIds) is BE-packed. */
            uint16_t teamNum = BE_READ_U16(&teamIds[idx]);
            TeamFileEx *teamPtr = _VerifyTeamLoaded(teamNum);
            if (teamPtr->team.teamControls != 1) {
                diyS->hasGroups += 1;
            }
        }
    }
}

/* ================================================================
 * DIY_LoadCurrentCompetitionSlot — load competition data from file
 * buffer slot into active competition structure
 *
 * asm: operates on diyFileBufferCopy + diyCompetitionStart
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _DIY_LoadCurrentCompetitionSlot(void)
{
    uint8_t *a2 = (uint8_t *)&diyFileBufferCopy;
    DIYCompetition *a2s = &diyFileBufferCopy.header;
    uint8_t *a3 = a2;
    uint16_t slotIdx = (uint16_t)a2s->roundComplete;  /* 0x54 */
    a3 += (uint32_t)slotIdx * 118;
    DIYTournamentSlot *s = (DIYTournamentSlot *)(a3 + offsetof(DIYTournamentState, slots));
#ifdef SWOS_DEBUG_AUTOSKIP
    SWOS_AUTOSKIP_LOG("LoadSlot: ENTER slotIdx=%u isLeagueFormat=%u numTeams=%u groups=%u teamsPerGroup=%u "
                      "compType(buf)=%d numTeams(buf)=%d compId(buf)=%u",
                      (unsigned)slotIdx, (unsigned)s->isLeagueFormat,
                      (unsigned)s->numTeams, (unsigned)s->groups,
                      (unsigned)s->teamsPerGroup,
                      (int)a2s->competitionType, (int)a2s->numTeams,
                      (unsigned)a2s->compId);
#endif

    uint8_t *a4 = (uint8_t *)&diyCompetitionStart;
    uint8_t *a5 = (uint8_t *)&diyCompetitionStart;
    DIYCompetition *a4s = (DIYCompetition *)a4;
    DIYCompetition *a5s = (DIYCompetition *)a5;

    /* Clear the "done loading" flag */
    a2s->competitionFinished = 0;  /* 0x56 */

    /* Check if this is a league/grouped slot (isLeagueFormat != 0) */
    if (s->isLeagueFormat != 0) {
        /* League path — init defaults, then fill from file buffer */
        _DIY_InitCompetitionDefaults();

        a5s->hasGroups = 1;  /* 0x48 */

        /* Copy match/score fields */
        a5s->matchStarted = a2s->matchStarted;  /* 0x44 */
        a5s->careerTeamInvolved = a2s->careerTeamInvolved;  /* 0x46 */

        /* Copy competition name */
        _StringCopy((char *)(a2 + 2 + 2), (char *)(a5 + 2 + 2));

        a5s->menuState = a2s->roundComplete;  /* 0x4A = 0x54 */
        a5s->roundIterator = a2s->totalMatchesInRound;  /* 0x4C = 0x52 */
        *(uint16_t *)&a5s->compId = *(uint16_t *)&a2s->compId;  /* 0x2C */

        /* Num teams */
        a5s->numTeams = (int16_t)s->numTeams;

        /* League-specific size/progress */
        a5s->matchPairsPerRound = (int16_t)s->groups;
        int32_t d0ext = (int16_t)s->divisionFactor;
        uint16_t divisor = s->groups;
        /* ASM: ext.l d0 then divu.w — sign-extend to 32-bit then unsigned divide.
         * Must NOT truncate to uint16_t before dividing (loses sign extension). */
        uint16_t quotient = (uint16_t)((uint32_t)d0ext / divisor);
        uint16_t remainder = (uint16_t)((uint32_t)d0ext % divisor);

        a5s->roundComplete = (int16_t)quotient;  /* 0x54 */
        if (remainder != 0) {
            a5s->competitionFinished = 1;  /* 0x56 */
        }

        /* Copy team ID table from diyFileBufferCopy.teamIds (a2+0x9A) into
         * matchData.league.leagueTeamIds (a5+0x12E). */
        {
            uint16_t *src = diyFileBufferCopy.teamIds;
            uint16_t *dst = ((DIYCompetitionEx *)a5)->matchData.league.leagueTeamIds;
            uint16_t count = (uint16_t)a2s->numTeams;
            for (int16_t i = (int16_t)count - 1; i >= 0; i--)
                *dst++ = *src++;
        }

        /* Build team grid and index maps */
        _DIY_BuildTeamGridAndIndexMaps(a2, a3, a5, a5);

        /* Play each team & start month */
        a5s->playEachTeam = (int16_t)s->playEachTeam;
        a5s->startMonth = (int8_t)s->progressionTable[0];

        /* Recompute derived fields */
        _DIY_RecomputeCompetitionDerivedFields();

        /* Copy additional config fields */
        a5s->avgSkill = a2s->avgSkill;          /* 0x30 */
        a5s->strengthTier = a2s->strengthTier;  /* 0x31 */
        a5s->pitchTypeOrSeason = a2s->pitchTypeOrSeason;  /* 0x34 */
        a5s->pitchType = a2s->pitchType;        /* 0x36 */
        a5s->startMonth = a2s->startMonth;      /* 0x38 */
        a5s->endMonth = a2s->endMonth;          /* 0x39 */
        a5s->substitutesCount = a2s->substitutesCount;  /* 0x3C */

        /* Extended fields from slot — write league.{pointsForAWin alias is
         * actually parent +0x62, but here we're at +0x62/+0x64/+0x66
         * which are pointsForAWin / league.leagueMinSubstitutes /
         * league.leagueMaxSubstitutes. */
        {
            DIYCompetitionEx *a5Ex = (DIYCompetitionEx *)a5;
            a5Ex->pointsForAWin = s->initConst168;
            /* a2+0x4C = bufCopy's roundIterator, a2+0x4E = bufCopy's numTeamsInStage */
            a5Ex->matchData.league.leagueMinSubstitutes = (uint16_t)((DIYCompetition *)a2)->roundIterator;
            a5Ex->matchData.league.leagueMaxSubstitutes = (uint16_t)((DIYCompetition *)a2)->numTeamsInStage;
        }

        /* Copy progression month bytes — a5+58+0x194 = a5+0x1CE = inside
         * leagueSectionState.phaseStartMonths. */
        {
            DIYCompetitionEx *a5Ex = (DIYCompetitionEx *)a5;
            uint8_t *src = s->progressionTable;
            /* phaseStartMonths starts at parent 0x1CE = leagueSectionState +0x08 */
            uint8_t *dst = a5Ex->leagueSectionState.phaseStartMonths;
            uint16_t count = s->matchesPerSlot;
            for (int16_t i = (int16_t)count - 1; i >= 0; i--)
                *dst++ = *src++;
        }

        a5s->field_2E = a2s->field_2E;  /* 0x2E */
        a5s->numTeamsInStage = a2s->numTeams;  /* 0x4E = numTeams from a2 */
    } else {
        /* Cup path — init DIY cup, then fill from file buffer */
        _InitDIYCup();

        a4s->hasGroups = 1;  /* 0x48 */

        /* Copy match/score fields */
        a4s->matchStarted = a2s->matchStarted;  /* 0x44 */
        a4s->careerTeamInvolved = a2s->careerTeamInvolved;  /* 0x46 */

        /* Copy competition name */
        _StringCopy((char *)(a2 + 4), (char *)(a4 + 4));

        a4s->numRounds = 1;  /* 0x5A */
        *(uint16_t *)&a4s->compId = *(uint16_t *)&a2s->compId;  /* 0x2C */

        /* Cup-specific fields from slot — write into cup roundLegs[0] (parent +0x66),
         * roundExtraTime[0] (+0x72), roundPenalties[0] (+0x7E). */
        {
            DIYCompetitionEx *a4Ex = (DIYCompetitionEx *)a4;
            a4s->numTeams = (int16_t)s->numTeams;
            a4Ex->matchData.cup.roundLegs[0]      = s->formatFlag16E;
            a4Ex->matchData.cup.roundExtraTime[0] = s->initConst170;
            a4Ex->matchData.cup.roundPenalties[0] = s->formatFlag172;
        }

        /* Copy team grid from diyFileBufferCopy.teamIds (a2+0x9A) into cupTeamIds (a4+0x24E). */
        {
            uint16_t *src = diyFileBufferCopy.teamIds;
            uint16_t *dst = ((DIYCompetitionEx *)a4)->cupTeamIds;
            uint16_t count = (uint16_t)a2s->numTeams;
            for (int16_t i = (int16_t)count - 1; i >= 0; i--)
                *dst++ = *src++;
        }

        /* Shuffle teams and build comp team list */
        _DIY_ShuffleTeamsAndBuildCompTeamList(a2, a3, a4, a5);

        /* Start month */
        a4s->startMonth = (int8_t)s->progressionTable[0];

        /* Init cup bracket state */
        _DIY_InitCupBracketState();

        /* Copy additional config fields */
        a4s->roundIterator = a2s->hasGroups;  /* 0x4C = 0x48 */
        a4s->avgSkill = a2s->avgSkill;          /* 0x30 */
        a4s->strengthTier = a2s->strengthTier;  /* 0x31 */
        a4s->pitchTypeOrSeason = a2s->pitchTypeOrSeason;  /* 0x34 */
        a4s->pitchType = a2s->pitchType;        /* 0x36 */
        a4s->startMonth = a2s->startMonth;      /* 0x38 */
        a4s->endMonth = a2s->endMonth;          /* 0x39 */
        a4s->substitutesCount = a2s->substitutesCount;  /* 0x3C */

        /* Cup bracket fields */
        a4s->playEachTeam = a2s->menuState;    /* 0x5E = 0x4A */
        ((DIYCompetitionEx *)a4)->minSubstitutes = (uint16_t)a2s->roundIterator;
        ((DIYCompetitionEx *)a4)->pointsForAWin = (uint16_t)a2s->numTeamsInStage;

        /* Copy progression month bytes into bracketScratch */
        {
            DIYCompetitionEx *a4Ex = (DIYCompetitionEx *)a4;
            uint8_t *src = s->progressionTable;
            uint8_t *dst = a4Ex->bracketScratch.progTable;
            uint16_t count = s->matchesPerSlot;
            a4Ex->bracketScratch.totalMatches = count;
            for (int16_t i = (int16_t)count - 1; i >= 0; i--)
                *dst++ = *src++;
        }

        a4s->field_2E = a2s->field_2E;  /* 0x2E */
        a4s->numTeamsInStage = a2s->numTeams;  /* 0x4E = numTeams from a2 */
    }
}

/* ================================================================
 * DIY_ResetAndLoadCurrentCompetitionSlot — reset competition state,
 * seed random, initialize team order, and load slot
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _DIY_ResetAndLoadCurrentCompetitionSlot(void)
{
    /* Reseed random from current tick */
#ifdef DETERMINISTIC_MODE
    _InitDeterministicMode();
#else
    randomSeed = currentTick;
    newRandomSeedCopy = (uint8_t)(currentTick >> 8);
#endif

    /* Clear flags in diyFileBufferCopy */
    {
        DIYCompetition *bufS = &diyFileBufferCopy.header;
        bufS->currentRoundIndex = 0;         /* 0x58 */
        bufS->roundComplete = 0;     /* 0x54 */
        (&diyCompetitionStart.base)->isActive = 0;
        (&diyCompetitionStart.base)->isActive = 0;  /* twice in asm */
        bufS->matchStarted = 0;   /* 0x44 */
        bufS->careerTeamInvolved = 0;   /* 0x46 */
        bufS->resultCode = (int16_t)0xFFFF;  /* 0x3A */
        diyFileBufferCopy.header.isActive = 1;
    }

    /* Initialize 64-byte sequential team order */
    {
        uint8_t *dst = diyFileBufferCopy.teamIndexOrder;  /* 0x5A (relocated team-index list) */
        uint8_t val = 0;
        for (int16_t i = 63; i >= 0; i--)
            *dst++ = val++;
    }

    /* Load the competition slot */
    _DIY_LoadCurrentCompetitionSlot();
}

/* ================================================================
 * DIY_InitCupFromTeamGridAndValidateTeams — initialize cup from
 * team grid, seed random, validate all teams loaded
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _DIY_InitCupFromTeamGridAndValidateTeams(void)
{
    /* Reseed random from current tick */
#ifdef DETERMINISTIC_MODE
    _InitDeterministicMode();
#else
    randomSeed = currentTick;
    newRandomSeedCopy = (uint8_t)(currentTick >> 8);
#endif

    DIYCompetitionEx *comp = &diyCompetitionStart;
    uint8_t *a2 = (uint8_t *)comp;
    DIYCompetition *a2s = &comp->base;

    /* Clear various fields */
    a2s->matchStarted = 0;    /* 0x44 */
    a2s->careerTeamInvolved = 0;    /* 0x46 */
    a2s->resultCode = (int16_t)0xFFFF;  /* 0x3A */
    a2s->menuState = 0;           /* 0x4A */

    /* halfTeams = numTeams / 2 */
    uint16_t numTeams = (uint16_t)a2s->numTeams;
    uint16_t half = numTeams >> 1;
    a2s->matchPairsPerRound = (int16_t)half;     /* 0x50 */
    a2s->totalMatchesInRound = (int16_t)half;   /* 0x52 */

    /* Clear more fields */
    comp->matchData.cup.currentMatchIndex = 0;
    a2s->roundComplete = 0;     /* 0x54 */
    a2s->currentRoundIndex = 0;         /* 0x58 */
    a2s->competitionFinished = 0;     /* 0x56 */
    comp->matchData.cup.competitionFormat = 0;

    /* Set current round match count from table */
    uint16_t roundIdx = (uint16_t)a2s->currentRoundIndex;
    comp->matchData.cup.roundFormat = comp->matchData.cup.roundLegs[roundIdx];

    /* Check for single-team round: if halfTeams==1 and matchCount!=2, clear matchCount */
    if (a2s->matchPairsPerRound == 1) {
        if (comp->matchData.cup.roundFormat != 2) {
            comp->matchData.cup.roundFormat = 0;
        }
    }

    /* Clear more state */
    diyCompetitionStart.bracketScratch.groupSectionFlag = 0;
    diyCompetitionStart.bracketScratch.secondLegCounter = 0;
    diyCompetitionStart.bracketScratch.curMonthIdx = 0;
    diyCompetitionStart.bracketScratch.matchCount = 0;

    /* Initialize 64-byte sequential team order in teamGridPairings */
    {
        uint8_t *dst = comp->teamGridPairings;
        uint8_t val = 0;
        for (int16_t i = 63; i >= 0; i--)
            *dst++ = val++;
    }

    /* Initialize 32-byte zero array at 0x8A (cup result flags) */
    {
        uint8_t val = 0;
        uint8_t *dst = diyCompetitionStart.matchData.cup.cupRoundResults;
        for (int16_t i = 31; i >= 0; i--)
            *dst++ = val;
    }

    /* Set random phase from start month */
    a2s->viewRandomPhase = (uint8_t)a2s->startMonth;
    viewRandomPhase = a2s->viewRandomPhase;

    /* Count missing teams */
    a2s->roundIterator = 0;  /* 0x4C */
    uint8_t *orderList = comp->teamGridPairings;
    uint16_t *teamGrid = comp->cupTeamIds;

    for (int16_t i = (int16_t)numTeams - 1; i >= 0; i--) {
        uint16_t idx = (uint8_t)*orderList++;
        /* teamGrid (cupTeamIds) is BE-packed per 4e5f378. */
        uint16_t teamNum = BE_READ_U16(&teamGrid[idx]);
        TeamFileEx *teamPtr = _VerifyTeamLoaded(teamNum);
        if (teamPtr->team.teamControls != 1) {
            a2s->roundIterator += 1;
        }
    }

    /* Shuffle team grid pairs */
    _DIY_ShuffleTeamGridPairs();
}

/* ================================================================
 * SelectDiy* menu callbacks (Chunk 108)
 * Set diySelected to the chosen competition type, then exit menu.
 * Asm: SelectDiyLeagueMenu, SelectDiyCupMenu, SelectDiyTournamentMenu,
 *      SelectDiyExitMenu
 * ================================================================ */
__attribute__((used, externally_visible))
void _SelectDiyLeagueMenu(void)
{
    diySelected = 0;
    _SetExitMenuFlag();
}

__attribute__((used, externally_visible))
void _SelectDiyCupMenu(void)
{
    diySelected = 1;
    _SetExitMenuFlag();
}

__attribute__((used, externally_visible))
void _SelectDiyTournamentMenu(void)
{
    diySelected = 2;
    _SetExitMenuFlag();
}

__attribute__((used, externally_visible))
void _SelectDiyExitMenu(void)
{
    diySelected = 0xFFFF;
    _SetExitMenuFlag();
}

/* ================================================================
 * Chunk 116: OpenDiy* + ShowDiyCompetitionForSource
 * ================================================================ */

/* ShowDiyCompetitionForSource — loads competition state from buffer,
 * shows the appropriate menu based on type (league=0, cup=1, tournament=else),
 * then stores state back. (SWOS2.s line 59179) */
__attribute__((used, externally_visible))
void _ShowDiyCompetitionForSource(uint8_t *header)
{
    _Career_LoadCompetitionSlotStateFromBuffer(header);
    uint16_t type = *(uint16_t *)header;

    if (type == 0) {
        _ShowDiyLeaguesTournamentMenu(1, 1, 0);
    } else if (type == 1) {
        _ShowDIYCupMenuWithParams(1, 1, 0);
    } else {
        _DIY_EditCompetitionSlot(1, 1, 0);
    }

    _Career_StoreCompetitionSlotStateToBuffer(header);
}

/* OpenDiyFromFile — open DIY from competitionFileBuffer (SWOS2.s line 59155) */
__attribute__((used, externally_visible))
void _OpenDiyFromFile(void)
{
    _ShowDiyCompetitionForSource(competitionFileBuffer);
}

/* OpenDiyPreset1 — open DIY from competitionContext (SWOS2.s line 59161) */
__attribute__((used, externally_visible))
void _OpenDiyPreset1(void)
{
    _ShowDiyCompetitionForSource(competitionContext);
}

/* OpenDiyPreset2 — open DIY from tertiaryCompetitionContext (SWOS2.s line 59167) */
__attribute__((used, externally_visible))
void _OpenDiyPreset2(void)
{
    _ShowDiyCompetitionForSource(tertiaryCompetitionContext);
}

/* OpenDiyPreset3 — open DIY from euroCupsCompetitionContext (SWOS2.s line 59173) */
__attribute__((used, externally_visible))
void _OpenDiyPreset3(void)
{
    _ShowDiyCompetitionForSource(euroCupsCompetitionContext);
}

/* ================================================================
 * Chunk 120: DIY Design Menu functions
 * ================================================================ */

extern MenuEntry menuEntries[];
extern uint16_t diyLeagueLookOnly;
extern uint16_t diyLeagueSelectTeams;
extern uint8_t designDiyLeagueMenu[];
extern uint16_t diyCupLookOnly;
extern uint16_t diyCupSelectTeams;
extern uint8_t designDiyCupMenu[];
extern uint16_t diyTournamentLookOnly;
extern uint16_t diyTournamentSelectTeams;
extern uint8_t designDiyTournamentMenu[];
extern uint16_t diyMinSubstitutes;
extern uint8_t aSelect0Teams[];   /* "SELECT %0 TEAMS" */

extern void _PrintFormatted(const uint8_t *fmt, uint8_t *out,
                     uint16_t n0, uint16_t n1, uint16_t n2, uint16_t n3,
                     uint16_t n4, uint16_t n5, uint16_t n6,
                     const uint8_t *strA, const uint8_t *strB, const uint8_t *strC);

/* ShowDesignDIYLeagueMenu — save, init defaults, show league design menu (SWOS2.s line 40734) */
__attribute__((used, externally_visible))
void _ShowDesignDIYLeagueMenu(void)
{
    _SaveDIYLeague();
    _DIY_InitCompetitionDefaults();
    diyLeagueLookOnly = 0;
    diyLeagueSelectTeams = 0;
    _ShowMenu(designDiyLeagueMenu);
}

/* ShowLeagueInfo — view-only league design menu (SWOS2.s line 40744) */
__attribute__((used, externally_visible))
void _ShowLeagueInfo(void)
{
    diyLeagueLookOnly = 1;
    diyLeagueSelectTeams = 0;
    _ShowMenu(designDiyLeagueMenu);
}

/* DesignDIYLeagueOK — set select teams flag, activate league menu (SWOS2.s line 41488) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueOK(void)
{
    diyLeagueSelectTeams = 1;
    _ActivateMenu(designDiyLeagueMenu);
}

/* SetNumberOfTeamsToSelect — format "SELECT N TEAMS" for league design (SWOS2.s line 41698) */
__attribute__((used, externally_visible))
void _SetNumberOfTeamsToSelect(void)
{
    uint8_t *str = (uint8_t *)menuEntries[23].fg;
    uint16_t numTeams = *(uint16_t *)&diyCompetitionStart.base.numTeams;
    _PrintFormatted(aSelect0Teams, str, numTeams, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* ShowDesignDIYCupMenu — copy buffer, init cup, show cup design menu (SWOS2.s line 43355) */
__attribute__((used, externally_visible))
void _ShowDesignDIYCupMenu(void)
{
    _DIY_CopyCompetitionFileBufferToCompetitionStart();
    _InitDIYCup();
    diyCupLookOnly = 0;
    diyCupSelectTeams = 0;
    _ShowMenu(designDiyCupMenu);
}

/* ShowCupInfo — view-only cup design menu (SWOS2.s line 43365) */
__attribute__((used, externally_visible))
void _ShowCupInfo(void)
{
    diyCupLookOnly = 1;
    diyCupSelectTeams = 0;
    _ShowMenu(designDiyCupMenu);
}

/* DesignDIYCupOK — set select teams flag, activate cup menu (SWOS2.s line 44818) */
__attribute__((used, externally_visible))
void _DesignDIYCupOK(void)
{
    diyCupSelectTeams = 1;
    _ActivateMenu(designDiyCupMenu);
}

/* DesignDIYCup_UpdateMinSubstitutesAndPointsStrings — format min subs + points (SWOS2.s line 44971) */
__attribute__((used, externally_visible))
void _DesignDIYCup_UpdateMinSubstitutesAndPointsStrings(void)
{
    _Int2Ascii(diyMinSubstitutes, (uint8_t *)menuEntries[13].fg);
    _Int2Ascii(*(uint16_t *)diyPointsForAWin, (uint8_t *)menuEntries[15].fg);
}

/* DesignDIYCup_UpdateSelectTeamsPrompt — format "SELECT N TEAMS" for cup (SWOS2.s line 45065) */
__attribute__((used, externally_visible))
void _DesignDIYCup_UpdateSelectTeamsPrompt(void)
{
    uint8_t *str = (uint8_t *)menuEntries[51].fg;
    uint16_t numTeams = *(uint16_t *)&diyCompetitionStart.base.numTeams;
    _PrintFormatted(aSelect0Teams, str, numTeams, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* ShowDesignDIYTournamentMenu — save, init tournament, show menu (SWOS2.s line 47530) */
__attribute__((used, externally_visible))
void _ShowDesignDIYTournamentMenu(void)
{
    _SaveDIYTournament();
    _InitDIYTournament();
    diyTournamentLookOnly = 0;
    diyTournamentSelectTeams = 0;
    _ShowMenu(designDiyTournamentMenu);
}

/* ShowTournamentInfo — view-only tournament design menu (SWOS2.s line 47540) */
__attribute__((used, externally_visible))
void _ShowTournamentInfo(void)
{
    diyTournamentLookOnly = 1;
    diyTournamentSelectTeams = 0;
    _ShowMenu(designDiyTournamentMenu);
}

/* DesignDIYTournamentOK — set select teams flag, activate tournament menu (SWOS2.s line 49923) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOK(void)
{
    diyTournamentSelectTeams = 1;
    _ActivateMenu(designDiyTournamentMenu);
}

/* DesignDIYTournament_UpdateTwoNumericFieldsText — format two fields (SWOS2.s line 50063) */
__attribute__((used, externally_visible))
void _DesignDIYTournament_UpdateTwoNumericFieldsText(void)
{
    DIYCompetition *buf = &diyFileBufferCopy.header;
    _Int2Ascii((uint16_t)buf->roundIterator,    (uint8_t *)menuEntries[11].fg);
    _Int2Ascii((uint16_t)buf->numTeamsInStage,  (uint8_t *)menuEntries[13].fg);
}

/* DesignDIYTournament_UpdateSelectTeamsPrompt — format "SELECT N TEAMS" for tournament (SWOS2.s line 50192) */
__attribute__((used, externally_visible))
void _DesignDIYTournament_UpdateSelectTeamsPrompt(void)
{
    DIYCompetition *buf = &diyFileBufferCopy.header;
    uint8_t *str = (uint8_t *)menuEntries[92].fg;
    _PrintFormatted(aSelect0Teams, str, (uint16_t)buf->numTeams, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* ==========================================================================
 * Chunk 126 — DesignDIYLeague Change Callbacks
 * ========================================================================== */

extern uint16_t diyLeagueStartingMonth;
extern uint16_t diyLeagueEndingMonth;
extern uint16_t diyLeagueMinSubstitutes;
extern uint16_t diyLeagueMaxSubstitutes;
extern uint8_t  aX0[];             /* "X %0" */
/* DesignDIYLeagueFinish now ported — call C version directly */

/* DesignDIYLeagueInit — init league design menu (SWOS2.s line 41323)
 * Falls through to DesignDIYLeagueFinish in original asm. */
__attribute__((used, externally_visible))
void _DesignDIYLeagueInit(void)
{
    if (diyLeagueSelectTeams != 0)
        _SetNumberOfTeamsToSelect();
    _DesignDIYLeagueFinish();
}

/* DesignDIYLeagueSetName — input text for league name (SWOS2.s line 41484) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueSetName(MenuEntry *entry)
{
    _InputText(25, (uint8_t *)entry);
}

/* DesignDIYLeagueChangePitchType — cycle pitch type (SWOS2.s line 41557) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueChangePitchType(MenuEntry *entry)
{
    uint16_t *pitchTypeField = (uint16_t *)&diyCompetitionStart.base.pitchType;
    *pitchTypeField += 1;
    if (*pitchTypeField == 7)
        *pitchTypeField = 0xFFFF;
    _CallDrawMenuItem(entry);
}

/* DesignDIYLeagueChangeStartMonth — cycle start month (SWOS2.s line 41567) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueChangeStartMonth(MenuEntry *entry)
{
    uint8_t *monthByte = (uint8_t *)&diyCompetitionStart.base.startMonth; /* DIYLEAGUE_STARTINGMONTH */
    *monthByte += 8;
    uint8_t val = *monthByte;
    if (val == 96) {
        *monthByte = 0;
        val = 0;
    }
    diyLeagueStartingMonth = (uint16_t)val / 8;
    _CallDrawMenuItem(entry);
}

/* DesignDIYLeagueChangeEndMonth — cycle end month (SWOS2.s line 41583) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueChangeEndMonth(MenuEntry *entry)
{
    uint8_t *monthByte = (uint8_t *)&diyCompetitionStart.base.endMonth; /* DIYLEAGUE_ENDINGMONTH */
    *monthByte += 8;
    uint8_t val = *monthByte;
    if (val == 96) {
        *monthByte = 0;
        val = 0;
    }
    diyLeagueEndingMonth = (uint16_t)val / 8;
    _CallDrawMenuItem(entry);
}

/* DesignDIYLeagueChangeNumberOfTeams — cycle num teams 2-24 (SWOS2.s line 41599) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueChangeNumberOfTeams(MenuEntry *entry)
{
    uint16_t *numTeams = (uint16_t *)&diyCompetitionStart.base.numTeams;
    *numTeams += 1;
    if (*numTeams >= 25)
        *numTeams = 2;
    entry->fgWord = *numTeams;  /* write number to entry's fg union */
    _CallDrawMenuItem(entry);
}

/* SetPlayEachTeamString — format "X N" string into entry (SWOS2.s line 41622) */
__attribute__((used, externally_visible))
void _SetPlayEachTeamString(MenuEntry *entry)
{
    uint16_t playEach = *(uint16_t *)&diyCompetitionStart.base.playEachTeam;
    uint8_t *out = (uint8_t *)entry->fg;
    _PrintFormatted(aX0, out, playEach, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* DesignDIYLeagueChangePlayEachTeam — cycle play each team 1-10 (SWOS2.s line 41611) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueChangePlayEachTeam(MenuEntry *entry)
{
    uint16_t *playEach = (uint16_t *)&diyCompetitionStart.base.playEachTeam;
    *playEach += 1;
    if (*playEach == 11)
        *playEach = 1;
    _SetPlayEachTeamString(entry);
    _CallDrawMenuItem(entry);
}

/* DesignDIYLeagueChangeNumPointsForWin — toggle 2/3 points (SWOS2.s line 41631) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueChangeNumPointsForWin(MenuEntry *entry)
{
    uint16_t *pts = (uint16_t *)diyPointsForAWin;
    *pts ^= 1;
    entry->fgWord = *pts;
    _CallDrawMenuItem(entry);
}

/* ConvertMinMaxSubstitutesNumbers — format min/max substitutes into menu strings (SWOS2.s line 41638) */
__attribute__((used, externally_visible))
void _ConvertMinMaxSubstitutesNumbers(void)
{
    uint8_t *minStr = (uint8_t *)menuEntries[16].fg;
    _Int2Ascii(diyLeagueMinSubstitutes, minStr);
    uint8_t *maxStr = (uint8_t *)menuEntries[17].fg;
    _Int2Ascii(diyLeagueMaxSubstitutes, maxStr);
}

/* DesignDIYLeagueChangeMinSubstitutes — cycle min subs 1..max (SWOS2.s line 41651) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueChangeMinSubstitutes(MenuEntry *entry)
{
    if (diyLeagueMaxSubstitutes == 0)
        return;
    diyLeagueMinSubstitutes += 1;
    if (diyLeagueMinSubstitutes > diyLeagueMaxSubstitutes)
        diyLeagueMinSubstitutes = 1;
    _ConvertMinMaxSubstitutesNumbers();
    _CallDrawMenuItem(entry);
}

/* DesignDIYLeagueChangeMaxSubstitutes — cycle max subs 0-5, adjust min (SWOS2.s line 41666) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueChangeMaxSubstitutes(MenuEntry *entry)
{
    diyLeagueMaxSubstitutes += 1;
    if (diyLeagueMaxSubstitutes > 5) {
        diyLeagueMaxSubstitutes = 0;
        diyLeagueMinSubstitutes = 0;
    }
    if (diyLeagueMaxSubstitutes == 1)
        diyLeagueMinSubstitutes = 1;
    _ConvertMinMaxSubstitutesNumbers();
    _CallDrawMenuItem(entry);
    _CallDrawMenuItem(&menuEntries[16]);  /* also redraw min substitutes entry */
}

/* ==========================================================================
 * Chunk 127 — DesignDIYCup Callbacks & Helpers (18 functions)
 * ========================================================================== */

extern uint16_t pitchType;
extern uint16_t diyCupPitchTypeOrSeasonSelection;
extern uint16_t diyCupStartMonthIndex;
extern uint16_t diyCupEndMonthIndex;
/* DesignDIYCupOnDraw now ported to C in Chunk 130 — call _DesignDIYCupOnDraw directly */

/* Static helper: compute round word offset from menu entry ordinal.
 * Replaces asm DesignDIYCup_GetRoundIndexFromMenuEntry which returned d1 and a0. */
static uint16_t getCupRoundWordOffset(MenuEntry *entry)
{
    int16_t d0 = (int16_t)(entry->ordinal - 0x12);
    int32_t d0l = d0;  /* ext.l */
    uint16_t roundIdx = (uint16_t)((uint32_t)d0l / 5);
    return roundIdx * 2;  /* lsl.w #1 */
}

/* DesignDIYCup_UpdateStartEndMonthIndices — compute month indices from byte fields (SWOS2.s line 44688) */
__attribute__((used, externally_visible))
void _DesignDIYCup_UpdateStartEndMonthIndices(void)
{
    uint8_t d0 = (uint8_t)diyCompetitionStart.base.startMonth;  /* DIYCOMPETITION_STARTMONTH (0x38) */
    if (d0 >= 96)
        d0 -= 96;
    diyCupStartMonthIndex = (uint16_t)d0 / 8;

    d0 = (uint8_t)diyCompetitionStart.base.endMonth;  /* DIYCOMPETITION_ENDMONTH (0x39) */
    if (d0 >= 0x60)
        d0 -= 0x60;
    diyCupEndMonthIndex = (uint16_t)d0 / 8;

    _DesignDIYCup_UpdateMinSubstitutesAndPointsStrings();
}

/* DesignDIYCup_UpdatePitchTypeOrSeasonSelectionVisibility — toggle seasonal/pitch UI (SWOS2.s line 44776) */
__attribute__((used, externally_visible))
void _DesignDIYCup_UpdatePitchTypeOrSeasonSelectionVisibility(void)
{
    diyCupPitchTypeOrSeasonSelection = *(uint16_t *)&diyCompetitionStart.base.pitchTypeOrSeason;

    if (gameType == 2 || gameType == 3 || gameType == 4)
        diyCupPitchTypeOrSeasonSelection = 1;

    if (diyCupPitchTypeOrSeasonSelection == 0) {
        /* Not seasonal: hide entries 5,6,7, show entry 4 */
        menuEntries[5].isInvisible = 1;
        menuEntries[6].isInvisible = 1;
        menuEntries[7].isInvisible = 1;
        menuEntries[4].isInvisible = 0;
    } else {
        /* Seasonal: hide entry 4, show entries 5,6,7 */
        menuEntries[4].isInvisible = 1;
        menuEntries[5].isInvisible = 0;
        menuEntries[6].isInvisible = 0;
        menuEntries[7].isInvisible = 0;
    }
}

/* DesignDIYCup_SetupTeamsSelectionUI — grey out editable entries for teams selection (SWOS2.s line 44711) */
__attribute__((used, externally_visible))
void _DesignDIYCup_SetupTeamsSelectionUI(void)
{
    MenuEntry *e = &menuEntries[2];
    for (int16_t i = 0x2D; i >= 0; i--) {
        if (e->background == 2) {  /* ENTRY_FRAME_AND_BACK_COLOR */
            uint16_t bgColor = e->bgColor;
            if ((bgColor & 0xF) != 0) {
                e->bgColor = (bgColor & 0xFFF0) | 9;  /* PINK_TO_BROWN_9 */
                e->isDisabled = 1;
            }
        }
        e++;
    }
    menuEntries[2].isInvisible = 1;
    menuEntries[0].isInvisible = 1;
    menuEntries[1].isInvisible = 0;
}

/* DesignDIYCupReinit — reinitialize cup design menu state (SWOS2.s line 44632) */
__attribute__((used, externally_visible))
void _DesignDIYCupReinit(void)
{
    if (pitchType != 0xFFFE) {  /* SEASONAL = 65534 = 0xFFFE */
        /* Non-seasonal: clear pitchTypeOrSeason, copy global pitchType */
        *(uint16_t *)&diyCompetitionStart.base.pitchTypeOrSeason = 0;
        *(uint16_t *)&diyCompetitionStart.base.pitchType = pitchType;

        /* Grey out entry[3] */
        menuEntries[3].bgColor = (menuEntries[3].bgColor & 0xFFF0) | 9;  /* PINK_TO_BROWN_9 */
        menuEntries[3].isDisabled = 1;

        /* Grey out entry[4] */
        menuEntries[4].bgColor = (menuEntries[4].bgColor & 0xFFF0) | 9;
        menuEntries[4].isDisabled = 1;
    }

    _DesignDIYCup_UpdatePitchTypeOrSeasonSelectionVisibility();
    _DesignDIYCup_UpdateStartEndMonthIndices();

    if (diyCupLookOnly != 0) {
        /* Look-only mode */
        _DesignDIYCup_SetupTeamsSelectionUI();
        menuEntries[48].isInvisible = 1;
        menuEntries[49].isInvisible = 1;
        menuEntries[51].isInvisible = 1;

        /* Color entries for current round */
        uint16_t val = (uint16_t)(&diyCompetitionStart.base)->currentRoundIndex;
        uint16_t startIdx = val * 5 + 0x12;
        MenuEntry *e = _CalcMenuEntryAddress(startIdx);
        for (int16_t i = 4; i >= 0; i--) {
            e->bgColor = (e->bgColor & 0xFFF0) | 0xB;
            e++;
        }
        _SetCurrentEntry(0x32);
    } else if (diyCupSelectTeams != 0) {
        /* Select teams mode */
        _DesignDIYCup_SetupTeamsSelectionUI();
        menuEntries[49].isInvisible = 1;
        _SetCurrentEntry(0x30);
    } else {
        /* Normal edit mode */
        menuEntries[48].isInvisible = 1;
        menuEntries[49].isInvisible = 0;
        menuEntries[51].isInvisible = 1;
    }
}

/* DesignDIYCupInit — init cup design menu (SWOS2.s line 44623)
 * Falls through to DesignDIYCupReinit in original asm. */
__attribute__((used, externally_visible))
void _DesignDIYCupInit(void)
{
    if (diyCupSelectTeams != 0)
        _DesignDIYCup_UpdateSelectTeamsPrompt();
    _DesignDIYCupReinit();
}

/* DesignDIYCupOnSelect2 — input text for cup name (SWOS2.s line 44741) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect2(MenuEntry *entry)
{
    _InputText(0x19, (uint8_t *)entry);
}

/* DesignDIYCupOnSelect3 — toggle pitch type / season selection (SWOS2.s line 44805) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect3(MenuEntry *entry)
{
    (void)entry;
    *(uint16_t *)&diyCompetitionStart.base.pitchTypeOrSeason ^= 1;
    _DesignDIYCup_UpdatePitchTypeOrSeasonSelectionVisibility();
    _CallDrawMultipleItems(5, &menuEntries[3]);
}

/* DesignDIYCupOnSelect4 — cycle pitch type 0-6, wraps to 0xFFFF (SWOS2.s line 44815) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect4(MenuEntry *entry)
{
    uint16_t *pt = (uint16_t *)&diyCompetitionStart.base.pitchType;
    *pt += 1;
    if (*pt == 7)
        *pt = 0xFFFF;
    _CallDrawMenuItem(entry);
}

/* DesignDIYCupOnSelect5 — cycle start month (SWOS2.s line 44825) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect5(MenuEntry *entry)
{
    uint8_t *monthByte = (uint8_t *)&diyCompetitionStart.base.startMonth;
    *monthByte += 8;
    uint8_t val = *monthByte;
    if (val == 0x60) {
        *monthByte = 0;
        val = 0;
    }
    diyCupStartMonthIndex = (uint16_t)val / 8;
    _CallDrawMenuItem(entry);
}

/* DesignDIYCupOnSelect7 — cycle end month (SWOS2.s line 44841) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect7(MenuEntry *entry)
{
    uint8_t *monthByte = (uint8_t *)&diyCompetitionStart.base.endMonth;
    *monthByte += 8;
    uint8_t val = *monthByte;
    if (val == 0x60) {
        *monthByte = 0;
        val = 0;
    }
    diyCupEndMonthIndex = (uint16_t)val / 8;
    _CallDrawMenuItem(entry);
}

/* DesignDIYCupOnSelect9 — cycle play-each-team count 0-2 (SWOS2.s line 44857) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect9(MenuEntry *entry)
{
    DIYCompetition *compCycleS = &diyCompetitionStart.base;
    compCycleS->playEachTeam += 1;
    if ((uint16_t)compCycleS->playEachTeam >= 3)
        compCycleS->playEachTeam = 0;
    _DesignDIYCupOnDraw();
    _CallDrawMenuItem(entry);
}

/* DesignDIYCupOnSelect11 — cycle number of rounds 1-6 (SWOS2.s line 44869) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect11(MenuEntry *entry)
{
    (void)entry;
    DIYCompetition *compRndS = &diyCompetitionStart.base;
    compRndS->numRounds += 1;
    if ((uint16_t)compRndS->numRounds >= 7)
        compRndS->numRounds = 1;
    uint16_t r = (uint16_t)compRndS->numRounds;
    compRndS->numTeams = (int16_t)(1 << r);
    _DesignDIYCupOnDraw();
    _DrawMenu();
}

/* DesignDIYCupOnSelect13 — cycle min substitutes (SWOS2.s line 44889) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect13(MenuEntry *entry)
{
    uint16_t points = *(uint16_t *)diyPointsForAWin;
    if (points == 0)
        return;
    diyMinSubstitutes += 1;
    if (diyMinSubstitutes > points)
        diyMinSubstitutes = 1;
    _DesignDIYCup_UpdateMinSubstitutesAndPointsStrings();
    _CallDrawMenuItem(entry);
}

/* DesignDIYCupOnSelect15 — cycle points for a win 0-5 (SWOS2.s line 44904) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect15(MenuEntry *entry)
{
    uint16_t *pts = (uint16_t *)diyPointsForAWin;
    *pts += 1;
    if (*pts > 5) {
        *pts = 0;
        diyMinSubstitutes = 0;
    }
    if (*pts == 1)
        diyMinSubstitutes = 1;
    _DesignDIYCup_UpdateMinSubstitutesAndPointsStrings();
    _CallDrawMenuItem(entry);
    _CallDrawMenuItem(&menuEntries[13]);  /* also redraw min substitutes entry */
}

/* DesignDIYCupOnSelect20 — XOR round type (SWOS2.s line 44923) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect20(MenuEntry *entry)
{
    uint16_t wordOff = getCupRoundWordOffset(entry);
    diyCompetitionStart.matchData.cup.roundLegs[wordOff >> 1] ^= 3;
    _DesignDIYCupOnDraw();
    _CallDrawMenuItem(entry);
    _CallDrawMultipleItems(2, &menuEntries[8]);
}

/* DesignDIYCupOnSelect21 — cycle qualifiers per round 0-2 (SWOS2.s line 44934) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect21(MenuEntry *entry)
{
    uint16_t round = getCupRoundWordOffset(entry) >> 1;
    /* asm: $72(a0,d1.w), a0=diyCompetitionStart, d1=round*2 -> parent 0x72 =
     * matchData.cup.roundExtraTime[round] (DIY-cup design "qualifiers per round").
     * Typed access auto-corrects the 64-bit namePtr-growth shift. */
    uint16_t *field = &diyCompetitionStart.matchData.cup.roundExtraTime[round];
    *field += 1;
    if (*field == 3)
        *field = 0;
    _DesignDIYCupOnDraw();
    _CallDrawMenuItem(entry);
}

/* DesignDIYCupOnSelect22 — cycle byes per round 0-2 (SWOS2.s line 44946) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect22(MenuEntry *entry)
{
    uint16_t round = getCupRoundWordOffset(entry) >> 1;
    /* asm: $7E(a0,d1.w), a0=diyCompetitionStart, d1=round*2 -> parent 0x7E =
     * matchData.cup.roundPenalties[round] (DIY-cup design "byes per round").
     * Typed access auto-corrects the 64-bit namePtr-growth shift. */
    uint16_t *field = &diyCompetitionStart.matchData.cup.roundPenalties[round];
    *field += 1;
    if (*field == 3)
        *field = 0;
    _DesignDIYCupOnDraw();
    _CallDrawMenuItem(entry);
}

/* ==========================================================================
 * Chunk 128 — DesignDIYTournament Callbacks & Helpers (17+1s functions)
 * ========================================================================== */

extern uint16_t diyTournamentUseSeasonalPitchEffective;
extern uint16_t diyTournamentStartMonthPage;
extern uint16_t diyTournamentEndMonthPage;
/* DesignDIYTournamentOnDraw now ported to C in Chunk 129 — call _DesignDIYTournamentOnDraw directly */

/* Static helper: get tournament stage block pointer from menu entry ordinal.
 * Replaces asm DesignDIYTournament_GetStageBlockFromMenuEntry (returns a3).
 * Side effect: writes stageIndex to diyFileBufferCopy[0x54]. */
static uint8_t *getTournamentStageBlock(MenuEntry *entry)
{
    int16_t d0 = (int16_t)(entry->ordinal - 0x11);
    int32_t d0l = d0;  /* ext.l */
    uint16_t stageIdx = (uint16_t)((uint32_t)d0l / 0xC);
    diyFileBufferCopy.header.roundComplete = (int16_t)stageIdx;
    uint16_t offset = stageIdx * 0x76;
    return (uint8_t *)&diyFileBufferCopy + offset;
}

/* Static helper: get the typed DIYTournamentSlot record for the entry's
 * stage. The stage-block byte offset has the slot record embedded at
 * +0x15A, so this just composes the two helpers. */
static DIYTournamentSlot *getTournamentSlotRecord(MenuEntry *entry)
{
    return (DIYTournamentSlot *)(getTournamentStageBlock(entry) + offsetof(DIYTournamentState, slots));
}

/* DesignDIYTournament_UpdateMonthPageIndices — compute month page indices (SWOS2.s line 49585) */
__attribute__((used, externally_visible))
void _DesignDIYTournament_UpdateMonthPageIndices(void)
{
    uint8_t d0 = (uint8_t)diyFileBufferCopy.header.startMonth;  /* DIYCOMPETITION_STARTMONTH (0x38) */
    if (d0 >= 0x60)
        d0 -= 0x60;
    diyTournamentStartMonthPage = (uint16_t)d0 / 8;

    d0 = (uint8_t)diyFileBufferCopy.header.endMonth;  /* DIYCOMPETITION_ENDMONTH (0x39) */
    if (d0 >= 0x60)
        d0 -= 0x60;
    diyTournamentEndMonthPage = (uint16_t)d0 / 8;

    _DesignDIYTournament_UpdateTwoNumericFieldsText();
}

/* DesignDIYTournament_ApplyInactiveMenuColors — grey out entries for teams selection (SWOS2.s line 49607) */
__attribute__((used, externally_visible))
void _DesignDIYTournament_ApplyInactiveMenuColors(void)
{
    MenuEntry *e = &menuEntries[2];
    for (int16_t i = 0x56; i >= 0; i--) {  /* 87 iterations */
        if (e->background == 2) {
            uint16_t bgColor = e->bgColor;
            if ((bgColor & 0xF) != 0) {
                e->bgColor = (bgColor & 0xFFF0) | 9;  /* PINK_TO_BROWN_9 */
                e->isDisabled = 1;
            }
        }
        e++;
    }
    menuEntries[2].isInvisible = 1;
    menuEntries[0].isInvisible = 1;
    menuEntries[1].isInvisible = 0;
}

/* DesignDIYTournament_UpdatePitchTypeSeasonalUI — toggle seasonal/pitch visibility (SWOS2.s line 49675) */
__attribute__((used, externally_visible))
void _DesignDIYTournament_UpdatePitchTypeSeasonalUI(void)
{
    DIYCompetition *buf = &diyFileBufferCopy.header;
    diyTournamentUseSeasonalPitchEffective = (uint16_t)buf->pitchTypeOrSeason;

    if (gameType == 2 || gameType == 3 || gameType == 4)
        diyTournamentUseSeasonalPitchEffective = 1;

    if (diyTournamentUseSeasonalPitchEffective == 0) {
        /* Not seasonal: hide entries 5,6,7, show entry 4 */
        menuEntries[5].isInvisible = 1;
        menuEntries[6].isInvisible = 1;
        menuEntries[7].isInvisible = 1;
        menuEntries[4].isInvisible = 0;
    } else {
        /* Seasonal: hide entry 4, show entries 5,6,7 */
        menuEntries[4].isInvisible = 1;
        menuEntries[5].isInvisible = 0;
        menuEntries[6].isInvisible = 0;
        menuEntries[7].isInvisible = 0;
    }
}

/* DesignDIYTournamentReinit — reinitialize tournament design menu state (SWOS2.s line 49522) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentReinit(void)
{
    DIYCompetition *buf = &diyFileBufferCopy.header;
    if (pitchType != 0xFFFE) {  /* SEASONAL */
        buf->pitchTypeOrSeason = 0;
        buf->pitchType         = (int16_t)pitchType;

        /* Grey out entry[3] */
        menuEntries[3].bgColor = (menuEntries[3].bgColor & 0xFFF0) | 9;
        menuEntries[3].isDisabled = 1;

        /* Grey out entry[4] */
        menuEntries[4].bgColor = (menuEntries[4].bgColor & 0xFFF0) | 9;
        menuEntries[4].isDisabled = 1;
    }

    _DesignDIYTournament_UpdatePitchTypeSeasonalUI();
    _DesignDIYTournament_UpdateMonthPageIndices();

    if (diyTournamentLookOnly != 0) {
        /* Look-only mode */
        _DesignDIYTournament_ApplyInactiveMenuColors();
        menuEntries[89].isInvisible = 1;
        menuEntries[90].isInvisible = 1;
        menuEntries[92].isInvisible = 1;

        /* Color entries for current stage block */
        uint16_t val = (uint16_t)diyFileBufferCopy.header.roundComplete;
        uint16_t startIdx = val * 0xC + 0x11;
        MenuEntry *e = _CalcMenuEntryAddress(startIdx);
        for (int16_t i = 0xB; i >= 0; i--) {  /* 12 iterations */
            if (e->background == 2) {
                uint16_t bgColor = e->bgColor;
                if ((bgColor & 0xF) != 0) {
                    e->bgColor = (bgColor & 0xFFF0) | 0xB;
                }
            }
            e++;
        }
        _SetCurrentEntry(0x5B);
    } else if (diyTournamentSelectTeams != 0) {
        /* Select teams mode */
        _DesignDIYTournament_ApplyInactiveMenuColors();
        menuEntries[90].isInvisible = 1;
        _SetCurrentEntry(0x59);
    } else {
        /* Normal edit mode */
        menuEntries[89].isInvisible = 1;
        menuEntries[90].isInvisible = 0;
        menuEntries[92].isInvisible = 1;
        menuEntries[91].x += 0x20;
    }
}

/* DesignDIYTournamentInit — init tournament design menu (SWOS2.s line 49513)
 * Falls through to DesignDIYTournamentReinit. */
__attribute__((used, externally_visible))
void _DesignDIYTournamentInit(void)
{
    if (diyTournamentLookOnly == 0)
        _DesignDIYTournament_UpdateSelectTeamsPrompt();
    _DesignDIYTournamentReinit();
}

/* DesignDIYTournamentOnSelect2 — input text for tournament name (SWOS2.s line 49637) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect2(MenuEntry *entry)
{
    _InputText(0x19, (uint8_t *)entry);
}

/* DesignDIYTournamentOnSelect3 — toggle pitch type / season (SWOS2.s line 49704) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect3(MenuEntry *entry)
{
    DIYCompetition *buf = &diyFileBufferCopy.header;
    (void)entry;
    buf->pitchTypeOrSeason ^= 1;
    _DesignDIYTournament_UpdatePitchTypeSeasonalUI();
    _CallDrawMultipleItems(5, &menuEntries[3]);
}

/* DesignDIYTournamentOnSelect4 — cycle pitch type 0-6, wraps to 0xFFFF (SWOS2.s line 49714) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect4(MenuEntry *entry)
{
    uint16_t *pt = (uint16_t *)&diyFileBufferCopy.header.pitchType;  /* DIYCOMPETITION_PITCHTYPE (0x36) */
    *pt += 1;
    if (*pt == 7)
        *pt = 0xFFFF;
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect5 — cycle start month (SWOS2.s line 49724) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect5(MenuEntry *entry)
{
    uint8_t *monthByte = (uint8_t *)&diyFileBufferCopy.header.startMonth;  /* DIYCOMPETITION_STARTMONTH (0x38) */
    *monthByte += 8;
    uint8_t val = *monthByte;
    if (val == 0x60) {
        *monthByte = 0;
        val = 0;
    }
    diyTournamentStartMonthPage = (uint16_t)val / 8;
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect7 — cycle end month (SWOS2.s line 49740) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect7(MenuEntry *entry)
{
    uint8_t *monthByte = (uint8_t *)&diyFileBufferCopy.header.endMonth;  /* DIYCOMPETITION_ENDMONTH (0x39) */
    *monthByte += 8;
    uint8_t val = *monthByte;
    if (val == 0x60) {
        *monthByte = 0;
        val = 0;
    }
    diyTournamentEndMonthPage = (uint16_t)val / 8;
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect9 — cycle field at base+0x4A (SWOS2.s line 49756) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect9(MenuEntry *entry)
{
    getTournamentStageBlock(entry);  /* side effect: sets stageIndex at 0x54 */
    DIYCompetition *bufCopyS = &diyFileBufferCopy.header;
    bufCopyS->menuState += 1;
    if ((uint16_t)bufCopyS->menuState >= 3)
        bufCopyS->menuState = 0;
    _DesignDIYTournamentOnDraw();
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect11 — cycle min substitutes per team (SWOS2.s line 49772) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect11(MenuEntry *entry)
{
    DIYCompetition *buf = &diyFileBufferCopy.header;
    uint16_t maxVal = (uint16_t)buf->numTeamsInStage;
    if (maxVal == 0)
        return;
    buf->roundIterator += 1;
    if ((uint16_t)buf->roundIterator > maxVal)
        buf->roundIterator = 1;
    _DesignDIYTournament_UpdateTwoNumericFieldsText();
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect13 — cycle num games per team 0-5 (SWOS2.s line 49787) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect13(MenuEntry *entry)
{
    uint16_t *field14 = (uint16_t *)&diyFileBufferCopy.header.numTeamsInStage;  /* DIYCOMPETITION_NUMTEAMSINSTAGE (0x4E) */
    uint16_t *field12 = (uint16_t *)&diyFileBufferCopy.header.roundIterator;     /* DIYCOMPETITION_ROUNDITERATOR (0x4C) */
    *field14 += 1;
    if (*field14 > 5) {
        *field14 = 0;
        *field12 = 0;
    }
    if (*field14 == 1)
        *field12 = 1;
    _DesignDIYTournament_UpdateTwoNumericFieldsText();
    _CallDrawMenuItem(entry);
    _CallDrawMenuItem(&menuEntries[11]);  /* redraw companion entry */
}

/* DesignDIYTournamentOnSelect21 — toggle slot.seedingType (SWOS2.s line 49806) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect21(MenuEntry *entry)
{
    DIYTournamentSlot *slot = getTournamentSlotRecord(entry);
    slot->seedingType ^= 1;
    _DesignDIYTournamentOnDraw();
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect24 — cycle slot.initConst170 0-2 (SWOS2.s line 49829) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect24(MenuEntry *entry)
{
    DIYTournamentSlot *slot = getTournamentSlotRecord(entry);
    slot->initConst170 += 1;
    if (slot->initConst170 == 3)
        slot->initConst170 = 0;
    _DesignDIYTournamentOnDraw();
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect25 — cycle slot.formatFlag172 0-2 (SWOS2.s line 49841) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect25(MenuEntry *entry)
{
    DIYTournamentSlot *slot = getTournamentSlotRecord(entry);
    slot->formatFlag172 += 1;
    if (slot->formatFlag172 == 3)
        slot->formatFlag172 = 0;
    _DesignDIYTournamentOnDraw();
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect26 — toggle slot.initConst168 (SWOS2.s line 49853) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect26(MenuEntry *entry)
{
    DIYTournamentSlot *slot = getTournamentSlotRecord(entry);
    slot->initConst168 ^= 1;
    _DesignDIYTournamentOnDraw();
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect28 — cycle slot.playEachTeam 1-4 (SWOS2.s line 49861) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect28(MenuEntry *entry)
{
    DIYTournamentSlot *slot = getTournamentSlotRecord(entry);
    slot->playEachTeam += 1;
    if (slot->playEachTeam == 5)
        slot->playEachTeam = 1;
    _DesignDIYTournamentOnDraw();
    _CallDrawMenuItem(entry);
}

/* ==========================================================================
 * Chunk 129 — DesignDIYTournament Remaining Functions (8+2s functions)
 *   OnDraw, OnSelect23, OnSelect20, AdjustStageQualifiers,
 *   CacheStageSelection, RedrawStageItemsFromBlock,
 *   RecalculateNumStages, +2 static math helpers
 * ========================================================================== */

/* Externs for Chunk 129 */
extern uint16_t diyTournamentSelectedStageIndex1Based;
extern uint8_t *diyTournamentSelectedStagePtr;
extern uint16_t diyTournamentPrevValueBackup;
extern uint16_t controlMask;

/* String constants used by OnDraw */
extern const uint8_t a3rdPlayOff[];      /* "3RD PLAY-OFF" */
extern const uint8_t aFinal_4[];         /* "FINAL" */
extern const uint8_t aSemiFinal[];       /* "SEMI-FINAL" */
extern const uint8_t aQuarterFinal[];    /* "QUARTER-FINAL" */
extern const uint8_t aKnockout[];        /* "KNOCKOUT" */
extern const uint8_t a0GroupsOf1[];      /* "%0 GROUPS OF %1" */
extern const uint8_t a1GroupOf1[];       /* "1 GROUP OF %1" */
extern const uint8_t aStar[];            /* "*" */
extern const uint8_t asc_135D9A[];       /* "-" */
extern const uint8_t aMatchWinnersQu[];  /* "(MATCH WINNERS QUALIFY)" */
extern const uint8_t aMatchWinnerWin[];  /* "(MATCH WINNER WINS TOURNAMENT)" */
extern const uint8_t aMatchWinnersOn[];  /* "(MATCH WINNERS ON AGGREGATE QUALIFY)" */
extern const uint8_t aMatchWinnerOnA[];  /* "(MATCH WINNER ON AGGREGATE WINS TOURNAM..." */
extern const uint8_t aGroupWinnerWin[];  /* "(GROUP WINNER WINS TOURNAMENT)" */
extern const uint8_t aTop0InGroupQua[];  /* "(TOP %0 IN GROUP QUALIFY)" */
extern const uint8_t aWinnersOfEachG[];  /* "(WINNERS OF EACH GROUP QUALIFY)" */
extern const uint8_t aWinnersOfEachG_0[];/* "(WINNERS OF EACH GROUP AND BEST RUNNER-..." */
extern const uint8_t aWinnersOfEachG_1[];/* "(WINNERS OF EACH GROUP AND BEST %1 RUNN..." */
extern const uint8_t aTop0InEachGrou[];  /* "(TOP %0 IN EACH GROUP QUALIFY)" */
extern const uint8_t aTop0InEachGrou_0[];/* "(TOP %0 IN EACH GROUP AND BEST RUNNER-U..." */
extern const uint8_t aTop0InEachGrou_1[];/* "(TOP %0 IN EACH GROUP AND BEST %1 RUNNE..." */

/* --- Static helper: computeGroupLayoutForTeams (SWOS2.s line 50168) ---
 * Asm: DIYTournament_ComputeGroupLayoutForTeams
 * Input: candidate (d0), current (d6), stage (d7)
 * Output: numGroups (d1), groupSize (d2), valid (d3 = 1 success, 0 fail) */
typedef struct { uint16_t numGroups; uint16_t groupSize; int16_t valid; } GroupLayoutResult;

static GroupLayoutResult computeGroupLayoutForTeams(uint16_t candidate, uint16_t current, uint16_t stage)
{
    GroupLayoutResult r;
    r.numGroups = 0;
    r.groupSize = 0;
    r.valid = 0;

    if (candidate == 0x3A || candidate == 0x3E)
        return r; /* invalid */

    if (current == 1 && candidate == 1) {
        r.numGroups = 1;
        r.groupSize = 1;
        r.valid = 1;
        return r;
    }

    if (candidate <= current)
        return r; /* invalid: candidate must be > current */

    uint16_t d2 = candidate;
    uint16_t d1 = 0;
    int32_t d3 = (int16_t)candidate; /* ext.l */

    /* Check if candidate is even and current*2 == candidate */
    if (!(d3 & 1)) {
        uint16_t doubled = current << 1;
        if (doubled == (uint16_t)d3) {
            if (stage == 6) {
                if ((uint16_t)d3 == 2) {
                    r.numGroups = 0;
                    r.groupSize = d2;
                    r.valid = 1;
                    return r;
                }
                /* fall through to search loop */
            } else {
                if (stage == 5 && (uint16_t)d3 > 0x30) {
                    /* fall through to search loop */
                } else {
                    r.numGroups = 0;
                    r.groupSize = d2;
                    r.valid = 1;
                    return r;
                }
            }
        }
    }

    /* Search loop: find valid numGroups d1 that divides d3 evenly */
    for (;;) {
        d1++;
        if (d1 == 9) return r; /* invalid */
        if (d1 > current) return r; /* invalid */
        if (stage == 6 && d1 == 2) return r; /* invalid */
        if (d1 == (uint16_t)d3) return r; /* invalid: numGroups == total */
        {
            uint32_t divResult = (uint32_t)d3 / (uint32_t)d1;
            uint32_t remainder = (uint32_t)d3 % (uint32_t)d1;
            if (remainder != 0) continue; /* doesn't divide evenly */
            if (divResult > 0x18) continue; /* group size > 24 */
            d2 = (uint16_t)divResult;
        }
        break;
    }

    r.numGroups = d1;
    r.groupSize = d2;
    r.valid = 1;
    return r;
}

/* --- Static helper: adjustGroupLayoutForMaxTeams (SWOS2.s line 50234) ---
 * Asm: DIYTournament_AdjustGroupLayoutForMaxTeams
 * Input: numGroups (d1), groupSize (d2), maxAllowed (d6), stage (d7)
 * Output: adjusted numGroups (d1), groupSize (d2) */
typedef struct { uint16_t numGroups; uint16_t groupSize; } GroupLayoutValues;

static GroupLayoutValues adjustGroupLayoutForMaxTeams(uint16_t numGroups, uint16_t groupSize,
                                                       uint16_t maxAllowed, uint16_t stage)
{
    GroupLayoutValues v;
    int32_t d3;

    d3 = (int32_t)(uint32_t)numGroups * (uint32_t)groupSize; /* mulu */

    if (numGroups == 0) {
        numGroups = 1;
        d3 = (int16_t)groupSize; /* ext.l of d2 (word) */
    } else {
        numGroups++;
    }

    /* Search loop */
    for (;;) {
        if (numGroups == 9) goto fallback;
        if (numGroups > maxAllowed) goto fallback;
        if (stage == 6 && numGroups == 2) goto fallback;
        if (numGroups == (uint16_t)d3) goto fallback;
        {
            uint32_t divResult = (uint32_t)d3 / (uint32_t)numGroups;
            uint32_t remainder = (uint32_t)d3 % (uint32_t)numGroups;
            if (remainder != 0) { numGroups++; continue; }
            if (divResult > 0x18) { numGroups++; continue; }
            groupSize = (uint16_t)divResult;
            v.numGroups = numGroups;
            v.groupSize = groupSize;
            return v;
        }
    }

fallback:
    /* Check if d3 is even and current*2 == d3 */
    if (!(d3 & 1)) {
        uint16_t doubled = maxAllowed << 1;
        if (doubled == (uint16_t)d3) {
            /* Special cases for stage 6 and 5 */
            if (stage == 6) {
                if ((uint16_t)d3 != 2) {
                    /* Reset: numGroups=1, groupSize=d3, restart */
                    numGroups = 1;
                    groupSize = (uint16_t)d3;
                    goto restart_search;
                }
            } else if (stage == 5) {
                if ((uint16_t)d3 > 0x30) {
                    numGroups = 1;
                    groupSize = (uint16_t)d3;
                    goto restart_search;
                }
            }
            /* d1=0 (no groups), d2=d3 (all in one) */
            v.numGroups = 0;
            v.groupSize = (uint16_t)d3;
            return v;
        }
    }
    /* Default fallback: reset to 1 group, try again */
    numGroups = 1;
    groupSize = (uint16_t)d3;
restart_search:
    /* Re-enter the search loop from the increment step */
    for (;;) {
        numGroups++;
        if (numGroups == 9) break;
        if (numGroups > maxAllowed) break;
        if (stage == 6 && numGroups == 2) break;
        if (numGroups == (uint16_t)d3) break;
        {
            uint32_t divResult = (uint32_t)d3 / (uint32_t)numGroups;
            uint32_t remainder = (uint32_t)d3 % (uint32_t)numGroups;
            if (remainder != 0) continue;
            if (divResult > 0x18) continue;
            groupSize = (uint16_t)divResult;
            v.numGroups = numGroups;
            v.groupSize = groupSize;
            return v;
        }
    }
    /* Should not reach here in normal operation, but safety: */
    v.numGroups = numGroups;
    v.groupSize = groupSize;
    return v;
}

/* --- _DIYTournament_RecalculateNumStages (SWOS2.s line 50128) ---
 * Counts stages until one has slot.divisionFactor == 1 (terminal stage), max 6 */
__attribute__((used, externally_visible))
void _DIYTournament_RecalculateNumStages(void)
{
    DIYTournamentSlot *slot = diyFileBufferCopy.slots;
    uint16_t d0 = 1;
    while (d0 < 6) {
        if (slot->divisionFactor == 1)
            break;
        slot += 1;   /* next tournament slot (sizeof == stride 0x76, both arches) */
        d0++;
    }
    diyFileBufferCopy.header.totalMatchesInRound = (int16_t)d0;
}

/* --- _DesignDIYTournament_CacheStageSelection (SWOS2.s line 49978) ---
 * Computes stage index from menu entry ordinal and caches pointer+index */
__attribute__((used, externally_visible))
void _DesignDIYTournament_CacheStageSelection(MenuEntry *entry)
{
    int16_t d0 = (int16_t)(entry->ordinal - 0x11);
    int32_t d0l = d0; /* ext.l */
    uint16_t stageIdx = (uint16_t)((uint32_t)d0l / 0xC);
    diyTournamentSelectedStageIndex1Based = stageIdx + 1;
    diyTournamentSelectedStagePtr = (uint8_t *)&diyFileBufferCopy + (uint32_t)stageIdx * 0x76;
}

/* --- _DesignDIYTournament_RedrawStageItemsFromBlock (SWOS2.s line 50114) ---
 * Converts block pointer to menu entry ordinal and redraws items.
 * blk=a0, count=d1 in asm. Computes: ordinal = ((blk-diyFileBufferCopy)/0x76)*0xC + 0x11 */
__attribute__((used, externally_visible))
void _DesignDIYTournament_RedrawStageItemsFromBlock(uint8_t *blk, uint16_t count)
{
    uint32_t offset = (uint32_t)(blk - (uint8_t *)&diyFileBufferCopy);
    uint16_t d0 = (uint16_t)(offset / 0x76);
    d0 = d0 * 0xC;
    d0 += 0x11;
    MenuEntry *a0 = _CalcMenuEntryAddress(d0);
    uint16_t numItems = count * 0xC;
    _CallDrawMultipleItems(numItems, a0);
}

/* --- _DesignDIYTournamentOnDraw (SWOS2.s line 49699) ---
 * Main draw callback for the tournament design menu.
 * Iterates 6 stages, configures visibility/content of menu entries.
 * IDA bug: ($2A26).w is menuEntries[9] (hardcoded absolute address). */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnDraw(void)
{
    DIYTournamentSlot *slot = diyFileBufferCopy.slots;
    uint16_t numStages = (uint16_t)diyFileBufferCopy.header.totalMatchesInRound;
    int16_t scanCount = (int16_t)numStages - 1;
    int16_t found3rdPlayoff = 0;

    /* Pre-scan: find knockout stage (isLeagueFormat==0) with formatFlag16E==2 */
    for (int16_t i = scanCount; i >= 0; i--) {
        if (slot->isLeagueFormat == 0) {
            if (slot->formatFlag16E == 2) {
                found3rdPlayoff = 1;
                break;
            }
        }
        slot += 1;   /* next tournament slot (sizeof == stride 0x76, both arches) */
    }

    if (!found3rdPlayoff) {
        /* Hide entries 9 and 8 */
        menuEntries[9].isInvisible = 1;
        menuEntries[8].isInvisible = 1;
    } else {
        /* Show entries 9 and 8. Note: asm used ($2A26).w which is menuEntries[9] */
        menuEntries[9].isInvisible = 0;
        menuEntries[8].isInvisible = 0;
    }

    /* Main loop: configure entries for each of 6 stage slots */
    MenuEntry *a4 = &menuEntries[17]; /* menuEntries+$3B8 */
    slot = diyFileBufferCopy.slots;
    int16_t d7 = 0;

    for (d7 = 0; d7 < 6; d7++) {
        /* Check if this slot is past the last active stage */
        if (d7 == (int16_t)numStages) {
            /* +0xEC peeks back at previous slot's numTeams field
             * (slot[d7-1].numTeams = current_slot - 0x76 + 0x162 = current_slot + 0xEC). */
            DIYTournamentSlot *prevSlot = (DIYTournamentSlot *)((uint8_t *)slot - 0x76);
            if (prevSlot->numTeams == 2)
                goto fill_remaining_invisible_1;
        }

        /* Entry pair: show both */
        a4->isInvisible = 0;
        a4++;
        a4->isInvisible = 0;

        /* Conditionally hide based on stage 5 or elimination type */
        if (d7 == 5) {
            a4->isInvisible = 1;
        } else if (slot->numTeams == 2) {
            if (slot->doubleCount == 0)
                a4->isInvisible = 1;
        }

        /* Next entry: show, set content to numTeamsInGroup */
        a4++;
        a4->isInvisible = 0;
        a4->fgWord = slot->numTeams;

        /* Next entry: advance */
        a4++;

        /* Check if d7 == numStages (past active stages) */
        if (d7 == (int16_t)numStages)
            goto fill_remaining_hidden;

        /* Show entry and set stage name string */
        a4->isInvisible = 0;
        {
            uint8_t *dst = (uint8_t *)a4->fg;
            if (slot->isLeagueFormat == 0) {
                /* Knockout stage: determine round name */
                const uint8_t *name;
                if (slot->doubleCount != 0) {
                    name = a3rdPlayOff; /* "3RD PLAY-OFF" */
                } else if (slot->numTeams == 2) {
                    name = aFinal_4; /* "FINAL" */
                } else if (slot->numTeams == 4) {
                    name = aSemiFinal; /* "SEMI-FINAL" */
                } else if (slot->numTeams == 8) {
                    name = aQuarterFinal; /* "QUARTER-FINAL" */
                } else {
                    name = aKnockout; /* "KNOCKOUT" */
                }
                _StringCopy(name, dst);
            } else {
                /* Group stage: "%0 GROUPS OF %1" or "1 GROUP OF %1" */
                uint16_t numGrp = slot->groups;
                uint16_t grpSize = slot->teamsPerGroup;
                const uint8_t *fmt;
                if (numGrp == 1) {
                    fmt = a1GroupOf1; /* "1 GROUP OF %1" */
                } else {
                    fmt = a0GroupsOf1; /* "%0 GROUPS OF %1" */
                }
                _PrintFormatted(fmt, dst, numGrp, grpSize, 0, 0, 0, 0, 0, 0, 0, 0);
            }
        }

        /* Next entry: home/away star indicator */
        a4++;
        a4->isInvisible = 0;
        if (slot->seedingType != 0) {
            a4->fg = (uintptr_t)(uintptr_t)aStar;  /* "*" */
        } else {
            a4->fg = (uintptr_t)(uintptr_t)asc_135D9A; /* "-" */
        }

        /* Next entry: qualification description */
        a4++;
        a4->isInvisible = 0;
        {
            uint8_t *dst = (uint8_t *)a4->fg;
            if (slot->isLeagueFormat == 0) {
                /* Knockout qualification text */
                if (slot->formatFlag16E != 2) {
                    if (slot->divisionFactor == 1) {
                        _StringCopy(aMatchWinnerWin, dst); /* "(MATCH WINNER WINS TOURNAMENT)" */
                    } else {
                        _StringCopy(aMatchWinnersQu, dst); /* "(MATCH WINNERS QUALIFY)" */
                    }
                } else {
                    if (slot->divisionFactor == 1) {
                        _StringCopy(aMatchWinnerOnA, dst); /* "(MATCH WINNER ON AGGREGATE WINS TOURNAM..." */
                    } else {
                        _StringCopy(aMatchWinnersOn, dst); /* "(MATCH WINNERS ON AGGREGATE QUALIFY)" */
                    }
                }
            } else {
                /* Group qualification text */
                uint16_t d2 = slot->groups;
                uint16_t d0 = slot->divisionFactor;
                const uint8_t *fmt;

                if (d0 == 1) {
                    _StringCopy(aGroupWinnerWin, dst); /* "(GROUP WINNER WINS TOURNAMENT)" */
                    goto after_qual_text;
                }

                if (d2 == 1) {
                    fmt = aTop0InGroupQua; /* "(TOP %0 IN GROUP QUALIFY)" */
                    goto print_qual;
                }

                if (d0 == d2) {
                    _StringCopy(aWinnersOfEachG, dst); /* "(WINNERS OF EACH GROUP QUALIFY)" */
                    goto after_qual_text;
                }

                {
                    int32_t d0ext = (int16_t)d0; /* ext.l */
                    uint16_t quotient = (uint16_t)((uint32_t)d0ext / (uint32_t)d2);
                    uint16_t remainder;
                    { uint32_t div = (uint32_t)d0ext; remainder = (uint16_t)(div % d2); }
                    uint16_t d1 = remainder; /* swap d0 gives remainder in low word */

                    if (quotient == 1) {
                        if (d1 == 1) {
                            _StringCopy(aWinnersOfEachG_0, dst);
                            goto after_qual_text;
                        }
                        fmt = aWinnersOfEachG_1;
                        goto print_qual;
                    }

                    if (d1 == 0) {
                        fmt = aTop0InEachGrou;
                        goto print_qual;
                    }

                    if (d1 == 1) {
                        fmt = aTop0InEachGrou_0;
                        goto print_qual;
                    }

                    fmt = aTop0InEachGrou_1;
                }
print_qual:
                _PrintFormatted(fmt, dst, d0, d2, 0, 0, 0, 0, 0, 0, 0, 0);
after_qual_text: ;
            }
        }

        /* Next entry: knockout-specific fields (legs/scoring) */
        a4++;
        if (slot->isLeagueFormat == 0) {
            /* Knockout path: show 4 entries for legs/play/goals config */
            a4->isInvisible = 0;
            {
                /* Set number value via double-indirection: fg → ptr → write */
                uint16_t **pp = (uint16_t **)(uintptr_t)a4->fg;
                **pp = slot->formatFlag16E;
            }
            a4++;
            a4->isInvisible = 0;
            {
                uint16_t **pp = (uint16_t **)(uintptr_t)a4->fg;
                **pp = slot->initConst170;
            }
            a4++;
            a4->isInvisible = 0;
            {
                uint16_t **pp = (uint16_t **)(uintptr_t)a4->fg;
                **pp = slot->formatFlag172;
            }
            a4++;
            a4->isInvisible = 1; /* hide separator */
            a4++;
            /* Set navigation links based on ordinal */
            {
                uint8_t low = (uint8_t)a4->ordinal;
                a4->upEntry = (int8_t)(low - 3);
                if (a4->ordinal < 0x4D)
                    a4->downEntry = (int8_t)(low + 9);
            }
            a4++;
            a4->isInvisible = 1; /* hide */
            a4++;
        } else {
            /* Group path: hide 3 knockout entries, show group config */
            a4->isInvisible = 1;
            a4++;
            a4->isInvisible = 1;
            a4++;
            a4->isInvisible = 1;
            a4++;
            /* Show group toggle entry */
            a4->isInvisible = 0;
            {
                uint16_t **pp = (uint16_t **)(uintptr_t)a4->fg;
                **pp = slot->initConst168;
            }
            a4++;
            /* Set navigation links */
            {
                uint8_t low = (uint8_t)a4->ordinal;
                a4->upEntry = (int8_t)(low - 1);
                a4->downEntry = (int8_t)(low - 1);
            }
            a4++;
            /* Show group count entry */
            a4->isInvisible = 0;
            {
                uint16_t **pp = (uint16_t **)(uintptr_t)a4->fg;
                **pp = slot->playEachTeam;
            }
            a4++;
        }

        /* Advance to next stage block */
        slot += 1;   /* next tournament slot (sizeof == stride 0x76, both arches) */
        continue;

fill_remaining_hidden:
        /* Past active stages: hide remaining 11 entries in this slot */
        a4 -= 3; /* back up 3 entries (lea -$38 * 3) */
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4++; /* skip one (already advanced past) */
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        goto fill_remaining_loop;
    }
    return;

fill_remaining_invisible_1:
    /* All entries from here until ordinal 0x59 ('Y') are hidden */
    {
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
    }

fill_remaining_loop:
    /* Continue hiding entries until ordinal reaches 0x59 */
    while (a4->ordinal != 0x59) {
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
        a4->isInvisible = 1;
        a4++;
    }
    return;
}

/* --- _DesignDIYTournamentOnSelect23 (SWOS2.s line 49637) ---
 * Toggle aggregate flag slot.formatFlag16E ^= 3, then redraw stage */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect23(MenuEntry *entry)
{
    uint8_t *stageBlock = getTournamentStageBlock(entry);
    DIYTournamentSlot *slot = (DIYTournamentSlot *)(stageBlock + offsetof(DIYTournamentState, slots));
    slot->formatFlag16E ^= 3;
    _DesignDIYTournamentOnDraw();
    _DesignDIYTournament_RedrawStageItemsFromBlock(stageBlock, 1);
    _CallDrawMultipleItems(2, &menuEntries[8]); /* menuEntries+$1C0 = entry 8 */
}

/* --- _DesignDIYTournamentOnSelect20 (SWOS2.s line 50144) ---
 * Adjust group layout and redraw */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect20(MenuEntry *entry)
{
    _DesignDIYTournament_CacheStageSelection(entry);
    uint8_t *a3 = diyTournamentSelectedStagePtr;
    DIYTournamentSlot *slot = (DIYTournamentSlot *)(a3 + offsetof(DIYTournamentState, slots));

    uint16_t d1 = slot->groups;
    uint16_t d2 = slot->teamsPerGroup;
    uint16_t d6 = slot->divisionFactor;

    /* Adjust group layout for max teams */
    uint16_t d7 = diyTournamentSelectedStageIndex1Based;
    GroupLayoutValues v = adjustGroupLayoutForMaxTeams(d1, d2, d6, d7);
    slot->isLeagueFormat = v.numGroups;
    slot->groups = v.numGroups;
    slot->teamsPerGroup = v.groupSize;

    _DesignDIYTournamentOnDraw();
    _DesignDIYTournament_RedrawStageItemsFromBlock(a3, 1);
    _CallDrawMultipleItems(2, &menuEntries[8]); /* menuEntries+$1C0 */
}

/* --- _DesignDIYTournament_AdjustStageQualifiers (SWOS2.s line 49992) ---
 * Interactive callback: adjusts qualifier count for current stage via control input.
 * Handles both first stage (numTeams) and subsequent stages (pass-through count). */
__attribute__((used, externally_visible))
void _DesignDIYTournament_AdjustStageQualifiers(MenuEntry *entry)
{
    _DesignDIYTournament_CacheStageSelection(entry);
    uint16_t d7 = diyTournamentSelectedStageIndex1Based;
    uint8_t *a3 = diyTournamentSelectedStagePtr;
    uint8_t *a2 = a3 - 0x76; /* previous stage */
    DIYTournamentSlot *slotA3 = (DIYTournamentSlot *)(a3 + offsetof(DIYTournamentState, slots));
    DIYTournamentSlot *slotA2 = (DIYTournamentSlot *)(a2 + offsetof(DIYTournamentState, slots));

    if (d7 == 1) {
        /* First stage: adjust numTeams at a3+0x32 */
        DIYCompetition *a3s = (DIYCompetition *)a3;
        diyTournamentPrevValueBackup = (uint16_t)a3s->numTeams;

        for (;;) {
            uint16_t ctrl = controlMask;
            if (ctrl == 0x81 || ctrl == 0x101) {
                /* Right/down: increment */
                a3s->numTeams += 1;
                if ((uint16_t)a3s->numTeams >= 0x41)
                    goto restore_first;
            } else if (ctrl == 0x41 || ctrl == 0x201) {
                /* Left/up: decrement */
                a3s->numTeams -= 1;
                if ((uint16_t)a3s->numTeams == 1)
                    goto restore_first;
            } else {
                return; /* no recognized input */
            }

            /* Check if value actually changed */
            if ((uint16_t)a3s->numTeams == diyTournamentPrevValueBackup)
                return;

            /* Compute new group layout */
            {
                uint16_t candidate = (uint16_t)a3s->numTeams;
                uint16_t current = slotA3->divisionFactor;
                GroupLayoutResult r = computeGroupLayoutForTeams(candidate, current, d7);
                if (!r.valid)
                    continue; /* try again */

                a3s->numTeams = (int16_t)candidate;
                slotA3->numTeams = (uint16_t)a3s->numTeams;
                slotA3->isLeagueFormat = r.numGroups;
                slotA3->groups = r.numGroups;
                slotA3->teamsPerGroup = r.groupSize;
            }

            _DIYTournament_RecalculateNumStages();
            _DesignDIYTournamentOnDraw();

            {
                uint16_t redrawCount = 1;
                _DesignDIYTournament_RedrawStageItemsFromBlock(a3, redrawCount);
            }
            _CallDrawMultipleItems(2, &menuEntries[8]);
            return;

restore_first:
            a3s->numTeams = (int16_t)diyTournamentPrevValueBackup;
            return;
        }
    }

    /* Subsequent stages (d7 > 1): adjust pass-through count at slotA2->divisionFactor */
    diyTournamentPrevValueBackup = slotA2->divisionFactor;

    for (;;) {
        uint16_t ctrl = controlMask;
        if (ctrl == 0x81 || ctrl == 0x101) {
            /* Right/down: increment */
            slotA2->divisionFactor += 1;
            if (slotA2->divisionFactor == slotA2->numTeams)
                goto restore_prev;
        } else if (ctrl == 0x41 || ctrl == 0x201) {
            /* Left/up: decrement */
            slotA2->divisionFactor -= 1;
            if ((int16_t)slotA2->divisionFactor < 0)
                goto restore_prev;
            if (slotA2->divisionFactor < slotA2->groups)
                goto restore_prev;
        } else {
            return; /* no recognized input */
        }

        /* Check if value actually changed */
        if (slotA2->divisionFactor == diyTournamentPrevValueBackup)
            return;

        /* Compute new group layout for next stage */
        {
            uint16_t candidate = slotA2->divisionFactor;
            uint16_t current = slotA3->divisionFactor;
            GroupLayoutResult r = computeGroupLayoutForTeams(candidate, current, d7);
            if (!r.valid)
                continue; /* try again */

            slotA3->isLeagueFormat = r.numGroups;
            slotA3->groups = r.numGroups;
            slotA3->teamsPerGroup = r.groupSize;

            /* Adjust previous stage's group layout if it was knockout */
            if (slotA2->isLeagueFormat == 0) {
                uint16_t d1a = slotA2->groups;
                uint16_t d2a = slotA2->teamsPerGroup;
                uint16_t d6a = slotA2->divisionFactor;
                GroupLayoutValues v = adjustGroupLayoutForMaxTeams(d1a, d2a, d6a, d7);
                slotA2->isLeagueFormat = v.numGroups;
                slotA2->groups = v.numGroups;
                slotA2->teamsPerGroup = v.groupSize;
            }

            slotA2->numTeams = slotA2->divisionFactor;
        }
        /* asm line 50046: move.w $164(a2),$162(a3) */
        slotA3->numTeams = slotA2->divisionFactor;

        _DIYTournament_RecalculateNumStages();
        _DesignDIYTournamentOnDraw();

        {
            uint16_t redrawCount = 2;
            if (d7 != 6 && slotA3->divisionFactor == 1)
                redrawCount = 3;
            _DesignDIYTournament_RedrawStageItemsFromBlock(a2, redrawCount);
        }
        _CallDrawMultipleItems(2, &menuEntries[8]);
        return;

restore_prev:
        slotA2->divisionFactor = diyTournamentPrevValueBackup;
        return;
    }
}

/* ==========================================================================
 * Chunk 130 — DesignDIYCup Draw & Cup Menu Helpers (10 functions)
 * ========================================================================== */

/* ASM externs for Chunk 130 */
extern uint16_t diyCupMatchListScrollIndex;
extern uint16_t diyCupMenuArg0Copy;
extern uint16_t diyCupMenuArg1;
extern uint16_t diyCupMenuArg0;
extern uint16_t diyCupMenuArg2;
extern uint8_t *diyCupMatchTeamNamePtr1;
extern uint8_t *diyCupMatchTeamNamePtr2;
extern uint16_t diyCupMatchListDrawModeFlag;
extern TeamFileEx *diyCupLastMatchTeam1Ptr;
extern TeamFileEx *diyCupLastMatchTeam2Ptr;
extern uint16_t diyCupLastMatchScoreTeam1;
extern uint16_t diyCupLastMatchScoreTeam2;
extern uint16_t chooseTeamsResult;
extern volatile uint16_t seasonUpdateInProgress;
extern volatile uint16_t fire;

/* ASM string constants for Chunk 130 */
extern uint8_t aV[];       /* "V" */
extern uint8_t aMinus[];   /* "-" */
extern uint8_t aOr[];      /* "OR " */
extern uint8_t aDraw[];    /* "DRAW" */
extern uint8_t aReplay[];  /* "REPLAY" */
extern uint8_t aReplays[]; /* "REPLAYS" */
extern uint8_t a1stLeg[];  /* "1ST LEG" */
extern uint8_t a2ndLeg[];  /* "2ND LEG" */
extern uint8_t aPlayCup[]; /* "PLAY CUP" */
extern const char *rndStringTable[];     /* const char *[15] */
extern void *replayStringTable[];  /* const char *[3] */

extern void _MenuCheckControls(void);

/* C functions from other modules */
extern void _InitCareerVariables(uint16_t gameType);
extern void _CopySelectedTeamNumbers(uint16_t count, uint8_t *dest);
extern void _InitTeams(void);
extern void _GoGetTeamsForPlay(uint8_t *gameTypeStr, uint8_t *playWhatStr,
                               uint16_t numTeams, uint16_t showClub,
                               uint16_t showNational, uint16_t showOrigCustom);

/* ---- Chunk 228: DesignDIYCup_BuildMatchListEntryStrings ---- */

/* Helper: resolve team name pointer from competition team table.
 * d0 = byte index, a4 = diyCompetitionStart base.
 * teamOffsetTable[idx] is signed byte offset from selTeamsPtr; the team
 * name field sits at +5 inside each team record, exactly where the
 * `selTeamNameBase` label aliases the buffer (see swos_globals.c) — so
 * `&selTeamNameBase + offset` is the team-name address. The Amiga
 * shorthand `(uint32_t)&selTeamNameBase + offset` truncates the pointer
 * on 64-bit. */
static uint8_t *getTeamNameFromIndex(int8_t idx, uint8_t *comp)
{
    DIYCompetitionEx *compEx = (DIYCompetitionEx *)comp;
    return diy_resolve_team_name(compEx->bracketScratch.teamOffsetTable[(uint16_t)(int16_t)idx]);
}

/* Helper: advance menu entry pointer and set isInvisible = 1 */
static MenuEntry *hideAndAdvance(MenuEntry *e)
{
    e->isInvisible = 1;
    return e + 1;
}

__attribute__((used, externally_visible))
void _DesignDIYCup_BuildMatchListEntryStrings(void)
{
    if (diyCupMatchListDrawModeFlag != 0) {
        /* Draw mode path */
        uint8_t *a4 = (uint8_t *)&diyCompetitionStart;
        DIYCompetition *a4cs = &diyCompetitionStart.base;
        MenuEntry *a3 = &menuEntries[18]; /* +$3F0 */
        uint16_t d7 = (uint16_t)a4cs->totalMatchesInRound;
        d7 >>= 1;
        uint8_t *a2 = diyCompetitionStart.teamGridPairings;
        uint8_t *a5 = diyCompetitionStart.teamShuffleOrder;
        uint16_t scrollIdx = diyCupMatchListScrollIndex;
        a5 += scrollIdx * 2;
        uint8_t *a6 = diyCompetitionStart.matchData.cup.cupRoundResults;
        uint16_t d6 = scrollIdx;
        int16_t d5 = 5;

        for (; d5 >= 0; d5--) {
            if (d6 >= d7) {
                /* Past end — hide 8 entries */
                for (int i = 0; i < 8; i++)
                    a3 = hideAndAdvance(a3);
            } else {
                /* "V" label */
                a3->fg = (uintptr_t)aV;
                a3->isInvisible = 0;
                a3++;

                int16_t d3 = 1;
                for (; d3 >= 0; d3--) {
                    int8_t d4 = (int8_t)*a5++;
                    int16_t d4w = d4;
                    uint8_t flagByte = a6[(uint16_t)d4w];
                    d4w = (int16_t)(d4w * 2);

                    if (flagByte & 2) {
                        /* Has result */
                        if (!(flagByte & 1)) {
                            /* Team A won */
                            int8_t teamIdx = a2[(uint16_t)d4w];
                            uint8_t *name = getTeamNameFromIndex(teamIdx, a4);
                            uint8_t *dst = (uint8_t *)a3->fg;
                            _StringCopy(name, dst);
                            a3->isInvisible = 0;
                            a3++;
                            a3->isInvisible = 1;
                            a3++;
                        } else {
                            /* Team B won */
                            int8_t teamIdx = a2[(uint16_t)d4w + 1];
                            uint8_t *name = getTeamNameFromIndex(teamIdx, a4);
                            uint8_t *dst = (uint8_t *)a3->fg;
                            _StringCopy(name, dst);
                            a3->isInvisible = 0;
                            a3++;
                            a3->isInvisible = 1;
                            a3++;
                        }
                    } else {
                        /* No result — show both teams */
                        int8_t teamIdx1 = a2[(uint16_t)d4w];
                        uint8_t *name1 = getTeamNameFromIndex(teamIdx1, a4);
                        uint8_t *dst = (uint8_t *)a3->fg;
                        _StringCopy(name1, dst);
                        a3->isInvisible = 0;
                        a3++;

                        /* "OR teamName2" */
                        int8_t teamIdx2 = a2[(uint16_t)d4w + 1];
                        uint8_t *name2 = getTeamNameFromIndex(teamIdx2, a4);
                        dst = (uint8_t *)a3->fg;
                        dst = _StringCopy((uint8_t *)aOr, dst);
                        _StringCopy(name2, dst);
                        a3->isInvisible = 0;
                        a3++;
                    }
                }

                /* Hide remaining 3 entries per row + 2 spacers */
                a3->isInvisible = 1; a3++;
                a3->isInvisible = 1; a3++;
                a3->isInvisible = 1; a3++;
            }
            d6++;
        }
        return;
    }

    /* Normal (non-draw) mode */
    uint8_t *a4 = (uint8_t *)&diyCompetitionStart;
    DIYCompetition *a4ns = &diyCompetitionStart.base;
    MenuEntry *a3 = &menuEntries[18]; /* +$3F0 */
    uint16_t d7 = (uint16_t)a4ns->totalMatchesInRound;
    uint8_t *a5 = diyCompetitionStart.matchData.cup.cupRoundResults;
    uint8_t *a6 = diyCompetitionStart.teamGridPairings;
    uint16_t scrollIdx = diyCupMatchListScrollIndex;
    a5 += scrollIdx;
    a6 += scrollIdx * 2;
    uint16_t d6 = scrollIdx;
    int16_t d5 = 5;

    for (; d5 >= 0; d5--) {
        if (d6 >= d7) {
            /* Past end — hide 8 entries */
            for (int i = 0; i < 8; i++)
                a3 = hideAndAdvance(a3);
        } else {
            uint16_t d4 = d6 * 2;

            /* "V" or "-" label */
            a3->fg = (uintptr_t)aV;
            if (*a5 & 0x80)
                a3->fg = (uintptr_t)aMinus;
            a3->isInvisible = 0;
            a3++;

            /* Team 1 name */
            int8_t teamIdx1 = (int8_t)a6[0];
            uint8_t *name1 = getTeamNameFromIndex(teamIdx1, a4);
            diyCupMatchTeamNamePtr1 = name1;
            uint8_t *dst = (uint8_t *)a3->fg;
            _StringCopy(name1, dst);
            a3->isInvisible = 0;
            a3++;

            /* Spacer */
            a3->isInvisible = 1;
            a3++;

            /* Team 2 name */
            int8_t teamIdx2 = (int8_t)a6[1];
            uint8_t *name2 = getTeamNameFromIndex(teamIdx2, a4);
            diyCupMatchTeamNamePtr2 = name2;
            dst = (uint8_t *)a3->fg;
            _StringCopy(name2, dst);
            a3->isInvisible = 0;
            a3++;

            /* Spacer */
            a3->isInvisible = 1;
            a3++;

            if (*a5 & 0x80) {
                /* Has result — show scores: cupRoundResults+0x40 (second-half score region).
                 * packedGoals was written here host-native via *(uint16_t *)&...=packedGoals
                 * (diy_competition.c:10257), so (packed>>8)=team1Score, (packed&0xFF)=team2Score.
                 * The previous byte access (scoreBase[d4], scoreBase[d4+1]) assumed Amiga BE
                 * byte order — on LE x86 that returned the scores swapped, so the cup match
                 * list showed "LAZIO 1-4 AC MILAN" when reality was LAZIO 4-1 AC MILAN. */
                uint8_t *scoreBase = (uint8_t *)&diyCompetitionStart.matchData.cup.cupRoundResults[0x40];
                uint16_t packed = *(uint16_t *)&scoreBase[d4];
                int16_t score1 = (int8_t)(packed >> 8);
                *(int16_t *)&a3->fg = score1;
                a3->isInvisible = 0;
                a3++;

                int16_t score2 = (int8_t)(packed & 0xFF);
                *(int16_t *)&a3->fg = score2;
                a3->isInvisible = 0;
                a3++;
            } else {
                /* No result — hide score entries */
                a3->isInvisible = 1;
                a3++;
                a3->isInvisible = 1;
                a3++;
            }

            /* Match result string.
             * ASM loads: d4 = goals (0xCA = cupRoundResults+0x40), d5 = aggregate (0x14E = _bss_2E0AE),
             * d6 = penalty (0x1CE = leagueSectionState.phaseStartMonths). All three are score halves of
             * the cup bracket result region. BuildMatchResultString expects: d4=leg2(goals), d5=leg1(agg),
             * d6=pens(penalty). Typed field access keeps the 64-bit struct shift correct (was
             * `a4 + 0x14E` / `+ 0x1CE` previously, which underflow 4 bytes on 64-bit due to
             * DIYCompetition.namePtr growth). */
            {
                uint16_t *goalsPtr = (uint16_t *)&diyCompetitionStart.matchData.cup.cupRoundResults[0x40];
                uint16_t goals = goalsPtr[d4 / 2];
                uint16_t *aggPtr = (uint16_t *)diyCompetitionStart._bss_2E0AE;
                uint16_t agg = aggPtr[d4 / 2];
                uint16_t *penPtr = (uint16_t *)diyCompetitionStart.leagueSectionState.phaseStartMonths;
                uint16_t pen = penPtr[d4 / 2];
                uint8_t flags = *a5;

                uint8_t *resultStr = _BuildMatchResultString(
                    0, goals, agg, pen,
                    diyCupMatchTeamNamePtr1,
                    diyCupMatchTeamNamePtr2,
                    (uint16_t)flags);

                a3->isInvisible = 0;
                dst = (uint8_t *)a3->fg;
                _StringCopy(resultStr, dst);
                a3++;
            }
        }

        a6 += 2;
        a5++;
        d6++;
    }
}

/* DesignDIYCupOnDraw — draw callback for cup design menu (SWOS2.s line 44793)
 * Iterates cup rounds, configures menu entries with round names and values. */
__attribute__((used, externally_visible))
void _DesignDIYCupOnDraw(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *a2s = &comp->base;
    MenuEntry *entry9 = &menuEntries[9];   /* menuEntries+$1F8 */

    /* Entry 9: show and write play-each-team value via double indirection */
    entry9->isInvisible = 0;
    uint16_t *numericPtr = *(uint16_t **)(uintptr_t)entry9->fg;
    *numericPtr = (uint16_t)a2s->playEachTeam;

    /* Entry 11: show and write numRounds */
    MenuEntry *entry11 = &menuEntries[11]; /* menuEntries+$268 */
    entry11->isInvisible = 0;
    entry11->fgWord = (uint16_t)a2s->numRounds;

    /* Entries 18+ : loop over 6 round slots (d7 = 0,2,4,6,8,10) */
    MenuEntry *a4 = &menuEntries[18];      /* menuEntries+$3F0 */
    uint16_t d7 = 0;
    uint16_t d6 = (uint16_t)a2s->numTeams;
    uint16_t d5 = 0;

    while (d7 < 0x0C) {
        uint16_t roundIdx = d7 >> 1;

        if (roundIdx < (uint16_t)a2s->numRounds) {
            /* Active round: show entries */
            a4->isInvisible = 0;
            uint16_t d1 = *(uint16_t *)&diyCompetitionStart.base.numRounds; /* DIY_COMPETITION_FIELD_3A+$20 */
            uint16_t d2 = *(uint16_t *)&diyCompetitionStart.base.subRoundCounter; /* DIY_COMPETITION_FIELD_3A+$22 */
            const char *roundStr = _GetNextCupRoundString(roundIdx, d1, d2);
            a4->fg = (uintptr_t)roundStr;
            a4++;

            /* Number of teams in this round */
            a4->isInvisible = 0;
            a4->fgWord = d6;
            a4++;

            /* Knockout type: double indirection write */
            a4->isInvisible = 0;
            numericPtr = *(uint16_t **)(uintptr_t)a4->fg;
            *numericPtr = comp->matchData.cup.roundLegs[roundIdx];
            a4++;

            /* Check if knockout type == 2 (aggregate) */
            if (comp->matchData.cup.roundLegs[roundIdx] == 2)
                d5 = 1;

            /* Qualifiers: double indirection write */
            a4->isInvisible = 0;
            numericPtr = *(uint16_t **)(uintptr_t)a4->fg;
            *numericPtr = comp->matchData.cup.roundExtraTime[roundIdx];
            a4++;

            /* Byes: double indirection write */
            a4->isInvisible = 0;
            numericPtr = *(uint16_t **)(uintptr_t)a4->fg;
            *numericPtr = comp->matchData.cup.roundPenalties[roundIdx];
            a4++;
        } else {
            /* Inactive round: hide all 5 entries */
            a4->isInvisible = 1; a4++;
            a4->isInvisible = 1; a4++;
            a4->isInvisible = 1; a4++;
            a4->isInvisible = 1; a4++;
            a4->isInvisible = 1; a4++;
        }

        d6 >>= 1;
        d7 += 2;
    }

    /* If no aggregate round found, hide entry 9 */
    if (d5 == 0)
        menuEntries[9].isInvisible = 1;
}

/* DesignDIYCupOnSelect48 — play cup callback (SWOS2.s line 44659) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect48(void)
{
    uint16_t numTeams = *(uint16_t *)&diyCompetitionStart.base.numTeams; /* DIY_COMPETITION_NUMTEAMS */
    uint8_t *teamListPtr = (uint8_t *)diyCompetitionStart.base.competitionName; /* DIY_COMPETITION_FIELD_2 + 2 */

    _GoGetTeamsForPlay(teamListPtr, aPlayCup, numTeams, 1, 1, 1);

    if (chooseTeamsResult == 0) {
        _SetExitMenuFlag();
        return;
    }

    _InitCareerVariables(1);
    _CopySelectedTeamNumbers(numTeams, (uint8_t *)diyCupTeamIds);
    _DIY_InitCupBracketState();
    _DIY_ShuffleTeamGridPairs();
    _InitTeams();
    _ShowDIYCupMenuWithParams(0, 0, 0);
    _DIY_CopyCompetitionStartToCompetitionFileBuffer();
    _SetExitMenuFlag();
}

/* DesignDIYCup_UpdateRoundOptionsVisibility — toggle scroll button visibility (SWOS2.s line 45931) */
__attribute__((used, externally_visible))
void _DesignDIYCup_UpdateRoundOptionsVisibility(void)
{
    uint16_t d0 = *(uint16_t *)&diyCompetitionStart.base.totalMatchesInRound; /* DIY_COMPETITION_FIELD_3A+$18 */

    if (diyCupMatchListDrawModeFlag != 0)
        d0 >>= 1;

    if (d0 <= 6) {
        menuEntries[16].isInvisible = 1; /* menuEntries+$380 */
        menuEntries[17].isInvisible = 1; /* menuEntries+$3B8 */
    } else {
        menuEntries[16].isInvisible = 0;
        menuEntries[17].isInvisible = 0;
    }
}

/* DesignDIYCup_BuildMenuTitleString — build cup menu title (SWOS2.s line 45949)
 * Concatenates: competition name + round name + optional replay/leg suffix */
__attribute__((used, externally_visible))
void _DesignDIYCup_BuildMenuTitleString(void)
{
    DIYCompetition *a2s = &diyCompetitionStart.base;
    uint8_t *a2 = (uint8_t *)&diyCompetitionStart;
    uint8_t *dst = (uint8_t *)menuEntries[0].fg;

    /* Copy competition name (at offset 4) */
    dst = _StringCopy(a2 + 4, dst);
    *dst++ = ' ';

    if (a2s->hasGroups != 0) {
        /* Tournament continuation path: use GetCupRoundString with diyFileBufferCopy */
        DIYCompetition *bufS = &diyFileBufferCopy.header;
        uint16_t d0 = (uint16_t)bufS->roundComplete;  /* 0x54 */
        uint16_t d1 = 0;
        uint16_t d2 = (uint16_t)bufS->totalMatchesInRound;  /* 0x52 */
        uint16_t d3 = (uint16_t)a2s->totalMatchesInRound * 2;
        /* +0x15A = first slot record's _pad_00 word */
        uint16_t d4 = *(uint16_t *)diyFileBufferCopy.slots->_pad_00;
        const char *roundStr = _GetCupRoundString(d0, d1, d2, d3, d4);
        dst = _StringCopy((const uint8_t *)roundStr, dst);
        /* Fall through to replay/leg check */
    } else {
        /* Direct cup path */
        uint16_t d0 = (uint16_t)a2s->currentRoundIndex;  /* 0x58 */

        if (diyCupMatchListDrawModeFlag != 0) {
            /* Draw mode */
            d0 += 1;
            uint16_t d1 = (uint16_t)a2s->numRounds;  /* 0x5A */
            uint16_t d2 = (uint16_t)a2s->subRoundCounter;  /* 0x5C */
            const char *roundStr = _GetNextCupRoundString(d0, d1, d2);
            dst = _StringCopy((const uint8_t *)roundStr, dst);
            *dst++ = ' ';
            dst = _StringCopy(aDraw, dst);
            *dst = 0;
            return;
        }

        /* Normal mode */
        uint16_t d1 = (uint16_t)a2s->numRounds;  /* 0x5A */
        uint16_t d2 = (uint16_t)a2s->subRoundCounter;  /* 0x5C */
        const char *roundStr = _GetNextCupRoundString(d0, d1, d2);
        dst = _StringCopy((const uint8_t *)roundStr, dst);
        /* Fall through to replay/leg check */
    }

    /* Check for replay suffix (groupSectionFlag doubles as replay counter here) */
    uint16_t replayCount = ((DIYCompetitionEx *)a2)->bracketScratch.groupSectionFlag;
    if (replayCount != 0) {
        const uint8_t *replayStr;

        if (replayCount > 3) {
            /* Many replays */
            if (a2s->totalMatchesInRound == 1)
                replayStr = aReplay;  /* "REPLAY" */
            else
                replayStr = aReplays; /* "REPLAYS" */
        } else {
            /* Table lookup: 1-3 replays */
            const char **table;
            if (a2s->totalMatchesInRound == 1)
                table = &rndStringTable[12]; /* rndStringTable+$30 = element 12 */
            else
                table = (const char **)replayStringTable;
            replayStr = (const uint8_t *)table[replayCount - 1];
        }

        *dst++ = ' ';
        dst = _StringCopy(replayStr, dst);
        *dst = 0;
        return;
    }

    /* Check for leg suffix */
    if (((DIYCompetitionEx *)a2)->matchData.cup.competitionFormat == 1) {
        *dst++ = ' ';
        dst = _StringCopy(a2ndLeg, dst); /* "2ND LEG" */
    } else if (((DIYCompetitionEx *)a2)->matchData.cup.roundFormat == 2) {
        *dst++ = ' ';
        dst = _StringCopy(a1stLeg, dst); /* "1ST LEG" */
    }

    *dst = 0;
}

/* DesignDIYCup_UpdateCurrentMatchTeamEntryStrings — update team name entries (SWOS2.s line 46158)
 * Inlines DIY_GetCurrentMatchTeamSlots logic to get team slot pointers. */
__attribute__((used, externally_visible))
void _DesignDIYCup_UpdateCurrentMatchTeamEntryStrings(void)
{
    /* Inline DIY_GetCurrentMatchTeamSlots: compute two team slot pointers */
    DIYCompetitionEx *baseEx = &diyCompetitionStart;
    uint8_t *base = (uint8_t *)baseEx;
    uint16_t idx = baseEx->matchData.cup.currentMatchIndex;
    uint16_t idx2 = idx * 2;
    int8_t slot1 = (int8_t)baseEx->teamGridPairings[idx2];
    int8_t slot2 = (int8_t)baseEx->teamGridPairings[idx2 + 1];

    MenuEntry *e = &menuEntries[11]; /* menuEntries+$268 */

    /* Entry 11: "V" separator, visible */
    e->isInvisible = 0;
    e->fg = (uintptr_t)aV;
    e++;

    /* Entry 12: team 1 name, visible. See getTeamNameFromIndex for the
     * `&selTeamNameBase + offset` rationale (Amiga shorthand truncates
     * the pointer on 64-bit). */
    e->isInvisible = 0;
    uint8_t *teamName1 = diy_resolve_team_name(baseEx->bracketScratch.teamOffsetTable[(int16_t)slot1]);
    e->fg = (uintptr_t)teamName1;
    e++;

    /* Entry 13: team 2 name, visible */
    e->isInvisible = 0;
    uint8_t *teamName2 = diy_resolve_team_name(baseEx->bracketScratch.teamOffsetTable[(int16_t)slot2]);
    e->fg = (uintptr_t)teamName2;
    e++;

    /* Entry 14: hidden */
    e->isInvisible = 1;
    e++;

    /* Entry 15: hidden */
    e->isInvisible = 1;
}

/* DesignDIYCup_DrawCurrentMatchTeamEntries — update strings and draw (SWOS2.s line 46148) */
__attribute__((used, externally_visible))
void _DesignDIYCup_DrawCurrentMatchTeamEntries(void)
{
    _DesignDIYCup_UpdateCurrentMatchTeamEntryStrings();
    _CallDrawMenuItem(&menuEntries[1]);   /* menuEntries+$38 = entry 1 */
    _CallDrawMultipleItems(5, &menuEntries[11]); /* menuEntries+$268 = entry 11 */
}

/* DesignDIYCup_HideCurrentMatchTeamEntries — hide team entries 11-15 (SWOS2.s line 46243) */
__attribute__((used, externally_visible))
void _DesignDIYCup_HideCurrentMatchTeamEntries(void)
{
    MenuEntry *e = &menuEntries[11]; /* menuEntries+$268 */
    e->isInvisible = 1; e++;
    e->isInvisible = 1; e++;
    e->isInvisible = 1; e++;
    e->isInvisible = 1; e++;
    e->isInvisible = 1;
}

/* DesignDIYCup_UpdateMatchListScrollAndRebuildStrings — adjust scroll and rebuild (SWOS2.s line 46190)
 * Returns non-zero if strings were rebuilt (caller should redraw). */
__attribute__((used, externally_visible))
int16_t _DesignDIYCup_UpdateMatchListScrollAndRebuildStrings(void)
{
    if (diyCupMatchListDrawModeFlag != 0) {
        diyCupMatchListScrollIndex = 0;
        _DesignDIYCup_BuildMatchListEntryStrings();
        return 1;
    }

    /* Calculate target scroll position */
    uint16_t d0 = 0;
    if (diyCupMenuArg0Copy != 0) {
        uint16_t totalMatches = *(uint16_t *)&diyCompetitionStart.base.totalMatchesInRound; /* DIY_COMPETITION_FIELD_3A+$18 */
        if (totalMatches > 6) {
            d0 = diyLeagueMinSubstitutes;
            if (diyCupMenuArg0Copy != 1) {
                d0 -= 1;
                if ((int16_t)d0 < 0) {
                    d0 = totalMatches - 1;
                }
            }
        }
    }

    /* Adjust scroll index to keep target visible in window of 3 */
    uint16_t changed = 0;

    /* Shrink loop: scroll down if target is above window */
    for (;;) {
        uint16_t windowEnd = diyCupMatchListScrollIndex + 3;
        if (d0 >= windowEnd)
            break;
        if (diyCupMatchListScrollIndex == 0)
            break;
        diyCupMatchListScrollIndex -= 1;
        changed = 1;
    }

    /* Grow loop: scroll up if target is below window */
    for (;;) {
        uint16_t windowEnd = diyCupMatchListScrollIndex + 3;
        if (d0 <= windowEnd)
            break;
        uint16_t maxScroll = *(uint16_t *)&diyCompetitionStart.base.totalMatchesInRound - 6;
        if (diyCupMatchListScrollIndex == maxScroll)
            break;
        diyCupMatchListScrollIndex += 1;
        changed = 1;
    }

    if (changed != 0) {
        _DesignDIYCup_BuildMatchListEntryStrings();
        return 1;
    }
    return 0;
}

/* DesignDIYCup_UpdateMatchListAndRedrawIfNeeded — scroll + conditional redraw (SWOS2.s line 46181) */
__attribute__((used, externally_visible))
void _DesignDIYCup_UpdateMatchListAndRedrawIfNeeded(void)
{
    if (_DesignDIYCup_UpdateMatchListScrollAndRebuildStrings() != 0)
        _DrawMenu();
}

/* DesignDIYCup_UpdateCurrentMatchResultHeaderEntries — show result header (SWOS2.s line 46737)
 * IDA bug: ($2A96).w = menuEntries[11] (absolute short address, should be symbolic). */
__attribute__((used, externally_visible))
void _DesignDIYCup_UpdateCurrentMatchResultHeaderEntries(void)
{
    TeamFileEx *team1 = diyCupLastMatchTeam1Ptr;
    TeamFileEx *team2 = diyCupLastMatchTeam2Ptr;
    uint16_t score1 = diyCupLastMatchScoreTeam1;
    uint16_t score2 = diyCupLastMatchScoreTeam2;

    MenuEntry *e = &menuEntries[11]; /* ($2A96).w = menuEntries+$268 = entry 11 */

    /* Entry 11: "-" separator */
    e->isInvisible = 0;
    e->fg = (uintptr_t)aMinus;
    e++;

    /* Entry 12: team 1 name */
    e->fg = (uintptr_t)team1->team.teamName;
    e->isInvisible = 0;
    e++;

    /* Entry 13: team 2 name */
    e->fg = (uintptr_t)team2->team.teamName;
    e->isInvisible = 0;
    e++;

    /* Entry 14: team 1 score */
    e->fgWord = score1;
    e->isInvisible = 0;
    e++;

    /* Entry 15: team 2 score */
    e->fgWord = score2;
    e->isInvisible = 0;
}

/* ==========================================================================
 * Chunk 131 — DesignDIYLeague remaining + DIYCup menu helpers (12 functions)
 * ========================================================================== */

extern uint16_t diyPitchTypeOrSeason_1;
extern uint8_t diyCupMenu[];
extern uint16_t writeOnlyVar01;
extern uint8_t aPlayLeague[];       /* "PLAY LEAGUE" */
extern uint8_t aWinnerQualifie[];   /* "WINNER QUALIFIES" */
extern uint8_t aTop0Qualify[];      /* "TOP %0 QUALIFY" */
extern uint8_t aTeamsPromoted0[];   /* "TEAMS PROMOTED  %0" */
extern uint8_t aTeamsPromoted0_0[]; /* "TEAMS PROMOTED  %0   TEAMS IN PLAYOFFS ..." */
extern uint8_t aTeamsRelegated[];   /* "TEAMS RELEGATED  %0" */
extern uint8_t aTeamsRelegated_0[]; /* "TEAMS RELEGATED  %0   TEAMS IN PLAYOFFS..." */
/* ---- Chunk 228: DesignDIYCup_UpdateMenuEntriesVisibilityAndSelection ---- */
extern uint8_t aNextRoundDraw[];   /* "NEXT ROUND DRAW" */
extern uint8_t aPlayReplays[];     /* "PLAY REPLAYS" */
extern uint8_t a2ndLegRound[];     /* "2ND LEG ROUND" */
extern uint8_t aNextRound[];       /* "NEXT ROUND" */
extern uint8_t aNextMatch[];       /* "NEXT MATCH" */
extern uint16_t fireResetFlag;

__attribute__((used, externally_visible))
void _DesignDIYCup_UpdateMenuEntriesVisibilityAndSelection(void)
{
    if (diyCupMenuArg1 != 0) {
        /* Hide entries 1-8 (offsets $38..$1C8 from menuEntries+$38) */
        MenuEntry *a0 = &menuEntries[1];
        a0->isInvisible = 1;
        a0[1].isInvisible = 1;
        a0[2].isInvisible = 1;
        a0[3].isInvisible = 1;
        a0[4].isInvisible = 1;
        a0[5].isInvisible = 1;
        a0[6].isInvisible = 1;
        a0[7].isInvisible = 1;
        return;
    }

    DIYCompetition *a1s = &diyCompetitionStart.base;
    uint16_t field4A = (uint16_t)a1s->menuState;  /* 0x4A */

    if (field4A == 6) {
        /* Competition complete */
        if (diyCupMenuArg0 == 0)
            fireResetFlag = 1;

        menuEntries[1].isInvisible = 1;
        MenuEntry *e3 = &menuEntries[3];
        e3->isInvisible = 1;
        e3[1].isInvisible = 1;
        e3[2].isInvisible = 1;
        MenuEntry *e6 = &menuEntries[6];
        e6->isInvisible = 1;
        e6[1].isInvisible = 1;
        e6[2].isInvisible = 1;
        _SetCurrentEntry(10);
        goto check_arg2;
    }

    if (field4A == 0) {
        if ((int16_t)diyCupMenuArg0Copy <= 0) {
            menuEntries[1].fg = (uintptr_t)aNextMatch;
            goto show_entry1_hide_rest;
        }
        goto show_match_entries;
    }

    if (field4A == 1) {
        fireResetFlag = 1;
        menuEntries[1].fg = (uintptr_t)a2ndLegRound;
        goto show_entry1_hide_rest;
    }

    if (field4A == 2) {
        fireResetFlag = 1;
        menuEntries[1].fg = (uintptr_t)aNextRoundDraw;
        goto show_entry1_hide_rest;
    }

    if (field4A == 3) {
        fireResetFlag = 1;
        menuEntries[1].fg = (uintptr_t)aPlayReplays;
        goto show_entry1_hide_rest;
    }

    if (field4A == 4) {
        fireResetFlag = 1;
        menuEntries[1].fg = (uintptr_t)aNextRound;
        goto show_entry1_hide_rest;
    }

    /* Default: field4A >= 5 (but != 6) */
    fireResetFlag = 1;
    menuEntries[1].fg = (uintptr_t)aNextRound;

show_entry1_hide_rest:
    menuEntries[1].isInvisible = 0;
    {
        MenuEntry *e3 = &menuEntries[3];
        e3->isInvisible = 1;
        e3[1].isInvisible = 1;
        e3[2].isInvisible = 1;
        MenuEntry *e6 = &menuEntries[6];
        e6->isInvisible = 1;
        e6[1].isInvisible = 1;
        e6[2].isInvisible = 1;
    }
    _SetCurrentEntry(1);
    goto check_arg2;

show_match_entries:
    menuEntries[1].isInvisible = 1;
    {
        uint16_t cmpResult = _DIY_CompareCurrentMatchTeamCategories();
        if (cmpResult == 0 || cmpResult == 1) {
            /* Show entries 6-8, hide entries 3-5 */
            MenuEntry *e6 = &menuEntries[6];
            e6->isInvisible = 0;
            e6[1].isInvisible = 0;
            e6[2].isInvisible = 0;
            menuEntries[7].isInvisible = 1;
            MenuEntry *e3 = &menuEntries[3];
            e3->isInvisible = 1;
            e3[1].isInvisible = 1;
            e3[2].isInvisible = 1;

            if (careerLoadedFlag != 0) {
                menuEntries[6].isInvisible = 1;
                menuEntries[7].isInvisible = 1;
                /* Copy x coordinate from entry 6 to entry 8 */
                menuEntries[8].x = menuEntries[6].x;
                menuEntries[8].dwHeight = 0xCA;
            }
            _SetCurrentEntry((uint16_t)a1s->slotStateC);  /* 0x42 */
        } else {
            /* Show entries 3-5, hide entries 6-8 */
            MenuEntry *e3 = &menuEntries[3];
            e3->isInvisible = 0;
            e3[1].isInvisible = 0;
            e3[2].isInvisible = 0;
            menuEntries[4].isInvisible = 1;
            MenuEntry *e6 = &menuEntries[6];
            e6->isInvisible = 1;
            e6[1].isInvisible = 1;
            e6[2].isInvisible = 1;
            _SetCurrentEntry((uint16_t)a1s->slotStateA);  /* 0x3E */
        }
        goto check_arg2;
    }

check_arg2:
    if (diyCupMenuArg2 != 0) {
        diyCupMenuArg2 = 0;
        fireResetFlag = 0;
        /* arg2=1 is set by Career_PlayMatchMenuLoop (via d2=1 → arg2=1
         * in _ShowDIYCupMenuWithParams). The original asm slams the
         * cursor to entry 10 (aExit) here. For the modern user the
         * expected behaviour is "PLAY MATCH brings up the cup fixture
         * menu with the cursor on MATCH so a second fire press starts
         * the game" — landing on EXIT means the user has to navigate
         * to MATCH manually, defeating the point of PLAY MATCH being
         * a shortcut. In career mode put the cursor on slotStateA
         * (= 3 = aMatch); outside career stay asm-faithful.
         *
         * Use gameType == GAME_TYPE_CAREER (4), NOT careerLoadedFlag
         * — that flag is only set by the season auto-advance helpers
         * and is 0 during the user's own match. */
        extern uint16_t gameType;
        if (gameType == 4 /* GAME_TYPE_CAREER */)
            _SetCurrentEntry((uint16_t)diyCompetitionStart.base.slotStateA);
        else
            _SetCurrentEntry(10);
    }
}

/* ================================================================
 * Chunk 229 — DIYLeagueTournamentOnRestore + DIYLeagueTournament16BeforeDraw
 * ================================================================ */

extern uint16_t diyLeagueTournament_fixtureRowHeight;
extern uint16_t diyLeagueTournament_fixtureRowYStep;
extern void *groupStringTable[];
extern uint16_t diyFixtureViewMode;
extern uint16_t numDiyResults;
extern uint16_t currentDiyLine;
extern uint16_t diyLeagueTournament_viewOptionsAvailable;
extern uint16_t highlightedLeagueTeamId;

/* DIYLeagueTournamentOnRestore — menu onReturn callback for DIY league/tournament menu */
/* Rebuilds title string, adjusts entry heights for >22 teams, computes fixture row count */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentOnRestore(void)
{
    DIYCompetition *compS = &diyCompetitionStart.base;
    uint8_t *comp = (uint8_t *)&diyCompetitionStart;  /* raw pointer for name data access */

    /* If halfTeams == 1, adjust bg color of entry 16 */
    if (compS->matchPairsPerRound == 1) {
        MenuEntry *a0 = &menuEntries[16];
        a0->bgColor = (a0->bgColor & 0xFFF0) | 0; /* PINK_TO_BROWN_0 = 0 */
    }

    /* Branch on diyFixtureViewMode */
    if (diyFixtureViewMode != 0) {
        if ((int16_t)diyFixtureViewMode < 0) {
            _DIYSetTeamNamesAndResults();
        } else {
            _DIYLeagueTournament_SetupCurrentFixtureTeamNames();
        }
    } else {
        /* Hide entries 11-15 */
        menuEntries[11].isInvisible = 1;
        menuEntries[12].isInvisible = 1;
        menuEntries[13].isInvisible = 1;
        menuEntries[14].isInvisible = 1;
        menuEntries[15].isInvisible = 1;
    }

    /* Build title string */
    {
        uint8_t *a1 = (uint8_t *)menuEntries[0].fg;
        uint8_t *titleSrc = comp + 4;
        a1 = _StringCopy(titleSrc, a1);

        if (compS->hasGroups != 0) {
            *a1++ = ' ';
            uint16_t d0 = (uint16_t)compS->menuState;
            uint16_t d1 = (uint16_t)compS->matchPairsPerRound;
            uint16_t d2 = (uint16_t)compS->roundIterator;
            uint16_t d3 = (uint16_t)compS->numTeams;
            const char *roundStr = _GetCupRoundString(d0, d1, d2, d3, 0);
            a1 = _StringCopy((const uint8_t *)roundStr, a1);
        }
    }

    /* Hide entries 16-23 */
    {
        MenuEntry *a0 = &menuEntries[16];
        int16_t d0;
        for (d0 = 7; d0 >= 0; d0--) {
            a0->isInvisible = 1;
            a0++;
        }
    }

    /* Set default fixture row dimensions */
    diyLeagueTournament_fixtureRowHeight = 6;
    diyLeagueTournament_fixtureRowYStep = 7;

    uint16_t numTeamsPerGroup = (uint16_t)compS->totalMatchesInRound;

    /* If numTeamsPerGroup > 22, adjust menu entry dimensions */
    if (numTeamsPerGroup > 22) {
        menuEntries[0].dwHeight = 9;

        /* Add 5 to y, subtract 5 from dwHeight for entries 1-8 */
        {
            MenuEntry *a0 = &menuEntries[1];
            int16_t d0;
            for (d0 = 7; d0 >= 0; d0--) {
                a0->y += 5;
                a0->dwHeight -= 5;
                a0++;
            }
        }

        /* Adjust entries 9 and 10 */
        menuEntries[9].y += 5;
        menuEntries[9].dwHeight -= 5;
        menuEntries[10].y += 5;
        menuEntries[10].dwHeight -= 5;

        /* Add 6 to y for entries 11-15 */
        {
            MenuEntry *a0 = &menuEntries[11];
            int16_t d0;
            for (d0 = 4; d0 >= 0; d0--) {
                a0->y += 6;
                a0++;
            }
        }

        /* Set dwHeight=7 for entries 24-30 */
        {
            MenuEntry *a0 = &menuEntries[24];
            int16_t d0;
            for (d0 = 6; d0 >= 0; d0--) {
                a0->dwHeight = 7;
                a0++;
            }
        }

        /* Set dwHeight=7 for entries 16-23 */
        {
            MenuEntry *a0 = &menuEntries[16];
            int16_t d0;
            for (d0 = 7; d0 >= 0; d0--) {
                a0->dwHeight = 7;
                a0++;
            }
        }
    }

    /* Compute number of fixture rows that fit */
    {
        int16_t d1 = menuEntries[11].y;
        int16_t d2 = menuEntries[0].y + menuEntries[0].dwHeight;
        d1 -= d2;

        uint16_t d3 = numTeamsPerGroup;
        d3 *= diyLeagueTournament_fixtureRowYStep;
        d3 += menuEntries[16].dwHeight + 1;

        uint16_t d0 = (uint16_t)((uint32_t)(uint16_t)d1 / d3);
        if (d0 > (uint16_t)compS->matchPairsPerRound)
            d0 = (uint16_t)compS->matchPairsPerRound;
        numDiyResults = d0;

        uint16_t d4 = d3 * numDiyResults;
        int16_t d5 = d1 - d4;
        int32_t d5ext = (int16_t)d5;
        d5 = (uint16_t)((uint32_t)d5ext / (uint16_t)(numDiyResults + 1));

        MenuEntry *a0 = &menuEntries[16];
        int16_t i;
        for (i = numDiyResults - 1; i >= 0; i--) {
            a0->isInvisible = 0;
            d2 += d5;
            a0->y = d2;
            d2 += d3;
            a0++;
        }

        /* If halfTeams == numDiyResults, hide scroll buttons */
        if ((uint16_t)compS->matchPairsPerRound == numDiyResults) {
            menuEntries[40].isInvisible = 1;
            menuEntries[41].isInvisible = 1;
        }
    }

    /* If diyHasGroups, adjust some entries */
    if (diyHasGroups != 0) {
        {
            MenuEntry *a0 = &menuEntries[11];
            int16_t d0;
            for (d0 = 4; d0 >= 0; d0--) {
                a0->y -= 80;
                a0++;
            }
        }

        /* Hide entries 31-39 */
        {
            MenuEntry *a0 = &menuEntries[31];
            a0[0].isInvisible = 1;
            a0[1].isInvisible = 1;
            a0[2].isInvisible = 1;
            a0[3].isInvisible = 1;
            a0[4].isInvisible = 1;
            a0[5].isInvisible = 1;
            a0[6].isInvisible = 1;
            a0[7].isInvisible = 1;
            a0[8].isInvisible = 1;
        }
        menuEntries[9].isInvisible = 1;

        /* Widen entries 3 and 5 */
        menuEntries[3].dwWidth += 25;
        menuEntries[5].x += 25;
        menuEntries[5].dwWidth += 25;
    }

    _DIYLeagueTournament_UpdateMenuEntryVisibilityAndFocus();
    /* nullsub_15 — does nothing */
    _DIYLeagueTournament_EnsureCurrentFixtureVisible();
}

/* DIYLeagueTournament16BeforeDraw — beforeDraw callback for fixture display entries 16-23 */
/* Draws league table rows and fixture rows for the current group */
__attribute__((used, externally_visible))
void _DIYLeagueTournament16BeforeDraw(MenuEntry *entry)
{
    DIYCompetition *compS = &diyCompetitionStart.base;
    uint8_t *comp = (uint8_t *)&diyCompetitionStart;  /* raw pointer for extended buffer access */
    int16_t d7 = entry->ordinal - 16 + currentDiyLine;
    uint16_t numTeamsPerGroup = (uint16_t)compS->totalMatchesInRound;

    /* Set group string if halfTeams != 1 */
    if (compS->matchPairsPerRound != 1) {
        entry->fg = (uintptr_t)groupStringTable[d7];
    }

    int16_t d2 = entry->y;

    /* Setup entries 24-30: set isInvisible and y */
    {
        MenuEntry *a0 = &menuEntries[24];
        int16_t d0;
        for (d0 = 6; d0 >= 0; d0--) {
            a0->isInvisible = 0;
            if (diyHasGroups)
                a0->isInvisible = 1;
            a0->y = d2;
            a0++;
        }
    }

    /* Draw entries 24-30 (7 items) */
    _DrawMultipleItems(7, &menuEntries[24]);

    /* Advance d2 past the header row */
    d2 += menuEntries[24].dwHeight + 1;

    uint16_t d4 = diyLeagueTournament_fixtureRowHeight;
    int16_t rowIdx = d7;
    uint16_t teamIdx = (uint16_t)rowIdx * numTeamsPerGroup;
    teamIdx <<= 1; /* byte offset into fixture table */

    MenuEntry *a5 = &menuEntries[31];
    uint16_t d5 = 1; /* team number, 1-based */

    do {
        /* Standings row for the team at rank (teamIdx>>1). teamEntryOffsets
         * holds 18 * team_index per the sorted-by-standings layout, so
         * the row index is fixtureOff / 18. Use diy_standings_row() so
         * the +0x2B4 base shift is derived from the struct layout
         * (handles 64-bit pointer-width growth). */
        uint16_t fixtureOff = diyCompetitionStart.matchData.league.teamEntryOffsets[teamIdx >> 1];
        DIYTeamStats *a4row = diy_standings_row(&diyCompetitionStart, fixtureOff / 18);

        /* Entry for team number */
        a5->y = d2;
        a5->fgWord = d5; /* ENTRYTYPE2UNION_NUMBER */
        a5++;

        /* Entry for team name */
        a5->y = d2;
        a5->dwHeight = d4;

        /* Get team pointer: row->teamSelOffset (signed) + selTeamsPtr base.
         * The Amiga shorthand `(uint32_t)&selTeamsPtr + offset` truncates
         * the pointer on 64-bit; full-width pointer arithmetic via
         * `&selTeamsPtr + offset` sign-extends the offset and reaches the
         * right team record whether it lives above or below selTeamsPtr. */
        TeamFileEx *teamPtr = (TeamFileEx *)diy_resolve_team(a4row->teamSelOffset);

        /* DEBUG: validate team name bytes are printable ASCII. If
         * corruption detected, flash color and halt so the debugger
         * can inspect. */
        {
            uint8_t *nameCheck = (uint8_t *)teamPtr->team.teamName;
            uint16_t ci;
            for (ci = 0; ci < 19; ci++) {
                uint8_t b = nameCheck[ci];
                if (b == 0) break;  /* null terminator is OK */
                if (b < 0x20) {
                    /* Non-printable byte found — corruption!
                     * panicReasonCode encodes: 0xCC00 | team d5 in high nibble, byte index in low byte.
                     * Set breakpoint on _PanicHaltLoop to inspect teamPtr, nameCheck, the buffer. */
                    panicReasonCode = (uint16_t)(0xCC00u | ((d5 & 0xF) << 4) | ci);
                    _FlashColor0Delay(500000, 0x0F00);  /* flash RED */
                    _PanicHaltLoop();
                }
            }
        }

        /* Set color based on team controls */
        uint8_t teamControls = teamPtr->team.teamControls;
        if (teamControls == 1)
            a5->bgColor = 10; /* COLOR_RED */
        else if (teamControls == 2)
            a5->bgColor = 11; /* COLOR_PURPLISH */
        else if (teamControls == 3)
            a5->bgColor = 13; /* COLOR_LIGHT_BLUE */
        else
            a5->bgColor = 14; /* COLOR_ORANGE */

        /* Set team name string */
        a5->fg = (uintptr_t)teamPtr->team.teamName;

        /* Determine row color (d3) based on view options and promotion/relegation zones */
        uint16_t d3;
        int16_t zoneHandled = 0;
        if (diyLeagueTournament_viewOptionsAvailable != 0) {
            uint16_t totalZones = (uint16_t)compS->roundComplete + (uint16_t)compS->competitionFinished +
                                  (uint16_t)compS->currentRoundIndex + (uint16_t)compS->numRounds;
            if (totalZones != 0) {
                uint16_t promoted = (uint16_t)compS->roundComplete;  /* 0x54 */
                int16_t applyToPlayer = 1; /* whether to apply d3 to bgColor if player-controlled */
                if (d5 <= promoted) {
                    /* In promotion zone */
                    if (compS->competitionFinished == 0)  /* 0x56 */
                        d3 = 9; /* PINK_TO_BROWN_9 */
                    else
                        d3 = 7; /* PINK_TO_BROWN_7 */
                } else {
                    uint16_t d0 = promoted + (uint16_t)compS->competitionFinished;
                    if (d5 <= d0) {
                        /* In promotion playoff zone */
                        d3 = 9; /* PINK_TO_BROWN_9 */
                    } else {
                        d0 = numTeamsPerGroup - (uint16_t)compS->currentRoundIndex;  /* 0x58 */
                        if (d5 > d0) {
                            /* In relegation zone */
                            if (compS->numRounds == 0)  /* 0x5A */
                                d3 = 9; /* PINK_TO_BROWN_9 */
                            else
                                d3 = 7; /* PINK_TO_BROWN_7 */
                        } else {
                            d0 -= (uint16_t)compS->numRounds;  /* 0x5A */
                            if (d5 > d0) {
                                /* In relegation playoff zone */
                                d3 = 9; /* PINK_TO_BROWN_9 */
                            } else {
                                /* Safe zone — d3=7, skip player-controlled check */
                                d3 = 7; /* PINK_TO_BROWN_7 */
                                applyToPlayer = 0;
                            }
                        }
                    }
                }

                /* If team is player-controlled, apply zone color */
                if (applyToPlayer && teamPtr->team.teamControls == 1)
                    a5->bgColor = d3;
                zoneHandled = 1;
            }
        }

        if (!zoneHandled) {
            /* Default row colors: odd/even alternation */
            d3 = 7;
            if (d5 & 1)
                d3 = 9;
        }
        /* Highlight player's team */
        if (highlightedLeagueTeamId == BE_READ_U16(teamPtr))
            a5->bgColor = 12; /* ORANGE_TO_BROWN_12 */

        a5++;

        /* Stats columns (7 entries): played, won, drawn, lost, GF, GA, points */
        a5->y = d2;
        a5->dwHeight = d4;
        a5->bgColor = d3;
        a5->fgWord = a4row->gamesPlayed;
        a5++;

        a5->y = d2;
        a5->dwHeight = d4;
        a5->bgColor = d3;
        a5->fgWord = a4row->wins;
        a5++;

        a5->y = d2;
        a5->dwHeight = d4;
        a5->bgColor = d3;
        a5->fgWord = a4row->draws;
        a5++;

        a5->y = d2;
        a5->dwHeight = d4;
        a5->bgColor = d3;
        a5->fgWord = a4row->losses;
        a5++;

        a5->y = d2;
        a5->dwHeight = d4;
        a5->bgColor = d3;
        a5->fgWord = a4row->goalsFor;
        a5++;

        a5->y = d2;
        a5->dwHeight = d4;
        a5->bgColor = d3;
        a5->fgWord = a4row->goalsAgainst;
        a5++;

        a5->y = d2;
        a5->dwHeight = d4;
        a5->bgColor = d3;
        a5->fgWord = a4row->points;

        /* Reset a5 back to entry 31 before drawing (asm reloads a5 here) */
        a5 = &menuEntries[31];

        /* Draw fixture rows (9 items starting at entry 31) */
        _DrawMultipleItems(9, &menuEntries[31]);

        d2 += diyLeagueTournament_fixtureRowYStep;
        teamIdx += 2;
        d5++;
    } while (d5 <= numTeamsPerGroup);
}

/* DIYSetMonthsOrPitchTypeEntries — toggle pitch type / month entry visibility (SWOS2.s line 41515) */
__attribute__((used, externally_visible))
void _DIYSetMonthsOrPitchTypeEntries(void)
{
    diyPitchTypeOrSeason_1 = (uint16_t)diyCompetitionStart.base.pitchTypeOrSeason;

    if (gameType == 2 || gameType == 3 || gameType == 4) /* PRESET_COMP, SEASON, CAREER */
        diyPitchTypeOrSeason_1 = 1;

    if (diyPitchTypeOrSeason_1 == 0) {
        /* Hide month entries 10-12, show pitch type entry 9 */
        menuEntries[10].isInvisible = 1;
        menuEntries[11].isInvisible = 1;
        menuEntries[12].isInvisible = 1;
        menuEntries[9].isInvisible = 0;
    } else {
        /* Show month entries 10-12, hide pitch type entry 9 */
        menuEntries[9].isInvisible = 1;
        menuEntries[10].isInvisible = 0;
        menuEntries[11].isInvisible = 0;
        menuEntries[12].isInvisible = 0;
    }
}

/* DIYLeagueSetVariousFields — compute month indices, set numeric menu fields (SWOS2.s line 41422) */
__attribute__((used, externally_visible))
void _DIYLeagueSetVariousFields(void)
{
    /* Start month: byte at offset 56, wrap at 96, divide by 8 */
    uint8_t startVal = (uint8_t)diyCompetitionStart.base.startMonth;  /* DIYCOMPETITION_STARTMONTH (0x38) */
    if (startVal >= 96)
        startVal -= 96;
    diyLeagueStartingMonth = (uint16_t)startVal / 8;

    /* End month: byte at offset 57 */
    uint8_t endVal = (uint8_t)diyCompetitionStart.base.endMonth;  /* DIYCOMPETITION_ENDMONTH (0x39) */
    if (endVal >= 96)
        endVal -= 96;
    diyLeagueEndingMonth = (uint16_t)endVal / 8;

    /* Entry 13: number of teams */
    menuEntries[13].fgWord = *(uint16_t *)&diyCompetitionStart.base.numTeams;

    /* Entry 14: play-each-team string */
    _SetPlayEachTeamString(&menuEntries[14]);

    /* Entry 15: points for a win */
    menuEntries[15].fgWord = *(uint16_t *)diyPointsForAWin;

    _ConvertMinMaxSubstitutesNumbers();
}

/* DIYLeagueOKSelected — gray out entries 3-19, hide/show OK/cancel (SWOS2.s line 41450) */
__attribute__((used, externally_visible))
void _DIYLeagueOKSelected(void)
{
    MenuEntry *e = &menuEntries[3];
    for (int16_t i = 0x10; i >= 0; i--) { /* 17 iterations: entries 3..19 */
        if (e->background == 2) {
            uint16_t bgVal = e->bgColor;
            if ((bgVal & 0xF) != 0) {
                bgVal = (bgVal & 0xFFF0) + 9; /* PINK_TO_BROWN_9 */
                e->bgColor = bgVal;
                e->isDisabled = 1;
            }
        }
        e++;
    }
    menuEntries[2].isInvisible = 1;
    menuEntries[0].isInvisible = 1;
    menuEntries[1].isInvisible = 0;
}

/* DesignDIYLeagueUpdatePromotionRelegationText — format promotion/relegation text (SWOS2.s line 41373) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueUpdatePromotionRelegationText(void)
{
    uint8_t *a6 = (uint8_t *)&diyCompetitionStart;
    DIYCompetition *a6s = (DIYCompetition *)a6;
    MenuEntry *e18 = &menuEntries[18];
    uint8_t *outBuf18 = (uint8_t *)e18->fg;

    uint8_t compType = a6[302]; /* offset $12E, extended buffer */

    if (compType >= 80 && compType <= 85) {
        /* Winner/qualifier format */
        uint16_t numQualify = (uint16_t)a6s->roundComplete;  /* 0x54 (league: num qualifiers) */
        if (numQualify == 1)
            _PrintFormatted(aWinnerQualifie, outBuf18, numQualify, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        else
            _PrintFormatted(aTop0Qualify, outBuf18, numQualify, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        e18->isInvisible = 0;
    } else {
        /* Promoted format */
        uint16_t numPromoted = (uint16_t)a6s->roundComplete;    /* 0x54 (league: num promoted) */
        uint16_t numPlayoffs = (uint16_t)a6s->competitionFinished;    /* 0x56 (league: num playoff spots) */
        if (numPlayoffs != 0) {
            _PrintFormatted(aTeamsPromoted0_0, outBuf18, numPromoted, numPlayoffs, 0, 0, 0, 0, 0, 0, 0, 0);
            e18->isInvisible = 0;
        } else if (numPromoted != 0) {
            _PrintFormatted(aTeamsPromoted0, outBuf18, numPromoted, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            e18->isInvisible = 0;
        } else {
            /* Both 0: move relegated text up to promoted position */
            menuEntries[19].y = e18->y;
        }
    }

    /* Relegated section */
    MenuEntry *e19 = &menuEntries[19];
    uint8_t *outBuf19 = (uint8_t *)e19->fg;
    uint16_t numRelegated = (uint16_t)a6s->currentRoundIndex;    /* 0x58 (league: num relegated) */
    uint16_t numRelPlayoffs = (uint16_t)a6s->numRounds;  /* 0x5A (league: num relegation playoffs) */

    if (numRelPlayoffs != 0) {
        _PrintFormatted(aTeamsRelegated_0, outBuf19, numRelegated, numRelPlayoffs, 0, 0, 0, 0, 0, 0, 0, 0);
        e19->isInvisible = 0;
    } else if (numRelegated != 0) {
        _PrintFormatted(aTeamsRelegated, outBuf19, numRelegated, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        e19->isInvisible = 0;
    }
    /* else: both 0, entry stays hidden */
}

/* DesignDIYLeagueFinish — menu onReturn for league design (SWOS2.s line 41328) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueFinish(void)
{
    if (pitchType != 0xFFFE) { /* not SEASONAL */
        *(uint16_t *)&diyCompetitionStart.base.pitchTypeOrSeason = 0; /* pitchTypeOrSeason = 0 */
        *(uint16_t *)&diyCompetitionStart.base.pitchType = pitchType; /* pitchType */

        /* Disable and gray out entry 8 */
        MenuEntry *e8 = &menuEntries[8];
        uint16_t bg8 = e8->bgColor;
        bg8 = (bg8 & 0xFFF0) + 9; /* PINK_TO_BROWN_9 */
        e8->bgColor = bg8;
        e8->isDisabled = 1;

        /* Disable and gray out entry 9 */
        MenuEntry *e9 = &menuEntries[9];
        uint16_t bg9 = e9->bgColor;
        bg9 = (bg9 & 0xFFF0) + 9;
        e9->bgColor = bg9;
        e9->isDisabled = 1;
    }

    _DIYSetMonthsOrPitchTypeEntries();
    _DIYLeagueSetVariousFields();

    if (diyLeagueLookOnly != 0) {
        _DesignDIYLeagueUpdatePromotionRelegationText();
        _DIYLeagueOKSelected();
        menuEntries[20].isInvisible = 1;
        menuEntries[21].isInvisible = 1;
        menuEntries[23].isInvisible = 1;
        _SetCurrentEntry(0x16); /* 22 */
        return;
    }

    if (diyLeagueSelectTeams != 0) {
        _DIYLeagueOKSelected();
        menuEntries[21].isInvisible = 1;
        _SetCurrentEntry(20);
        return;
    }

    /* Default view */
    menuEntries[20].isInvisible = 1;
    menuEntries[21].isInvisible = 0;
    menuEntries[23].isInvisible = 1;
}

/* DesignDIYLeagueChooseTeams — choose teams, start league play (SWOS2.s line 41488) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueChooseTeams(void)
{
    uint16_t numTeams = *(uint16_t *)&diyCompetitionStart.base.numTeams;
    uint8_t *teamListPtr = (uint8_t *)diyCompetitionStart.base.competitionName; /* FIELD_2 + 2 */

    _GoGetTeamsForPlay(teamListPtr, aPlayLeague, numTeams, 1, 1, 1);

    if (chooseTeamsResult == 0) {
        _SetExitMenuFlag();
        return;
    }

    _InitCareerVariables(1);
    _CopySelectedTeamNumbers(numTeams, (uint8_t *)diyLeagueTeamIds);
    _DIY_RecomputeCompetitionDerivedFields();
    _DIY_ShuffleLeagueTeamOrder();
    _InitTeams();
    _ShowDiyLeaguesTournamentMenu(0, 0, 0);
    _RestoreDIYLeague();
    _SetExitMenuFlag();
}

extern volatile uint16_t shortFire; /* for debug logging */
/* ShowDIYCupMenuWithParams — store params, show cup menu (SWOS2.s line 43290) */
__attribute__((used, externally_visible))
void _ShowDIYCupMenuWithParams(uint16_t arg0, uint16_t arg1, uint16_t arg2)
{
    SWOS_AUTOSKIP_LOG("ShowDIYCupMenuWithParams: ENTER arg0=%u arg1=%u arg2=%u seasonUpdate=%u",
                      (unsigned)arg0, (unsigned)arg1, (unsigned)arg2,
                      (unsigned)seasonUpdateInProgress);
    writeOnlyVar01 = 1;
    diyCupMenuArg0 = arg0;
    diyCupMenuArg0Copy = arg0;
    diyCupMenuArg1 = arg1;
    diyCupMenuArg2 = arg2;

    if (seasonUpdateInProgress != 0) {
#ifdef SWOS_DEBUG_AUTOSKIP
        unsigned debounce_iters = 0;
#endif
        do {
            _MenuCheckControls();
#ifdef SWOS_SDL_BUILD
            /* Pace at 50 Hz AND pump SDL events. _MenuCheckControls
             * reads pl1Fire from `ciaa_pra` which is only updated by
             * `InputSDL_PostFrame` on the SDL event-pump path; without
             * driving Platform_VBlank here the loop spins reading the
             * cached "fire pressed" state forever. On Linux the
             * SDL_Timer thread incidentally rescues us by full-pumping
             * events when the main thread looks idle, but on Windows
             * that backstop doesn't fire and the loop hangs. */
            extern int Platform_VBlank(void);
            Platform_VBlank();
#endif
#ifdef SWOS_DEBUG_AUTOSKIP
            debounce_iters++;
            if (debounce_iters <= 3 || debounce_iters == 100 || debounce_iters % 100 == 0) {
                SWOS_AUTOSKIP_LOG("  debounce iter=%u fire=%u shortFire=%u",
                                  debounce_iters, (unsigned)fire, (unsigned)shortFire);
            }
#endif
        } while (fire != 0);
        SWOS_AUTOSKIP_LOG("ShowDIYCupMenuWithParams: debounce exited fire=%u", (unsigned)fire);
    }

    SWOS_AUTOSKIP_LOG("ShowDIYCupMenuWithParams: -> _ShowMenu(diyCupMenu)");
    _ShowMenu(diyCupMenu);
    SWOS_AUTOSKIP_LOG("ShowDIYCupMenuWithParams: <- _ShowMenu returned");
    writeOnlyVar01 = 0;
}

/* DIYCupFinish — finalize cup menu display (SWOS2.s line 45807) */
__attribute__((used, externally_visible))
void _DIYCupFinish(void)
{
    uint16_t off74 = *(uint16_t *)&diyCompetitionStart.base.menuState; /* FIELD_3A+$10 */

    if (off74 == 6 || diyCupMenuArg0Copy == 0) {
        _DesignDIYCup_HideCurrentMatchTeamEntries();
    } else if ((int16_t)diyCupMenuArg0Copy < 0) {
        _DesignDIYCup_UpdateCurrentMatchResultHeaderEntries();
    } else {
        _DesignDIYCup_UpdateCurrentMatchTeamEntryStrings();
    }

    _DesignDIYCup_UpdateRoundOptionsVisibility();
    _DesignDIYCup_UpdateMenuEntriesVisibilityAndSelection();
    _DesignDIYCup_BuildMatchListEntryStrings();
    _DesignDIYCup_UpdateMatchListScrollAndRebuildStrings();
    _DesignDIYCup_BuildMenuTitleString();
}

/* DIYCupInit — init cup menu state, then fall through to finish (SWOS2.s line 45770) */
extern uint8_t currentMenu[]; /* needed by SWOS_AUTOSKIP_LOG sites */
__attribute__((used, externally_visible))
void _DIYCupInit(void)
{
    SWOS_AUTOSKIP_LOG("DIYCupInit: ENTER seasonUpdate=%u arg0=%u arg1=%u arg2=%u "
                      "menuState=%u roundComplete=%u competitionFinished=%u hasGroups=%u",
                      (unsigned)seasonUpdateInProgress,
                      (unsigned)diyCupMenuArg0, (unsigned)diyCupMenuArg1,
                      (unsigned)diyCupMenuArg2,
                      (unsigned)*(uint16_t *)&diyCompetitionStart.base.menuState,
                      (unsigned)*(uint16_t *)&diyCompetitionStart.base.roundComplete,
                      (unsigned)*(uint16_t *)&diyCompetitionStart.base.competitionFinished,
                      (unsigned)*(uint16_t *)&diyCompetitionStart.base.hasGroups);

    diyCupMatchListScrollIndex = 0;

    if (diyCupMenuArg0 != 0) {
        *(uint16_t *)&diyCompetitionStart.base.menuState = 0; /* FIELD_3A+$10 */
        if (*(uint16_t *)&diyCompetitionStart.base.roundComplete != 0) { /* FIELD_3A+$1A */
            *(uint16_t *)&diyCompetitionStart.base.menuState = 6;
            diyCupMenuArg0Copy = 0;
        }
    }

    if (*(uint16_t *)&diyCompetitionStart.base.competitionFinished != 0) { /* FIELD_3A+$1C */
        if (*(uint16_t *)&diyCompetitionStart.base.hasGroups == 0) { /* FIELD_3A+$E */
            diyCupMenuArg0Copy = 0;
            *(uint16_t *)&diyCompetitionStart.base.menuState = 6;
        }
    }

    if (diyCupMenuArg1 != 0) {
        SWOS_AUTOSKIP_LOG("DIYCupInit: arg1!=0 branch -> SetCurrentEntry(10)");
        diyCupMenuArg0Copy = 0;
        _SetCurrentEntry(10);
    }

    *(uint16_t *)&diyCompetitionStart.base.isActive = 1; /* FIELD_2 */
    diyCupMatchListDrawModeFlag = 0;

    if (seasonUpdateInProgress != 0) {
        SWOS_AUTOSKIP_LOG("DIYCupInit: seasonUpdate!=0 branch -> SetCurrentEntry(0x42=66)");
        diyCupMenuArg0Copy = 0;
        _SetCurrentEntry(0x42); /* 66 */
        menuEntries[9].isInvisible = 1;
        menuEntries[10].isInvisible = 1;
    }

    {
        Menu *m = (Menu *)currentMenu;
        SWOS_AUTOSKIP_LOG("DIYCupInit: pre-Finish selectedEntry=%d",
                          swos_autoskip_entry_ordinal((void *)m->selectedEntry, menuEntries));
    }

    /* Original asm falls through to DIYCupFinish via bra.w *+4 */
    _DIYCupFinish();

    {
        Menu *m = (Menu *)currentMenu;
        MenuEntry *sel = (MenuEntry *)m->selectedEntry;
        int ord = swos_autoskip_entry_ordinal((void *)m->selectedEntry, menuEntries);
        SWOS_AUTOSKIP_LOG("DIYCupInit: EXIT selectedEntry=%d controlMask=0x%04x onSelect=%p",
                          ord,
                          sel ? (unsigned)sel->controlMask : 0u,
                          sel ? (void *)(uintptr_t)sel->onSelect : (void *)0);
    }
}

/* DIYCupOnSelect10 — cup select/play callback (SWOS2.s line 45845) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect10(void)
{
    DIYCompetition *a0 = &diyCompetitionStart.base;
    int16_t hasGroups        = a0->hasGroups;
    int16_t competitionFinished = a0->competitionFinished;

    if (hasGroups != 0) {
        if (!(hasGroups == 2 && competitionFinished != 0)) {
            /* hasGroups != 0 and not the (==2 && has replays) exit-only case */
            a0->hasGroups = (int16_t)0xFFFF;
        }
    } else {
        /* hasGroups == 0 */
        if (competitionFinished != 0)
            a0->isActive = 2;
        /* else: both 0, just exit */
    }
    _SetExitMenuFlag();
}

/* DIYCupOnSelect16 — scroll match list up (SWOS2.s line 45878) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect16(void)
{
    if (diyCupMatchListScrollIndex == 0)
        return;
    diyCupMatchListScrollIndex--;
    _DesignDIYCup_BuildMatchListEntryStrings();
    _DrawMenu();
}

/* DIYCupOnSelect17 — scroll match list down (SWOS2.s line 45889) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect17(void)
{
    uint16_t d0 = diyCupMatchListScrollIndex + 6;
    uint16_t d1 = *(uint16_t *)&diyCompetitionStart.base.totalMatchesInRound; /* FIELD_3A+$18 */

    if (diyCupMatchListDrawModeFlag != 0)
        d1 >>= 1;

    if (d0 >= d1)
        return;

    diyCupMatchListScrollIndex++;
    _DesignDIYCup_BuildMatchListEntryStrings();
    _DrawMenu();
}

/* ==========================================================================
 * Chunk 132 — DIYLeagueChangePitchTypeOrSeason, DIYCupOnSelect1/3-8/Common,
 *             DesignDIYTournamentOnSelect89 (10 functions)
 * ========================================================================== */

extern uint16_t _DIY_PlayCupMatchAndAdvance(int16_t allowSquadChanges, int16_t mode);
extern uint8_t aPlayTournament[]; /* "PLAY TOURNAMENT" */

/* DIYLeagueChangePitchTypeOrSeason — toggle pitchTypeOrSeason, redraw (SWOS2.s line 41373) */
__attribute__((used, externally_visible))
void _DIYLeagueChangePitchTypeOrSeason(void)
{
    (&diyCompetitionStart.base)->pitchTypeOrSeason ^= 1; /* eori.w #1 */
    _DIYSetMonthsOrPitchTypeEntries();
    _CallDrawMultipleItems(5, &menuEntries[8]); /* entries 8-12 */
}

/* DIYCupOnSelectCommon — play cup match and update state (SWOS2.s line 46119)
 * Called from DIYCupOnSelect3-8 with mode parameter. */
static void DIYCupOnSelectCommonC(uint16_t mode)
{
    uint16_t result = _DIY_PlayCupMatchAndAdvance(0, mode);
    if (result != 0)
        return;

    /* Match results stored in globals by internal match code */
    diyCupMenuArg0Copy = 0xFFFF;

    DIYCompetition *a0s = &diyCompetitionStart.base;
    uint8_t *a0 = (uint8_t *)&diyCompetitionStart;

    if (a0s->roundComplete == 0)  /* 0x54 */
        goto state_check;

    if (a0s->hasGroups != 0) {  /* 0x48 */
        if (diyCupMenuArg0 != 0)
            goto state6;
        goto state_check_replays;
    }

state_check_replays:
    if (((DIYCompetitionEx *)a0)->matchData.cup.competitionFormat != 0)
        goto check_32E;
    if (((DIYCompetitionEx *)a0)->matchData.cup.roundFormat != 2)
        goto check_32E;
    if (diyCupMenuArg0 != 0)
        goto state6;
    a0s->menuState = 1;  /* 0x4A */
    goto state_check;

check_32E:
    if (((DIYCompetitionEx *)a0)->bracketScratch.secondLegCounter != 0)
        goto check_32E_nonzero;

    if (a0s->hasGroups != 0) {  /* 0x48 */
        /* state 5 */
        a0s->menuState = 5;
        if (a0s->hasGroups == 2)
            goto final_draw;
        goto state_check;
    }

    if (((DIYCompetitionEx *)a0)->bracketScratch.groupSectionFlag != 0)
        goto state_check_finished;

    {
        uint16_t roundIdx = (uint16_t)a0s->currentRoundIndex;  /* 0x58 */
        if (roundIdx + 1 == (uint16_t)a0s->numRounds)  /* 0x5A */
            goto state_check_finished;
    }
    goto state2;

check_32E_nonzero:
    if (((DIYCompetitionEx *)a0)->bracketScratch.groupSectionFlag != 0)
        goto state3;
    if (a0s->hasGroups != 0)  /* 0x48 */
        goto state3;
    {
        uint16_t roundIdx = (uint16_t)a0s->currentRoundIndex;  /* 0x58 */
        if (roundIdx + 1 != (uint16_t)a0s->numRounds)  /* 0x5A */
            goto state2;
    }
    if (diyCupMenuArg0 != 0)
        goto state6;

state3:
    a0s->menuState = 3;  /* 0x4A */
    goto state_check;

state2:
    a0s->menuState = 2;
    goto state_check;

state_check_finished:
    if (diyCupMenuArg0 != 0)
        goto state6;
    a0s->menuState = 4;
    goto state_check;

state6:
    a0s->menuState = 6;
    goto state_check;

state_check:
    if (a0s->competitionFinished != 0)  /* 0x56 */
        a0s->menuState = 6;

final_draw:
    _DesignDIYCup_UpdateCurrentMatchResultHeaderEntries();
    _DesignDIYCup_UpdateMatchListScrollAndRebuildStrings();
    _DesignDIYCup_UpdateMenuEntriesVisibilityAndSelection();
    _DesignDIYCup_BuildMatchListEntryStrings();
    _DrawMenu();
}

/* DIYCupOnSelect3 — cup round select callback (SWOS2.s line 46078) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect3(MenuEntry *entry)
{
    *(uint16_t *)&diyCompetitionStart.base.slotStateA = entry->ordinal; /* FIELD_3A+4 */
    DIYCupOnSelectCommonC(2);
}

/* DIYCupOnSelect4 — cup round select callback (SWOS2.s line 46085) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect4(MenuEntry *entry)
{
    *(uint16_t *)&diyCompetitionStart.base.slotStateA = entry->ordinal;
    DIYCupOnSelectCommonC(3);
}

/* DIYCupOnSelect5 — cup round select callback (SWOS2.s line 46092) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect5(MenuEntry *entry)
{
    *(uint16_t *)&diyCompetitionStart.base.slotStateA = entry->ordinal;
    DIYCupOnSelectCommonC(0);
}

/* DIYCupOnSelect6 — cup round select callback (SWOS2.s line 46099) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect6(MenuEntry *entry)
{
    *(uint16_t *)&diyCompetitionStart.base.slotStateC = entry->ordinal; /* FIELD_3A+8 */
    DIYCupOnSelectCommonC(1);
}

/* DIYCupOnSelect7 — cup round select callback (SWOS2.s line 46106) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect7(MenuEntry *entry)
{
    *(uint16_t *)&diyCompetitionStart.base.slotStateC = entry->ordinal;
    DIYCupOnSelectCommonC(3);
}

/* DIYCupOnSelect8 — cup round select callback (SWOS2.s line 46113) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect8(MenuEntry *entry)
{
    *(uint16_t *)&diyCompetitionStart.base.slotStateC = entry->ordinal;
    DIYCupOnSelectCommonC(0);
}

/* DIYCupOnSelect1 — cup state dispatch (SWOS2.s line 45634) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect1(void)
{
    DIYCompetitionEx *a0 = &diyCompetitionStart;
    DIYCompetition   *a0s = &a0->base;

    diyCupMatchListDrawModeFlag = 0;
    uint16_t state = (uint16_t)a0s->menuState;  /* 0x4A */

    switch (state) {
    case 1:
        a0s->menuState = 0;
        _DIY_UpdateCompetitionProgress();
        break;

    case 2:
        diyCupMatchListDrawModeFlag = 1;
        _DesignDIYCup_UpdateRoundOptionsVisibility();
        if (diyCupMenuArg0 != 0) {
            a0s->menuState = 6;
        } else if (a0->bracketScratch.secondLegCounter != 0) {
            a0s->menuState = 3;
        } else {
            a0s->menuState = 4;
        }
        break;

    case 3:
        a0s->menuState = 0;
        _DIY_UpdateCompetitionProgress();
        break;

    case 4:
        a0s->menuState = 0;
        _DIY_UpdateCompetitionProgress();
        break;

    case 5:
        a0s->isActive = 2;
        _SetExitMenuFlag();
        return;

    default:
        /* State >= 6 or 0: loc_134806 path */
        diyCupMenuArg0Copy = 1;
        _DesignDIYCup_UpdateMatchListAndRedrawIfNeeded();
        _DesignDIYCup_DrawCurrentMatchTeamEntries();
        /* loc_134C96 */
        _DesignDIYCup_UpdateMenuEntriesVisibilityAndSelection();
        _CallDrawMultipleItems(8, &menuEntries[1]);
        return;
    }

    /* Common path for cases 1-4: loc_13481A */
    _DesignDIYCup_BuildMenuTitleString();
    _DesignDIYCup_HideCurrentMatchTeamEntries();
    diyCupMatchListScrollIndex = 0;
    _DesignDIYCup_BuildMatchListEntryStrings();
    _DesignDIYCup_UpdateMenuEntriesVisibilityAndSelection();
    _DrawMenu();
}

/* DesignDIYTournamentOnSelect89 — choose teams & play tournament (SWOS2.s line 48984) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect89(void)
{
    DIYCompetition *buf = &diyFileBufferCopy.header;
    uint16_t numTeams      = (uint16_t)buf->numTeams;
    uint8_t *teamListPtr   = (uint8_t *)buf->competitionName;

    _GoGetTeamsForPlay(teamListPtr, aPlayTournament, numTeams, 1, 1, 1);

    if (chooseTeamsResult == 0) {
        _SetExitMenuFlag();
        return;
    }

    _InitCareerVariables(1);
    _CopySelectedTeamNumbers(numTeams, (uint8_t *)diyFileBufferCopy.teamIds);
    /* nullsub_12 — no-op */
    _DIYTournament_RecalculateMatchesPerTeam();
    _DIY_LoadCurrentCompetitionSlot();
    _InitTeams();
    _DIY_EditCompetitionSlot(0, 0, 0);
    _RestoreDIYTournament();
    _SetExitMenuFlag();
}

/* ---- Chunk 141: SetupDIYCompetitionStatsMenu ---- */
extern uint16_t diyUnknownWord_2C;
extern uint16_t diyNumberOfTeams;
extern uint8_t *diyNameOfCompetitionPtr;
extern uint8_t *diyTeamsBufferPtr;
extern uint16_t diyCompetitionType;
extern uint8_t competitionStatsMenu[];

__attribute__((used, externally_visible))
void _SetupDIYCompetitionStatsMenu(void) {
    DIYCompetitionEx *a0 = &diyCompetitionStart;
    DIYCompetition   *a0s = &a0->base;
    uint16_t isCup = ((uint16_t)a0s->competitionType == DIY_CUP);

    /* a0+44 = 0x2C = compId/slotId word (DIYCompetition fields compId/slotId
     * read as a uint16; we want the same packed value). */
    diyUnknownWord_2C = *(uint16_t *)&a0s->compId;
    diyNumberOfTeams  = (uint16_t)a0s->numTeams;
    if ((uint16_t)a0s->hasGroups)
        diyNumberOfTeams = (uint16_t)a0s->numTeamsInStage;

    diyNameOfCompetitionPtr = (uint8_t *)a0s->competitionName;
    /* a0+302 = league teamIds region; a0+590 = cup teamIds region.
     * Match the existing struct layout: leagueTeamIds is at parent 0x12E
     * (= a0+0x12E = a0+302), cupTeamIds at parent 0x24E (= a0+0x24E = a0+590). */
    diyTeamsBufferPtr = isCup
        ? (uint8_t *)a0->cupTeamIds
        : (uint8_t *)a0->matchData.league.leagueTeamIds;
    diyCompetitionType = isCup ? 1 : 0;

    if ((uint16_t)a0s->hasGroups)
        diyCompetitionType = 2;

    _ShowMenu(competitionStatsMenu);
}

/* ================================================================
 * Chunk 148: ChooseTeamsSetHeader, CheckNumTeamsAllowed, Bureaucracy
 * ================================================================ */

extern uintptr_t chooseTeamsHeader1stPart;  /* pointer to header prefix string (or 0) */
extern uintptr_t gameTypeStr;               /* pointer to game type string (or 0) */
extern uint8_t *curCountryStr;
extern uint16_t selectTeamsNoLimit;
extern uint16_t numNeededTeams;
extern uintptr_t playWhatStr;               /* pointer to "play what" string */
extern uint8_t aChooseTeamsFor[];          /* "CHOOSE TEAMS FOR " */
extern uint8_t aChangeTeamsFor[];          /* "CHANGE TEAMS FOR " */
extern uint8_t aSelect1MoreTea[];          /* "SELECT 1 MORE TEAM" */
extern uint8_t aSelect0MoreTea[];          /* "SELECT %0 MORE TEAMS" */
extern uint8_t aSelect1LessTea[];          /* "SELECT 1 LESS TEAM" */
extern uint8_t aSelect0LessTea[];          /* "SELECT %0 LESS TEAMS" */
extern uint8_t aError[];                   /* "ERROR" */
extern int16_t bureaucracyMenuItems[];     /* {58, 57, 55, 0, -1} */

extern MenuEntry menuEntries[];
extern void _PrintFormatted(const uint8_t *fmt, uint8_t *out,
                            uint16_t n0, uint16_t n1, uint16_t n2, uint16_t n3,
                            uint16_t n4, uint16_t n5, uint16_t n6,
                            const uint8_t *strA, const uint8_t *strB, const uint8_t *strC);
extern uint16_t _GetStringLength(const uint8_t *str);
extern uint8_t *_StringCopy(const uint8_t *src, uint8_t *dst);
extern void _DrawMenuItemsWaitForVSync(const int16_t *items);

/* ChooseTeamsSetHeader: build "CHOOSE/CHANGE TEAMS FOR <type> - <country>"
 * header string in menuEntries[0].fg. */
__attribute__((used, externally_visible))
void _ChooseTeamsSetHeader(void)
{
    uint8_t *dst = (uint8_t *)menuEntries[0].fg;
    uint8_t *headerPrefix = (uint8_t *)chooseTeamsHeader1stPart;

    if (headerPrefix == 0) {
        headerPrefix = aChooseTeamsFor;
        if (numSelectedTeams == numNeededTeams) {
            headerPrefix = aChangeTeamsFor;
        }
    }
    dst = _StringCopy(headerPrefix, dst);

    uint8_t *gameType = (uint8_t *)gameTypeStr;
    if (gameType != 0) {
        dst = _StringCopy(gameType, dst);
    }

    /* Append " - <country>" if total length <= 47 (0x2F) */
    uint16_t headerLen = _GetStringLength((uint8_t *)menuEntries[0].fg);
    uint16_t countryLen = _GetStringLength(curCountryStr);
    if ((uint16_t)(headerLen + countryLen) <= 0x2F) {
        *dst++ = ' ';
        *dst++ = '-';
        *dst++ = ' ';
        _StringCopy(curCountryStr, dst);
    }
}

/* CheckNumTeamsAllowed: update menu entry visibility and status text
 * based on numSelectedTeams vs numNeededTeams. */
__attribute__((used, externally_visible))
void _CheckNumTeamsAllowed(void)
{
    if (selectTeamsNoLimit != 0) {
        /* No limit mode: hide play, status, and info entries */
        menuEntries[57].isInvisible = 1;
        menuEntries[58].isInvisible = 1;
        menuEntries[55].isInvisible = 1;
        return;
    }

    if (numSelectedTeams == numNeededTeams) {
        /* Exact match: show play button, hide status text */
        menuEntries[57].isInvisible = 0;
        menuEntries[58].isInvisible = 1;
        menuEntries[57].content = 2;
        menuEntries[57].fg = playWhatStr;
    } else {
        /* Mismatch: hide play button, show status text */
        menuEntries[57].isInvisible = 1;
        menuEntries[58].isInvisible = 0;
    }

    /* Show entry 55 (unless no teams selected at all) */
    menuEntries[55].isInvisible = 0;
    if (numSelectedTeams == 0)
        menuEntries[55].isInvisible = 1;

    /* Build status text in entry 58's fg buffer */
    uint8_t *statusDst = (uint8_t *)menuEntries[58].fg;
    int16_t diff = (int16_t)numSelectedTeams - (int16_t)numNeededTeams;

    if (diff == 0) {
        /* Equal — write "ERROR" (entry is hidden anyway) */
        _StringCopy(aError, statusDst);
    } else if (diff < 0) {
        /* Need more teams */
        uint16_t needed = (uint16_t)(-diff);
        if (needed == 1) {
            _StringCopy(aSelect1MoreTea, statusDst);
        } else {
            _PrintFormatted(aSelect0MoreTea, statusDst,
                            needed, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }
    } else {
        /* Too many teams */
        uint16_t excess = (uint16_t)diff;
        if (excess == 1) {
            _StringCopy(aSelect1LessTea, statusDst);
        } else {
            _PrintFormatted(aSelect0LessTea, statusDst,
                            excess, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }
    }
}

/* Bureaucracy: update header and team count status, then redraw items.
 * Saves/restores a5 (menu entry pointer) around calls. */
__attribute__((used, externally_visible))
void _Bureaucracy(void)
{
    _ChooseTeamsSetHeader();
    _CheckNumTeamsAllowed();
    _DrawMenuItemsWaitForVSync(bureaucracyMenuItems);
}


/* ================================================================
 * Chunk 150 — DIY League/Tournament Menu Helpers (13 functions)
 * ================================================================ */

extern uint16_t currentDiyLine;
extern uint16_t diyAllMatchesPlayed;
extern uint16_t diyFixtureViewMode;
extern uint16_t diyTournamentViewOnly;
extern uint16_t diyLeagueTournament_viewOptionsAvailable;
extern uint16_t diyMinSubstitutes;
extern volatile uint16_t seasonUpdateInProgress;
extern TeamFileEx *diyLeftTeamPtr;
extern TeamFileEx *diyRightTeamPtr;
extern uint16_t diyLeftTeamResult;
extern uint16_t diyRightTeamResult;
extern uint16_t numDiyResults;
/* diyLeagueTeamIds already declared above as uint16_t[] */
extern uint16_t diyLeagueTotalMatches;
extern uint16_t diyLeagueMatchProgress;
extern uint16_t diyInitFocusOverride;

extern uint8_t aPlayMatch[];       /* "PLAY MATCH" */
extern uint8_t aV[];               /* "V" */
extern uint8_t aMinus[];           /* "-" */
extern uint8_t aNextRound[];       /* "NEXT ROUND" */
extern uint8_t aNextMatch[];       /* "NEXT MATCH" */

extern void _SetupDIYCompetitionStatsMenu(void);
extern void _DrawMenu(void);
extern void _CallDrawMultipleItems(uint16_t maxItems, MenuEntry *firstEntry);

/* Forward declarations for Chunk 152 functions */
void _DIYLeagueTournamentMatchCommon(uint16_t action);
void _DIYLeagueTournament_UpdateMenuEntryVisibilityAndFocus(void);

/* ================================================================
 * jmp_SetExitMenuFlag_7  (SWOS2.s line 42414)
 * ================================================================ */
__attribute__((used, externally_visible))
void _jmp_SetExitMenuFlag_7(void)
{
    _SetExitMenuFlag();
}

/* ================================================================
 * DIYLeagueTournamentStats  (SWOS2.s line 42419)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentStats(void)
{
    _SetupDIYCompetitionStatsMenu();
}

/* ================================================================
 * DIYLeagueTournamentExit  (SWOS2.s line 42424)
 *
 * Handles exit from DIY league/tournament menu with various guards.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentExit(void)
{
    DIYCompetitionEx *compEx = &diyCompetitionStart;
    uint8_t *a0 = (uint8_t *)compEx;
    int16_t field48 = (int16_t)compEx->base.hasGroups;
    /* +0x60 is minSubstitutes on 32-bit; on 64-bit the raw offset reads
     * 8 bytes too early due to DIYCompetition pointer-width growth.
     * Use the typed field. See project_raw_offset_64bit_shift_trap.md. */
    int16_t field60 = (int16_t)compEx->minSubstitutes;

    extern uint16_t youreSacked;
    extern uint16_t gameType;
    SWOS_AUTOSKIP_LOG("[bug1] _DIYLeagueTournamentExit CALLED: field48=0x%04X field60=0x%04X "
                      "youreSacked=%u gameType=%u diyTournamentViewOnly=%u",
                      (unsigned)(uint16_t)field48, (unsigned)(uint16_t)field60,
                      (unsigned)youreSacked, (unsigned)gameType,
                      (unsigned)diyTournamentViewOnly);

    if (field48 == 0) {
        /* field48 == 0 path */
        if (field60 != 0)
            goto set_ff_and_exit;
        *(int16_t *)&a0[2] = 2;    /* competitionType area */
        if (diyTournamentViewOnly != 0) {
            /* fall through to exit */
        }
        SWOS_AUTOSKIP_LOG("[bug1] _DIYLeagueTournamentExit: path A — SetExitMenuFlag");
        _SetExitMenuFlag();
        return;
    }

    if (field48 == 2 && field60 == 0) {
        if (diyTournamentViewOnly != 0) {
            /* fall through */
        }
        SWOS_AUTOSKIP_LOG("[bug1] _DIYLeagueTournamentExit: path B — SetExitMenuFlag");
        _SetExitMenuFlag();
        return;
    }

set_ff_and_exit:
    if (field48 != 0) {
        *(int16_t *)&a0[0x48] = (int16_t)0xFFFF;
    }
    SWOS_AUTOSKIP_LOG("[bug1] _DIYLeagueTournamentExit: path C — set 0x48=0xFFFF, SetExitMenuFlag");
    _SetExitMenuFlag();
}

/* ================================================================
 * DIYLeagueTournament_UpdateViewOptionsAvailability  (SWOS2.s line 42452)
 *
 * Sets diyLeagueTournament_viewOptionsAvailable to 1 if
 * enough matches played (>=75% or group stage) and gameType==4.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournament_UpdateViewOptionsAvailability(void)
{
    diyLeagueTournament_viewOptionsAvailable = 0;

    /* Check if in group stage (hasGroups field at $3A+$E = $48) */
    if ((&diyCompetitionStart.base)->hasGroups != 0) {
        diyLeagueTournament_viewOptionsAvailable = 1;
        return;
    }

    if (gameType != 4)
        return;

    uint8_t teamIdByte = diyLeagueTeamIds[0];
    if (teamIdByte >= 0x50 && teamIdByte <= 0x55) {
        /* Group stage range ('P'..'U') → view available */
        diyLeagueTournament_viewOptionsAvailable = 1;
        return;
    }

    /* Check if 75% of matches played */
    uint16_t threshold = (uint16_t)((uint16_t)(diyLeagueTotalMatches * 3) >> 2);
    if (diyLeagueMatchProgress >= threshold) {
        diyLeagueTournament_viewOptionsAvailable = 1;
    }
}

/* ================================================================
 * DIYSetTeamNamesAndResults  (SWOS2.s line 42560)
 *
 * Populates 5 consecutive menu entries (starting at 11, 0x268/56)
 * with separator, left team name, right team name, left score, right score.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYSetTeamNamesAndResults(void)
{
    TeamFileEx *a1 = diyLeftTeamPtr;
    TeamFileEx *a2 = diyRightTeamPtr;
    uint16_t d1 = diyLeftTeamResult;
    uint16_t d2 = diyRightTeamResult;

    MenuEntry *e = &menuEntries[11];  /* 0x268/56=11 */
    e->isInvisible = 0;
    e->fg = (uintptr_t)aMinus;         /* "-" */

    e++;  /* entry 12 */
    e->isInvisible = 0;
    e->fg = (uintptr_t)a1->team.teamName;

    e++;  /* entry 13 */
    e->isInvisible = 0;
    e->fg = (uintptr_t)a2->team.teamName;

    e++;  /* entry 14 */
    e->isInvisible = 0;
    e->fgWord = d1;         /* left score as 16-bit value in fg field */

    e++;  /* entry 15 */
    e->isInvisible = 0;
    e->fgWord = d2;         /* right score */
}

/* ================================================================
 * DIYLeagueTournament_EnsureCurrentFixtureVisible  (SWOS2.s line 42960)
 *
 * Adjusts currentDiyLine so the current fixture index is within
 * the visible scroll window. Returns nonzero if scroll changed.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _DIYLeagueTournament_EnsureCurrentFixtureVisible(void)
{
    uint16_t targetLine = 0;

    if (diyFixtureViewMode != 0) {
        /* field_3A + $22 = offset 0x5C = subRoundCounter */
        targetLine = (&diyCompetitionStart.base)->subRoundCounter;
        if (diyFixtureViewMode != 1) {
            if ((int16_t)(targetLine - 1) >= 0) {
                targetLine--;
            } else {
                /* Wrap: use totalRounds-1 from field_3A+$16 = offset 0x50 */
                targetLine = (uint16_t)(&diyCompetitionStart.base)->matchPairsPerRound - 1;
            }
        }
    }

    uint16_t changed = 0;

    /* Scroll up while target is above visible area */
    while (targetLine < currentDiyLine) {
        currentDiyLine--;
        changed = 1;
    }

    /* Scroll down while target is below visible area */
    for (;;) {
        uint16_t visibleEnd = currentDiyLine + numDiyResults;
        if (targetLine < visibleEnd)
            break;
        currentDiyLine++;
        changed = 1;
    }

    return changed;
}

/* ================================================================
 * DIYLeagueTournament_ScrollFixtureListToCurrentFixture  (SWOS2.s line 42951)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournament_ScrollFixtureListToCurrentFixture(void)
{
    if (_DIYLeagueTournament_EnsureCurrentFixtureVisible() != 0) {
        _DrawMenu();
    }
}

/* ================================================================
 * DIYLeagueTournamentScrollUp  (SWOS2.s line 42922)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentScrollUp(void)
{
    if (currentDiyLine == 0) return;
    currentDiyLine--;
    /* nullsub_15 omitted (no-op) */
    _DrawMenu();
}

/* ================================================================
 * DIYLeagueTournamentScrollDown  (SWOS2.s line 42933)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentScrollDown(void)
{
    uint16_t bottom = currentDiyLine + numDiyResults;
    /* field_3A + $16 = offset 0x50 = halfTeams (total fixture lines) */
    if (bottom >= (uint16_t)(&diyCompetitionStart.base)->matchPairsPerRound)
        return;
    currentDiyLine++;
    /* nullsub_15 omitted (no-op) */
    _DrawMenu();
}

/* ================================================================
 * DIYLeagueTournament_SetupCurrentFixtureTeamNames  (SWOS2.s line 42615)
 *
 * Gets current fixture team pointers and fills 5 menu entries:
 * "V" separator, left team name, right team name, 2 invisible entries.
 * ================================================================ */
extern uint8_t selTeamNameBase;

__attribute__((used, externally_visible))
void _DIYLeagueTournament_SetupCurrentFixtureTeamNames(void)
{
    _DIY_GetCurrentFixtureTeamPtrs();

    /* _DIY_GetCurrentFixtureTeamPtrs stores results in fixtureTeam1CompPtr/fixtureTeam2CompPtr */
    uint8_t *team1 = fixtureTeam1CompPtr;
    uint8_t *team2 = fixtureTeam2CompPtr;

    MenuEntry *e = &menuEntries[11];  /* 0x268/56=11 */
    e->isInvisible = 0;
    e->fg = (uintptr_t)aV;             /* "V" */

    e++;  /* entry 12 */
    e->isInvisible = 0;
    /* DIYTeamStats teamSelOffset (+0x2B4) → name offset, add selTeamNameBase.
     * teamSelOffset is int32_t (signed) — careerTeam lives BELOW
     * selTeamsPtr in BSS so its offset is negative. Reading into
     * uint32_t and then adding to a pointer on 64-bit zero-extends the
     * value to a huge positive number (~4 GB), and the pointer add
     * carries past the 32-bit boundary, leaving fg with upper-32-bits=2
     * and a low-half that looks almost-valid — exactly the corruption
     * pattern that left the bottom fixture row's left-team-name blank.
     * Keep signed so the 64-bit sign-extension handles negative offsets
     * correctly. */
    {
        int32_t nameOff = ((DIYTeamStats *)(team1 + DIY_STANDINGS_OFFSET))->teamSelOffset;
        e->fg = (uintptr_t)diy_resolve_team_name(nameOff);
    }

    e++;  /* entry 13 */
    e->isInvisible = 0;
    {
        int32_t nameOff = ((DIYTeamStats *)(team2 + DIY_STANDINGS_OFFSET))->teamSelOffset;
        e->fg = (uintptr_t)diy_resolve_team_name(nameOff);
    }

    e++;  /* entry 14 — invisible */
    e->isInvisible = 1;

    e++;  /* entry 15 — invisible */
    e->isInvisible = 1;
}

/* ================================================================
 * DIYLeagueTournament_DrawPlayMatchAndFixtureHeader  (SWOS2.s line 42604)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournament_DrawPlayMatchAndFixtureHeader(void)
{
    _DIYLeagueTournament_SetupCurrentFixtureTeamNames();

    MenuEntry *e1 = &menuEntries[1];  /* 0x38/56=1 */
    e1->fg = (uintptr_t)aPlayMatch;    /* "PLAY MATCH" */
    _CallDrawMenuItem(e1);

    _CallDrawMultipleItems(5, &menuEntries[11]);  /* 0x268/56=11, draw 5 entries */
}

/* ================================================================
 * DIYLeagueTournamentNextMatch  (SWOS2.s line 42586)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentNextMatch(void)
{
    DIYCompetitionEx *compEx = &diyCompetitionStart;
    uint8_t *a0 = (uint8_t *)compEx;
    /* +0x60 = minSubstitutes; raw offset wrong on 64-bit, use typed
     * field. See project_raw_offset_64bit_shift_trap.md. */
    int16_t field60 = (int16_t)compEx->minSubstitutes;

    if (field60 == 0) {
        *(int16_t *)&a0[2] = 2;
        _SetExitMenuFlag();
        return;
    }

    if (diyFixtureViewMode == 1) return;

    diyFixtureViewMode = 1;
    _DIYLeagueTournament_ScrollFixtureListToCurrentFixture();
    _DIYLeagueTournament_DrawPlayMatchAndFixtureHeader();

    /* loc_132E24 tail: update visibility and draw entries */
    _DIYLeagueTournament_UpdateMenuEntryVisibilityAndFocus();
    _CallDrawMultipleItems(8, &menuEntries[1]);  /* 0x38/56=1, draw 8 entries */
}

/* ================================================================
 * DIYLeagueTournamentInit  (SWOS2.s line 42219)
 *
 * Initialize DIY league/tournament menu state.
 * ================================================================ */
extern uint16_t diyTournamentType; /* used by SWOS_AUTOSKIP_LOG */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentInit(void)
{
    SWOS_AUTOSKIP_LOG("DIYLeagueInit: ENTER seasonUpdate=%u type=%u viewOnly=%u focusOverride=%u "
                      "minSubs=%u hasGroups=%u",
                      (unsigned)seasonUpdateInProgress,
                      (unsigned)diyTournamentType,
                      (unsigned)diyTournamentViewOnly,
                      (unsigned)diyInitFocusOverride,
                      (unsigned)diyMinSubstitutes,
                      (unsigned)(&diyCompetitionStart.base)->hasGroups);

    currentDiyLine = 0;
    diyAllMatchesPlayed = 0;
    _DIYLeagueTournament_UpdateViewOptionsAvailability();

    if (diyMinSubstitutes == 0) {
        /* Check hasGroups (field_3A+$E = 0x48) */
        if ((&diyCompetitionStart.base)->hasGroups == 0) {
            diyAllMatchesPlayed = 1;
            diyFixtureViewMode = 0;
        }
    }

    if (diyTournamentViewOnly != 0) {
        SWOS_AUTOSKIP_LOG("DIYLeagueInit: viewOnly!=0 -> SetCurrentEntry(10)");
        diyFixtureViewMode = 0;
        _SetCurrentEntry(10);
    }

    /* diyCompetitionStart+DIY_COMPETITION_FIELD_2 = offset 2 */
    *(uint16_t *)&diyCompetitionStart.base.isActive = 1;

    if (seasonUpdateInProgress != 0) {
        SWOS_AUTOSKIP_LOG("DIYLeagueInit: seasonUpdate!=0 -> SetCurrentEntry(42)");
        diyFixtureViewMode = 0;
        _SetCurrentEntry(42);
        menuEntries[9].isInvisible = 1;    /* 0x1F8/56=9 */
        menuEntries[10].isInvisible = 1;   /* 0x230/56=10 */
    }

    {
        Menu *m = (Menu *)currentMenu;
        SWOS_AUTOSKIP_LOG("DIYLeagueInit: pre-OnRestore selectedEntry=%d",
                          swos_autoskip_entry_ordinal((void *)m->selectedEntry, menuEntries));
    }

    /* bra.w *+4 falls through to DIYLeagueTournamentOnRestore */
    _DIYLeagueTournamentOnRestore();

    {
        Menu *m = (Menu *)currentMenu;
        MenuEntry *sel = (MenuEntry *)m->selectedEntry;
        int ord = swos_autoskip_entry_ordinal((void *)m->selectedEntry, menuEntries);
        SWOS_AUTOSKIP_LOG("DIYLeagueInit: EXIT selectedEntry=%d controlMask=0x%04x onSelect=%p",
                          ord,
                          sel ? (unsigned)sel->controlMask : 0u,
                          sel ? (void *)(uintptr_t)sel->onSelect : (void *)0);
    }
}

/* ================================================================
 * Chunk 151: IsNationalTeamCompetitionOrQualification
 *
 * Scans nationalTeamCompetitionsAndQualifications1 list for a match.
 * Returns 0 if found (is national), 1 if not found.
 *
 * In asm, returns via Z flag: Z=set if found, Z=clear if not found.
 * ================================================================ */
extern void *nationalTeamCompetitionsAndQualifications1[];

__attribute__((used, externally_visible))
uint16_t _IsNationalTeamCompetitionOrQualification(uint32_t key)
{
    uint32_t *p = (uint32_t *)nationalTeamCompetitionsAndQualifications1;
    while (*p != 0xFFFFFFFF) {
        if (*p == key)
            return 0;  /* found — is national */
        p++;
    }
    return 1;  /* not found — not national */
}

/* ================================================================
 * Chunk 151: InitCompetitionFromRecord
 *
 * Dispatcher: checks record type byte at offset 1 and calls the
 * appropriate init function.
 *   type 0 → InitDIYCompetitionFromTableEntry
 *   type 1 → InitDIYCupFromRecord
 *   else   → InitDIYLeagueFromRecord
 * ================================================================ */
extern void *europeanCupsandNationalTeamCompetitionsAndQualifications[];
extern void *countriesTable[];
extern const char *divisionStringTable[];
extern const char aCup_0[];
extern const char aLeague_1[];

/* Helper: check if record pointer is in the european cups / national team list.
 * If found, clear the field at offset 0x3C in the competition buffer. */
static void checkEuropeanCupsAndClear3C(uint8_t *record, uint8_t *comp)
{
    uint32_t *p = (uint32_t *)europeanCupsandNationalTeamCompetitionsAndQualifications;
    while (*p != 0xFFFFFFFF) {
        if (*p == (uintptr_t)record) {
            ((DIYCompetition *)comp)->substitutesCount = 0;  /* 0x3C */
            return;
        }
        p++;
    }
}

/* Helper: if not a national team competition, prepend country name + space to dst.
 * Returns updated dst pointer.
 *
 * Records like euroCup/cupWinnersCup/uefaCup carry teamNumber = -1 (= 0xFF) at
 * record[2], meaning "no specific country". countriesTable[255] is NULL (the
 * Amiga binary leaves that slot as `dc.l 0`). The original 68k code dereferenced
 * NULL+1 anyway and got back zero bytes from the system vector area, so the
 * empty walk terminated immediately and StringCopy copied an empty string — a
 * latent bug that doesn't fault on Amiga because low memory is mapped there.
 * On SDL low memory is unmapped and the read SIGSEGVs, so we have to bail out
 * explicitly when there's no valid country slot. */
static uint8_t *prependCountryNameIfClub(uint8_t *record, uint8_t *dst)
{
    if (_IsNationalTeamCompetitionOrQualification((uintptr_t)record) != 0) {
        uint16_t countryIdx = (uint8_t)((Competition *)record)->teamNumber;
        uint8_t *countryEntry = (uint8_t *)countriesTable[countryIdx];
        if (countryEntry == 0)
            return dst;
        uint8_t *namePtr = countryEntry + 1;
        while (*namePtr) namePtr++;
        namePtr++; /* past null — points to second string */
        dst = _StringCopy(namePtr, dst);
        *dst++ = ' ';
    }
    return dst;
}

/* Helper: set pitch type fields at offsets 0x34/0x36 in competition buffer. */
static void setPitchTypeFields(uint8_t *comp)
{
    DIYCompetition *compS = (DIYCompetition *)comp;
    compS->pitchTypeOrSeason = 1;  /* 0x34 */
    if ((int16_t)pitchType != (int16_t)0xFFFE) {
        compS->pitchTypeOrSeason = 0;
        compS->pitchType = (int16_t)pitchType;  /* 0x36 */
    }
}

/* ================================================================
 * Chunk 227: InitDIYCompetitionFromTableEntry, InitDIYCupFromRecord,
 *            InitDIYLeagueFromRecord
 * ================================================================ */

__attribute__((used, externally_visible))
void _InitDIYCompetitionFromTableEntry(uint16_t idx, uint8_t *table)
{
    /* `table` points at a CompetitionLeague_* record whose
     * `divisions[]` starts at byte 13. The Division struct is
     * 6 bytes per entry. The original asm tested byte
     * (idx*6 + 13) = divisions[idx].teams (field at Division
     * offset 0). */
    Division *divisions = (Division *)(table + 13);
    if (divisions[idx].teams & 1) {
        panicReasonCode = 0x1313;
        _PanicHaltLoop();
    }

    Competition       *compHdr = (Competition *)table;
    CompetitionLeague *league  = (CompetitionLeague *)table;
    uint16_t d7 = idx;

    _DIY_InitCompetitionDefaults();

    uint8_t *a6 = (uint8_t *)&diyCompetitionStart;
    DIYCompetition *a6s = (DIYCompetition *)a6;
    a6s->substitutesCount = 1;  /* 0x3C */
    checkEuropeanCupsAndClear3C(table, a6);

    a6s->field_2E   = (uint8_t)compHdr->compNumber;  /* 0x2E */
    a6s->startMonth = compHdr->beginMonth;           /* 0x38 */
    a6s->endMonth   = compHdr->endMonth;             /* 0x39 */

    /* a5+0xA reads a word starting at CompetitionLeague.rounds; the
     * `rounds < 0` sign test asks the original asm whether the
     * MSBit of that combined word is set. */
    if ((int16_t)*(uint16_t *)&league->rounds < 0)
        ((DIYCompetitionEx *)a6)->matchData.league.hasGroupsDiy = 1;

    setPitchTypeFields(a6);

    /* Per-division fields from divisions[idx]. */
    Division *div = &divisions[idx];
    uint16_t d1;

    d1 = (uint8_t)div->promoted;
    a6s->roundComplete = (int16_t)d1;   /* 0x54 */

    d1 = (uint8_t)div->promotedPo;
    if ((int8_t)d1 < 0) {
        d1 = (uint8_t)(-(int8_t)d1);
        d1 >>= 1;
    }
    a6s->competitionFinished = (int16_t)d1;   /* 0x56 */

    d1 = (uint8_t)div->relegated;
    a6s->currentRoundIndex = (int16_t)d1;       /* 0x58 */

    d1 = (uint8_t)div->relegatedPo;
    if ((int8_t)d1 < 0) {
        d1 = (uint8_t)(-(int8_t)d1);
        d1 >>= 1;
    }
    a6s->numRounds = (int16_t)d1;      /* 0x5A */

    int16_t numTeams = (int8_t)div->teams;
    a6s->numTeams = numTeams;           /* 0x32 */

    a6s->matchPairsPerRound = 1;                /* 0x50 */

    /* Build competition name at offset 4 */
    uint8_t *a1 = a6 + 4;
    a1 = prependCountryNameIfClub(table, a1);

    /* Division/competition name */
    uint8_t *nameSource;
    int8_t nameOffset = compHdr->unk1;
    if (nameOffset == 0) {
        nameSource = (uint8_t *)divisionStringTable[d7];
    } else {
        uint8_t *customTable = table + 5 + nameOffset;
        nameSource = *(uint8_t **)(customTable + (d7 << 3));
    }
    a6s->namePtr = nameSource;  /* 0x28 */
    a1 = _StringCopy(nameSource, a1);

    ((DIYCompetitionEx *)a6)->pointsForAWin = (int16_t)league->pointsForWin;

    int8_t typeVal = league->rounds;
    if (typeVal < 0)
        typeVal = 1;
    a6s->playEachTeam = (int16_t)typeVal;  /* 0x5E */

    uint16_t c = (uint8_t)league->subsReserves;
    ((DIYCompetitionEx *)a6)->matchData.league.leagueMinSubstitutes = (c & 0xF0) >> 4;
    ((DIYCompetitionEx *)a6)->matchData.league.leagueMaxSubstitutes = c & 0x0F;

    _DIY_RecomputeCompetitionDerivedFields();
    _DIY_ShuffleLeagueTeamOrder();
}

__attribute__((used, externally_visible))
void _InitDIYCupFromRecord(uint8_t *cupRec)
{
    Competition    *compHdr = (Competition *)cupRec;
    CompetitionCup *cup     = (CompetitionCup *)cupRec;

    if (cup->teams & 1) {
        panicReasonCode = 0x1313;
        _PanicHaltLoop();
    }

    _InitDIYCup();

    uint8_t *a6 = (uint8_t *)&diyCompetitionStart;
    DIYCompetition *a6s = (DIYCompetition *)a6;
    a6s->substitutesCount = 1;  /* 0x3C */
    checkEuropeanCupsAndClear3C(cupRec, a6);

    a6s->field_2E        = (uint8_t)compHdr->compNumber;  /* 0x2E */
    a6s->subRoundCounter = (int8_t)cup->firstRoundNo;     /* 0x5C */
    a6s->startMonth      = compHdr->beginMonth;           /* 0x38 */
    a6s->endMonth        = compHdr->endMonth;             /* 0x39 */

    setPitchTypeFields(a6);

    a6s->numTeams = (int8_t)cup->teams; /* 0x32 */

    /* Build competition name at offset 4 */
    uint8_t *a1 = a6 + 4;
    a1 = prependCountryNameIfClub(cupRec, a1);

    /* Cup name */
    uint8_t *nameSource = (uint8_t *)aCup_0; /* "CUP" default */
    int8_t nameOffset = compHdr->unk1;
    if (nameOffset != 0) {
        nameSource = *(uint8_t **)(cupRec + 5 + nameOffset);
    }
    a6s->namePtr = nameSource;  /* 0x28 */
    a1 = _StringCopy(nameSource, a1);

    a6s->playEachTeam = (int8_t)cup->awayGoalsMode;  /* 0x5E */

    uint16_t c = (uint8_t)cup->subsReserves;
    ((DIYCompetitionEx *)a6)->minSubstitutes = (c & 0xF0) >> 4;
    ((DIYCompetitionEx *)a6)->pointsForAWin = c & 0x0F;

    /* Count number of rounds: log2(numTeams/2) */
    uint16_t d2 = 0;
    uint16_t d0 = (uint16_t)a6s->numTeams >> 1;   /* 0x32 */
    while (d0) {
        d2++;
        d0 >>= 1;
    }
    d2--;

    /* Unpack round flags from variant tail at &cupRec[0xE].
     * Variants differ past offset 0xE (CompetitionCup_6_2_12 has
     * rounds[6] starting there, CompetitionCup_6_2_64 same), so
     * read as raw bytes — the encoding is the same per-round-byte
     * regardless of variant tail. */
    uint8_t *recPtr = cupRec;
    DIYCompetitionEx *compEx = (DIYCompetitionEx *)a6;
    for (int16_t i = 0; i <= (int16_t)d2; i++) {
        uint8_t flags = recPtr[0xE];
        compEx->matchData.cup.roundLegs[i]      = (flags >> 6) & 3;
        compEx->matchData.cup.roundExtraTime[i] = (flags >> 4) & 3;
        compEx->matchData.cup.roundPenalties[i] = (flags >> 2) & 3;
        recPtr++;
    }

    _DIY_InitCupBracketState();

    /* Third-place match — byte 9 = CompetitionCup.unk1 holds the
     * third-place team count. */
    if (cup->unk1 != 0) {
        int16_t val = cup->unk1;
        DIYCompetition *diyStartS = &diyCompetitionStart.base;
        diyStartS->numRounds = val;  /* 0x5A (= 0x3A + 0x20) */
    }

    /* Competition.unk4 == 0 → shuffle. */
    if (compHdr->unk4 == 0)
        _DIY_ShuffleTeamGridPairs();
}

__attribute__((used, externally_visible))
void _InitDIYLeagueFromRecord(uint8_t *leagueRec)
{
    uint8_t *a5 = leagueRec;
    Competition *compHdr = (Competition *)leagueRec;

    _InitDIYTournament();

    uint8_t *a6 = (uint8_t *)&diyFileBufferCopy;
    DIYCompetition *a6s = (DIYCompetition *)a6;
    a6s->substitutesCount = 1;  /* 0x3C */
    checkEuropeanCupsAndClear3C(a5, a6);

    a6s->field_2E = (uint8_t)compHdr->compNumber;  /* 0x2E */
    a6s->matchPairsPerRound = 0;        /* 0x50 */

    if (compHdr->unk4 != 0) {
        a6s->matchPairsPerRound = 1;  /* 0x50 */

        int8_t seedOff = compHdr->unk3;
        if (seedOff == 0) {
            panicReasonCode = 0x5295;
            _PanicHaltLoop();
        }

        /* The seed table is the static competition's unk1[] array of
         * packed (country, ordinal) byte pairs. On Amiga m68k a
         * `move.w (an)+,d0` reads it as a big-endian uint16 — the same
         * format every other team-id consumer expects (e.g. the values
         * `_CopySelectedTeamNumbers` just wrote into diyTournamentTeamIds
         * via PACK_BE_U16). Reading as raw `*(uint16_t *)` on the SDL LE
         * host byte-swaps each entry, the find-loop below never matches,
         * the reorder no-ops, and the cup grid stays in selTeamsPtr
         * resolution order — exactly the "preset WC wrong groups"
         * symptom (Group A draws as Romania/Russia/Sweden/Switzerland
         * because those are selTeamsPtr[0..3] from batched file load,
         * instead of unk1[0..3] which is Colombia/Romania/Switzerland/
         * Trinidad-Tobago). Decode each entry once via BE_READ_U16 so
         * the comparison sees the right value on both hosts.
         *
         * +text[2]-growth delta: seedOff (= unk3) is the 32-bit Amiga byte
         * distance from a5+7 to unk1[]; every preset competition reaching
         * this seed-reorder (unk4 != 0 → all are CompetitionCup/Cup2 static
         * templates with a `char *text[2]` pair) has unk1[] pushed +8 on
         * 64-bit. Must match the writers (career_season.c euro-cup list /
         * career_eurocups.c) and readers (career_teams.c, career_init.c). */
        const uint8_t *seedTable = (const uint8_t *)(a5 + 7 + seedOff
                          + 2u * (sizeof(char *) - sizeof(uint32_t)));
        uint8_t *a3 = ((DIYTournamentState *)a6)->teamIndexOrder;
        int16_t count = (int8_t)a5[0xF] - 1;
        /* Find-loop bound: search ONLY the teams just loaded into
         * diyFileBufferCopy.teamIds — the record's team count (a5[0xF] ==
         * count + 1), NOT a6s->numTeams. At this point numTeams still holds
         * _InitDIYTournament's generic init-table default (diyTournamentInit-
         * Table[1] == 24); the real count isn't written until ~40 lines below.
         * With the 24 bound the loop reads past the loaded teams into stale
         * teamIds slots [count..23]; on a season-end auto-advance those carry
         * a previous competition's ids, so a stale entry can falsely match a
         * seed and return an out-of-range index (e.g. 16 for a 16-team euro
         * cup). That index gets swapped into teamIndexOrder and later panics
         * _VerifyTeamLoaded(0) in _DIY_CacheSelectedTeamPointers. (The original
         * relied on those slots being zero so no false match occurred.) */
        uint16_t numTeams16 = (uint16_t)(count + 1);  /* = a5[0xF], loaded team count */

        for (int16_t i = count; i >= 0; i--) {
            uint16_t seedId = BE_READ_U16(seedTable);
            seedTable += 2;
            uint16_t *teamIds = ((DIYTournamentState *)a6)->teamIds;
            uint16_t d1 = 0;

            /* Find seedId in team list. teamIds (a6+0x9A =
             * diyTournamentTeamIds) is BE-packed; compare against the
             * BE_READ_U16 seedId so both sides are in the same
             * packed-value space on either host endian. */
            while (d1 < numTeams16) {
                if (BE_READ_U16(teamIds) == seedId)
                    break;
                teamIds++;
                d1++;
            }

            if (d1 < numTeams16) {
                /* Check if d1 is already at position a3 */
                if ((uint8_t)d1 != *a3) {
                    /* Swap: find d1 in the shuffle indices and swap with a3 position */
                    uint8_t *scan = ((DIYTournamentState *)a6)->teamIndexOrder;
                    while (*scan != (uint8_t)d1)
                        scan++;
                    uint8_t tmp = *scan;
                    *scan = *a3;
                    *a3 = tmp;
                }
            }

            a3++;
        }
    }

    a6s->startMonth = compHdr->beginMonth;   /* 0x38 */
    a6s->endMonth   = compHdr->endMonth;     /* 0x39 */

    setPitchTypeFields(a6);

    a6s->numTeams = (int8_t)a5[0xF];  /* 0x32 — variant-tail byte */

    /* Build competition name at offset 4 */
    uint8_t *a1 = a6 + 4;
    a1 = prependCountryNameIfClub(a5, a1);

    /* League name */
    uint8_t *nameSource = (uint8_t *)aLeague_1; /* "LEAGUE" default */
    int8_t nameOffset = compHdr->unk1;
    if (nameOffset != 0) {
        nameSource = *(uint8_t **)(a5 + 5 + nameOffset);
    }
    a6s->namePtr = nameSource;  /* 0x28 */
    a1 = _StringCopy(nameSource, a1);

    a6s->menuState = (int8_t)a5[0xA];              /* 0x4A */

    uint16_t dc = a5[0xD];
    a6s->roundIterator = (dc & 0xF0) >> 4;    /* 0x4C */
    a6s->numTeamsInStage = dc & 0x0F;                     /* 0x4E */

    DIYTournamentSlot *firstSlot = ((DIYTournamentState *)a6)->slots;
    firstSlot->_pad_00[0] = 0;
    firstSlot->_pad_00[1] = 0;

    int16_t d7 = (int8_t)a5[0xC];
    int16_t d6 = (int8_t)a5[0xB];

    uint8_t *a2 = a5 + 0x22;
    uint8_t *stageRec = a5 + 0xE;
    DIYTournamentSlot *slot = firstSlot;

    /* First stage header — a0 == a6 here, so within struct */
    int8_t numStages = (int8_t)*stageRec++;
    a6s->totalMatchesInRound = (int16_t)numStages;  /* 0x52 */

    int8_t numTeamsPerStage = (int8_t)*stageRec;
    a6s->numTeams = (int16_t)numTeamsPerStage;  /* 0x32 */

    int16_t d2 = a6s->totalMatchesInRound - 1;  /* 0x52 */

    for (; d2 >= 0; d2--) {
        slot->seedingType  = (int8_t)a5[9];
        slot->initConst168 = d7;
        slot->playEachTeam = d6;

        uint16_t groupTeams = *stageRec++;
        slot->numTeams = groupTeams;

        slot->doubleCount = 0;
        uint8_t cupFlag = *stageRec;
        if ((int8_t)cupFlag < 0) {
            /* High bit signals "doubled cup-bracket count" — also flagged
             * on the first slot's _pad_00 word. */
            *(uint16_t *)firstSlot->_pad_00 = 1;
            slot->doubleCount = 1;
            cupFlag = 0;
        }
        slot->isLeagueFormat = cupFlag;

        uint8_t matchesVal = *stageRec++;
        slot->groups = matchesVal;

        uint8_t groupCount = *stageRec++;
        slot->teamsPerGroup = groupCount;

        /* Validate: if cup mode and group count is odd, panic */
        if (slot->isLeagueFormat != 0) {
            if (groupCount & 1) {
                panicReasonCode = 0x1313;
                _PanicHaltLoop();
            }
        }

        uint8_t qualifiers = *stageRec;
        slot->divisionFactor = qualifiers;

        /* Unpack round flags from a2 */
        uint8_t flags = *a2++;
        slot->formatFlag16E = (flags >> 6) & 3;
        slot->initConst170  = (flags >> 4) & 3;
        slot->formatFlag172 = (flags >> 2) & 3;

        slot += 1;   /* next tournament slot (sizeof == stride 0x76, both arches) */
    }

    /* Post-loop check: if totalRounds == 6, skip */
    if (a6s->totalMatchesInRound != 6) {  /* 0x52 (= 0x3A + 0x18) */
        /* slot is now the slot AFTER the last initialized one.
         * a0+0xEE = slot[-1].numTeams (peek-back), a0+0x162 = slot[0].numTeams. */
        slot->numTeams = slot[-1].numTeams;
    }

    _DIYTournament_RecalculateMatchesPerTeam();
    _DIY_LoadCurrentCompetitionSlot();
}

__attribute__((used, externally_visible))
void _InitCompetitionFromRecord(uint16_t index, uint8_t *record)
{
    int8_t type = ((Competition *)record)->type;
    if (type == 0)
        _InitDIYCompetitionFromTableEntry(index, record);
    else if (type == 1)
        _InitDIYCupFromRecord(record);
    else
        _InitDIYLeagueFromRecord(record);
}

/* ================================================================
 * Chunk 152 — DIY League/Tournament Match Callbacks & Visibility
 * ================================================================ */

extern uint16_t diyTournamentType;
extern uint16_t diyHasGroups;
extern uint16_t careerLoadedFlag;
extern uint16_t fireResetFlag;
extern uint16_t highlightedLeagueTeamId;

extern uint16_t _DIY_PlayOrSimulateLeagueFixture(int16_t allowSquadChanges, int16_t selector);

extern void _SetCurrentEntry(uint16_t index);

/* ================================================================
 * DIYLeagueTournamentMatch  (SWOS2.s line 42381)
 *
 * Menu callback (a5=entry). Stores ordinal to field3A+4, calls
 * MatchCommon with action=2 (play match).
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentMatch(MenuEntry *entry)
{
    *(uint16_t *)&diyCompetitionStart.base.slotStateA = entry->ordinal;
    _DIYLeagueTournamentMatchCommon(2);
}

/* ================================================================
 * DIYLeagueTournamentOnSelect4  (SWOS2.s line 42388)
 *
 * Menu callback. Stores ordinal to field3A+4, calls MatchCommon
 * with action=3 (view highlights from match list).
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentOnSelect4(MenuEntry *entry)
{
    *(uint16_t *)&diyCompetitionStart.base.slotStateA = entry->ordinal;
    _DIYLeagueTournamentMatchCommon(3);
}

/* ================================================================
 * DIYLeagueTournamentResult  (SWOS2.s line 42395)
 *
 * Menu callback. Stores ordinal to field3A+4, calls MatchCommon
 * with action=0 (show result/simulate).
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentResult(MenuEntry *entry)
{
    *(uint16_t *)&diyCompetitionStart.base.slotStateA = entry->ordinal;
    _DIYLeagueTournamentMatchCommon(0);
}

/* ================================================================
 * DIYLeagueTournamentViewMatch  (SWOS2.s line 42402)
 *
 * Menu callback. Stores ordinal to field3A+8, calls MatchCommon
 * with action=1 (view match replay).
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentViewMatch(MenuEntry *entry)
{
    *(uint16_t *)&diyCompetitionStart.base.slotStateC = entry->ordinal;
    _DIYLeagueTournamentMatchCommon(1);
}

/* ================================================================
 * DIYLeagueTournamentViewHighlights  (SWOS2.s line 42409)
 *
 * Menu callback. Stores ordinal to field3A+8, calls MatchCommon
 * with action=3 (view highlights).
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentViewHighlights(MenuEntry *entry)
{
    *(uint16_t *)&diyCompetitionStart.base.slotStateC = entry->ordinal;
    _DIYLeagueTournamentMatchCommon(3);
}

/* ================================================================
 * DIYLeagueTournamentViewResult  (SWOS2.s line 42416)
 *
 * Menu callback. Stores ordinal to field3A+8, calls MatchCommon
 * with action=0 (view result).
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentViewResult(MenuEntry *entry)
{
    extern uint16_t youreSacked;
    extern uint16_t gameType;
    SWOS_AUTOSKIP_LOG("[bug1] _DIYLeagueTournamentViewResult CALLED: ordinal=%u "
                      "youreSacked=%u gameType=%u",
                      (unsigned)entry->ordinal,
                      (unsigned)youreSacked, (unsigned)gameType);
    *(uint16_t *)&diyCompetitionStart.base.slotStateC = entry->ordinal;
    _DIYLeagueTournamentMatchCommon(0);
}

/* ================================================================
 * DIYLeagueTournamentMatchCommon  (SWOS2.s line 42422)
 *
 * Common match handler. Calls DIY_PlayOrSimulateLeagueFixture,
 * updates view state, sorts league table, redraws menu.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentMatchCommon(uint16_t action)
{
    /* ASM passes d6 (allowSquadChanges) implicitly via register preservation
     * from the outer caller. diyTournamentType==1 for career (squad changes allowed),
     * 0 for standalone DIY (no squad changes). */
    uint16_t result = _DIY_PlayOrSimulateLeagueFixture(diyTournamentType, action);
    if (result != 0)
        return;

    _DIYLeagueTournament_UpdateViewOptionsAvailability();
    diyFixtureViewMode = 0xFFFF;   /* -1 = show results */
    /* nullsub_15 omitted (no-op) */

    /* diyLeftTeamPtr/diyRightTeamPtr/diyLeftTeamResult/diyRightTeamResult
     * populated by internal match/sim code */

    uint16_t setAllPlayed = 0;

    if (diyHasGroups != 0) {
        /* With groups: move separator entry x to 0x97 */
        menuEntries[10].x = 0x97;  /* entry at $230/56 = 10 */
        setAllPlayed = 1;
    } else {
        DIYCompetitionEx *a0Ex = &diyCompetitionStart;
        if (diyTournamentType != 0) {
            /* Tournament: check leagueCurrentSlot (a0+0x1C0, used as phase-boundary flag) */
            if ((int16_t)a0Ex->leagueCurrentSlot != 0)
                setAllPlayed = 1;
            /* else fall through to league check */
        }
        if (!setAllPlayed) {
            /* League check: minSubstitutes (=section counter) == 0 AND base.hasGroups != 1 */
            if ((int16_t)a0Ex->minSubstitutes == 0 && a0Ex->base.hasGroups != 1)
                setAllPlayed = 1;
        }
    }

    if (setAllPlayed)
        diyAllMatchesPlayed = 1;

    _DIY_SortLeagueTableByStandings();
    _DIYSetTeamNamesAndResults();
    _DIYLeagueTournament_EnsureCurrentFixtureVisible();
    _DIYLeagueTournament_UpdateMenuEntryVisibilityAndFocus();
    _DrawMenu();
}

/* ================================================================
 * DIYLeagueTournament_UpdateMenuEntryVisibilityAndFocus
 * (SWOS2.s line 42486)
 *
 * Controls which menu entries are visible/hidden and sets the
 * current selected entry based on view mode and game state.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournament_UpdateMenuEntryVisibilityAndFocus(void)
{
    /* If view-only mode, hide entries 1-8 (action buttons) */
    if (diyTournamentViewOnly != 0) {
        menuEntries[1].isInvisible = 1;
        menuEntries[2].isInvisible = 1;
        menuEntries[3].isInvisible = 1;
        menuEntries[4].isInvisible = 1;
        menuEntries[5].isInvisible = 1;
        menuEntries[6].isInvisible = 1;
        menuEntries[7].isInvisible = 1;
        menuEntries[8].isInvisible = 1;
        return;
    }

    DIYCompetitionEx *compEx = &diyCompetitionStart;

    if (diyAllMatchesPlayed != 0) {
        /* All matches played */
        if (diyTournamentType == 0)
            fireResetFlag = 1;

        /* Hide entries 1, 3-8 */
        menuEntries[1].isInvisible = 1;
        menuEntries[3].isInvisible = 1;
        menuEntries[4].isInvisible = 1;
        menuEntries[5].isInvisible = 1;
        menuEntries[6].isInvisible = 1;
        menuEntries[7].isInvisible = 1;
        menuEntries[8].isInvisible = 1;

        _SetCurrentEntry(10);
        return;
    }

    /* Not all played — check minSubstitutes (parent +0x60). Asm reads
     * `*(int16_t *)&a1[0x60]` — that's the 32-bit raw offset to
     * `minSubstitutes`. On 64-bit `DIYCompetition` grew from 0x60 to
     * 0x68 (namePtr pointer-width change), so the raw offset lands
     * inside the base struct's padding (reads as 0) and the menu always
     * takes the "NEXT ROUND" branch instead of showing MATCH/RESULT
     * for the player's pending fixture. Use the typed field.
     * See memory project_raw_offset_64bit_shift_trap.md. */
    if ((int16_t)compEx->minSubstitutes != 0) {
        /* minSubstitutes != 0 — check fixture view mode */
        if ((int16_t)diyFixtureViewMode <= 0)
            goto show_next_match;
        /* diyFixtureViewMode > 0 — show match view options */
        goto show_match_view;
    }

    /* field $60 == 0 — show "NEXT ROUND" */
    fireResetFlag = 1;
    menuEntries[1].fg = (uintptr_t)aNextRound;
    goto show_next_button;

show_next_match:
    menuEntries[1].fg = (uintptr_t)aNextMatch;

show_next_button:
    /* Show entry 1, hide entries 3-8 */
    menuEntries[1].isInvisible = 0;
    menuEntries[3].isInvisible = 1;
    menuEntries[4].isInvisible = 1;
    menuEntries[5].isInvisible = 1;
    menuEntries[6].isInvisible = 1;
    menuEntries[7].isInvisible = 1;
    menuEntries[8].isInvisible = 1;

    if (diyInitFocusOverride != 0) {
        diyInitFocusOverride = 0;
        _SetCurrentEntry(10);
        return;
    }
    _SetCurrentEntry(1);
    return;

show_match_view:
    /* Hide entry 1 */
    menuEntries[1].isInvisible = 1;

    {
        uint16_t teamClass = _DIY_GetCurrentFixtureTeamClass();
        if (teamClass == 0 || teamClass == 1) {
            /* Both teams computer-controlled — show VIEW options (6,8), hide 7 */
            menuEntries[6].isInvisible = 0;
            menuEntries[7].isInvisible = 0;  /* cleared then re-set below */
            menuEntries[8].isInvisible = 0;
            menuEntries[7].isInvisible = 1;  /* net: entry 7 hidden */

            /* Hide entries 3-5 (MATCH options) */
            menuEntries[3].isInvisible = 1;
            menuEntries[4].isInvisible = 1;
            menuEntries[5].isInvisible = 1;

            /* Career mode check: hide entry 6 if career */
            if (careerLoadedFlag != 0) {
                menuEntries[6].isInvisible = 1;
                /* Copy x from entry 6 to entry 8, set entry 8 width */
                menuEntries[8].x = menuEntries[6].x;
                menuEntries[8].dwWidth = 0xCA;
            }

            _SetCurrentEntry(*(uint16_t *)&diyCompetitionStart.base.slotStateC);
            return;
        }

        /* Human team involved — show PLAY options (entries 3,5), hide entry 4 */
        menuEntries[3].isInvisible = 0;
        menuEntries[4].isInvisible = 0;  /* cleared then re-set below */
        menuEntries[5].isInvisible = 0;
        menuEntries[4].isInvisible = 1;  /* net: entry 4 hidden */

        /* Hide entries 6-8 (VIEW options) */
        menuEntries[6].isInvisible = 1;
        menuEntries[7].isInvisible = 1;
        menuEntries[8].isInvisible = 1;

        _SetCurrentEntry(*(uint16_t *)&diyCompetitionStart.base.slotStateA);
        return;
    }
}

/* ================================================================
 * Chunk 153: ShowDiyLeaguesTournamentMenu  (SWOS2.s line 40652)
 *
 * Sets up DIY league/tournament globals, sorts league table,
 * waits for fire release if season update in progress, then
 * displays the diyLeagueTournamentMenu.
 * ================================================================ */

extern uint8_t diyLeagueTournamentMenu[];

__attribute__((used, externally_visible))
void _ShowDiyLeaguesTournamentMenu(uint16_t type, uint16_t viewOnly, uint16_t focusOverride)
{
    SWOS_AUTOSKIP_LOG("ShowDiyLeaguesTournamentMenu: ENTER type=%u viewOnly=%u focus=%u seasonUpdate=%u",
                      (unsigned)type, (unsigned)viewOnly, (unsigned)focusOverride,
                      (unsigned)seasonUpdateInProgress);
    writeOnlyVar01 = 1;
    diyTournamentType = type;
    diyFixtureViewMode = type;
    diyTournamentViewOnly = viewOnly;
    diyInitFocusOverride = focusOverride;
    _DIY_SortLeagueTableByStandings();

    if (seasonUpdateInProgress != 0) {
#ifdef SWOS_DEBUG_AUTOSKIP
        unsigned debounce_iters = 0;
#endif
        do {
            _MenuCheckControls();
#ifdef SWOS_SDL_BUILD
            /* See _ShowDIYCupMenuWithParams: Platform_VBlank both
             * paces at 50 Hz and pumps SDL events so KEYUP for the
             * just-pressed CONTINUE actually reaches ciaa_pra. */
            extern int Platform_VBlank(void);
            Platform_VBlank();
#endif
#ifdef SWOS_DEBUG_AUTOSKIP
            debounce_iters++;
            if (debounce_iters <= 3 || debounce_iters == 100 || debounce_iters % 100 == 0) {
                SWOS_AUTOSKIP_LOG("  debounce iter=%u fire=%u shortFire=%u",
                                  debounce_iters, (unsigned)fire, (unsigned)shortFire);
            }
#endif
        } while (fire != 0);
        SWOS_AUTOSKIP_LOG("ShowDiyLeaguesTournamentMenu: debounce exited fire=%u", (unsigned)fire);
    }

    SWOS_AUTOSKIP_LOG("ShowDiyLeaguesTournamentMenu: -> _ShowMenu(diyLeagueTournamentMenu)");
    _ShowMenu((uint8_t *)&diyLeagueTournamentMenu);
    SWOS_AUTOSKIP_LOG("ShowDiyLeaguesTournamentMenu: <- _ShowMenu returned");
    writeOnlyVar01 = 0;
}

/* ================================================================
 * FindTeamInDIY  (SWOS2.s line 35451)
 *
 * Finds a team in a DIY competition, returning its position/rank
 * index. Handles leagues, cups, and tournaments (including group
 * stage sub-competitions for cups/tournaments).
 *
 * The function modifies globals diyFoundTeamIndex and
 * diyCompHasGroupStage. For tournaments with a group stage, it can
 * recurse into a secondary competition (comp + 0x420) and search
 * there using either the cup or league path.
 *
 * Uses seq/sne on big-endian word globals: writes 0xFF to high byte
 * of diyCompHasGroupStage, modeled as 0xFF00 in C for correctness.
 *
 * Asm: in d0 = teamNo, a6 = comp; out d1 = index (or 0xFFFF)
 * ================================================================ */
extern uint16_t diyFoundTeamIndex;
extern uint16_t diyCompHasGroupStage;
extern uint16_t findTeamInDIY_SuppressSideEffectsFlag;
extern uint16_t isNationalSelector;

__attribute__((used, externally_visible))
uint16_t _FindTeamInDIY(uint16_t teamNo, DIYCompetition *comp)
{
    /* Extended area raw pointer — offsets beyond 0x60 are not in DIYCompetition struct */
    uint8_t *raw = (uint8_t *)comp;
    DIYCompetitionEx *compEx = (DIYCompetitionEx *)comp;

    diyFoundTeamIndex = 0;
    diyCompHasGroupStage = 0;

    uint16_t numTeams = (uint16_t)comp->numTeams;
    uint16_t compType = (uint16_t)comp->competitionType;

    if (compType == 1)    /* DIY_CUP */
        goto cup_entry;
    if (compType == 2)    /* DIY_TOURNAMENT */
        goto tournament_entry;

    /* --- LEAGUE path ---
     * seq: set high byte to 0xFF if minSubstitutes == 0 (Amiga read at
     * +96 = struct field minSubstitutes). */
    diyCompHasGroupStage = (compEx->minSubstitutes == 0) ? 0xFF00 : 0;

league_search:
    {
        uint16_t *leagueTeamIds = compEx->matchData.league.leagueTeamIds;
        uint16_t *entryOffs     = compEx->matchData.league.teamEntryOffsets;
        for (uint16_t i = 0; i < numTeams; i++) {
            int16_t rawIdx = (int16_t)entryOffs[i];
            uint16_t teamIdx = (uint16_t)((uint32_t)(int32_t)rawIdx / 18u);
            /* leagueTeamIds is BE-packed (4e5f378) — compare against
             * host-native teamNo via BE_READ_U16. */
            if (BE_READ_U16(&leagueTeamIds[teamIdx]) == teamNo)
                return i;
        }
        goto not_found;
    }

cup_entry:
    /* sne: set high byte to 0xFF if finishedFlag != 0 */
    diyCompHasGroupStage = (comp->competitionFinished != 0) ? 0xFF00 : 0;

cup_search:
    {
        /* cupTeamIds and teamGridPairings are typed members of
         * DIYCompetitionEx (at struct offsets 0x24E / 0x2CE on 32-bit,
         * shifted to 0x252 / 0x2D2 on 64-bit due to namePtr growth).
         * The original `raw + 590` / `raw + 718` hardcoded offsets
         * pointed 4 bytes before the intended field on 64-bit, so cup
         * lookups never matched and the function fell through to the
         * not-found panic at end-of-season chairman opinion. Same fix
         * as career_eurocups.c::copyTitleHolderGrid. */
        uint16_t *cupTeamIds = compEx->cupTeamIds;
        int8_t *cupIndexTable = (int8_t *)compEx->teamGridPairings;
        int16_t d1 = -1;
        uint16_t rem = numTeams;
        while (rem-- > 0) {
            d1++;
            int16_t idx = (int16_t)(int8_t)cupIndexTable[d1];
            /* cupTeamIds is BE-packed (4e5f378). */
            if (BE_READ_U16(&cupTeamIds[idx]) == teamNo) {
                /* Found in cup — post-processing */
                if (comp->competitionFinished != 0 && (uint16_t)d1 < 2) {
                    /* Group stage, position 0 or 1 */
                    if (teamNo == (uint16_t)comp->resultCode)
                        return 0;
                    return 1;
                }
                goto cup_bracket;
            }
        }
        goto not_found;

    cup_bracket:
        {
            uint16_t roundVal = (uint16_t)comp->matchPairsPerRound;
            uint16_t doubled = roundVal << 1;
            if ((uint16_t)d1 < doubled) {
                /* Before knockout rounds: return roundVal - 1 */
                return roundVal - 1;
            }
            /* In knockout rounds */
            diyFoundTeamIndex = 1;
            uint16_t pos = (uint16_t)d1;
            uint16_t bracket = 32;
            while (pos < bracket)
                bracket >>= 1;
            /* bracket is largest power of 2 <= pos */
            return bracket + (bracket >> 1) - 1;
        }
    }

tournament_entry:
    {
        /* sne: set high byte to 0xFF if exitFlag != 0 */
        diyCompHasGroupStage = (comp->currentRoundIndex != 0) ? 0xFF00 : 0;

        /* SWOS2.s FindTeamInDIY (line 61903) tournament path:
         *   lea $9A(a6),a0   ; tournTeamIds at raw byte 0x9A of comp
         *   lea $5A(a6),a1   ; tournIndexTable at raw byte 0x5A of comp
         * For DIY_TOURNAMENT comps, comp is diyFileBufferCopy or an
         * overlay alias of it. The writer _InitDIYTournament writes the
         * 64-byte shuffle index table at `a2raw + 0x5A` (raw byte
         * offset). On 64-bit, `&comp->numRounds` evaluates to
         * `comp + 0x5E` (struct field shifted by namePtr growth) — 4
         * bytes past the actual data position. Use the raw byte
         * offset to match both the asm semantic and the writer. */
        uint16_t *tournTeamIds = ((DIYTournamentState *)raw)->teamIds;
        int8_t *tournIndexTable = (int8_t *)((DIYTournamentState *)raw)->teamIndexOrder;
        int16_t d1 = -1;
        uint16_t rem = numTeams;
        while (rem-- > 0) {
            d1++;
            int16_t idx = (int16_t)(int8_t)tournIndexTable[d1];
            /* tournTeamIds is BE-packed (4e5f378). */
            if (BE_READ_U16(&tournTeamIds[idx]) == teamNo) {
                /* Found in tournament */
                uint16_t stageConfig = (uint16_t)comp->roundComplete;
                uint16_t stageOffset = stageConfig * 118;
                uint8_t *stagePtr = raw + stageOffset;
                DIYTournamentSlot *stageSlot = (DIYTournamentSlot *)(stagePtr + offsetof(DIYTournamentState, slots));

                if (comp->currentRoundIndex != 0) {
                    /* Has group stage */
                    uint16_t threshold = stageSlot->numTeams;
                    if ((uint16_t)d1 < threshold) {
                        /* Team is in group stage — switch to the current-stage
                         * competition. This used to be `raw += 0x420` because
                         * diyCompetitionStart sat 0x420 after diyFileBufferCopy
                         * in one BSS run; they are now separate buffers, so step
                         * to it by symbol. */
                        /* BUG-D FIX (layer 2): switch to the QUERIED context's own
                         * current-stage image — the legacy `raw += 0x420` read
                         * comp+0x420 (the context's embedded stage). Post-untangle,
                         * each *Context / slot-0 CompetitionSlotBuffer carries its
                         * stage in `stageSnapshot`; only the master working buffer
                         * diyFileBufferCopy has its live stage in the SEPARATE
                         * diyCompetitionStart global. Phase A wrongly used
                         * diyCompetitionStart for ALL, so a *Context query read
                         * whatever competition was last loaded (e.g. the player's
                         * league at season-end chairman time) instead of the euro
                         * cup's stage → finalist not found → 0x6006. (layer 1: also
                         * repoint compEx, which the original group-switch left at the
                         * stale master — the asm uses one base reg a6, the C split it
                         * into comp/raw AND compEx.) */
                        if ((uint8_t *)comp == (uint8_t *)&diyFileBufferCopy)
                            raw = (uint8_t *)&diyCompetitionStart;
                        else
                            raw = (uint8_t *)comp + offsetof(CompetitionSlotBuffer, stageSnapshot);
                        comp = (DIYCompetition *)raw;
                        compEx = (DIYCompetitionEx *)raw;
                        numTeams = (uint16_t)comp->numTeams;
                        if (stageSlot->isLeagueFormat == 0)
                            goto cup_search;
                        else
                            goto league_search;
                    }
                }

                goto tournament_post;

            tournament_post:
                {
                    uint16_t threshold = stageSlot->numTeams;
                    if ((uint16_t)d1 < threshold) {
                        return threshold / 2 - 1;
                    }

                    diyFoundTeamIndex = 1;

                    /* raw+0x15A = first slot record's _pad_00 word — flag set
                     * during _InitDIYTournament when any stage is doubled. */
                    if (*(uint16_t *)(((DIYTournamentState *)raw)->slots[0]._pad_00) != 0) {
                        if ((uint16_t)d1 == 3 || (uint16_t)d1 == 4)
                            return (uint16_t)d1;
                    }

                    /* Walk through stage blocks to find which stage the team is in */
                    uint16_t pos = (uint16_t)d1;
                    DIYTournamentSlot *spSlot = ((DIYTournamentState *)raw)->slots;
                    uint16_t stageThreshold;
                    do {
                        spSlot += 1;   /* next tournament slot (sizeof == stride 0x76) */
                        stageThreshold = spSlot->numTeams;
                    } while (pos < stageThreshold);

                    /* sp+0xEC = previous slot's numTeams (looking back one stride);
                     * spSlot[-1].numTeams. */
                    uint16_t stageNumTeams = spSlot[-1].numTeams;

                    if (findTeamInDIY_SuppressSideEffectsFlag != 0) {
                        uint16_t rank = _GetMoreExpensiveTeamsCount(teamNo, (DIYCompetition *)raw);
                        if (rank >= stageThreshold && rank < stageNumTeams) {
                            return rank;
                        }
                    }

                    /* Default: midpoint between stageThreshold and stageNumTeams */
                    return stageThreshold + (stageNumTeams - stageThreshold) / 2 - 1;
                }
            }
        }
        goto not_found;
    }

not_found:
    if (isNationalSelector != 0)
        return 0xFFFF;
    panicReasonCode = 0x6006;
    _PanicHaltLoop();
    /* not reached */
}

/* ================================================================
 * InitializeNewSeason_BuildTeamBudgetRankTable  (SWOS2.s line 35655)
 *
 * Builds a per-team "budget rank" table for the current competition.
 * For each team in the competition, looks up the team ID via an
 * index table, then counts how many teams have higher average player
 * prices. Stores the result in teamBudgetRankTable[].
 *
 * Selects team ID array and index table based on competition type:
 *   league:     teamIds=302, indexTable=238
 *   cup:        teamIds=590, indexTable=718
 *   tournament: teamIds=154, indexTable=90
 *
 * Asm: no params (uses competitionFileBuffer global)
 * ================================================================ */
extern uint8_t teamBudgetRankTable[];

__attribute__((used, externally_visible))
void _InitializeNewSeason_BuildTeamBudgetRankTable(void)
{
    DIYCompetitionEx *compEx = (DIYCompetitionEx *)competitionFileBuffer;
    DIYCompetition *comp = &compEx->base;
    uint8_t *raw = competitionFileBuffer;
    uint16_t numTeams = (uint16_t)comp->numTeams;
    uint16_t compType = (uint16_t)comp->competitionType;
    uint16_t *teamIdArray;
    int8_t *indexTable;

    if (compType == 0) {        /* DIY_LEAGUE — Amiga +302/+238 = leagueTeamIds / shuffleAndPad */
        teamIdArray = compEx->matchData.league.leagueTeamIds;
        indexTable  = (int8_t *)compEx->matchData.league.shuffleAndPad;
    } else if (compType == 1) { /* DIY_CUP — Amiga +590/+718 = cupTeamIds / teamGridPairings */
        teamIdArray = compEx->cupTeamIds;
        indexTable  = (int8_t *)compEx->teamGridPairings;
    } else {                    /* DIY_TOURNAMENT */
        /* SWOS2.s InitializeNewSeason_BuildTeamBudgetRankTable
         * (line 62128) tournament branch:
         *   lea $9A(a6),a5  ; teamIds at raw byte 0x9A
         *   lea $5A(a6),a4  ; indexTable at raw byte 0x5A
         * Same conventions as FindTeamInDIY tournament path: the
         * writer (_InitDIYTournament) parks the shuffle indices at
         * raw byte 0x5A of diyFileBufferCopy, and the team IDs live
         * at raw byte 0x9A (= diyTournamentTeamIds in the overlay).
         * `&comp->numRounds` shifts to byte 0x5E on 64-bit and would
         * miss the data; use raw byte offsets to stay correct. */
        teamIdArray = (uint16_t *)(raw + 0x9A);
        indexTable = (int8_t *)(raw + 0x5A);
    }

    for (uint16_t i = 0; i < numTeams; i++) {
        int16_t idx = (int16_t)(int8_t)indexTable[i];  /* sign-extend byte to word */
        /* teamIdArray entries are BE-packed `{country, ordinal}` per the
         * 4e5f378 convention; read via BE_READ_U16 so we pass the
         * (country<<8)|ordinal id everything downstream expects. */
        uint16_t teamNo = BE_READ_U16(&teamIdArray[idx]);
        uint16_t rank = _GetMoreExpensiveTeamsCount(teamNo, comp);
        teamBudgetRankTable[idx] = (uint8_t)rank;
    }
}

/* ================================================================
 * GetMoreExpensiveTeamsCount  (SWOS2.s line 35620)
 *
 * Counts how many teams in a DIY competition have a higher average
 * player price than the given team. Selects the appropriate team ID
 * array based on competition type: league (offset 302), cup (offset
 * 590), or tournament (offset 154).
 *
 * Asm: in d0 = teamNo, a6 = competition pointer; out d2 = count
 * ================================================================ */
extern uint16_t _GetAveragePlayerPrice(TeamFileEx *team);

__attribute__((used, externally_visible))
uint16_t _GetMoreExpensiveTeamsCount(uint16_t teamNo, DIYCompetition *comp)
{
    DIYCompetitionEx *compEx = (DIYCompetitionEx *)comp;
    uint16_t numTeams = (uint16_t)comp->numTeams;
    uint16_t compType = (uint16_t)comp->competitionType;
    uint16_t *teamIds;

    if (compType == 0) {        /* DIY_LEAGUE */
        teamIds = compEx->matchData.league.leagueTeamIds;
    } else if (compType == 1) { /* DIY_CUP */
        teamIds = compEx->cupTeamIds;
    } else {                    /* DIY_TOURNAMENT */
        /* See _InitializeNewSeason_BuildTeamBudgetRankTable note —
         * +154 here refers to diyFileBufferCopy +0x9A, a sibling
         * buffer with its own (untyped) layout. */
        teamIds = (uint16_t *)((uint8_t *)comp + 154);
    }

    /* Get our team's average player price */
    TeamFileEx *myTeam = _VerifyTeamLoaded(teamNo);
    uint16_t myPrice = _GetAveragePlayerPrice(myTeam);

    /* Count teams with higher average price.
     * teamIds points at competitionFileBuffer +0x12E/+0x24E/+0x9A — all
     * BE-packed `{country, ordinal}` per the 4e5f378 convention. Use
     * BE_READ_U16 so the comparison and _VerifyTeamLoaded lookup see
     * the same id format as the rest of the call chain. */
    uint16_t count = 0;
    for (uint16_t i = 0; i < numTeams; i++) {
        uint16_t otherId = BE_READ_U16(&teamIds[i]);
        if (otherId == teamNo)
            continue;
        TeamFileEx *otherTeam = _VerifyTeamLoaded(otherId);
        uint16_t otherPrice = _GetAveragePlayerPrice(otherTeam);
        if (otherPrice > myPrice)
            count++;
    }
    return count;
}

/* ================================================================
 * DIY_RecomputeCompetitionDerivedProgressionTable  (SWOS2.s line 35327)
 *
 * Computes an interpolated progression table in diyCompetitionStart.
 * count = *(uint16_t *)(diyCompetitionStart + 0x1C4)
 * start = diyCompetitionStart[0x38]
 * end   = diyCompetitionStart[0x39]
 * output = diyCompetitionStart + 0x1CE
 *
 * If count==1, stores just the start value.
 * Otherwise for i=0..count: output[i] = start + i*(end-start)/(count-1)
 * If end < start, adds 0x60 to end (wraps around month range).
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_RecomputeCompetitionDerivedProgressionTable(void)
{
    DIYCompetitionEx *comp = &diyCompetitionStart;
    DIYCompetition *compS = &comp->base;
    uint16_t count = comp->leagueTotalMatches;
    uint8_t startVal = (uint8_t)compS->startMonth;
    uint8_t endVal = (uint8_t)compS->endMonth;
    uint8_t *out = comp->leagueSectionState.phaseStartMonths;

    if (count == 1) {
        out[0] = startVal;
        return;
    }

    /* If end < start, wrap by adding 0x60 */
    if (endVal < startVal)
        endVal += 0x60;

    uint16_t range = (uint16_t)(endVal - startVal) & 0xFF;  /* andi.w #$FF,d3 */
    uint16_t divisor = count - 1;

    for (uint16_t i = 0; i <= divisor; i++) {
        uint16_t val = (i * range) / divisor;
        out[i] = (uint8_t)(val + startVal);
    }
}

/* ===================================================================
 * Chunk 190: GetCachedOrCalculateViewResult,
 *            GetCachedOrCalculateExtraTimeResult,
 *            GetCachedOrCalculatePenaltyShootoutResult,
 *            ProcessDIYLeagueMatchResultAndCareerUpdates,
 *            _ProcessDIYKnockoutMatchResultAndCareerUpdates
 *
 * Match result processing for DIY competitions: cache-checked result
 * calculation, career stat updates, and loss flag determination.
 * =================================================================== */
extern int16_t cachedViewResultTeam1Goals;
extern int16_t cachedViewResultTeam2Goals;
extern int16_t cachedExtraTimeTeam1Goals;
extern int16_t cachedExtraTimeTeam2Goals;
extern int16_t cachedPenaltyShootoutTeam1Score;
extern int16_t cachedPenaltyShootoutTeam2Score;
extern uint16_t careerTeamLostMatch;
extern uint16_t careerAutoAdvanceInProgress;
extern TeamFileEx careerTeam;

/* Cache-checked result functions — static, called only from Process* below */
static void getCachedOrCalcViewResult(TeamFileEx *team1, TeamFileEx *team2,
    uint8_t *comp, uint16_t modeFlag, uint16_t goalsIn1, uint16_t goalsIn2, uint16_t statCol,
    uint16_t *outGoals1, uint16_t *outGoals2)
{
    team1->team.andWith0xFE &= ~1;  /* bclr #0,$17(a1) */
    team2->team.andWith0xFE &= ~1;  /* bclr #0,$17(a2) */
    if (cachedViewResultTeam1Goals >= 0) {
        *outGoals1 = (uint16_t)cachedViewResultTeam1Goals;
        *outGoals2 = (uint16_t)cachedViewResultTeam2Goals;
        cachedViewResultTeam1Goals = -1;
        return;
    }
    _CalculateViewResult((DIYCompetition *)comp, modeFlag, goalsIn1, goalsIn2, statCol,
                         team1, team2, outGoals1, outGoals2);
}

static void getCachedOrCalcExtraTimeResult(uint16_t *outD3, uint16_t *outD4)
{
    if (cachedExtraTimeTeam1Goals >= 0) {
        *outD3 = (uint16_t)cachedExtraTimeTeam1Goals;
        *outD4 = (uint16_t)cachedExtraTimeTeam2Goals;
        cachedExtraTimeTeam1Goals = -1;
        return;
    }
    _ExtraTimeSecondPhaseScoring(outD3, outD4);
}

static void getCachedOrCalcPenaltyResult(int16_t *outD5, uint16_t *outD6)
{
    if (cachedPenaltyShootoutTeam1Score >= 0) {
        *outD5 = cachedPenaltyShootoutTeam1Score;
        *outD6 = (uint16_t)cachedPenaltyShootoutTeam2Score;
        cachedPenaltyShootoutTeam1Score = -1;
        return;
    }
    _PenaltyShootoutScorerPair(outD5, outD6);
}

/* Determine d7 flags and careerTeamLostMatch from match result.
 * Shared logic between league and knockout process functions. */
static void updateCareerAfterResult(TeamFileEx *team1, TeamFileEx *team2,
    uint16_t d1, uint16_t d2, uint16_t d7)
{
    if (careerAutoAdvanceInProgress)
        return;
    if (team1 != &careerTeam && team2 != &careerTeam)
        return;

    /* Update player form: a0=diyCompetitionStart, a1=team1, a2=team2, d1=goals1, d2=goals2 */
    _UpdateCareerPlayersFormAfterMatch(&diyCompetitionStart, team1, team2, d1, d2);

    /* Begin contract negotiation: a0=diyCompetitionStart, a1=team1 */
    _BeginContractNegotiationSequence(&diyCompetitionStart, team1);

    careerTeamLostMatch = 0;

    /* Check if result was decisive (bit 1 set = not a draw) */
    if (!(d7 & 2))
        return;

    /* Determine if career team lost (faithfully reproduces asm logic) */
    if (team1 == &careerTeam) {
        /* Career is team1: bit 0 set (team2 higher) → return, else set flag */
        if (d7 & 1)
            return;
    } else {
        /* Career is team2: bit 0 set (team2 higher) → set flag, else return */
        if (!(d7 & 1))
            return;
    }
    careerTeamLostMatch = 1;
}

__attribute__((used, externally_visible))
void _ProcessDIYLeagueMatchResultAndCareerUpdates(TeamFileEx *team1, TeamFileEx *team2,
    uint16_t *outGoals1, uint16_t *outGoals2, uint16_t *outFlags)
{
    DIYCompetitionEx *compEx = &diyCompetitionStart;
    DIYCompetition *diy = &compEx->base;
    uint8_t *comp = (uint8_t *)compEx;

    /* d0: seq from playEachTeam comparison — 0xFF if playEachTeam==1, else 0x00 */
    uint16_t modeFlag = 0;
    if (diy->playEachTeam == 1)
        modeFlag = 0x00FF;  /* seq sets byte to 0xFF, high byte stays 0 */

    uint16_t goalsIn1 = compEx->matchData.league.leagueMinSubstitutes;
    uint16_t goalsIn2 = compEx->matchData.league.leagueMaxSubstitutes;
    uint16_t statCol  = *(uint16_t *)&diy->compId;  /* word read of compId+slotId at 0x2C */

    uint16_t d1, d2;
    getCachedOrCalcViewResult(team1, team2, comp, modeFlag, goalsIn1, goalsIn2, statCol, &d1, &d2);

    /* Build flags byte d7 */
    uint16_t d7 = 0;
    if (d1 == d2) {
        /* Draw: bit 7 set, bit 1 clear */
        d7 = 0x80;
    } else {
        /* cmp.b d1,d2: compare low bytes — for small goal counts this is same as word */
        if ((uint8_t)d2 > (uint8_t)d1)
            d7 |= 1;   /* bit 0: team2 scored more */
        d7 |= 0x82;    /* bits 7 and 1 set */
    }

    /* Career updates (form, contract negotiation, loss flag) */
    updateCareerAfterResult(team1, team2, d1, d2, d7);

    *outGoals1 = d1;
    *outGoals2 = d2;
    *outFlags = d7;
}

__attribute__((used, externally_visible))
void _ProcessDIYKnockoutMatchResultAndCareerUpdates(TeamFileEx *team1, TeamFileEx *team2,
    uint16_t inFirstLegTeam1, uint16_t inFirstLegTeam2,
    uint16_t *outPackedGoals, uint16_t *outPackedAggregate,
    uint16_t *outPackedPenalty, uint16_t *outFlags)
{
    DIYCompetitionEx *compEx = &diyCompetitionStart;
    DIYCompetition *diy = &compEx->base;
    uint8_t *raw = (uint8_t *)compEx;
    uint16_t slotIdx = (uint16_t)diy->currentRoundIndex;

    uint16_t d7 = 0;
    uint16_t d1 = 0, d2 = 0;   /* this match goals */
    uint16_t d3 = 0, d4 = 0;   /* aggregate or temp */
    uint16_t d5 = inFirstLegTeam1, d6 = inFirstLegTeam2;  /* first-leg or penalty */

    if (compEx->bracketScratch.groupSectionFlag != 0)
        goto singleLegWithBit6;

    if (compEx->matchData.cup.roundFormat != 2)
        goto singleLeg;

    if (compEx->matchData.cup.competitionFormat != 0)
        goto returnLeg;

    /* === First leg of two (competitionFormat==0) === */
    {
        uint8_t *comp = (uint8_t *)compEx;
        getCachedOrCalcViewResult(team1, team2, comp, 0,
            compEx->minSubstitutes, compEx->pointsForAWin,
            *(uint16_t *)&diy->compId, &d1, &d2);  /* word read of compId+slotId at 0x2C */
    }
    if (d1 == d2)
        goto drawResult;
    goto finalizeResult;

returnLeg:
    /* === Return leg (competitionFormat != 0) === */
    d7 |= 0x20;  /* bset #5 */
    {
        uint8_t *comp = (uint8_t *)compEx;
        getCachedOrCalcViewResult(team1, team2, comp, 0,
            compEx->minSubstitutes, compEx->pointsForAWin,
            *(uint16_t *)&diy->compId, &d1, &d2);  /* word read of compId+slotId at 0x2C */
    }
    d3 = d1 + d5;  /* aggregate team1 */
    d4 = d2 + d6;  /* aggregate team2 */
    if (d3 != d4)
        goto aggregateNotEqual;

    /* Aggregate tied — check away goals rule */
    if ((uint16_t)diy->playEachTeam != 1)
        goto returnLegExtraTime;
    if (d2 == d5)
        goto returnLegExtraTime;
    d7 |= 0x10;  /* bset #4: decided by away goals */
    if (d2 > d5)
        goto team2WinsReturnLeg;
    goto team1WinsReturnLeg;

returnLegExtraTime:
    /* Check if extra time is enabled — roundExtraTime[slotIdx] */
    if (compEx->matchData.cup.roundExtraTime[slotIdx] == 0)
        goto returnLegCheckPenalties;
    if (compEx->matchData.cup.roundExtraTime[slotIdx] != 1)
        goto returnLegCheckPenalties;

    d7 |= 4;  /* bset #2: went to extra time */
    {
        uint16_t et1, et2;
        getCachedOrCalcExtraTimeResult(&et1, &et2);
        d1 += et1;  /* total this-leg team1 */
        d2 += et2;  /* total this-leg team2 */
    }
    d3 = d1 + d5;  /* new aggregate team1 */
    d4 = d2 + d6;  /* new aggregate team2 */
    if (d3 != d4)
        goto aggregateNotEqual;

    /* Still tied — check away goals in extra time */
    if ((uint16_t)diy->playEachTeam == 0)
        goto returnLegExtraTimeTied;
    if (d2 == d5)
        goto returnLegExtraTimeTied;
    d7 |= 0x10;
    if (d2 > d5)
        goto team2WinsReturnLeg;
    goto team1WinsReturnLeg;

returnLegExtraTimeTied:
    d3 = d5;
    d4 = d6;
    goto returnLegCheckPenalties;

aggregateNotEqual:
    /* cmp.b d3,d4 — compare low bytes of aggregate */
    if ((uint8_t)d4 > (uint8_t)d3)
        goto team2WinsReturnLeg;
team1WinsReturnLeg:
    d3 = d5; d4 = d6;
    goto team1WinsFinal;
team2WinsReturnLeg:
    d3 = d5; d4 = d6;
    goto team2WinsFinal;

singleLegWithBit6:
    d7 |= 0x40;  /* bset #6 */
singleLeg:
    d7 &= ~0x20;  /* bclr #5 */
    {
        uint16_t mode = 0;
        if (compEx->matchData.cup.roundFormat == 0)
            mode = 0x00FF;  /* seq d0 */
        uint8_t *comp = (uint8_t *)compEx;
        getCachedOrCalcViewResult(team1, team2, comp, mode,
            compEx->minSubstitutes, compEx->pointsForAWin,
            *(uint16_t *)&diy->compId, &d1, &d2);  /* word read of compId+slotId at 0x2C */
    }
    if (d1 != d2)
        goto finalizeResult;

    d3 = d1;
    d4 = d2;

    /* Check extra time — roundExtraTime[slotIdx] */
    if (compEx->matchData.cup.roundExtraTime[slotIdx] == 0)
        goto checkPenalties;
    if (compEx->matchData.cup.roundExtraTime[slotIdx] == 1)
        goto singleExtraTime;
    if (compEx->bracketScratch.groupSectionFlag == 0)
        goto checkPenalties;

singleExtraTime:
    d7 |= 4;  /* bset #2 */
    {
        uint16_t et1, et2;
        getCachedOrCalcExtraTimeResult(&et1, &et2);
        /* ASM: exg d1,d3; exg d2,d4; add d3,d1; add d4,d2
         * Effect: d1 = et1 + view1, d2 = et2 + view2 */
        d1 += et1;
        d2 += et2;
    }
    if (d1 != d2)
        goto finalizeResult;

checkPenalties:
    /* roundPenalties[slotIdx] */
    if (compEx->matchData.cup.roundPenalties[slotIdx] == 0)
        goto drawResult;
    if (compEx->matchData.cup.roundPenalties[slotIdx] == 1)
        goto doPenalties;
    if (compEx->bracketScratch.groupSectionFlag == 0)
        goto drawResult;

doPenalties:
    d7 |= 8;  /* bset #3 */
    {
        int16_t pen1;
        uint16_t pen2;
        getCachedOrCalcPenaltyResult(&pen1, &pen2);
        d5 = (uint16_t)pen1;
        d6 = pen2;
    }
    if ((uint8_t)d6 > (uint8_t)d5)
        goto team2WinsFinal;
    goto team1WinsFinal;

returnLegCheckPenalties:
    /* roundPenalties[slotIdx] */
    if (compEx->matchData.cup.roundPenalties[slotIdx] == 0)
        goto drawResult;
    if (compEx->matchData.cup.roundPenalties[slotIdx] == 1)
        goto doPenaltiesReturnLeg;
    if (compEx->bracketScratch.groupSectionFlag == 0)
        goto drawResult;

doPenaltiesReturnLeg:
    d7 |= 8;
    {
        int16_t pen1;
        uint16_t pen2;
        getCachedOrCalcPenaltyResult(&pen1, &pen2);
        d5 = (uint16_t)pen1;
        d6 = pen2;
    }
    if ((uint8_t)d6 > (uint8_t)d5)
        goto team2WinsFinal;
    goto team1WinsFinal;

drawResult:
    d7 |= 0x80;
    d7 &= ~2;
    goto careerUpdate;

finalizeResult:
    if ((uint8_t)d2 > (uint8_t)d1)
        goto team2WinsFinal;
team1WinsFinal:
    d7 &= ~1;
    goto setDecisive;
team2WinsFinal:
    d7 |= 1;
setDecisive:
    d7 |= 0x82;

careerUpdate:
    updateCareerAfterResult(team1, team2, d1, d2, d7);

    /* Pack results: d6=(d5<<8)+d6, d4=(d3<<8)+d4, d5=d4, d4=(d1<<8)+d2 */
    *outPackedPenalty = (d5 << 8) + d6;
    *outPackedAggregate = (d3 << 8) + d4;
    *outPackedGoals = (d1 << 8) + d2;
    *outFlags = d7;
}

/* ========================================================================
 * Chunk 191 — DIY_GetCurrentMatchTeamSlots, DIY_CompareCurrentMatchTeamCategories
 * ======================================================================== */

/* DIY_GetCurrentMatchTeamSlots
 *
 * Reads the current match index from diyCompetitionStart + 0x64, looks up two
 * team slot indices in the team grid pairs table at offset 0x2CE, and returns
 * two pointers: base + slotIdx * 4 for each slot.
 *
 * Original asm returns a1, a2 as the two slot pointers.
 */
/* Internal helper: return the two team slot indices (signed bytes) for
 * the current match. Used by the C-side callers — converts the
 * "fake pointer" `base + slotIdx*4` returned by the externally-visible
 * `_DIY_GetCurrentMatchTeamSlots` into a clean (slotIdx1, slotIdx2)
 * pair so that callers can use `bracketScratch.teamOffsetTable[slotIdx]`
 * directly. */
static void getCurrentMatchTeamSlotIndices(int16_t *outSlot1, int16_t *outSlot2)
{
    DIYCompetitionEx *baseEx = &diyCompetitionStart;
    uint16_t matchIdx = baseEx->matchData.cup.currentMatchIndex;
    uint8_t *grid = baseEx->teamGridPairings;
    *outSlot1 = (int16_t)(int8_t)grid[matchIdx * 2];
    *outSlot2 = (int16_t)(int8_t)grid[matchIdx * 2 + 1];
}

__attribute__((used, externally_visible))
void _DIY_GetCurrentMatchTeamSlots(uint8_t **outSlot1, uint8_t **outSlot2) {
    DIYCompetitionEx *baseEx = &diyCompetitionStart;
    uint8_t *base = (uint8_t *)baseEx;
    uint16_t matchIdx = baseEx->matchData.cup.currentMatchIndex;
    uint8_t *grid = baseEx->teamGridPairings;

    int8_t slot1 = (int8_t)grid[matchIdx * 2];
    int8_t slot2 = (int8_t)grid[matchIdx * 2 + 1];

    *outSlot1 = base + (int16_t)slot1 * 4;
    *outSlot2 = base + (int16_t)slot2 * 4;
}

/* DIY_CompareCurrentMatchTeamCategories
 *
 * Calls DIY_GetCurrentMatchTeamSlots, then loads the team pointer from each
 * slot (offset 0x344, selTeamsPtr-relative), and compares teamControls (offset 4).
 *
 * Returns:
 *   - If both teams have the same control type → return that type
 *   - If one is TEAM_COMPUTER (1) → return the other's type
 *   - Otherwise → return 2
 */
__attribute__((used, externally_visible))
uint16_t _DIY_CompareCurrentMatchTeamCategories(void) {
    int16_t slot1Idx, slot2Idx;
    getCurrentMatchTeamSlotIndices(&slot1Idx, &slot2Idx);

    /* Load team pointer from bracketScratch.teamOffsetTable[slotIdx] (selTeamsPtr-relative) */
    DIYCompetitionEx *baseEx = &diyCompetitionStart;
    TeamFileEx *team1 = (TeamFileEx *)diy_resolve_team(baseEx->bracketScratch.teamOffsetTable[slot1Idx]);
    TeamFileEx *team2 = (TeamFileEx *)diy_resolve_team(baseEx->bracketScratch.teamOffsetTable[slot2Idx]);

    int16_t ctrl1 = (int16_t)(int8_t)team1->team.teamControls;
    int16_t ctrl2 = (int16_t)(int8_t)team2->team.teamControls;

    if (ctrl1 == ctrl2)
        return (uint16_t)ctrl1;
    if (ctrl2 == 1) /* TEAM_COMPUTER */
        return (uint16_t)ctrl1;
    if (ctrl1 == 1) /* TEAM_COMPUTER */
        return (uint16_t)ctrl2;
    return 2;
}

/* ========================================================================
 * Chunk 192 — PlayDIYLeagueMatchAndCacheResult, PlayDIYCupFixtureAndCacheResult
 * ======================================================================== */

/* Externs for Chunk 192 */
extern uint16_t isCareer;
extern uint16_t exitGameFlag;
extern uint16_t gameCanceled;
extern uint8_t  gameName[];
extern uint8_t  gameRound;
extern uint16_t gamePitchTypeOrSeason;
extern uint16_t gameSeason;
extern uint16_t gamePitchType;
extern uint16_t team1GoalsFirstLeg;
extern uint16_t team2GoalsFirstLeg;
extern int16_t  extraTimeState;
extern int16_t  penaltiesState;
extern uint16_t secondLeg;
extern uint16_t playing2ndGame;
extern uint16_t isGameFriendly;
extern uint16_t statsTeam1GoalsCopy;
extern uint16_t statsTeam2GoalsCopy;
extern uint16_t statsTeam1Goals;
extern uint16_t statsTeam2Goals;
extern uint16_t team1PenaltyGoals;
extern uint16_t team2PenaltyGoals;

extern uint8_t *_StringCopy(const uint8_t *src, uint8_t *dst);
/* _InitializeInGameTeamsAndStartGame declared in game_init.h */

/* Helper: compute gameSeason from viewRandomPhase byte */
static uint16_t computeGameSeason(uint8_t phase) {
    uint8_t d0 = phase;
    if (d0 >= 0x60)
        d0 -= 0x60;
    return (uint16_t)((uint32_t)d0 / 8);
}

/* PlayDIYLeagueMatchAndCacheResult
 *
 * Plays a DIY league match: shows SetupPlayers menu, sets up game name/round,
 * game parameters, then calls InitializeInGameTeamsAndStartGame. Caches the
 * view result goals on success.
 *
 * Returns 0 on success (Z=1), nonzero on cancel (Z=0).
 * Caller checks Z flag via bridge.
 */
__attribute__((used, externally_visible))
uint16_t _PlayDIYLeagueMatchAndCacheResult(TeamFileEx *team1, TeamFileEx *team2,
                                            int16_t allowSquadChanges, int16_t mode) {
    DIYCompetition *diy = &diyCompetitionStart.base;

    /* Call SetupPlayers: d1=diyLeagueMaxSubstitutes, d2=substitutesCount */
    _SetupPlayers(team1, team2, (int16_t)diyLeagueMaxSubstitutes,
                  diy->substitutesCount, allowSquadChanges, mode);

    if (exitGameFlag != 0)
        return 1;

    /* Set isCareer if one team is careerTeam */
    isCareer = 0;
    if (team1 == &careerTeam || team2 == &careerTeam)
        isCareer = 1;

    /* Copy competition name to gameName */
    _StringCopy((const uint8_t *)(uint8_t *)diyCompetitionStart.base.competitionName, gameName);
    gameRound = 0;

    /* If hasGroups, build round string */
    if (diy->hasGroups != 0) {
        const char *roundStr = _GetCupRoundString(
            (uint16_t)diy->menuState, (uint16_t)diy->matchPairsPerRound,
            (uint16_t)diy->roundIterator, (uint16_t)diy->numTeams, 0);
        _StringCopy((const uint8_t *)roundStr, &gameRound);
    }

    /* Set game parameters from competition */
    gamePitchTypeOrSeason = diy->pitchTypeOrSeason;
    gameSeason = computeGameSeason(diy->viewRandomPhase);
    gamePitchType = diy->pitchType;
    team1GoalsFirstLeg = 0;
    team2GoalsFirstLeg = 0;
    extraTimeState = 0;
    penaltiesState = 0;
    secondLeg = 0;
    playing2ndGame = 0;
    isGameFriendly = 0;

    /* d7 = word at offset 0x2C (compId|slotId) */
    /* Read (compId, slotId) word at +0x2C. Don't use PACK_BE_U16 here:
     * the word was written via *(uint16_t *)&compId = slot, so on
     * SDL LE the compId byte holds `slot` and slotId holds 0 (Amiga BE
     * is the reverse). Reading via *(uint16_t *) preserves the value
     * across hosts; PACK_BE_U16(compId, slotId) gives `slot << 8` on
     * SDL instead of `slot`. */
    int16_t d7val = (int16_t)*(uint16_t *)&diy->compId;

    /* d0 = (playEachTeam == 1) ? 0xFF : 0 */
    int16_t d0val = (diy->playEachTeam == 1) ? 0xFF : 0;

    _InitializeInGameTeamsAndStartGame(d0val,
        (int16_t)diyLeagueMinSubstitutes, (int16_t)diyLeagueMaxSubstitutes,
        diy->substitutesCount, d7val,
        (TeamFile *)team1, (TeamFile *)team2);

    if (gameCanceled != 0)
        return 1;

    /* Cache view result */
    cachedViewResultTeam1Goals = statsTeam1GoalsCopy;
    cachedViewResultTeam2Goals = statsTeam2GoalsCopy;

    isCareer = 0;
    return 0;
}

/* PlayDIYCupFixtureAndCacheResult
 *
 * Plays a DIY cup fixture: shows SetupPlayers menu, builds game name/round
 * with replay/leg suffix, sets game parameters, then routes to one of three
 * match paths (first leg, second leg, auto/sim) depending on competition state.
 * Caches view result goals, extra time goals, and penalty scores on success.
 *
 * Returns 0 on success (Z=1), nonzero on cancel (Z=0).
 * Implicit params from caller: allowSquadChanges (d6), mode (d7).
 */
__attribute__((used, externally_visible))
uint16_t _PlayDIYCupFixtureAndCacheResult(TeamFileEx *team1, TeamFileEx *team2,
                                           int16_t allowSquadChanges, int16_t mode) {
    /* d1 = diyPointsForAWin (word), d2 = substitutesCount → SetupPlayers */
    DIYCompetition *diyStart = &diyCompetitionStart.base;
    _SetupPlayers(team1, team2, *(int16_t *)diyPointsForAWin,
                  diyStart->substitutesCount, allowSquadChanges, mode);  /* 0x3C */

    if (exitGameFlag != 0)
        return 1;

    /* Set isCareer if one team is careerTeam */
    isCareer = 0;
    if (team1 == &careerTeam || team2 == &careerTeam)
        isCareer = 1;

    /* a6 = diyCompetitionStart */
    DIYCompetitionEx *compEx = &diyCompetitionStart;
    uint8_t *a6 = (uint8_t *)compEx;
    DIYCompetition *diy = &compEx->base;

    /* a5 = a6 + exitFlag * 2 — exitFlagVal indexes into the cup
     * roundLegs/roundExtraTime/roundPenalties arrays. */
    int16_t exitFlagVal = diy->currentRoundIndex;
    uint8_t *a5 = a6 + exitFlagVal * 2;
    uint16_t cupRoundIdx = (uint16_t)exitFlagVal;

    /* --- String building section --- */
    int16_t d2; /* tracks d2 through string building for later caching */

    /* Copy competition name to gameName */
    _StringCopy(a6 + 4, gameName);

    /* Build game round string */
    uint8_t *strDst = (uint8_t *)&gameRound;

    if (diy->hasGroups != 0) {
        /* Groups path: use diyFileBufferCopy fields */
        DIYCompetition *bufCopyS = &diyFileBufferCopy.header;
        uint16_t d0 = (uint16_t)bufCopyS->roundComplete;  /* 0x54 */
        uint16_t d1 = 0;
        d2 = bufCopyS->totalMatchesInRound;                       /* 0x52 */
        uint16_t d3 = (uint16_t)diy->totalMatchesInRound;         /* 0x52 from main */
        d3 <<= 1;
        /* +0x15A = first slot record's _pad_00 (read as uint16). */
        uint16_t d4 = *(uint16_t *)diyFileBufferCopy.slots->_pad_00;
        const char *roundStr = _GetCupRoundString(d0, d1, (uint16_t)d2, d3, d4);
        strDst = _StringCopy((const uint8_t *)roundStr, strDst);
    } else {
        /* Non-groups path */
        uint16_t d0 = (uint16_t)diy->currentRoundIndex;
        uint16_t d1 = (uint16_t)diy->numRounds;
        d2 = diy->subRoundCounter;
        const char *roundStr = _GetNextCupRoundString(d0, d1, (uint16_t)d2);
        strDst = _StringCopy((const uint8_t *)roundStr, strDst);
    }

    /* Append replay/leg suffix to round string */
    uint16_t replayField = ((DIYCompetitionEx *)a6)->bracketScratch.groupSectionFlag;

    if (replayField != 0) {
        /* Replay suffix: rndStringTable[12+replay-1] for replay <= 3, else "REPLAY" */
        const uint8_t *suffixStr = aReplay; /* "REPLAY" default */
        if (replayField <= 3) {
            suffixStr = (const uint8_t *)rndStringTable[11 + replayField];
        }
        *strDst++ = ' ';
        strDst = _StringCopy(suffixStr, strDst);
    } else {
        /* Check two-leg conditions */
        uint16_t field14A = compEx->matchData.cup.competitionFormat;
        uint16_t field14C = compEx->matchData.cup.roundFormat;
        if (field14A == 1) {
            *strDst++ = ' ';
            strDst = _StringCopy(a2ndLeg, strDst);
        } else if (field14C == 2) {
            *strDst++ = ' ';
            strDst = _StringCopy(a1stLeg, strDst);
        }
    }
    *strDst = 0; /* null terminate */
    /* --- End string building --- */

    /* Set game parameters from competition */
    gamePitchTypeOrSeason = diy->pitchTypeOrSeason;
    gameSeason = computeGameSeason(diy->viewRandomPhase);
    gamePitchType = diy->pitchType;
    team1GoalsFirstLeg = 0;
    team2GoalsFirstLeg = 0;
    extraTimeState = 0;
    penaltiesState = 0;
    secondLeg = 0;
    playing2ndGame = 0;
    isGameFriendly = 0;

    /* Cache d2 (side effect from string building) into cachedViewResultTeam2Goals */
    cachedViewResultTeam2Goals = (int16_t)d2;

    /* Common values for InitializeInGameTeamsAndStartGame */
    /* Read (compId, slotId) word at +0x2C. Don't use PACK_BE_U16 here:
     * the word was written via *(uint16_t *)&compId = slot, so on
     * SDL LE the compId byte holds `slot` and slotId holds 0 (Amiga BE
     * is the reverse). Reading via *(uint16_t *) preserves the value
     * across hosts; PACK_BE_U16(compId, slotId) gives `slot << 8` on
     * SDL instead of `slot`. */
    int16_t d7val = (int16_t)*(uint16_t *)&diy->compId;

    /* Determine play path based on roundLegs[cupRoundIdx], replayField, competitionFormat */
    int16_t a5_66 = (int16_t)compEx->matchData.cup.roundLegs[cupRoundIdx];

    if (a5_66 == 0 || a5_66 == 1)
        goto path_C;
    if (replayField != 0)
        goto path_C;

    if (compEx->matchData.cup.competitionFormat != 0)
        goto path_B;

    /* === Path A: first leg (a5_66 > 1, replayField == 0, field14A == 0) === */
    {
        _InitializeInGameTeamsAndStartGame(0,
            (int16_t)diyMinSubstitutes, *(int16_t *)diyPointsForAWin,
            diyStart->substitutesCount, d7val,  /* 0x3C */
            (TeamFile *)team1, (TeamFile *)team2);
        if (gameCanceled != 0)
            return 1;
        cachedViewResultTeam1Goals = statsTeam1GoalsCopy;
        cachedViewResultTeam2Goals = statsTeam2GoalsCopy;
        goto done;
    }

path_B: /* === Second leg === */
    {
        secondLeg = 1;

        /* Extra time condition: roundExtraTime[cupRoundIdx] */
        int16_t a5_72 = (int16_t)compEx->matchData.cup.roundExtraTime[cupRoundIdx];
        if (a5_72 == 1 || (a5_72 > 1 && replayField != 0))
            extraTimeState = 1;

        /* Penalties condition: roundPenalties[cupRoundIdx] */
        int16_t a5_7E = (int16_t)compEx->matchData.cup.roundPenalties[cupRoundIdx];
        if (a5_7E == 1 || (a5_7E > 1 && replayField != 0))
            penaltiesState = 1;

        /* Load first-leg goals from BSS_00_2E0AE region (_bss_2E0AE, at
         * parent 0x14E). Host-native uint16 + byte-extract: see the
         * matching read in _DIY_PlayCupMatchAndAdvance for why a
         * byte-pair load gives swapped scores on LE x86. */
        uint16_t matchIdx = compEx->matchData.cup.currentMatchIndex;
        uint16_t packedLeg1 = ((uint16_t *)compEx->_bss_2E0AE)[matchIdx];
        team1GoalsFirstLeg = (packedLeg1 >> 8) & 0xFF;
        team2GoalsFirstLeg = packedLeg1 & 0xFF;

        /* playing2ndGame = playEachTeam */
        playing2ndGame = (uint16_t)diy->playEachTeam;

        /* Play the match */
        _InitializeInGameTeamsAndStartGame(0,
            (int16_t)diyMinSubstitutes, *(int16_t *)diyPointsForAWin,
            diyStart->substitutesCount, d7val,  /* 0x3C */
            (TeamFile *)team1, (TeamFile *)team2);
        if (gameCanceled != 0)
            return 1;

        cachedViewResultTeam1Goals = statsTeam1GoalsCopy;
        cachedViewResultTeam2Goals = statsTeam2GoalsCopy;

        /* Cache extra time goals if extra time was actually played (state < 0) */
        if (extraTimeState < 0) {
            cachedExtraTimeTeam1Goals = statsTeam1Goals - statsTeam1GoalsCopy;
            cachedExtraTimeTeam2Goals = statsTeam2Goals - statsTeam2GoalsCopy;
        }

        /* Cache penalty scores if penalties were actually played (state < 0) */
        if (penaltiesState < 0) {
            cachedPenaltyShootoutTeam1Score = team1PenaltyGoals;
            cachedPenaltyShootoutTeam2Score = team2PenaltyGoals;
        }
        goto done;
    }

path_C: /* === Auto-play/sim path === */
    {
        /* Extra time condition: roundExtraTime[cupRoundIdx] */
        int16_t a5_72 = (int16_t)compEx->matchData.cup.roundExtraTime[cupRoundIdx];
        if (a5_72 == 1 || (a5_72 > 1 && replayField != 0))
            extraTimeState = 1;

        /* Penalties condition: roundPenalties[cupRoundIdx] */
        int16_t a5_7E = (int16_t)compEx->matchData.cup.roundPenalties[cupRoundIdx];
        if (a5_7E == 1 || (a5_7E > 1 && replayField != 0))
            penaltiesState = 1;

        /* seq d0: clr.w d0; tst.w cup.roundFormat; seq d0 → 0x00FF if roundFormat==0 */
        uint16_t field14C = compEx->matchData.cup.roundFormat;
        int16_t randomizePitch = (field14C == 0) ? 0xFF : 0;

        /* Play the match */
        _InitializeInGameTeamsAndStartGame(randomizePitch,
            (int16_t)diyMinSubstitutes, *(int16_t *)diyPointsForAWin,
            diyStart->substitutesCount, d7val,  /* 0x3C */
            (TeamFile *)team1, (TeamFile *)team2);
        if (gameCanceled != 0)
            return 1;

        cachedViewResultTeam1Goals = statsTeam1GoalsCopy;
        cachedViewResultTeam2Goals = statsTeam2GoalsCopy;

        /* Cache extra time goals if extra time was actually played (state < 0) */
        if (extraTimeState < 0) {
            cachedExtraTimeTeam1Goals = statsTeam1Goals - statsTeam1GoalsCopy;
            cachedExtraTimeTeam2Goals = statsTeam2Goals - statsTeam2GoalsCopy;
        }

        /* Cache penalty scores if penalties were actually played (state < 0) */
        if (penaltiesState < 0) {
            cachedPenaltyShootoutTeam1Score = team1PenaltyGoals;
            cachedPenaltyShootoutTeam2Score = team2PenaltyGoals;
        }
        goto done;
    }

done:
    isCareer = 0;
    return 0;
}

/* ========================================================================
 * Chunk 193 — DIY_PlayCupMatchAndAdvance, DIY_PlayOrSimulateLeagueFixture
 * ======================================================================== */

/* Externs for Chunk 193 */
extern uint16_t careerMatchSimulatedFlag;
extern TeamFileEx *diyCupLastMatchTeam1Ptr;
extern TeamFileEx *diyCupLastMatchTeam2Ptr;
extern uint16_t diyCupLastMatchScoreTeam1;
extern uint16_t diyCupLastMatchScoreTeam2;

/* DIY_PlayCupMatchAndAdvance
 *
 * Orchestrates a complete DIY cup match: routes to play/sim path based on mode,
 * calls _ProcessDIYKnockoutMatchResultAndCareerUpdates for result processing,
 * updates competition state (match results, goals, penalties, indices),
 * advances to next round when all matches complete, handles two-leg logic,
 * and updates career stats if applicable.
 *
 * Params: allowSquadChanges (d6 — implicit), mode (d7)
 * Returns 0 on success (Z=1), nonzero on cancel (Z=0).
 */
__attribute__((used, externally_visible))
uint16_t _DIY_PlayCupMatchAndAdvance(int16_t allowSquadChanges, int16_t mode) {
    uint8_t *base = (uint8_t *)&diyCompetitionStart;
    DIYCompetition *baseS = (DIYCompetition *)base;

#ifdef SWOS_DEBUG_AUTOSKIP
    {
        DIYCompetitionEx *bex = &diyCompetitionStart;
        SWOS_AUTOSKIP_LOG("[bug4-cup] PlayCupMatch ENTER compId=0x%04X "
                          "roundIdx=%d/%d matchIdx=%d/%d roundFormat=%d "
                          "competitionFormat=%d roundComplete=%d "
                          "competitionFinished=%d careerTeamInvolved=%d mode=%d",
                          (unsigned)*(uint16_t *)&baseS->compId,
                          (int)baseS->currentRoundIndex, (int)baseS->numRounds,
                          (int)bex->matchData.cup.currentMatchIndex,
                          (int)baseS->totalMatchesInRound,
                          (int)bex->matchData.cup.roundFormat,
                          (int)bex->matchData.cup.competitionFormat,
                          (int)baseS->roundComplete,
                          (int)baseS->competitionFinished,
                          (int)baseS->careerTeamInvolved,
                          (int)mode);
    }
#endif

    /* Set progressFieldA = 1 */
    baseS->matchStarted = 1;  /* 0x44 */

    _DIY_SyncProgressToDiyFileBufferCopy();

    /* Get current match team slot indices → team pointers via
     * bracketScratch.teamOffsetTable[slotIdx]. */
    int16_t slot1Idx, slot2Idx;
    getCurrentMatchTeamSlotIndices(&slot1Idx, &slot2Idx);

    DIYCompetitionEx *baseExForSlot = &diyCompetitionStart;
    TeamFileEx *team1 = (TeamFileEx *)diy_resolve_team(baseExForSlot->bracketScratch.teamOffsetTable[slot1Idx]);
    TeamFileEx *team2 = (TeamFileEx *)diy_resolve_team(baseExForSlot->bracketScratch.teamOffsetTable[slot2Idx]);

    /* Route match based on mode */
    if (mode == 0) {
        /* Sim path: check if any team is human/coach (teamControls == 2 or 3).
         *
         * DEVIATION FROM ASM: skip SetupPlayers entirely during a season
         * transition (seasonUpdateInProgress != 0). The asm `SetupPlayers`
         * has no such guard and would show playMatchMenu here — but on the
         * Amiga the team data loaded for non-player cups via
         * Career_AutoAdvanceCupCompetitionFromRecord apparently has
         * teamControls == TEAM_COMPUTER for every slot, so this branch is
         * never taken. On our C build the player's careerTeam (with
         * teamControls=2 or 3) lingers in selTeamsPtr from the previous
         * competition's load, so the auto-advance trips SetupPlayers and
         * the season-transition chain stalls on playMatchMenu. Suppressing
         * the menu here matches the observable Amiga behaviour and lets
         * the cup/league result chain auto-skip uninterrupted. */
        uint8_t ctrl1 = *((uint8_t *)team1 + 4); /* teamControls byte */
        uint8_t ctrl2 = *((uint8_t *)team2 + 4);
        if ((ctrl1 == 2 || ctrl2 == 2 || ctrl1 == 3 || ctrl2 == 3) &&
            seasonUpdateInProgress == 0 &&
            careerAutoAdvanceInProgress == 0) {
            /* At least one human/coach team — show SetupPlayers with no squad changes.
             * careerAutoAdvanceInProgress guard added for the same reason as the
             * matching deviation in _DIY_PlayOrSimulateLeagueFixture: a stale ctrl=2
             * in selTeamsPtr from a previous interactive load would otherwise stall
             * Career_PostMatchUpdate's auto-advance on a playMatchMenu for a cup
             * the user isn't even participating in. */
            _SetupPlayers(team1, team2, *(int16_t *)diyPointsForAWin,
                          baseS->substitutesCount, 0, 0);  /* 0x3C */
            if (exitGameFlag != 0)
                return 1;
        }
        /* Play sim match */
        _InitInGameTeams_WithMatchParam(team1, team2);
    } else if (mode == 1) {
        if (_PlayDIYCupFixtureAndCacheResult(team1, team2, allowSquadChanges, 0) != 0)
            return 1;
    } else if (mode == 3) {
        if (_PlayDIYCupFixtureAndCacheResult(team1, team2, allowSquadChanges, 1) != 0)
            return 1;
    } else {
        /* mode==2 or other */
        if (_PlayDIYCupFixtureAndCacheResult(team1, team2, allowSquadChanges, 0) != 0)
            return 1;
    }

    /* === Post-match processing === */

    /* Load first-leg goals (d5/d6) if two-leg enabled.
     *
     * two_leg_path rotates first-leg packedGoals into _bss_2E0AE via a
     * host-native read + ror#8 + host-native write. On LE x86 the
     * physical byte layout of the stored value is therefore inverted vs
     * the Amiga BE layout, so the Amiga's byte-pair load (byte0/byte1)
     * returns the scores swapped for the second-leg perspective. Read
     * host-native and split high/low byte instead — that yields the
     * same logical value on either endian. */
    uint16_t firstLeg1 = 0, firstLeg2 = 0;
    DIYCompetitionEx *baseEx = (DIYCompetitionEx *)base;
    if (baseEx->matchData.cup.competitionFormat != 0) {
        uint16_t mIdx = baseEx->matchData.cup.currentMatchIndex;
        uint16_t packed = ((uint16_t *)baseEx->_bss_2E0AE)[mIdx];
        firstLeg1 = (packed >> 8) & 0xFF;
        firstLeg2 = packed & 0xFF;
    }

    /* Process knockout match result → d4/d5/d6/d7 */
    uint16_t packedGoals, packedAggregate, packedPenalty, resultFlags;
    _ProcessDIYKnockoutMatchResultAndCareerUpdates(team1, team2, firstLeg1, firstLeg2,
        &packedGoals, &packedAggregate, &packedPenalty, &resultFlags);

    /* Asm returns team ptrs in a1/a2 and scores in d1/d2 for caller
     * (DIYCupOnSelectCommon stores them into diyCupLastMatch* globals).
     * Since C doesn't preserve register side effects, store them here. */
    diyCupLastMatchTeam1Ptr = team1;
    diyCupLastMatchTeam2Ptr = team2;
    diyCupLastMatchScoreTeam1 = packedGoals >> 8;
    diyCupLastMatchScoreTeam2 = packedGoals & 0xFF;

    uint8_t *a3 = base;
    DIYCompetitionEx *a3Ex = baseEx;

    /* Store result flags byte in cupRoundResults[matchIndex] */
    uint16_t matchIdx = a3Ex->matchData.cup.currentMatchIndex;
    a3Ex->matchData.cup.cupRoundResults[matchIdx] = (uint8_t)resultFlags;

    /* Store packed goals at cupRoundResults+0x40 + matchIndex*2 (score region) */
    uint16_t mIdx2 = matchIdx * 2;
    *(uint16_t *)&a3Ex->matchData.cup.cupRoundResults[0x40 + mIdx2] = packedGoals;

    /* If competitionFormat == 0, store aggregate at +0x14E (BSS_00_2E0AE)
     * + matchIndex*2 — second-leg aggregate score buffer. */
    if (a3Ex->matchData.cup.competitionFormat == 0) {
        *(uint16_t *)&a3Ex->_bss_2E0AE[mIdx2] = packedAggregate;
    }

    /* Store penalty at +0x1CE + matchIndex*2 — uses
     * leagueSectionState.phaseStartMonths memory as a penalty score
     * buffer (this region is repurposed depending on competition mode). */
    *(uint16_t *)&a3Ex->leagueSectionState.phaseStartMonths[mIdx2] = packedPenalty;

    /* Increment match counter */
    a3Ex->bracketScratch.matchCount += 1;

    /* Check if career team was in this match */
    if (team1 == &careerTeam || team2 == &careerTeam) {
        baseS->careerTeamInvolved = 1;  /* 0x46 */
        careerMatchSimulatedFlag = 1;
    }

    _DIY_SyncProgressToDiyFileBufferCopy();

    /* Advance match index */
    a3Ex->matchData.cup.currentMatchIndex += 1;
    baseS->roundComplete = 0;  /* 0x54: clear currentRound flag */

    uint16_t newMatchIdx = a3Ex->matchData.cup.currentMatchIndex;
    uint16_t totalRounds = (uint16_t)baseS->totalMatchesInRound;  /* 0x52 */

    if (newMatchIdx == totalRounds) {
        /* All matches in this round completed */
        baseS->roundComplete = 1;  /* 0x54 */
        a3Ex->matchData.cup.currentMatchIndex = 0;

        /* Update viewRandomPhase for next round from month table */
        uint8_t *monthTable = a3Ex->bracketScratch.progTable;
        uint16_t curMonthIdx = a3Ex->bracketScratch.curMonthIdx;
        uint16_t totalMonths = a3Ex->bracketScratch.totalMatches;
        if (curMonthIdx + 1 != totalMonths) {
            baseS->viewRandomPhase = monthTable[curMonthIdx + 1];  /* 0x2F */
            viewRandomPhase = baseS->viewRandomPhase;
        }
    }

    /* Career stat updates if applicable */
    if (careerLoadedFlag == 0 && (gameType == 4 || gameType == 3)) {
        _Career_UpdateStatsAfterMatch(team1, team2,
            1, packedGoals, packedAggregate, packedPenalty, resultFlags);
    }

#ifdef SWOS_DEBUG_AUTOSKIP
    {
        int careerInvolved = (team1 == &careerTeam || team2 == &careerTeam);
        SWOS_AUTOSKIP_LOG("[bug4-cup]   PlayCupMatch played compId=0x%04X "
                          "newMatchIdx=%d roundComplete=%d competitionFinished=%d "
                          "packedGoals=0x%04X flags=0x%02X careerInvolved=%d",
                          (unsigned)*(uint16_t *)&baseS->compId,
                          (int)newMatchIdx, (int)baseS->roundComplete,
                          (int)baseS->competitionFinished,
                          (unsigned)packedGoals, (unsigned)(resultFlags & 0xFF),
                          careerInvolved);
    }
#endif

    /* Check if round is complete */
    if (baseS->roundComplete == 0)  /* 0x54 */
        return 0;

    /* Two-leg check: if 2-leg format and first leg wasn't played yet, stop */
    if (a3Ex->matchData.cup.roundFormat == 2) {
        if (a3Ex->matchData.cup.competitionFormat != 1)
            return 0;
    }

    /* Round complete — determine next action */
    if (baseS->hasGroups != 0)  /* 0x48 */
        goto final_or_groups; /* hasGroups → skip to final/groups handler */

    /* Non-groups: check if more rounds remain */
    {
        uint16_t curExit = (uint16_t)baseS->currentRoundIndex;  /* 0x58 */
        if (curExit + 1 != (uint16_t)baseS->numRounds) {  /* 0x5A */
            /* More rounds: recount, check unselected, shuffle */
            _DIY_RecountMissingTeamsInTeamGrid();
            _DIY_CountUnselectedTeams();
            if (a3Ex->bracketScratch.groupSectionFlag == 0)
                _DIY_ShuffleTeamOrderList();
            return 0;
        }
    }

final_or_groups:
    /* Final round or groups completion handler */
    _DIY_RecountMissingTeamsInTeamGrid();
    if (_DIY_CountUnselectedTeams() != 0)
        return 0;

#ifdef SWOS_DEBUG_AUTOSKIP
    SWOS_AUTOSKIP_LOG("[bug4-cup]   PlayCupMatch SETS competitionFinished=1 "
                      "compId=0x%04X currentRoundIdx=%d/numRounds=%d hasGroups=%d",
                      (unsigned)*(uint16_t *)&baseS->compId,
                      (int)baseS->currentRoundIndex, (int)baseS->numRounds,
                      (int)baseS->hasGroups);
#endif
    /* All teams accounted for — competition/stage finished */
    baseS->competitionFinished = 1;  /* 0x56 */

    if (baseS->hasGroups != 0) {  /* 0x48 */
        /* Has groups: check if this groups stage is done */
        DIYCompetition *bufCopyS2 = &diyFileBufferCopy.header;
        uint16_t bufCopyCurRound = (uint16_t)bufCopyS2->roundComplete;   /* 0x54 */
        uint16_t bufCopyTotalRounds = (uint16_t)bufCopyS2->totalMatchesInRound; /* 0x52 */
        if (bufCopyCurRound + 1 != bufCopyTotalRounds)
            return 0; /* groups stage not done yet */
    }

    /* Determine competition winner from grid bracket */
    {
        int8_t slotIdx = (int8_t)a3Ex->teamGridPairings[0];  /* first grid slot */
        if (a3Ex->matchData.cup.cupRoundResults[0] & 0x01)
            slotIdx = (int8_t)a3Ex->teamGridPairings[1]; /* second grid slot */
        int16_t slotWord = (int16_t)slotIdx;
        /* cupTeamIds is BE-packed (4e5f378) — read the winner's id
         * with BE_READ_U16 so resultCode holds the packed value
         * downstream comparisons expect. */
        baseS->resultCode = (int16_t)BE_READ_U16(&a3Ex->cupTeamIds[slotWord]);
    }

    return 0;
}

/* DIY_PlayOrSimulateLeagueFixture
 *
 * Orchestrates a complete DIY league match: routes to play/sim path based on
 * selector, calls ProcessDIYLeagueMatchResultAndCareerUpdates, advances league
 * progress, updates league table (wins/draws/losses/goals/points), sorts
 * standings, and processes promotions/relegations.
 *
 * Params: allowSquadChanges (d6 — implicit), selector (d7)
 * Returns 0 on success (Z=1), nonzero on cancel (Z=0).
 */
__attribute__((used, externally_visible))
uint16_t _DIY_PlayOrSimulateLeagueFixture(int16_t allowSquadChanges, int16_t selector) {
    DIYCompetitionEx *baseEx = &diyCompetitionStart;
    uint8_t *base = (uint8_t *)baseEx;
    DIYCompetition *baseS = &baseEx->base;

    /* Set progressFieldA = 1 */
    baseS->matchStarted = 1;  /* 0x44 */

    _DIY_SyncProgressToDiyFileBufferCopy();

    /* Get current fixture team comp pointers */
    _DIY_GetCurrentFixtureTeamPtrs();
    uint8_t *compPtr1 = fixtureTeam1CompPtr;
    uint8_t *compPtr2 = fixtureTeam2CompPtr;

    /* Build team file pointers from DIYTeamStats teamSelOffset (+0x2B4).
     * teamSelOffset is a SIGNED byte-offset into selTeamsPtr — positive
     * values index forward (other teams), negative values index back
     * (careerTeam lives below selTeamsPtr in memory). */
    int32_t teamSelOff1 = (int32_t)((DIYTeamStats *)(compPtr1 + DIY_STANDINGS_OFFSET))->teamSelOffset;
    int32_t teamSelOff2 = (int32_t)((DIYTeamStats *)(compPtr2 + DIY_STANDINGS_OFFSET))->teamSelOffset;
    TeamFileEx *team1 = (TeamFileEx *)diy_resolve_team(teamSelOff1);
    TeamFileEx *team2 = (TeamFileEx *)diy_resolve_team(teamSelOff2);

    /* [bug23-fix] Log fixture details when both sides resolve to the
     * same TeamFileEx — that's how the qualifier auto-play loop got
     * into _VerifyTeamLoaded(0) on NATIONAL.CAR. Capture which row in
     * the standings table the duplicate came from + the current
     * fixture index so we can trace back to the broken bracket bytes. */
    if (team1 == team2) {
        DIYCompetitionEx *bx = &diyCompetitionStart;
        DIYCompetition  *bs = &bx->base;
        uint8_t *cb = (uint8_t *)bx;
        SWOS_DBG("[bug23-fix] duplicate team fixture: "
                        "compPtr1=%p compPtr2=%p teamSelOff1=%d teamSelOff2=%d "
                        "team1=team2=%p selTeamsPtr=%p "
                        "leagueCurrentSlot=%u matchesPlayedCount=%u "
                        "roundComplete=%u currentRoundIndex=%d "
                        "numTeams=%d compType=%u\n",
                (void *)compPtr1, (void *)compPtr2,
                (int)teamSelOff1, (int)teamSelOff2,
                (void *)team1, (void *)&selTeamsPtr,
                (unsigned)bx->leagueCurrentSlot,
                (unsigned)bx->matchesPlayedCount,
                (unsigned)bs->roundComplete,
                (int)bs->currentRoundIndex,
                (int)bs->numTeams, (unsigned)bs->competitionType);
        /* Dump first fixture row from each likely fixture-list offset.
         * The fixture-team-pair table on a league lives somewhere we
         * haven't fully nailed down — show 32 bytes from the row that
         * compPtr1 points into so we can see what byte-pair generated
         * the offsets. */
        const uint8_t *r1 = compPtr1;
        SWOS_DBG("[bug23-fix]   compPtr1 row[0..31]:");
        for (int i = 0; i < 32; i++) SWOS_DBG(" %02X", r1[i]);
        SWOS_DBG("\n");
        SWOS_DBG_FLUSH();
    }

    /* Safety guard ONLY for auto-advance paths. If either teamSelOffset
     * offset is genuinely out of range for selTeamsPtr during
     * careerAutoAdvanceInProgress / seasonUpdateInProgress, the
     * standings table at diyCompetitionStart+0x2B4 is garbage (uninit
     * competition context for a slot the user isn't really in).
     *
     * The previous unsigned `>= maxOff` test misclassified legitimate
     * negative offsets (careerTeam, e.g. -42288 = 0xFFFF5AD0) as
     * corruption — the bail then prevented the player's competitions
     * from auto-advancing and froze the post-sack chairman-memo flow
     * (Playaveli's first-season AC Milan repro). Use a sign-aware
     * range that still catches the 17 MB-class positive corruption but
     * accepts plausible negative careerTeam offsets up to ~1 MB
     * backward.
     *
     * On bail we must:
     *   1. NOT dereference the garbage TeamFileEx pointer.
     *   2. Force the caller's `while (leagueCurrentSlot == 0)` loop in
     *      `_Career_AutoAdvanceLeagueFixtures` to exit — otherwise it
     *      re-calls us forever.
     *   3. Log only on the first bail per process (a static flag);
     *      the auto-advance can call us hundreds of times per match
     *      and we don't want to overrun the log writer.
     *
     * IMPORTANT: do NOT apply this guard during interactive play —
     * bailing silently there makes the MATCH / VIEW RESULT buttons
     * appear dead. */
    extern uint16_t numSelectedTeams;
    if (careerAutoAdvanceInProgress != 0 || seasonUpdateInProgress != 0) {
        int32_t maxPositive = (int32_t)numSelectedTeams * (int32_t)sizeof(TeamFileEx);
        const int32_t maxNegative = -1 * 1024 * 1024;
        if (teamSelOff1 > maxPositive || teamSelOff1 < maxNegative ||
            teamSelOff2 > maxPositive || teamSelOff2 < maxNegative) {
#ifdef SWOS_DEBUG_AUTOSKIP
            static int s_bug1_bail_logged = 0;
            if (!s_bug1_bail_logged) {
                s_bug1_bail_logged = 1;
                SWOS_AUTOSKIP_LOG("[bug1] PlayOrSimLeague: bail (first occurrence) — "
                                  "bogus teamSelOffset (team1off=%d team2off=%d maxPositive=%d "
                                  "numSelTeams=%u careerAutoAdv=%u seasonUpd=%u selector=%d)",
                                  (int)teamSelOff1, (int)teamSelOff2,
                                  (int)maxPositive, (unsigned)numSelectedTeams,
                                  (unsigned)careerAutoAdvanceInProgress,
                                  (unsigned)seasonUpdateInProgress,
                                  (int)selector);
            }
#endif
            /* Force the outer auto-advance loop to exit instead of
             * retrying the same bogus fixture. Setting
             * leagueCurrentSlot != 0 makes
             * `while (leagueCurrentSlot == 0)` in
             * _Career_AutoAdvanceLeagueFixtures break. */
            baseEx->leagueCurrentSlot = 1;
            return 1;
        }
    }

    /* Route match based on selector */
    if (selector == 0) {
        /* Sim path: check if any team is human/coach (teamControls == 2 or 3).
         *
         * DEVIATION FROM ASM: skip SetupPlayers during seasonUpdateInProgress
         * AND during careerAutoAdvanceInProgress. The asm has NO guard here
         * because it relies on the team data feeding into the auto-advance
         * always having teamControls=1 (computer-only) — both
         * `_Career_PostMatchUpdate` and the season-transition path
         * temporarily set careerTeam.teamControls=1 around their
         * auto-advance calls. On our SDL build the player's team can still
         * land in selTeamsPtr with teamControls=2 from a previous
         * interactive load (preset competition / DIY), and the auto-advance
         * pulls that ctrl=2 in, tripping SetupPlayers and stalling the
         * career post-match flow on a playMatchMenu the user can't exit
         * (team1==team2 because the fixture being auto-advanced is for a
         * competition the user isn't even in — Spanish Cup in the
         * tester's repro). Both guards collapse the menu pop-up to its
         * intended Amiga behaviour: "auto-advance is silent". */
        uint8_t ctrl1 = *((uint8_t *)team1 + 4);
        uint8_t ctrl2 = *((uint8_t *)team2 + 4);
        if ((ctrl1 == 2 || ctrl2 == 2 || ctrl1 == 3 || ctrl2 == 3) &&
            seasonUpdateInProgress == 0 &&
            careerAutoAdvanceInProgress == 0) {
            /* At least one human/coach team — show SetupPlayers */
            _SetupPlayers(team1, team2, (int16_t)diyLeagueMaxSubstitutes,
                          baseS->substitutesCount, 0, 0);  /* 0x3C */
            if (exitGameFlag != 0)
                return 1;
        }
        /* Sim match */
        _InitInGameTeams_WithMatchParam(team1, team2);
    } else if (selector == 1) {
        if (_PlayDIYLeagueMatchAndCacheResult(team1, team2, allowSquadChanges, 0) != 0)
            return 1;
    } else if (selector == 3) {
        if (_PlayDIYLeagueMatchAndCacheResult(team1, team2, allowSquadChanges, 1) != 0)
            return 1;
    } else {
        /* selector==2: explicitly set allowSquadChanges=1, mode=0 */
        if (_PlayDIYLeagueMatchAndCacheResult(team1, team2, 1, 0) != 0)
            return 1;
    }

    /* === Post-match processing === */

    /* Get match results */
    uint16_t goals1, goals2, flags;
    _ProcessDIYLeagueMatchResultAndCareerUpdates(team1, team2, &goals1, &goals2, &flags);

    /* Advance league progress using comp pointers */
    _DIY_AdvanceLeagueProgressAfterMatch(compPtr1, compPtr2);

    /* Check if career team was in this match */
    if (team1 == &careerTeam || team2 == &careerTeam) {
        baseS->careerTeamInvolved = 1;  /* 0x46 */
        careerMatchSimulatedFlag = 1;
    }

    _DIY_SyncProgressToDiyFileBufferCopy();

    /* Career stat updates if applicable */
    if (careerLoadedFlag == 0 && (gameType == 4 || gameType == 3)) {
        uint16_t packedGoals = (goals1 << 8) + goals2;
        _Career_UpdateStatsAfterMatch(team1, team2,
            0, packedGoals, 0, 0, flags);
    }

    /* Increment total match counter (matchesPlayedCount) */
    baseEx->matchesPlayedCount += 1;

    /* League table updates */
    DIYTeamStats *r1 = (DIYTeamStats *)(compPtr1 + DIY_STANDINGS_OFFSET);
    DIYTeamStats *r2 = (DIYTeamStats *)(compPtr2 + DIY_STANDINGS_OFFSET);
    uint16_t d1 = goals1, d2 = goals2;
    if (d1 != d2) {
        if (d1 > d2) {
            /* Team 1 wins */
            r1->wins   += 1;
            r2->losses += 1;
            r1->points += baseEx->pointsForAWin;
        } else {
            /* Team 2 wins */
            r1->losses += 1;
            r2->wins   += 1;
            r2->points += baseEx->pointsForAWin;
        }
    } else {
        /* Draw */
        r1->draws  += 1;
        r2->draws  += 1;
        r1->points += 1;
        r2->points += 1;
    }

    /* Goals always update */
    r1->goalsFor     += d1;
    r1->goalsAgainst += d2;
    r2->goalsFor     += d2;
    r2->goalsAgainst += d1;

    /* Sort standings and process promotions/relegations if diyMinSubstitutes == 0 */
    if (diyMinSubstitutes == 0) {
        _DIY_SortLeagueTableByStandings();

        DIYCompetitionEx *compEx = &diyCompetitionStart;
        DIYCompetition *a5s = &compEx->base;
        uint16_t *standingsLookup = compEx->matchData.league.leagueTeamIds;
        /* Promotions/relegations output buffer: byte offset 0x60 into
         * _bss_2E0AE (parent struct offset 0x1AE on 32-bit, 0x1B2 on
         * 64-bit due to namePtr growth). Typed field access keeps the
         * 64-bit shift correct — was `a5 + 0x1AE` previously, which
         * underflowed 4 bytes on 64-bit. */
        uint16_t *promRelOutput = (uint16_t *)(compEx->_bss_2E0AE + 0x60);
        int16_t *rankings = (int16_t *)compEx->matchData.league.teamEntryOffsets;

        /* Determine winner */
        int16_t doWinner = 1;
        if (a5s->hasGroups != 0) {  /* 0x48 */
            /* Has groups — only determine winner if all group rounds done */
            DIYCompetition *bufCopyS3 = &diyFileBufferCopy.header;
            uint16_t bufCopyCurRound = (uint16_t)bufCopyS3->roundComplete;   /* 0x54 */
            if (bufCopyCurRound + 1 != (uint16_t)bufCopyS3->totalMatchesInRound)    /* 0x52 */
                doWinner = 0;
        }
        if (doWinner) {
            int16_t ranking = rankings[0];
            uint16_t idx = (uint16_t)((uint32_t)(uint16_t)ranking / 18);
            /* standingsLookup (leagueTeamIds) is BE-packed per 4e5f378.
             * resultCode is compared host-native elsewhere (e.g. cup
             * result-flip detection) so unpack into the host-native
             * packed value here. */
            a5s->resultCode = (int16_t)BE_READ_U16(&standingsLookup[idx]);  /* 0x3A */
        }

        /* Process promotions */
        {
            uint16_t numPromotions = (uint16_t)a5s->competitionFinished;  /* 0x56 */
            if (numPromotions != 0) {
                uint16_t promOffset = (uint16_t)a5s->roundComplete;  /* 0x54 */
                int16_t *src = rankings + promOffset;
                for (uint16_t i = 0; i < numPromotions; i++) {
                    int16_t rank = *src++;
                    uint16_t lookupIdx = (uint16_t)((uint32_t)(uint16_t)rank / 18);
                    /* Preserve BE byte order from standingsLookup into the
                     * promotion output region. diyQualifiedTeamIndices —
                     * the eventual consumer via
                     * _DIY_CopyCompetitionTeamIndexTable — expects BE
                     * bytes per 4e5f378. */
                    uint16_t teamId = BE_READ_U16(&standingsLookup[lookupIdx]);
                    BE_WRITE_U16(promRelOutput, teamId);
                    promRelOutput++;
                }
            }
        }

        /* Process relegations */
        {
            uint16_t numRelegations = (uint16_t)a5s->numRounds;  /* 0x5A */
            if (numRelegations != 0) {
                uint16_t totalEntries = (uint16_t)a5s->numTeams;  /* 0x32 */
                uint16_t field58 = (uint16_t)a5s->currentRoundIndex;  /* 0x58 */
                uint16_t relOffset = totalEntries - field58 - numRelegations;
                int16_t *src = rankings + relOffset;
                for (uint16_t i = 0; i < numRelegations; i++) {
                    int16_t rank = *src++;
                    uint16_t lookupIdx = (uint16_t)((uint32_t)(uint16_t)rank / 18);
                    /* BE-preserving copy — see promotion loop above. */
                    uint16_t teamId = BE_READ_U16(&standingsLookup[lookupIdx]);
                    BE_WRITE_U16(promRelOutput, teamId);
                    promRelOutput++;
                }
            }
        }
    }

    /* Store team pointers and results for DIYSetTeamNamesAndResults.
     * In asm, a1/a2/d1/d2 survive as register outputs and are stored
     * by the caller (DIYLeagueTournamentMatchCommon). In C we must
     * store them here since register values don't persist. */
    diyLeftTeamPtr = team1;
    diyRightTeamPtr = team2;
    diyLeftTeamResult = goals1;
    diyRightTeamResult = goals2;

    return 0;
}
