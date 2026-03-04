/* diy_competition.c — DIY competition/tournament management, player pricing.
 *
 * Chunk 60: 16 functions ported from SWOS2.s.
 */

#include "diy_competition.h"
#include "game_types.h"
#include "string_utils.h"
#include "random.h"
#include "panic.h"
#include "menu_system.h"
#include "text_render.h"
#include "input_text.h"

/* ---- Constants ---- */
#define DIY_CUP                         1

/* ---- ASM externs (globals defined in assembly) ---- */
/* These remain as byte arrays because diyCompetitionStart is actually 0x5C8 bytes
 * (only the first 0x60 bytes match DIY_Competition). Typed access via casts. */
extern uint8_t diyFileBufferCopy[];
extern uint8_t diyCompetitionStart[];
extern uint8_t diyTeamShuffleIndices[];
extern uint16_t diyTeamEntryOffsetsByIndex[];
extern uint16_t diyTournamentTeamIds[];
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

/* ASM data tables */
extern uint32_t cupRoundStringTable[];
extern uint8_t diyLeagueSeasonStateTable[];
extern uint8_t diyLeagueScratchBuffer[];
extern uint8_t competitionFileBuffer[];
extern uint8_t competitionContext[];
extern uint8_t tertiaryCompetitionContext[];
extern uint8_t euroCupsCompetitionContext[];

/* ASM globals for cup bracket */
extern uint16_t diyCupTeamIds[];

/* ---- C bridges to asm functions ---- */
extern uint8_t *_VerifyTeamLoaded(uint16_t teamNum);
extern void _CDIY_RecomputeCompetitionDerivedProgressionTable(void);

/* ---- ASM functions still in asm (called via bridge) ---- */
/* _ShowDiyLeaguesTournamentMenu now ported to C — declared in diy_competition.h */
extern void _asm_ShowDIYCupMenuWithParams(uint16_t d0, uint16_t d1, uint16_t d2);
extern void _asm_Career_AutoAdvanceInternationalCompetitionFixtures(void);

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
extern uint32_t diyCompetitionPtr;
extern uint32_t *diyCompetitionTablePtr;
extern uint8_t diyCompetitionContext[];
extern uint32_t currentCompetitionPtr[];
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
extern uint32_t aFriendly;  /* string pointer "FRIENDLY" */
extern uint8_t worldCup[];
extern uint8_t diyPointsForAWin[];

/* ---- Fixture data ---- */
extern uint8_t europeanChampionships[];
extern uint8_t europeanChampionships2[];
extern uint8_t euroChampFixtureData[];
extern uint32_t fixtureSchedulePtrTable[];
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
uint16_t _GetAveragePlayerPrice(uint8_t *team)
{
    TeamFileEx *t = (TeamFileEx *)team;
    uint16_t total = 0;
    for (int16_t i = 0; i < 16; i++) {
        total += (uint8_t)t->players[i].price;
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
 * asm: a2=diy buffer, a3=competition slot
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_ShuffleTeams(uint8_t *diy, uint8_t *slot)
{
    uint8_t *teamIndices = diy + 0x5A;
    uint16_t count = *(uint16_t *)(slot + 0x162);
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
 * asm: a2=diy buffer, a3=competition slot
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_SortAndShuffleTeamsByValue(uint8_t *diy, uint8_t *slot)
{
    uint16_t *teamIds = (uint16_t *)(diy + 0x9A);
    uint8_t *teamIndices = diy + 0x5A;
    uint16_t numTeams = *(uint16_t *)(slot + 0x162);

    /* Bubble sort: sort teamIndices by descending average player price */
    for (int16_t outer = (int16_t)numTeams - 2; outer >= 0; outer--) {
        uint8_t *p = teamIndices;
        for (int16_t inner = outer; inner >= 0; inner--) {
            /* Get avg price for team at p[0] */
            int16_t idx0 = (int8_t)p[0];
            uint16_t teamNum0 = teamIds[idx0];
            uint8_t *team0 = _VerifyTeamLoaded(teamNum0);
            uint16_t price0 = _GetAveragePlayerPrice(team0);

            /* Get avg price for team at p[1] */
            int16_t idx1 = (int8_t)p[1];
            uint16_t teamNum1 = teamIds[idx1];
            uint8_t *team1 = _VerifyTeamLoaded(teamNum1);
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
    uint16_t isCup = *(uint16_t *)(slot + 0x15E);
    uint16_t groupSize, groupCount;

    if (isCup) {
        groupSize = *(uint16_t *)(slot + 0x16C);
        groupCount = *(uint16_t *)(slot + 0x16A);
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
    DIY_Competition *comp = (DIY_Competition *)diyCompetitionStart;
    uint8_t *compRaw = (uint8_t *)comp;
    uint8_t *shuffleIdx = diyTeamShuffleIndices;
    uint16_t *teamOffsets = (uint16_t *)(compRaw + 0x6E);
    uint16_t count = (uint16_t)comp->numTeams;

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
    DIY_Competition *comp = (DIY_Competition *)diyCompetitionStart;
    uint8_t *compRaw = (uint8_t *)comp;
    *(uint16_t *)(compRaw + 0x32E) = 0;

    uint8_t *flags = compRaw + 0x8A;
    uint16_t numTeams = (uint16_t)comp->totalRounds;

    for (int16_t i = (int16_t)numTeams - 1; i >= 0; i--) {
        if (!(flags[i] & 2)) {
            *(uint16_t *)(compRaw + 0x32E) += 1;
        }
    }

    return *(uint16_t *)(compRaw + 0x32E);
}

/* Helper: clear league table stats for 64 entries */
static void clearLeagueTableStats(uint8_t *base)
{
    for (int16_t i = 0x3F; i >= 0; i--) {
        *(uint16_t *)(base + 0x2B8) = 0;
        *(uint16_t *)(base + 0x2BA) = 0;
        *(uint16_t *)(base + 0x2BC) = 0;
        *(uint16_t *)(base + 0x2BE) = 0;
        *(uint16_t *)(base + 0x2C0) = 0;
        *(uint16_t *)(base + 0x2C2) = 0;
        *(uint16_t *)(base + 0x2C4) = 0;
        base += 0x12;
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
    DIY_Competition *a2 = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a2raw = (uint8_t *)a2;

    a2->competitionType = 0;             /* competition type = 0 (league) */
    *(uint16_t *)&a2->compId = 0;       /* 0x2C: compId + field_2D as word */
    *(uint16_t *)a2->field_2 = 0;       /* 0x02 */
    a2->progressFieldA = 0;              /* 0x44 */
    a2->progressFieldC = 0;              /* 0x46 */
    a2->resultCode = (int16_t)0xFFFF;    /* 0x3A */
    a2->substitutesCount = 1;            /* 0x3C */
    *(uint16_t *)(a2raw + 0x68) = 0;    /* extended field */
    a2->progressField4 = 3;              /* 0x3E */
    a2->progressField6 = 2;              /* 0x40 */
    a2->progressField8 = 8;              /* 0x42 */
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
    *(uint16_t *)(a2raw + 0x62) = 2;    /* extended field */
    *(uint16_t *)(a2raw + 0x64) = 3;    /* extended field */
    *(uint16_t *)(a2raw + 0x66) = 5;    /* extended field */
    a2->halfTeams = 1;                   /* 0x50 */
    a2->subRoundCounter = 0;             /* 0x5C */
    a2->currentRound = 0;               /* 0x54 */
    a2->finishedFlag = 0;               /* 0x56 */
    a2->exitFlag = 0;                    /* 0x58 */
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
    randomSeed = currentTick;
    newRandomSeedCopy = (uint8_t)(currentTick >> 8);

    DIY_Competition *a2 = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a2raw = (uint8_t *)a2;
    a2->progressFieldA = 0;              /* 0x44 */
    a2->progressFieldC = 0;              /* 0x46 */
    a2->resultCode = (int16_t)0xFFFF;    /* 0x3A */
    *(uint16_t *)(a2raw + 0x1BE) = 0;   /* extended */
    a2->subRoundCounter = 0;             /* 0x5C */
    *(uint16_t *)(a2raw + 0x1CC) = 0;   /* extended */
    *(uint16_t *)(a2raw + 0x1CA) = 0;   /* extended */
    *(uint16_t *)(a2raw + 0x1C2) = 0;   /* extended */
    *(uint16_t *)(a2raw + 0x1C0) = 0;   /* extended */
    /* Copy current values to working copies */
    *(uint16_t *)(a2raw + 0x6C) = *(uint16_t *)(a2raw + 0x6A);  /* extended */
    *(uint16_t *)(a2raw + 0x60) = (uint16_t)a2->playEachTeam;   /* 0x5E→0x60 */

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
    DIY_Competition *a2 = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a2raw = (uint8_t *)a2;
    uint8_t *teamIndices = a2raw + 0xEE;    /* extended */
    uint16_t *teamIds = (uint16_t *)(a2raw + 0x12E);  /* extended */
    uint16_t count = (uint16_t)a2->numTeams;

    for (int16_t i = (int16_t)count - 1; i >= 0; i--) {
        int8_t idx = (int8_t)teamIndices[i];
        uint16_t teamNum = teamIds[(uint16_t)(int16_t)idx];
        uint8_t *teamPtr = _VerifyTeamLoaded(teamNum);

        /* Store relative pointer: teamPtr - &selTeamsPtr (asm: suba.l #selTeamsPtr,a0) */
        uint16_t entryIdx = (uint16_t)(int16_t)idx;
        uint8_t *entry = a2raw + entryIdx * 0x12;
        *(uint32_t *)(entry + 0x2B4) = (uint32_t)(teamPtr - &selTeamsPtr);
    }
}

/* ================================================================
 * DIY_SortLeagueTableByStandings — multi-criteria bubble sort
 * Sort by: points > goal diff > goals scored > alphabetical name
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_SortLeagueTableByStandings(void)
{
    DIY_Competition *a4s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a4 = (uint8_t *)a4s;
    uint16_t numGroups = (uint16_t)a4s->halfTeams;
    uint16_t teamsPerGroup = (uint16_t)a4s->totalRounds;
    uint16_t groupStride = teamsPerGroup * 2;  /* word entries */
    uint16_t groupOffset = 0;

    for (int16_t g = (int16_t)numGroups - 1; g >= 0; g--) {
        uint16_t *p1 = (uint16_t *)(a4 + 0x6E + groupOffset);

        for (int16_t pass = (int16_t)teamsPerGroup - 2; pass >= 0; pass--) {
            uint16_t *p2 = p1 + 1;

            for (int16_t j = pass; j >= 0; j--) {
                uint8_t *teamA = a4 + *p1;
                uint8_t *teamB = a4 + *p2;

                /* Compare points (0x2C4) — higher is better */
                uint16_t ptsA = *(uint16_t *)(teamA + 0x2C4);
                uint16_t ptsB = *(uint16_t *)(teamB + 0x2C4);

                if (ptsA > ptsB) goto no_swap;
                if (ptsA < ptsB) goto do_swap;

                /* Points equal — compare goal difference */
                int16_t gdA = (int16_t)*(uint16_t *)(teamA + 0x2C0) -
                              (int16_t)*(uint16_t *)(teamA + 0x2C2);
                int16_t gdB = (int16_t)*(uint16_t *)(teamB + 0x2C0) -
                              (int16_t)*(uint16_t *)(teamB + 0x2C2);
                if (gdA > gdB) goto no_swap;
                if (gdA < gdB) goto do_swap;

                /* Goal diff equal — compare goals scored */
                uint16_t gfA = *(uint16_t *)(teamA + 0x2C0);
                uint16_t gfB = *(uint16_t *)(teamB + 0x2C0);
                if (gfA > gfB) goto no_swap;
                if (gfA < gfB) goto do_swap;

                /* Goals scored equal — alphabetical comparison */
                {
                    uint8_t *nameA = &selTeamNameBase + *(uint32_t *)(teamA + 0x2B4);
                    uint8_t *nameB = &selTeamNameBase + *(uint32_t *)(teamB + 0x2B4);
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
        groupOffset += groupStride;
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
    uint16_t *teamIds = (uint16_t *)table;
    uint16_t totalPlayers = count * 16;
    uint32_t totalSkill = 0;

    for (int16_t t = (int16_t)count - 1; t >= 0; t--) {
        uint8_t *team = _VerifyTeamLoaded(teamIds[t]);
        /* Read skill byte at offset 0x6C for each of 16 players, stride 0x26 (38) */
        TeamFileEx *tf = (TeamFileEx *)team;
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
    DIY_Competition *a2 = (DIY_Competition *)diyFileBufferCopy;
    uint8_t *a2raw = (uint8_t *)a2;

    a2->competitionType = 2;             /* competition type = 2 (tournament) */
    *(uint16_t *)&a2->compId = 2;       /* 0x2C: compId + field_2D as word */
    *(uint16_t *)a2->field_2 = 0;       /* 0x02 */
    a2->progressFieldA = 0;              /* 0x44 */
    a2->progressFieldC = 0;              /* 0x46 */
    a2->resultCode = (int16_t)0xFFFF;    /* 0x3A */
    a2->substitutesCount = 1;            /* 0x3C */
    a2->progressField4 = 3;              /* 0x3E */
    a2->progressField6 = 2;              /* 0x40 */
    a2->progressField8 = 8;              /* 0x42 */
    a2->field_2E = 0xFF;                 /* 0x2E */
    a2->halfTeams = 0;                   /* 0x50 */

    /* Copy "DIY TOURNAMENT" string to offset 4 */
    _StringCopy(aDiyTournament, a2raw + 4);

    a2->startMonth = 0x38;               /* start month */
    a2->endMonth = 0x20;                 /* end month */
    a2->pitchTypeOrSeason = 1;           /* 0x34 */
    a2->pitchType = (int16_t)0xFFFF;     /* 0x36 */
    a2->nonNationalCount = 3;            /* 0x4C */
    a2->field4E = 5;                     /* 0x4E */
    a2->field4A = 0;                     /* 0x4A */
    *(uint16_t *)(a2raw + 0x15A) = 0;   /* extended */

    /* Initialize 6 competition slots from init table */
    const uint16_t *initTbl = diyTournamentInitTable;
    uint8_t *slot = a2raw;

    a2->totalRounds = (int16_t)*initTbl++;   /* total teams: 0x52 */
    a2->currentRound = 0;                    /* 0x54 */
    a2->numTeams = (int16_t)*initTbl;        /* numTeams = first entry: 0x32 */

    for (int16_t s = 5; s >= 0; s--) {
        *(uint16_t *)(slot + 0x162) = *initTbl++;  /* teams in slot */
        *(uint16_t *)(slot + 0x15E) = *initTbl;    /* cup flag */
        *(uint16_t *)(slot + 0x160) = 0;
        *(uint16_t *)(slot + 0x15C) = 0;
        *(uint16_t *)(slot + 0x16A) = *initTbl++;  /* groups */
        *(uint16_t *)(slot + 0x16C) = *initTbl++;  /* teams per group */
        *(uint16_t *)(slot + 0x164) = *initTbl;    /* peek next (not consumed) */
        *(uint16_t *)(slot + 0x16E) = 1;
        *(uint16_t *)(slot + 0x170) = 1;
        *(uint16_t *)(slot + 0x172) = 1;
        *(uint16_t *)(slot + 0x168) = 2;
        *(uint16_t *)(slot + 0x166) = 1;
        slot += 0x76;
    }

    /* Initialize 64 team indices (0..63) — offset 58+0x20 = 0x5A from base = numRounds field */
    uint8_t *indices = (uint8_t *)&a2->numRounds;
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
    DIY_Competition *a0s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a0 = (uint8_t *)a0s;
    uint16_t *roundFormats = (uint16_t *)(a0 + 0x66);  /* extended */
    uint16_t *roundLegs = (uint16_t *)(a0 + 0x7E);     /* extended */
    uint16_t numRounds = (uint16_t)a0s->numRounds;

    uint16_t totalMatches = 0;
    uint16_t lastRoundHasExtraMatch = 0;

    for (int16_t r = (int16_t)numRounds - 1; r >= 0; r--) {
        totalMatches += 2;
        if (roundFormats[r] == 2) {
            /* Format 2: check round legs */
            lastRoundHasExtraMatch = 1;
            if (roundLegs[r] == 1) {
                lastRoundHasExtraMatch = 0;
                totalMatches--;
            }
        }
    }

    *(uint16_t *)(a0 + 0x334) = totalMatches;  /* extended */
    uint16_t matchCount = totalMatches - lastRoundHasExtraMatch;

    uint8_t startMonth = (uint8_t)a0s->startMonth;
    uint8_t endMonth = (uint8_t)a0s->endMonth;
    uint8_t *progTable = a0 + 0x338;  /* extended */

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
    DIY_Competition *bufs = (DIY_Competition *)diyFileBufferCopy;
    uint8_t *buf = (uint8_t *)bufs;
    uint16_t numSlots = (uint16_t)bufs->totalRounds;

    /* Sum total matches across all slots */
    uint16_t totalMatches = 0;
    uint8_t *slotPtr = buf;
    for (int16_t s = (int16_t)numSlots - 1; s >= 0; s--) {
        totalMatches += *(uint16_t *)(slotPtr + 0x174);  /* extended */
        slotPtr += 0x76;
    }

    uint8_t startMonth = (uint8_t)bufs->startMonth;
    uint8_t endMonth = (uint8_t)bufs->endMonth;
    uint8_t *progTable = buf + 0x176;

    if (totalMatches == 1) {
        progTable[0] = startMonth;
        return;
    }

    /* Ensure endMonth > startMonth for interpolation */
    uint8_t adjEndMonth = endMonth;
    if (adjEndMonth < startMonth)
        adjEndMonth += 0x60;

    uint8_t range = adjEndMonth - startMonth;
    uint16_t steps = totalMatches - 1;

    /* Walk through slots, filling progression table per-slot */
    uint8_t *slotBuf = buf - 0x76;
    uint8_t *progSlot = progTable - 0x76;
    uint16_t slotMatchesRemaining = 0;
    uint16_t matchIdx = 0;

    for (uint16_t i = 0; i <= steps; i++) {
        if (slotMatchesRemaining == 0) {
            slotBuf += 0x76;
            slotMatchesRemaining = *(uint16_t *)(slotBuf + 0x174);
            progSlot += 0x76;
            progTable = progSlot;
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
    *(uint16_t *)((DIY_Competition *)diyFileBufferCopy)->field_2 = 1;  /* DIY_COMPETITION_FIELD_2 */
    /* nullsub_12 call — no-op */
    ((DIY_Competition *)diyFileBufferCopy)->exitFlag = 0;              /* 0x58: FIELD_3A + 0x1E */
    ((DIY_Competition *)diyFileBufferCopy)->currentRound = 0;          /* 0x54: FIELD_3A + 0x1A */

    *(uint16_t *)((DIY_Competition *)diyCompetitionStart)->field_2 = 0;

    /* Recalculate match counts per slot */
    uint8_t *slotPtr = (uint8_t *)(DIY_Competition *)diyFileBufferCopy;
    uint16_t numSlots = (uint16_t)((DIY_Competition *)diyFileBufferCopy)->totalRounds;

    for (int16_t s = (int16_t)numSlots - 1; s >= 0; s--) {
        uint16_t isCup = *(uint16_t *)(slotPtr + 0x15E);  /* extended */
        if (isCup) {
            /* Cup: calculate matches from groups and teams */
            uint16_t teamsPerGroup = *(uint16_t *)(slotPtr + 0x16C);
            uint16_t halfTeams = teamsPerGroup >> 1;
            uint16_t matchPairs = *(uint16_t *)(slotPtr + 0x16C);
            uint32_t total = (uint32_t)(matchPairs - 1) * matchPairs;
            total >>= 1;
            total = total / halfTeams;
            total *= *(uint16_t *)(slotPtr + 0x166);
            *(uint16_t *)(slotPtr + 0x174) = (uint16_t)total;
        } else {
            /* League: default 2 matches */
            *(uint16_t *)(slotPtr + 0x174) = 2;
            if (*(uint16_t *)(slotPtr + 0x16E) == 2) {
                /* skip */
            } else if (*(uint16_t *)(slotPtr + 0x172) == 1) {
                *(uint16_t *)(slotPtr + 0x174) = 1;
            }
        }
        slotPtr += 0x76;
    }

    /* Rebuild matches-per-team progression table */
    _DIYTournament_BuildMatchesPerTeamProgressionTable();

    /* Compute average team strength tier */
    uint16_t numTeams = (uint16_t)((DIY_Competition *)diyFileBufferCopy)->numTeams;
    uint32_t result = _DIY_ComputeAverageTeamStrengthTier(numTeams, (uint8_t *)diyTournamentTeamIds);
    ((DIY_Competition *)diyFileBufferCopy)->avgSkill = (uint8_t)(result >> 16);      /* 0x30 */
    ((DIY_Competition *)diyFileBufferCopy)->strengthTier = (uint8_t)(result & 0xFF); /* 0x31 */
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
 * SaveDIYTournament  (SWOS2.s line 32343)
 * Copies 1450 words from competitionFileBuffer to diyFileBufferCopy.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SaveDIYTournament(void)
{
    copyWords((volatile uint16_t *)&diyFileBufferCopy,
              (const volatile uint16_t *)competitionFileBuffer, 1449);
}

/* ================================================================
 * RestoreDIYTournament  (SWOS2.s line 32354)
 * Copies 1450 words from diyFileBufferCopy to competitionFileBuffer.
 * ================================================================ */
__attribute__((used, externally_visible))
void _RestoreDIYTournament(void)
{
    copyWords((volatile uint16_t *)competitionFileBuffer,
              (const volatile uint16_t *)&diyFileBufferCopy, 1449);
}

/* ================================================================
 * DIY_CopyCompetitionFileBufferToDiyFileBufferCopy  (SWOS2.s line 32365)
 * Same as SaveDIYTournament — 1450 words competitionFileBuffer→diyFileBufferCopy.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_CopyCompetitionFileBufferToDiyFileBufferCopy(void)
{
    copyWords((volatile uint16_t *)&diyFileBufferCopy,
              (const volatile uint16_t *)competitionFileBuffer, 1449);
}

/* ================================================================
 * DIY_CopyDiyFileBufferCopyToCompetitionFileBuffer  (SWOS2.s line 32376)
 * 1450 words diyFileBufferCopy→competitionFileBuffer.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_CopyDiyFileBufferCopyToCompetitionFileBuffer(void)
{
    copyWords((volatile uint16_t *)competitionFileBuffer,
              (const volatile uint16_t *)&diyFileBufferCopy, 0x5A9);
}

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
    DIY_Competition *a2 = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a2raw = (uint8_t *)a2;

    a2->competitionType = DIY_CUP;       /* competition type = cup */
    *(uint16_t *)&a2->compId = 1;       /* 0x2C: compId + field_2D as word */
    a2->field4A = 0;                     /* 0x4A */
    a2->subRoundCounter = 1;             /* 0x5C */
    *(uint16_t *)a2->field_2 = 0;       /* 0x02 */
    a2->progressFieldA = 0;              /* 0x44 */
    a2->progressFieldC = 0;              /* 0x46 */
    a2->resultCode = (int16_t)0xFFFF;    /* 0x3A */
    a2->substitutesCount = 1;            /* 0x3C */
    a2->progressField4 = 3;              /* 0x3E */
    a2->progressField6 = 2;              /* 0x40 */
    a2->progressField8 = 8;              /* 0x42 */
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
    *(uint16_t *)(a2raw + 0x60) = 3;    /* extended field */
    *(uint16_t *)(a2raw + 0x62) = 5;    /* extended field */
    *(uint16_t *)(a2raw + 0x64) = 0;    /* extended field */

    /* Initialize 6 round config slots: legs=1 for each format field */
    uint8_t *a0 = a2raw;
    for (int16_t s = 5; s >= 0; s--) {
        *(uint16_t *)(a0 + 0x66) = 1;   /* extended */
        *(uint16_t *)(a0 + 0x72) = 1;   /* extended */
        *(uint16_t *)(a0 + 0x7E) = 1;   /* extended */
        a0 += 2;
    }

    /* Initialize 64 team indices (0..63) at offset 0x2CE */
    uint8_t *indices = a2raw + 0x2CE;
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
    DIY_Competition *a1s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a1 = (uint8_t *)a1s;
    uint8_t *orderList = a1 + 0x30E;  /* extended */
    uint16_t count = (uint16_t)a1s->halfTeams;

    /* Initialize sequential order: 0, 1, 2, ..., count-1 */
    uint8_t val = 0;
    for (int16_t i = (int16_t)count - 1; i >= 0; i--) {
        *orderList++ = val++;
    }

    /* Fisher-Yates shuffle */
    orderList = a1 + 0x30E;
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
    DIY_Competition *a2s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a2 = (uint8_t *)a2s;
    uint8_t *gridPairs = a2 + 0x2CE;  /* extended */
    uint16_t count = (uint16_t)a2s->halfTeams;
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
    if (((DIY_Competition *)diyCompetitionStart)->hasGroups == 0)
        return;
    ((DIY_Competition *)diyFileBufferCopy)->progressFieldA = ((DIY_Competition *)diyCompetitionStart)->progressFieldA;
    ((DIY_Competition *)diyFileBufferCopy)->progressFieldC = ((DIY_Competition *)diyCompetitionStart)->progressFieldC;
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
    /* Part 1: checksum entire 2000-byte scratch buffer */
    uint8_t checksum = 0;
    uint8_t *p = diyLeagueScratchBuffer;
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

    uint16_t count = *(uint16_t *)teamFileBuffer;
    uint16_t offset = *(uint16_t *)(teamFileBuffer + 4);
    uint8_t *teamData = diyLeagueScratchBuffer + offset;

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
    DIY_Competition *a6s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a6 = (uint8_t *)a6s;
    uint16_t count = (uint16_t)a6s->halfTeams;
    uint8_t temp[64];
    uint8_t *lowHalf = temp;
    uint8_t *highHalf = temp + count;
    uint8_t *grid = a6 + 0x2CE;
    uint8_t *flags = a6 + 0x8A;

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
    volatile uint8_t *vdst = a6 + 0x2CE;
    uint16_t total = count * 2;
    for (int16_t j = (int16_t)total - 1; j >= 0; j--) {
        *vdst++ = *vsrc++;
    }
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

    DIY_Competition *a0s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a0 = (uint8_t *)a0s;
    uint8_t *grid = a0 + 0x2CE;  /* extended */
    uint16_t count = (uint16_t)a0s->halfTeams;

    /* Copy grid[0..count-1] to temp (volatile to prevent memcpy optimization) */
    uint8_t temp[64];
    volatile uint8_t *vsrc = grid;
    volatile uint8_t *vdst = temp;
    for (int16_t i = (int16_t)count - 1; i >= 0; i--) {
        *vdst++ = *vsrc++;
    }

    /* Reorder: grid[i] = temp[orderList[i]] */
    uint8_t *orderList = a0 + 0x30E;
    uint8_t *dst = a0 + 0x2CE;
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
    gridByte &= 0xFF;
    uint16_t teamNum = teamIds[gridByte];
    uint8_t *teamPtr = _VerifyTeamLoaded(teamNum);
    if (teamPtr[4] != 1) {
        *(uint16_t *)(comp + 0x4C) -= 1;
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
    DIY_Competition *a6s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a6 = (uint8_t *)a6s;
    uint16_t *teamIds = (uint16_t *)(a6 + 0x24E);  /* extended */
    uint8_t *grid = a6 + 0x2CE;  /* extended */
    uint8_t *flags = a6 + 0x8A;  /* extended */

    if (a6s->hasGroups != 0) {
        /* Tournament slot path */
        uint8_t *slot = (uint8_t *)(DIY_Competition *)diyFileBufferCopy;
        uint16_t slotIdx = (uint16_t)((DIY_Competition *)diyFileBufferCopy)->currentRound;
        slot += (uint32_t)slotIdx * 0x76;

        if (*(uint16_t *)(slot + 0x15C) != 0) {  /* extended */
            /* Slot has cup flag set */
            if (!(flags[0] & 2))
                return;
            decrementMissingIfNotLoaded(a6, teamIds, (uint8_t)grid[0]);
            decrementMissingIfNotLoaded(a6, teamIds, (uint8_t)grid[1]);
            return;
        }
        if (*(uint16_t *)(slot + 0x164) == 1)
            goto mainLoop;
        if (*(uint16_t *)(slot + 0x1D2) == 0)
            goto mainLoop;
        return;
    }

mainLoop:
    {
        uint16_t count = (uint16_t)a6s->totalRounds;
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
    DIY_Competition *a2s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a2 = (uint8_t *)a2s;
    uint8_t *gridPtr = a2 + 0x2CE;            /* extended */
    uint16_t *teamIds = (uint16_t *)(a2 + 0x24E);  /* extended */
    uint16_t count = (uint16_t)a2s->numTeams;

    /* Part 1: build relative pointer cache */
    for (int16_t i = (int16_t)count - 1; i >= 0; i--) {
        int16_t gridIdx = (int8_t)*gridPtr++;   /* ext.w d6: sign-extend */
        uint16_t teamNum = teamIds[gridIdx];
        uint8_t *teamPtr = _VerifyTeamLoaded(teamNum);
        uint32_t relPtr = (uint32_t)(teamPtr - &selTeamsPtr);
        *(uint32_t *)(a2 + (uint16_t)gridIdx * 4 + 0x344) = relPtr;  /* extended */
    }

    /* Part 2: count non-national teams */
    a2s = (DIY_Competition *)diyCompetitionStart;
    a2 = (uint8_t *)a2s;
    a2s->nonNationalCount = 0;
    gridPtr = a2 + 0x2CE;              /* extended */
    teamIds = (uint16_t *)(a2 + 0x24E);  /* extended */
    count = (uint16_t)a2s->numTeams;

    for (int16_t j = (int16_t)count - 1; j >= 0; j--) {
        uint16_t gridByte = (uint8_t)*gridPtr++;  /* andi.w #$FF: zero-extend */
        uint16_t teamNum = teamIds[gridByte];
        uint8_t *teamPtr = _VerifyTeamLoaded(teamNum);
        if (teamPtr[4] != 1) {
            a2s->nonNationalCount += 1;
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

    DIY_Competition *a2s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a2 = (uint8_t *)a2s;
    uint16_t halfTeamsVal = (uint16_t)a2s->numTeams >> 1;
    a2s->halfTeams = (int16_t)halfTeamsVal;
    a2s->totalRounds = (int16_t)halfTeamsVal;

    /* Count number of rounds (log2 of halfTeams) */
    uint16_t numRoundsVal = 0;
    uint16_t temp = (uint16_t)a2s->numTeams >> 1;
    while (temp != 0) {
        numRoundsVal++;
        temp >>= 1;
    }
    a2s->numRounds = (int16_t)numRoundsVal;

    a2s->currentRound = 0;
    a2s->exitFlag = 0;
    *(uint16_t *)a2s->field_2 = 1;
    a2s->finishedFlag = 0;
    *(uint16_t *)(a2 + 0x14A) = 0;    /* extended */

    /* Get round format for current round (round 0) */
    uint16_t roundIdx = (uint16_t)a2s->exitFlag;  /* 0 */
    *(uint16_t *)(a2 + 0x14C) = *(uint16_t *)(a2 + 0x66 + roundIdx * 2);  /* extended */

    /* If only 1 team pair and replay mode != 2, clear replay */
    if (a2s->halfTeams == 1) {
        if (*(uint16_t *)(a2 + 0x14C) != 2) {
            *(uint16_t *)(a2 + 0x14C) = 0;
        }
    }

    /* Clear counters */
    *(uint16_t *)(a2 + 0x330) = 0;  /* extended */
    *(uint16_t *)(a2 + 0x32E) = 0;  /* extended */
    *(uint16_t *)(a2 + 0x336) = 0;  /* extended */
    *(uint16_t *)(a2 + 0x332) = 0;  /* extended */

    /* Clear 32 bytes of flags at offset 0x8A (volatile to prevent memset optimization) */
    volatile uint8_t *flagPtr = a2 + 0x8A;  /* extended */
    for (int16_t i = 0x1F; i >= 0; i--) {
        *flagPtr++ = 0;
    }

    /* Set random phase from start month */
    a2s->viewRandomPhase = (uint8_t)a2s->startMonth;
    viewRandomPhase = a2s->viewRandomPhase;

    /* Build bracket progression table */
    _DIY_InitCupBracketProgressionTable();

    /* Compute average team strength tier */
    uint16_t numTeams = (uint16_t)((DIY_Competition *)diyCompetitionStart)->numTeams;
    uint32_t result = _DIY_ComputeAverageTeamStrengthTier(numTeams, (uint8_t *)diyCupTeamIds);
    ((DIY_Competition *)diyCompetitionStart)->avgSkill = (uint8_t)(result >> 16);
    ((DIY_Competition *)diyCompetitionStart)->strengthTier = (uint8_t)(result & 0xFF);
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
    DIY_Competition *a2s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a2 = (uint8_t *)a2s;

    /* numTeams / matchesPerFixtureRound = groupsPerRound */
    int16_t numTeams = a2s->numTeams;
    uint16_t matchesPerRound = (uint16_t)a2s->halfTeams;
    uint16_t groupsPerRound = (uint16_t)((uint32_t)(int32_t)numTeams / matchesPerRound);
    a2s->totalRounds = (int16_t)groupsPerRound;

    *(uint16_t *)(a2 + 0x1CC) = 0;  /* extended */

    uint16_t halfGroups = groupsPerRound >> 1;
    *(uint16_t *)(a2 + 0x1C6) = halfGroups;  /* extended */

    uint16_t matchesPerHalf = halfGroups * matchesPerRound;
    *(uint16_t *)(a2 + 0x1C8) = matchesPerHalf;  /* extended */

    *(uint16_t *)(a2 + 0x1CA) = 0;  /* extended */
    *(uint16_t *)(a2 + 0x1C2) = 0;  /* extended */
    *(uint16_t *)(a2 + 0x1C0) = 0;  /* extended */

    /* totalMatchesPerSection = (groupsPerRound - 1) * groupsPerRound / 2 */
    uint16_t totalMatchesPerSection = (uint16_t)(((uint16_t)(groupsPerRound - 1) * groupsPerRound) >> 1);
    *(uint16_t *)(a2 + 0x6A) = totalMatchesPerSection;  /* extended */
    *(uint16_t *)(a2 + 0x6C) = totalMatchesPerSection;  /* extended */

    /* matchesPerSeason = totalMatchesPerSection / halfGroups * playEachTeam */
    int16_t totalSigned = (int16_t)totalMatchesPerSection;
    uint16_t matchesPerSeason = (uint16_t)((uint32_t)(int32_t)totalSigned / halfGroups);
    matchesPerSeason *= (uint16_t)a2s->playEachTeam;
    *(uint16_t *)(a2 + 0x1C4) = matchesPerSeason;  /* extended */

    *(uint16_t *)(a2 + 0x60) = (uint16_t)a2s->playEachTeam;  /* extended = 0x5E */
    *(uint16_t *)(a2 + 0x1BE) = 0;  /* extended */
    *(uint16_t *)a2s->field_2 = 1;

    a2s->viewRandomPhase = (uint8_t)a2s->startMonth;
    viewRandomPhase = a2s->viewRandomPhase;

    _CDIY_RecomputeCompetitionDerivedProgressionTable();

    uint16_t teamCount = (uint16_t)((DIY_Competition *)diyCompetitionStart)->numTeams;
    uint32_t result = _DIY_ComputeAverageTeamStrengthTier(teamCount, (uint8_t *)diyLeagueTeamIds);
    ((DIY_Competition *)diyCompetitionStart)->avgSkill = (uint8_t)(result >> 16);
    ((DIY_Competition *)diyCompetitionStart)->strengthTier = (uint8_t)(result & 0xFF);
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
    DIY_Competition *a0s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a0 = (uint8_t *)a0s;
    uint16_t d5 = (uint16_t)a0s->subRoundCounter;
    d5 = (uint16_t)(d5 * (uint16_t)a0s->totalRounds);
    uint8_t *fixturePositions = a0 + 0xEE + d5;  /* extended */

    /* Select fixture schedule: EC fixture data or general table */
    uint8_t *schedule;
    uint8_t compId = a0s->field_2E;
    if (compId == europeanChampionships[0] || compId == europeanChampionships2[0]) {
        schedule = euroChampFixtureData;
    } else {
        uint16_t groups = (uint16_t)a0s->totalRounds;
        schedule = (uint8_t *)fixtureSchedulePtrTable[groups - 2];
    }

    /* Advance to current round fixture pair */
    uint16_t roundIdx = *(uint16_t *)(a0 + 0x1CC);  /* extended */
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

    /* Home/away swap if flag bit 0 is set */
    if (a0[0x61] & 1) {  /* extended */
        *team1Out = team2;
        *team2Out = team1;
    } else {
        *team1Out = team1;
        *team2Out = team2;
    }
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
    DIY_Competition *a0s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a0 = (uint8_t *)a0s;

    /* Increment matches played for both teams */
    *(uint16_t *)(team1 + 0x2B8) += 1;
    *(uint16_t *)(team2 + 0x2B8) += 1;

    /* Reset match counter if phase-change flag set */
    if (*(uint16_t *)(a0 + 0x1C0)) {
        *(uint16_t *)(a0 + 0x1C0) = 0;
        *(uint16_t *)(a0 + 0x1CA) = 0;
    }

    *(uint16_t *)(a0 + 0x1CA) += 1;
    a0s->subRoundCounter += 1;

    if ((uint16_t)a0s->subRoundCounter != (uint16_t)a0s->halfTeams)
        return;

    /* All fixtures in this round done */
    a0s->subRoundCounter = 0;
    *(uint16_t *)(a0 + 0x1CC) += 1;
    *(uint16_t *)(a0 + 0x1C0) = 0;

    if (*(uint16_t *)(a0 + 0x1CA) == *(uint16_t *)(a0 + 0x1C8)) {
        /* Phase boundary reached */
        *(uint16_t *)(a0 + 0x1C0) = 1;
        *(uint16_t *)(a0 + 0x1C2) += 1;
        uint16_t phaseIdx = *(uint16_t *)(a0 + 0x1C2);
        a0s->viewRandomPhase = a0[0x1CE + phaseIdx];
        viewRandomPhase = a0s->viewRandomPhase;
    }

    *(uint16_t *)(a0 + 0x6C) -= 1;
    if (*(uint16_t *)(a0 + 0x6C) != 0)
        return;

    *(uint16_t *)(a0 + 0x60) -= 1;
    if (*(uint16_t *)(a0 + 0x60) == 0)
        return;

    /* Reset for next section */
    *(uint16_t *)(a0 + 0x6C) = *(uint16_t *)(a0 + 0x6A);
    *(uint16_t *)(a0 + 0x1CC) = 0;
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

    /* Follow pointer at offset 0x2B4 + selTeamsPtr base */
    uint8_t *sel1 = &selTeamsPtr + *(uint32_t *)(team1 + 0x2B4);
    uint8_t *sel2 = &selTeamsPtr + *(uint32_t *)(team2 + 0x2B4);

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

    uint8_t *comp = (uint8_t *)(DIY_Competition *)diyCompetitionStart;
    uint16_t *sortedIndices = (uint16_t *)(comp + 0x6E);  /* extended */
    uint16_t *teamIds = (uint16_t *)(comp + 0x12E);       /* extended */
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

    uint8_t *buf = (uint8_t *)(DIY_Competition *)diyFileBufferCopy;
    uint8_t *indices = (uint8_t *)&((DIY_Competition *)diyFileBufferCopy)->numRounds;  /* 0x5A */
    uint16_t *teamIds = (uint16_t *)(buf + 0x9A);  /* extended */
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
    uint8_t *a2 = (uint8_t *)(DIY_Competition *)diyFileBufferCopy;
    uint16_t offset = (uint16_t)((DIY_Competition *)diyFileBufferCopy)->currentRound;
    uint8_t *a3 = a2 + (uint32_t)offset * 0x76;
    uint8_t *a4 = (uint8_t *)(DIY_Competition *)diyCompetitionStart;
    uint8_t *a5 = (uint8_t *)(DIY_Competition *)diyCompetitionStart;

    if (*(uint16_t *)(a3 + 0x15E) != 0) {  /* extended */
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
 * Asm: a3 = dst
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_CopyCompetitionTeamIndexTable(uint8_t *dst)
{
    DIY_Competition *comps = (DIY_Competition *)diyCompetitionStart;
    uint8_t *comp = (uint8_t *)comps;
    uint16_t count = (uint16_t)comps->finishedFlag + (uint16_t)comps->numRounds;
    if (count == 0)
        return;

    uint16_t *src = (uint16_t *)(comp + 0x1AE);  /* extended */
    uint16_t *d = (uint16_t *)dst;
    for (int16_t i = (int16_t)(count - 1); i >= 0; i--) {
        *d++ = *src++;
    }
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

    uint8_t *scratch = diyLeagueScratchBuffer;
    DIY_Competition *comps = (DIY_Competition *)diyCompetitionStart;
    uint8_t *comp = (uint8_t *)comps;
    uint16_t numTeams = (uint16_t)comps->numTeams;
    uint8_t *teamData = comp + 0x2CE;  /* extended */
    const uint8_t *w = weights;

    for (int16_t i = (int16_t)(numTeams - 1); i >= 0; i--) {
        int8_t rawIdx = (int8_t)*teamData++;
        int16_t idx = (int16_t)rawIdx;
        int8_t weight = (int8_t)w[idx];

        if (weight != 0) {
            /* Look up team ID from cup team IDs */
            uint16_t teamId = diyCupTeamIds[idx];
            /* VerifyTeamLoaded returns the team pointer */
            uint8_t *team = _VerifyTeamLoaded(teamId);
            /* Add weight to scratch at country number index */
            uint16_t countryNum = *(uint16_t *)(team + 2);
            scratch[countryNum] += (uint8_t)weight;
            /* Check if this is the career team */
            if (team == (uint8_t *)&careerTeam) {
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
    DIY_Competition *comps = (DIY_Competition *)diyCompetitionStart;
    uint8_t *comp = (uint8_t *)comps;
    /* Only process for league competitions (type == 0) */
    if (comps->competitionType != 0)
        return;

    uint8_t *scratch = diyLeagueScratchBuffer;

    /* Process promoted teams (from top of table) */
    uint16_t promotedCount = (uint16_t)comps->currentRound;
    if (promotedCount != 0) {
        uint16_t *indices = (uint16_t *)(comp + 0x6E);  /* extended */
        for (int16_t i = (int16_t)(promotedCount - 1); i >= 0; i--) {
            uint16_t entryOff = *indices++;
            uint8_t *entry = comp + entryOff;
            /* Follow pointer at 0x2B4 and add selTeamsPtr base */
            uint32_t selOff = *(uint32_t *)(entry + 0x2B4);
            uint8_t *team = &selTeamsPtr + selOff;
            /* Read country number and decrement scratch */
            uint16_t countryNum = *(uint16_t *)(team + 2);
            scratch[countryNum]--;
            /* Check if career team */
            if (team == (uint8_t *)&careerTeam) {
                playPromotedAnimation = 1;
            }
        }
    }

    /* Process relegated teams (from bottom of table) */
    uint16_t relegatedCount = (uint16_t)comps->exitFlag;
    if (relegatedCount == 0)
        return;

    uint16_t numTeams = (uint16_t)comps->numTeams;
    uint16_t *bottomIndices = (uint16_t *)(comp + 0x6E) + numTeams;  /* extended */
    for (int16_t i = (int16_t)(relegatedCount - 1); i >= 0; i--) {
        uint16_t entryOff = *(--bottomIndices);
        uint8_t *entry = comp + entryOff;
        uint32_t selOff = *(uint32_t *)(entry + 0x2B4);
        uint8_t *team = &selTeamsPtr + selOff;
        uint16_t countryNum = *(uint16_t *)(team + 2);
        scratch[countryNum]++;
        if (team == (uint8_t *)&careerTeam) {
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
        if (*ptr == teamId)
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
    uint16_t *ptr = diyTournamentTeamIds;
    while (ptr != endPtr) {
        if (*ptr == teamId)
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
 * Used for extended fields (offset >= 0x60) beyond the typed DIY_Competition base.
 * Base-struct fields (0x00-0x5F) use proper struct access via DIY_Competition. */
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

    DIY_Competition *bufCopy = (DIY_Competition *)diyFileBufferCopy;
    DIY_Competition *comp = (DIY_Competition *)diyCompetitionStart;
    uint8_t *bufCopyRaw = (uint8_t *)bufCopy;
    uint8_t *compRaw = (uint8_t *)comp;

restart_loop:
    ;
    /* a3 = bufCopy + bufCopy[0x54] * 0x76 (current slot) */
    uint16_t slotIdx = (uint16_t)bufCopy->currentRound;
    uint8_t *slot = bufCopyRaw + (uint16_t)(slotIdx * 0x76);

    /* Check if slot has active flag at +0x15E */
    if (RD16(slot, 0x15E) == 0) {
        /* Cup path (slot not active / cup-type competition) */
        /* a4=comp, a5=comp */
        comp->hasGroups = 1;  /* DIY_COMPETITION_FIELD_3A+$E(a4) = 1 */

        /* Check if current round is last */
        uint16_t curRound = (uint16_t)bufCopy->currentRound;
        if ((uint16_t)(curRound + 1) == RD16U(bufCopyRaw, 0x72)) {
            comp->hasGroups = 2;
        }

        /* Copy fields from bufCopy to comp */
        comp->nonNationalCount = bufCopy->hasGroups;  /* field_3A+$12(a4) = field_3A+$E(a2) */
        comp->progressField6 = bufCopy->progressField6;  /* field_3A+6 */
        comp->progressField4 = bufCopy->progressField4;  /* field_3A+4 */
        comp->progressField8 = bufCopy->progressField8;  /* field_3A+8 */

        /* Call ShowDIYCupMenuWithParams */
        _asm_ShowDIYCupMenuWithParams(editA, editB, editC);

        /* Copy results back from comp to bufCopy */
        bufCopy->hasGroups = comp->nonNationalCount;  /* field_3A+$E(a2) = field_3A+$12(a4) */
        bufCopy->resultCode = comp->resultCode;  /* field_3A+0 */
        bufCopy->progressFieldA = comp->progressFieldA;  /* field_3A+$A */
        bufCopy->progressFieldC = comp->progressFieldC;  /* field_3A+$C */
        bufCopy->progressField6 = comp->progressField6;  /* field_3A+6 */
        bufCopy->progressField4 = comp->progressField4;  /* field_3A+4 */
        bufCopy->progressField8 = comp->progressField8;  /* field_3A+8 */

        /* sne — set byte if nonzero: if comp's finishedFlag != 0, set bufCopy's to 0xFF, else 0 */
        if (comp->finishedFlag != 0) {
            *(uint8_t *)&bufCopy->finishedFlag = 0xFF;
        } else {
            *(uint8_t *)&bufCopy->finishedFlag = 0x00;
        }

        /* Check game type */
        if (gameType == 4 || gameType == 3) {
            /* For career/contest: check if round is not yet started */
            if (comp->currentRound != 0) {
                _DIY_UpdateCompetitionProgress();
            }
            return;
        }

        /* Non-career path: check hasGroups */
        if (comp->hasGroups < 0)
            return;

        _DIY_AdvanceToNextCompetitionSlot();

        if (bufCopy->exitFlag != 0)  /* exit flag */
            return;
        goto restart_loop;
    }

    /* League/tournament path (slot IS active) */
    /* a5=comp */
    comp->hasGroups = 1;  /* DIY_COMPETITION_FIELD_3A+$E(a5) */

    /* Check if last round */
    uint16_t round = (uint16_t)bufCopy->currentRound;
    if ((uint16_t)(round + 1) == (uint16_t)bufCopy->totalRounds) {
        comp->hasGroups = 2;
    }

    /* Copy fields from bufCopy to comp */
    comp->progressField6 = bufCopy->progressField6;  /* field_3A+6 */
    comp->progressField4 = bufCopy->progressField4;  /* field_3A+4 */
    comp->progressField8 = bufCopy->progressField8;  /* field_3A+8 */

    /* Call ShowDiyLeaguesTournamentMenu */
    _ShowDiyLeaguesTournamentMenu(editA, editB, editC);

    /* Copy results back */
    bufCopy->resultCode = comp->resultCode;
    bufCopy->progressFieldA = comp->progressFieldA;
    bufCopy->progressFieldC = comp->progressFieldC;
    bufCopy->progressField6 = comp->progressField6;
    bufCopy->progressField4 = comp->progressField4;
    bufCopy->progressField8 = comp->progressField8;

    /* seq — set byte if zero: if DIY_COMPETITION_SIZE(a5) == 0, set byte to 0xFF */
    if (RD16(compRaw, 96) == 0) {  /* DIY_COMPETITION_SIZE = 96 = 0x60, extended */
        *(uint8_t *)&bufCopy->finishedFlag = 0xFF;
    } else {
        *(uint8_t *)&bufCopy->finishedFlag = 0x00;
    }

    /* Check game type */
    if (gameType == 4 || gameType == 3) {
        return;
    }

    /* Non-career: check hasGroups */
    if (comp->hasGroups < 0)
        return;

    _DIY_AdvanceToNextCompetitionSlot();

    if (bufCopy->exitFlag != 0)
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
    DIY_Competition *a0s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a0 = (uint8_t *)a0s;
    uint8_t *teamGrid = a0 + 0x2CE;      /* a3, extended */
    uint8_t *selFlags = a0 + 0x8A;       /* a2, extended */
    uint8_t *scoreTab1 = a0 + 0xCA;      /* a1, extended */
    int16_t d1 = 0;
    int16_t d2 = 0;

    while (d1 < a0s->totalRounds) {
        /* Check bit 1 of selection flags */
        if (selFlags[d1] & 0x02) {
            /* Bit 1 set — skip (already selected), just increment d2 */
            d1++;
            d2++;
            continue;
        }

        /* Swap entries at d1 and d2 in all parallel arrays */
        /* Team grid (word entries) */
        {
            int16_t idx1 = d1 * 2;
            int16_t idx2 = d2 * 2;
            int16_t tmp;

            /* teamGrid at word offsets */
            tmp = *(int16_t *)(teamGrid + idx1);
            *(int16_t *)(teamGrid + idx1) = *(int16_t *)(teamGrid + idx2);
            *(int16_t *)(teamGrid + idx2) = tmp;

            /* scoreTab1 at word offsets */
            tmp = *(int16_t *)(scoreTab1 + idx1);
            *(int16_t *)(scoreTab1 + idx1) = *(int16_t *)(scoreTab1 + idx2);
            *(int16_t *)(scoreTab1 + idx2) = tmp;

            /* scoreTab1 + 0x84 at word offsets */
            tmp = *(int16_t *)(scoreTab1 + idx1 + 0x84);
            *(int16_t *)(scoreTab1 + idx1 + 0x84) = *(int16_t *)(scoreTab1 + idx2 + 0x84);
            *(int16_t *)(scoreTab1 + idx2 + 0x84) = tmp;

            /* scoreTab1 + 0x104 at word offsets */
            tmp = *(int16_t *)(scoreTab1 + idx1 + 0x104);
            *(int16_t *)(scoreTab1 + idx1 + 0x104) = *(int16_t *)(scoreTab1 + idx2 + 0x104);
            *(int16_t *)(scoreTab1 + idx2 + 0x104) = tmp;
        }

        /* Selection flags (byte entries) */
        {
            uint8_t tmp = selFlags[d1];
            selFlags[d1] = selFlags[d2];
            selFlags[d2] = tmp;
        }

        /* Update order list if not has-groups mode */
        if (a0s->hasGroups == 0) {
            uint8_t *orderList = a0 + 0x30E;  /* a4 */
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
    /* Check finished flag — field_3A+$1C = offset 0x56 */
    if (((DIY_Competition *)diyCompetitionStart)->finishedFlag != 0)
        return;

    DIY_Competition *a0s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a0 = (uint8_t *)a0s;

    /* Check competition flow:
     * If field_2+$148 (offset 0x14A) == 1 → cup knock-out path
     * Else if $14C == 2 → 2-leg path
     */
    if (RD16(a0, 0x14A) == 1) {
        /* Fall through to knockout path */
    } else if (RD16(a0, 0x14C) == 2) {
        /* 2-leg cup path */
        goto two_leg_path;
    }

    /* Knockout / main path */
    if (RD16(a0, 0x32E) != 0) {
        /* Second leg path */
        goto second_leg_path;
    }

    /* Reorder team grid by order list */
    _DIY_ReorderTeamGridByOrderList();
    a0s = (DIY_Competition *)diyCompetitionStart;  /* reload after call */
    a0 = (uint8_t *)a0s;

    /* Check if sub-round counter needs extra increment */
    if (RD16(a0, 0x14A) != 2 && RD16(a0, 0x330) == 0) {
        int16_t roundIdx = a0s->exitFlag;
        int16_t roundTypeOfs = roundIdx * 2;
        if (RD16(a0, 0x7E + roundTypeOfs) != 1) {
            /* Add extra match counter */
            WR16(a0, 0x336, RD16(a0, 0x336) + 1);
        }
    }

    /* Advance counters */
    WR16(a0, 0x336, RD16(a0, 0x336) + 1);  /* extended */
    a0s->exitFlag += 1;

    /* Halve round size */
    {
        int16_t half = (int16_t)((uint16_t)a0s->halfTeams >> 1);
        a0s->halfTeams = half;
        a0s->totalRounds = half;
    }

    /* Reset fixture index and set round type */
    WR16(a0, 0x64, 0);  /* extended */
    {
        int16_t rndIdx = a0s->exitFlag * 2;
        WR16(a0, 0x14C, RD16(a0, 0x66 + rndIdx));  /* extended */
    }

    /* If only 1 team left and round type != 2, clear round type */
    if (a0s->halfTeams == 1 && RD16(a0, 0x14C) != 2) {
        WR16(a0, 0x14C, 0);  /* extended */
    }

    /* Clear group/second-leg flags */
    WR16(a0, 0x330, 0);  /* extended */
    WR16(a0, 0x32E, 0);  /* extended */
    a0s->currentRound = 0;
    WR16(a0, 0x14A, 0);  /* extended */

    /* Clear selection flags: fill 0x8A..0x8A+0x1F with 0 (longword stores to avoid memset) */
    {
        uint32_t *p = (uint32_t *)(a0 + 0x8A);
        p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0;
        p[4] = 0; p[5] = 0; p[6] = 0; p[7] = 0;
    }

    goto final_check;

two_leg_path:
    /* 2-leg round: advance sub-round and fixture, rotate scores */
    WR16(a0, 0x336, RD16(a0, 0x336) + 1);  /* extended */
    WR16(a0, 0x14A, RD16(a0, 0x14A) + 1);  /* extended */
    WR16(a0, 0x64, 0);  /* extended */
    a0s->currentRound = 0;

    {
        uint8_t *scores = a0 + 0xCA;     /* a1, extended */
        uint8_t *scores2 = a0 + 0x14E;   /* a2, extended */
        uint8_t *grid = a0 + 0x2CE;      /* a3, extended */
        uint8_t *flags = a0 + 0x8A;      /* extended */
        int16_t count = a0s->totalRounds - 1;

        for (int16_t i = 0; i <= count; i++) {
            flags[i] = 0x20;  /* ' ' */
            /* ror.w #8 on team grid word: swap high/low bytes */
            uint16_t gridVal = *(uint16_t *)(grid + i * 2);
            gridVal = (uint16_t)((gridVal >> 8) | (gridVal << 8));
            *(uint16_t *)(grid + i * 2) = gridVal;

            /* ror.w #8 on scores2 word: swap bytes */
            uint16_t sVal = *(uint16_t *)(scores + i * 2);
            sVal = (uint16_t)((sVal >> 8) | (sVal << 8));
            *(uint16_t *)(scores2 + i * 2) = sVal;
        }
    }
    goto final_check;

second_leg_path:
    /* Group stage second leg */
    _DIY_ReorderTeamsBySelectionFlags();
    a0s = (DIY_Competition *)diyCompetitionStart;
    a0 = (uint8_t *)a0s;

    /* Check counters for extra match increment */
    if (RD16(a0, 0x330) == 0 && RD16(a0, 0x14C) != 2) {  /* extended */
        WR16(a0, 0x336, RD16(a0, 0x336) + 1);  /* extended */
    }

    /* Set teams per round from second leg count */
    a0s->totalRounds = (int16_t)RD16(a0, 0x32E);  /* extended */

    /* If round type was 2, clear it */
    if (RD16(a0, 0x14C) == 2) {  /* extended */
        WR16(a0, 0x14C, 0);
    }

    WR16(a0, 0x14A, 0);  /* extended */
    WR16(a0, 0x330, RD16(a0, 0x330) + 1);  /* extended */
    WR16(a0, 0x64, 0);  /* extended */
    a0s->currentRound = 0;

    {
        uint8_t *grid = a0 + 0x2CE;  /* extended */
        int16_t count = a0s->totalRounds;
        uint8_t d1val = 0;

        if (RD16(a0, 0x330) != 0) {
            d1val |= 0x40;  /* bset #6 */
        }

        uint8_t *flags = a0 + 0x8A;
        for (int16_t i = 0; i < count; i++) {
            flags[i] = d1val;
            /* ror.w #8 on grid */
            uint16_t gridVal = *(uint16_t *)(grid + i * 2);
            gridVal = (uint16_t)((gridVal >> 8) | (gridVal << 8));
            *(uint16_t *)(grid + i * 2) = gridVal;
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
    DIY_Competition *a2s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a2 = (uint8_t *)a2s;

    /* Check promotion count (0x56) — if nonzero, done */
    if (a2s->finishedFlag != 0)
        return;
    /* Check sub-round counter (0x14A) */
    if (RD16(a2, 0x14A) == 1)  /* extended */
        return;

    /* Get round index and competition ID */
    int16_t roundIdx = a2s->exitFlag;
    if (a2s->hasGroups != 0) {
        /* Use diyFileBufferCopy round index */
        roundIdx = ((DIY_Competition *)diyFileBufferCopy)->currentRound;
    }
    uint8_t compId = a2s->compId;

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

    if (RD16(a2, 0x330) == 0) {  /* extended */
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

    uint8_t *teamGrid = a2 + 0x2CE;  /* a1 points here */
    int16_t numTeams = a2s->totalRounds;

    for (int16_t i = 0; i < numTeams; i++) {
        /* Read two team indices from grid */
        int8_t idx1 = (int8_t)*teamGrid++;
        int16_t off1 = (int16_t)idx1 * 4;
        uint8_t *team1 = &selTeamsPtr + RD32U(a2, 0x344 + off1);
        uint16_t teamId1 = *(uint16_t *)team1;

        int8_t idx2 = (int8_t)*teamGrid++;
        int16_t off2 = (int16_t)idx2 * 4;
        uint8_t *team2 = &selTeamsPtr + RD32U(a2, 0x344 + off2);
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
            if (RD16(a2, 0x14C) == 0) {  /* extended */
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
            if (RD16(a2, 0x14C) == 0) {  /* extended */
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
        *(uint16_t *)(a5 + 0x0C) = teamId2;
        *(uint16_t *)(a5 + 0x0E) = teamId1;
        a5 += 0x0C;

next_fixture:
        ;
    }

    /* Check if any fixtures recorded */
    if (cacheEntry[5] == 0) {
        if (a2s->nonNationalCount != 0) {
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

    if (type == 0) {
        _DIY_RecordLeagueFixtureResultAndAdvanceProgress();
    } else if (type == 1) {
        _DIY_RecordCupBracketProgress();
    } else {
        /* type == 2: hybrid */
        DIY_Competition *fbCopyS = (DIY_Competition *)diyFileBufferCopy;
        uint8_t *fbCopy = (uint8_t *)fbCopyS;
        if (fbCopyS->exitFlag != 0)
            return;
        uint16_t pitchType2 = (uint16_t)fbCopyS->currentRound;
        uint8_t *slot = fbCopy + (uint16_t)(pitchType2 * 0x76);
        if (RD16(slot, 0x15E) == 0) {
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
    DIY_Competition *a6s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a6 = (uint8_t *)a6s;

    /* Check if there are results to process */
    if (RD16(a6, 0x60) == 0)  /* extended */
        return;

    /* Find matching cache entry */
    uint8_t *cache = teamSelectionKeyCache;
    int16_t cacheCount = teamSelectionKeyCacheCount - 1;
    int16_t curRound = ((DIY_Competition *)diyFileBufferCopy)->currentRound;
    uint8_t compId = a6s->compId;

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

    /* Get selected team pointers */
    uint8_t *sel1 = &selTeamsPtr + RD32U(team1Comp, 0x2B4);
    uint8_t *sel2 = &selTeamsPtr + RD32U(team2Comp, 0x2B4);

    /* Check if team1 is human-controlled (category == 1) */
    if (sel1[4] == 1) {
        if (sel2[4] == 1) {
            /* Both human — skip recording */
            goto check_continue;
        }
        /* Check FRIENDLY string */
        if (a6s->namePtr == aFriendly || a6s->playEachTeam == 1) {
            resultPtr[0x0A] = 2;
        } else {
            resultPtr[0x0A] = 0;
        }
    } else {
        if (a6s->namePtr == aFriendly || a6s->playEachTeam == 1) {
            resultPtr[0x0A] = 1;
        } else {
            resultPtr[0x0A] = 0;
        }
    }

    cache[5]++;
    cache[6]++;
    resultPtr[0x0B] = 0;
    *(uint16_t *)(resultPtr + 0x0C) = *(uint16_t *)sel1;
    *(uint16_t *)(resultPtr + 0x0E) = *(uint16_t *)sel2;
    resultPtr += 0x0C;

check_continue:
    /* Check if league has more fixtures in this round */
    if (RD16(a6, 0x1C0) == 0)
        goto process_next;
    if (RD16(a6, 0x60) != 0) {
        /* More to process — continue searching cache */
        cache += *(uint16_t *)(cache + 2);
        cacheCount--;
        if (cacheCount >= 0) {
            if (cache[0] == compId) {
                if (a6s->hasGroups == 0 || cache[7] == (uint8_t)curRound) {
                    resultPtr = cache;
                    goto process_next;
                }
            }
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
    DIY_Competition *a2s = (DIY_Competition *)diyCompetitionStart;
    uint8_t *a2 = (uint8_t *)a2s;
    a2s->viewRandomPhase = (uint8_t)a2s->startMonth;  /* copy start month to view random phase byte */
    WR16(a2, 0x1BE, 0);  /* extended */
    a2s->subRoundCounter = 0;
    WR16(a2, 0x1CC, 0);  /* extended */
    WR16(a2, 0x1CA, 0);  /* extended */
    WR16(a2, 0x1C2, 0);  /* extended */
    WR16(a2, 0x1C0, 0);  /* extended */
    WR16(a2, 0x6C, RD16(a2, 0x6A));  /* extended */
    WR16(a2, 0x60, (int16_t)a2s->playEachTeam);  /* extended = playEachTeam */

    /* Reset per-team stats: 64 entries, stride 0x12 */
    for (int16_t i = 0; i <= 0x3F; i++) {
        WR16(a2, 0x2B8, 0);
        WR16(a2, 0x2BA, 0);
        WR16(a2, 0x2BC, 0);
        WR16(a2, 0x2BE, 0);
        WR16(a2, 0x2C0, 0);
        WR16(a2, 0x2C2, 0);
        WR16(a2, 0x2C4, 0);
        a2 += 0x12;
    }
}

/* ================================================================
 * DIY_RebuildCompetitionAndTeamSelectionCaches (SWOS2.s line 35046)
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
    uint16_t *scratchList = (uint16_t *)((uint8_t *)&diyCompetitionStart + 0x2CE);  /* extended */
    /* Actually, competitionTeamIdScratchList4 is a separate small buffer.
     * Let's use its address from asm data label. */
    extern uint16_t competitionTeamIdScratchList4[];
    uint16_t *a5 = competitionTeamIdScratchList4;

    DIY_Competition *ctxS = (DIY_Competition *)diyCompetitionContext;
    uint8_t *ctx = (uint8_t *)ctxS;
    int16_t count = ctxS->numTeams - 1;

    for (int16_t i = count; i >= 0; i--) {
        uint8_t *compPtr = (uint8_t *)tablePtr[0];
        uint32_t divIndex = tablePtr[1];
        tablePtr += 2;

        if (compPtr == (uint8_t *)currentCompetitionPtr[0] &&
            (uint16_t)divIndex == currentDivisionIndex) {
            /* Current competition — copy from competitionFileBuffer */
            /* Career_CopyCompetitionTeamByIndex inline:
             * reads divIndex from tablePtr stream, looks up team ID at
             * competition+0x1AE + idx*2 */
            uint32_t idx = *(uint32_t *)((uint8_t *)tablePtr - 4);  /* re-read d0 */
            uint8_t *dest = competitionFileBuffer;
            uint16_t teamId = *(uint16_t *)(dest + 0x1AE + (uint16_t)divIndex * 2);
            *a5++ = teamId;
        } else {
            /* Not current — auto-advance international fixtures */
            internationalAutoAdvanceFlag = 1;
            _SaveSelectedTeams();
            _asm_Career_AutoAdvanceInternationalCompetitionFixtures();
            _RestoreSelectedTeams();
            internationalAutoAdvanceFlag = 0;

            /* Copy from diyCompetitionStart */
            uint16_t teamId = *(uint16_t *)((uint8_t *)&diyCompetitionStart + 0x1AE + (uint16_t)divIndex * 2);  /* extended */
            *a5++ = teamId;
        }
    }

    /* Append teams to selection */
    a5 = competitionTeamIdScratchList4;
    uint16_t *teamIdDest = (uint16_t *)(ctx + 0x24E);  /* extended */
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
    uint16_t field54 = *(uint16_t *)(diy + 0x54);
    uint8_t *src = diy + 0x5A;
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
    uint16_t field54 = *(uint16_t *)(diy + 0x54);
    uint8_t *src = diy + 0x5A;
    uint8_t *dst = diyTeamShuffleScratch;

    if (field54 == 0) {
        /* Case 0: copy 24 teams straight */
        for (int16_t i = 23; i >= 0; i--)
            *dst++ = *src++;
    } else if (field54 == 1) {
        /* Case 1: 16-team league pattern with group lookup */
        uint8_t *groupMap = diy + 0x11A;
        uint8_t *divTable = diy + 0x66;
        uint16_t d1 = 0;
        int16_t d2 = 3;
        /* Build group bitmask from first 4 division entries */
        for (; d2 >= 0; d2--) {
            uint16_t idx = (uint8_t)(*divTable++);
            uint8_t grp = groupMap[idx];
            int16_t grpExt = (int8_t)grp;
            d1 |= (1 << grpExt);
        }
        /* Search diyGroupPatternLookup for matching bitmask */
        uint8_t *lookup = diyGroupPatternLookup;
        for (;;) {
            if ((int8_t)*lookup < 0) {
                /* End of table — panic */
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
        uint8_t *teamSrc = diy + 0x5A;
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
    if (*(uint16_t *)(diy + 0x54) != 0)
        return;

    uint16_t field164 = *(uint16_t *)(slot + 0x164);
    int32_t d3ext = (int16_t)field164;
    uint16_t teamsPerGroup = *(uint16_t *)(slot + 0x16A);
    uint16_t quotient = (uint16_t)((uint32_t)(uint16_t)d3ext / teamsPerGroup);

    uint16_t *tableOffsets = (uint16_t *)(comp + 0x6E);
    int16_t d4 = 0x2328;
    int16_t d1 = 0;

    uint16_t numGroups = *(uint16_t *)(comp + 0x50);
    uint16_t teamsInGroup = *(uint16_t *)(comp + 0x52);

    for (; d1 < (int16_t)numGroups; d1++) {
        int16_t d2 = 0;
        uint16_t *a1 = tableOffsets;
        for (; d2 < (int16_t)teamsInGroup; d2++) {
            uint16_t offset = *a1++;
            if (d2 == quotient) {
                /* Exact match: bonus 1000 */
                *(uint16_t *)(comp + offset + 0x2C4) += 0x3E8;
            } else if (d2 < quotient) {
                /* Below quotient: decreasing bonus */
                *(uint16_t *)(comp + offset + 0x2C4) += (uint16_t)d4;
                d4 -= 100;
            }
            /* d2 > quotient: no bonus */
        }
        tableOffsets += teamsInGroup;
    }

    /* Reset to 1 group, all teams in single group */
    *(uint16_t *)(comp + 0x50) = 1;
    *(uint16_t *)(comp + 0x52) = *(uint16_t *)(comp + 0x32);

    /* Sort by standings */
    _DIY_SortLeagueTableByStandings();

    /* Apply group position bonuses */
    uint8_t *groupMap = diy + 0x11A;
    int16_t d2count = 3;
    uint16_t *a1sorted = (uint16_t *)(comp + 0x86);

    for (; d2count >= 0; d2count--) {
        uint16_t offset = *a1sorted++;
        uint8_t *entry = comp + offset;
        int32_t offExt = (int32_t)offset;
        uint16_t entryIdx = (uint16_t)((uint32_t)(uint16_t)offExt / 0x12);
        int8_t grpVal = (int8_t)groupMap[entryIdx];
        int16_t grpExt = (int16_t)grpVal;
        int16_t bonus = (6 - grpExt) * 100;
        *(uint16_t *)(entry + 0x2C4) += (uint16_t)bonus;
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

    /* Check if world cup competition */
    uint8_t compNum = diy[0x2E];
    if (compNum == worldCup[0]) {
        _DIY_AdjustWorldCupBracketScores(diy, slot, comp);
        goto copy_back;
    }

    {
        uint16_t field164 = *(uint16_t *)(slot + 0x164);
        int32_t d3ext = (int16_t)field164;
        uint16_t teamsPerGroup = *(uint16_t *)(slot + 0x16A);
        uint16_t d3quot = (uint16_t)((uint32_t)(uint16_t)d3ext / teamsPerGroup);
        uint16_t d3rem;
        {
            int32_t fullDiv = (int32_t)(int16_t)field164;
            uint16_t q = (uint16_t)((uint32_t)(uint16_t)fullDiv / teamsPerGroup);
            d3rem = (uint16_t)((uint32_t)(uint16_t)fullDiv % teamsPerGroup);
            d3quot = q;
        }

        uint16_t *tableOffsets = (uint16_t *)(comp + 0x6E);
        int16_t d5 = 0x7D00;
        uint16_t numGroups = *(uint16_t *)(comp + 0x50);
        uint16_t teamsInGroup = *(uint16_t *)(comp + 0x52);

        for (int16_t d1 = (int16_t)numGroups - 1; d1 >= 0; d1--) {
            int16_t d4 = d5;
            int16_t d2 = 0;
            for (; d2 < (int16_t)teamsInGroup; d2++) {
                uint16_t offset = *tableOffsets++;
                if (d2 == d3quot) {
                    /* Exact match: bonus 2000 */
                    *(uint16_t *)(comp + offset + 0x2C4) += 0x7D0;
                } else if (d2 < d3quot) {
                    /* Below: decreasing bonus */
                    *(uint16_t *)(comp + offset + 0x2C4) += (uint16_t)d4;
                    d4 -= 0x3E8;
                }
                /* Above: no bonus */
            }
            d5 -= 100;
        }

        /* Reset to 1 group, all teams */
        *(uint16_t *)(comp + 0x50) = 1;
        *(uint16_t *)(comp + 0x52) = *(uint16_t *)(comp + 0x32);

        /* Sort */
        _DIY_SortLeagueTableByStandings();

        /* Apply group-position bonuses if there's a remainder */
        uint8_t *groupMap = diy + 0x11A;
        if (d3rem != 0) {
            uint16_t skipEntries = d3quot * teamsPerGroup;
            uint16_t *a1 = (uint16_t *)(comp + 0x6E) + skipEntries;
            uint16_t entryStride = 0x12 * *(uint16_t *)(slot + 0x16C);

            for (int16_t i = (int16_t)d3rem - 1; i >= 0; i--) {
                uint16_t offset = *a1++;
                uint8_t *entry = comp + offset;
                int32_t offExt = (int32_t)offset;
                uint16_t entryIdx = (uint16_t)((uint32_t)(uint16_t)offExt / 0x12);
                int8_t grpVal = (int8_t)groupMap[entryIdx];
                int16_t grpExt = (int16_t)grpVal;
                int16_t bonus = (8 - grpExt) * 25;
                *(uint16_t *)(entry + 0x2C4) += (uint16_t)bonus;
            }

            /* Sort again */
            _DIY_SortLeagueTableByStandings();
        }
    }

copy_back:
    {
        /* Copy sorted offsets to team order, then to team list */
        uint16_t *sortedOffsets = (uint16_t *)(comp + 0x6E);
        uint8_t *teamOrder = comp + 0xEE;
        uint16_t numTeams = *(uint16_t *)(comp + 0x32);

        for (int16_t i = (int16_t)numTeams - 1; i >= 0; i--) {
            uint16_t offset = *sortedOffsets++;
            int32_t offExt = (int32_t)offset;
            uint8_t idx = (uint8_t)((uint32_t)(uint16_t)offExt / 0x12);
            *teamOrder++ = idx;
        }

        /* Copy team order to diy team list */
        uint8_t *orderSrc = comp + 0xEE;
        uint8_t *diyDst = diy + 0x5A;
        for (int16_t i = (int16_t)numTeams - 1; i >= 0; i--)
            *diyDst++ = *orderSrc++;

        /* Check if pair swap needed */
        uint16_t legs = *(uint16_t *)(slot + 0x164);
        if (legs == 1) return;
        if (*(uint16_t *)(slot + 0x1D2) == 0) return;

        /* Swap first pair in diy team list */
        uint8_t *pairStart = diy + 0x5A;
        uint8_t tmp = pairStart[0];
        pairStart[0] = pairStart[1];
        pairStart[1] = tmp;
    }
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

    uint16_t legs = *(uint16_t *)(slot + 0x164);
    uint16_t field58 = *(uint16_t *)(diy + 0x58);

    if (!(legs == 1 && field58 == 0)) {
        _DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs();

        /* Check if pair swap needed */
        uint16_t is15C = *(uint16_t *)(slot + 0x15C);
        if (is15C != 0) {
            goto do_pair_swap;
        }
        if (legs == 1) goto check_worldcup;
        if (*(uint16_t *)(slot + 0x1D2) == 0) goto check_worldcup;

    do_pair_swap:
        {
            uint8_t *grid = comp + 0x2CE;
            uint8_t tmp = grid[0];
            grid[0] = grid[1];
            grid[1] = tmp;
        }
    }

check_worldcup:
    {
        uint8_t compNum = diy[0x2E];
        if (compNum == worldCup[0]) {
            _DIY_AdjustWorldCupBracketScores(diy, slot, comp);
        }
    }

    /* Copy team grid from comp to diy */
    {
        uint8_t *gridSrc = comp + 0x2CE;
        uint8_t *gridDst = diy + 0x5A;
        uint16_t count = *(uint16_t *)(comp + 0x32);
        if (*(uint16_t *)(slot + 0x15C) != 0)
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

    uint8_t compNum = diy[0x2E];

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
        uint16_t field54 = *(uint16_t *)(diy + 0x54);
        uint16_t field50 = *(uint16_t *)(diy + 0x50);

        if (field54 == 0 && field50 == 0)
            goto build_index_maps;

        uint16_t seedingType = *(uint16_t *)(slot + 0x160);
        if (seedingType == 0) {
            _DIY_ShuffleTeams(diy, slot);
            goto build_index_maps;
        }

        /* Seeded bracket */
        int16_t d5 = -1;
        if (field54 != 0 && *(uint16_t *)(slot + 0xE8) != 0) {
            d5 = 0;
        } else {
            _DIY_SortAndShuffleTeamsByValue(diy, slot);
        }

        /* Interleave teams into groups */
        {
            uint8_t *srcTeams = diy + 0x5A;
            uint8_t *dst = diyTeamShuffleScratch;

            if (d5 >= 0) {
                d5 = *(int16_t *)(slot + 0xF4);
            }

            uint16_t numGroupSlots = *(uint16_t *)(slot + 0x16C);
            uint16_t numGroups = *(uint16_t *)(slot + 0x16A);
            uint16_t numTeams = *(uint16_t *)(slot + 0x162);

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
            uint8_t *dst = diy + 0x5A;
            uint16_t numTeams = *(uint16_t *)(slot + 0x162);
            for (int16_t i = (int16_t)numTeams - 1; i >= 0; i--)
                *dst++ = *src++;
        }
    }

build_index_maps:
    {
        /* Build team order and offset maps */
        uint8_t *teamList = diy + 0x5A;
        uint8_t *teamOrder = comp + 0xEE;
        uint16_t *teamOffsets = (uint16_t *)(comp + 0x6E);
        uint16_t numTeams = *(uint16_t *)(slot + 0x162);

        for (int16_t i = (int16_t)numTeams - 1; i >= 0; i--) {
            uint8_t idx = *teamList++;
            *teamOrder++ = idx;
            int16_t idxExt = (int8_t)idx;
            *teamOffsets++ = (uint16_t)(idxExt * 0x12);
        }

        /* Build group map: for each team, store which group slot it's in */
        teamList = diy + 0x5A;
        uint8_t *groupMap = diy + 0x11A;
        uint16_t numGroupSlots = *(uint16_t *)(slot + 0x16C);
        uint16_t numGroups = *(uint16_t *)(slot + 0x16A);

        uint8_t grpIdx = 0;
        for (uint16_t g = 0; g < numGroups; g++) {
            for (int16_t s = (int16_t)numGroupSlots - 1; s >= 0; s--) {
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

    uint8_t compNum = diy[0x2E];

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
        uint16_t field54 = *(uint16_t *)(diy + 0x54);
        uint16_t field50 = *(uint16_t *)(diy + 0x50);

        if (field54 != 0 || field50 != 0) {
            uint16_t seedingType = *(uint16_t *)(slot + 0x160);
            if (seedingType == 0) {
                _DIY_ShuffleTeams(diy, slot);
                goto build_comp_list;
            }

            /* Seeded: check if sort needed */
            if (field54 != 0) {
                if (*(uint16_t *)(slot + 0xE8) != 0 || *(uint16_t *)(slot + 0xEA) != 0)
                    goto after_sort;
            }
            _DIY_SortAndShuffleTeamsByValue(diy, slot);

        after_sort:
            /* Random home/away swap for each pair */
            {
                uint8_t *teamList = diy + 0x5A;
                uint16_t numTeams = *(uint16_t *)(slot + 0x162);
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
                uint8_t *dst = diy + 0x5A;
                uint16_t numTeams = *(uint16_t *)(slot + 0x162);
                for (int16_t i = (int16_t)numTeams - 1; i >= 0; i--)
                    *dst++ = *src++;
            }
        }
    }

build_comp_list:
    {
        /* Copy team list to competition order list */
        uint8_t *teamList = diy + 0x5A;
        uint8_t *compOrder = comp + 0x2CE;
        uint16_t numTeams = *(uint16_t *)(slot + 0x162);
        uint16_t copyCount = numTeams;

        if (*(uint16_t *)(slot + 0x15C) != 0)
            copyCount <<= 1;

        for (int16_t i = (int16_t)copyCount - 1; i >= 0; i--)
            *compOrder++ = *teamList++;

        /* Verify all teams loaded and count missing */
        uint16_t *teamIds = (uint16_t *)(diy + 0x9A);
        *(uint16_t *)(diy + 0x48) = 0;
        teamList = diy + 0x5A;
        int16_t count = (int16_t)copyCount - 1;

        for (; count >= 0; count--) {
            uint16_t idx = (uint8_t)*teamList++;
            uint16_t teamNum = teamIds[idx];
            uint8_t *teamPtr = _VerifyTeamLoaded(teamNum);
            if (teamPtr[4] != 1) {
                *(uint16_t *)(diy + 0x48) += 1;
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
    uint8_t *a2 = diyFileBufferCopy;
    uint8_t *a3 = a2;
    uint16_t slotIdx = *(uint16_t *)(a2 + 58 + 0x1A);  /* DIY_COMPETITION_FIELD_3A + 0x1A */
    a3 += (uint32_t)slotIdx * 118;

    uint8_t *a4 = diyCompetitionStart;
    uint8_t *a5 = diyCompetitionStart;

    /* Clear the "done loading" flag */
    *(uint16_t *)(a2 + 58 + 0x1C) = 0;

    /* Check if this is a league (slot field 0x15E != 0) */
    if (*(uint16_t *)(a3 + 0x15E) != 0) {
        /* League path — init defaults, then fill from file buffer */
        _DIY_InitCompetitionDefaults();

        *(uint16_t *)(a5 + 58 + 0x0E) = 1;

        /* Copy match/score fields */
        *(uint16_t *)(a5 + 58 + 0x0A) = *(uint16_t *)(a2 + 58 + 0x0A);
        *(uint16_t *)(a5 + 58 + 0x0C) = *(uint16_t *)(a2 + 58 + 0x0C);

        /* Copy competition name */
        _StringCopy((char *)(a2 + 2 + 2), (char *)(a5 + 2 + 2));

        *(uint16_t *)(a5 + 58 + 0x10) = *(uint16_t *)(a2 + 58 + 0x1A);
        *(uint16_t *)(a5 + 58 + 0x12) = *(uint16_t *)(a2 + 58 + 0x18);
        *(uint16_t *)(a5 + 2 + 0x2A) = *(uint16_t *)(a2 + 2 + 0x2A);

        /* Num teams */
        *(uint16_t *)(a5 + 50) = *(uint16_t *)(a3 + 0x162);  /* DIY_COMPETITION_NUMTEAMS */

        /* League-specific size/progress */
        *(uint16_t *)(a5 + 58 + 0x16) = *(uint16_t *)(a3 + 362);
        uint16_t d0field = *(uint16_t *)(a3 + 0x164);
        int32_t d0ext = (int16_t)d0field;
        uint16_t divisor = *(uint16_t *)(a3 + 0x16A);
        uint16_t quotient = (uint16_t)((uint32_t)(uint16_t)d0ext / divisor);
        uint16_t remainder = (uint16_t)((uint32_t)(uint16_t)d0ext % divisor);

        *(uint16_t *)(a5 + 58 + 0x1A) = quotient;
        if (remainder != 0) {
            *(uint16_t *)(a5 + 58 + 0x1C) = 1;
        }

        /* Copy team ID table */
        {
            uint16_t *src = (uint16_t *)(a2 + 0x9A);
            uint16_t *dst = (uint16_t *)(a5 + 2 + 0x12C);
            uint16_t count = *(uint16_t *)(a2 + 50);  /* numTeams from diyFileBufferCopy */
            for (int16_t i = (int16_t)count - 1; i >= 0; i--)
                *dst++ = *src++;
        }

        /* Build team grid and index maps */
        _DIY_BuildTeamGridAndIndexMaps(a2, a3, a5, a5);

        /* Play each team & start month */
        *(uint16_t *)(a5 + 94) = *(uint16_t *)(a3 + 358);   /* PLAYEACHTEAM */
        a5[56] = (uint8_t)*(uint16_t *)(a3 + 374);           /* STARTMONTH */

        /* Recompute derived fields */
        _DIY_RecomputeCompetitionDerivedFields();

        /* Copy additional config fields */
        a5[2 + 0x2E] = a2[2 + 0x2E];
        a5[2 + 0x2F] = a2[2 + 0x2F];
        *(uint16_t *)(a5 + 52) = *(uint16_t *)(a2 + 52);    /* PITCHTYPEORSEASON */
        *(uint16_t *)(a5 + 54) = *(uint16_t *)(a2 + 54);    /* PITCHTYPE */
        a5[56] = a2[56];                                       /* STARTMONTH */
        a5[57] = a2[57];                                       /* ENDMONTH */
        *(uint16_t *)(a5 + 58 + 2) = *(uint16_t *)(a2 + 58 + 2);

        /* Extended fields from slot */
        *(uint16_t *)(a5 + 2 + 0x60) = *(uint16_t *)(a3 + 0x168);
        *(uint16_t *)(a5 + 2 + 0x62) = *(uint16_t *)(a2 + 58 + 0x12);
        *(uint16_t *)(a5 + 2 + 0x64) = *(uint16_t *)(a2 + 58 + 0x14);

        /* Copy progression month bytes */
        {
            uint8_t *src = (uint8_t *)(a3 + 374);
            uint8_t *dst = (uint8_t *)(a5 + 58 + 0x194);
            uint16_t count = *(uint16_t *)(a3 + 372);
            for (int16_t i = (int16_t)count - 1; i >= 0; i--)
                *dst++ = *src++;
        }

        a5[2 + 0x2C] = a2[2 + 0x2C];
        *(uint16_t *)(a5 + 58 + 0x14) = *(uint16_t *)(a2 + 50);
    } else {
        /* Cup path — init DIY cup, then fill from file buffer */
        _InitDIYCup();

        *(uint16_t *)(a4 + 0x48) = 1;

        /* Copy match/score fields */
        *(uint16_t *)(a4 + 58 + 0x0A) = *(uint16_t *)(a2 + 58 + 0x0A);
        *(uint16_t *)(a4 + 58 + 0x0C) = *(uint16_t *)(a2 + 58 + 0x0C);

        /* Copy competition name */
        _StringCopy((char *)(a2 + 4), (char *)(a4 + 4));

        *(uint16_t *)(a4 + 0x5A) = 1;
        *(uint16_t *)(a4 + 2 + 0x2A) = *(uint16_t *)(a2 + 2 + 0x2A);

        /* Cup-specific fields from slot */
        *(uint16_t *)(a4 + 50) = *(uint16_t *)(a3 + 0x162);   /* numTeams */
        *(uint16_t *)(a4 + 2 + 0x64) = *(uint16_t *)(a3 + 0x16E);
        *(uint16_t *)(a4 + 2 + 0x70) = *(uint16_t *)(a3 + 0x170);
        *(uint16_t *)(a4 + 2 + 0x7C) = *(uint16_t *)(a3 + 0x172);

        /* Copy team grid from file buffer */
        {
            uint16_t *src = (uint16_t *)(a2 + 58 + 0x60);
            uint16_t *dst = (uint16_t *)(a4 + 2 + 0x24C);
            uint16_t count = *(uint16_t *)(a2 + 50);
            for (int16_t i = (int16_t)count - 1; i >= 0; i--)
                *dst++ = *src++;
        }

        /* Shuffle teams and build comp team list */
        _DIY_ShuffleTeamsAndBuildCompTeamList(a2, a3, a4, a5);

        /* Start month */
        a4[56] = (uint8_t)*(uint16_t *)(a3 + 0x176);

        /* Init cup bracket state */
        _DIY_InitCupBracketState();

        /* Copy additional config fields */
        *(uint16_t *)(a4 + 58 + 0x12) = *(uint16_t *)(a2 + 58 + 0x0E);
        a4[2 + 0x2E] = a2[2 + 0x2E];
        a4[2 + 0x2F] = a2[2 + 0x2F];
        *(uint16_t *)(a4 + 52) = *(uint16_t *)(a2 + 52);    /* PITCHTYPEORSEASON */
        *(uint16_t *)(a4 + 54) = *(uint16_t *)(a2 + 54);    /* PITCHTYPE */
        a4[56] = a2[56];                                       /* STARTMONTH */
        a4[57] = a2[57];                                       /* ENDMONTH */
        *(uint16_t *)(a4 + 58 + 2) = *(uint16_t *)(a2 + 58 + 2);

        /* Cup bracket fields */
        *(uint16_t *)(a4 + 94) = *(uint16_t *)(a2 + 58 + 0x10);   /* PLAYEACHTEAM */
        *(uint16_t *)(a4 + 96) = *(uint16_t *)(a2 + 58 + 0x12);   /* DIY_COMPETITION_SIZE */
        *(uint16_t *)(a4 + 2 + 0x60) = *(uint16_t *)(a2 + 58 + 0x14);

        /* Copy progression month bytes */
        {
            uint8_t *src = (uint8_t *)(a3 + 0x176);
            uint8_t *dst = (uint8_t *)(a4 + 56 + 0x300);
            uint16_t count = *(uint16_t *)(a3 + 0x174);
            *(uint16_t *)(a4 + 52 + 0x300) = count;
            for (int16_t i = (int16_t)count - 1; i >= 0; i--)
                *dst++ = *src++;
        }

        a4[2 + 0x2C] = a2[2 + 0x2C];
        *(uint16_t *)(a4 + 58 + 0x14) = *(uint16_t *)(a2 + 50);
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
    randomSeed = currentTick;
    newRandomSeedCopy = (uint8_t)(currentTick >> 8);

    /* Clear flags in diyFileBufferCopy */
    *(uint16_t *)(diyFileBufferCopy + 58 + 0x1E) = 0;
    *(uint16_t *)(diyFileBufferCopy + 58 + 0x1A) = 0;

    /* Clear diyCompetitionStart field_2 (twice in asm, replicated) */
    *(uint16_t *)(diyCompetitionStart + 2) = 0;
    *(uint16_t *)(diyCompetitionStart + 2) = 0;

    /* Clear more flags */
    *(uint16_t *)(diyFileBufferCopy + 58 + 0x0A) = 0;
    *(uint16_t *)(diyFileBufferCopy + 58 + 0x0C) = 0;

    /* Set initial values */
    *(uint16_t *)(diyFileBufferCopy + 58) = (uint16_t)0xFFFF;
    *(uint16_t *)(diyFileBufferCopy + 2) = 1;

    /* Initialize 64-byte sequential team order */
    {
        uint8_t *dst = diyFileBufferCopy + 58 + 0x20;
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
    randomSeed = currentTick;
    newRandomSeedCopy = (uint8_t)(currentTick >> 8);

    uint8_t *a2 = diyCompetitionStart;

    /* Clear various fields */
    *(uint16_t *)(a2 + 0x44) = 0;
    *(uint16_t *)(a2 + 0x46) = 0;
    *(uint16_t *)(a2 + 0x3A) = (uint16_t)0xFFFF;
    *(uint16_t *)(a2 + 0x4A) = 0;

    /* halfTeams = numTeams / 2 */
    uint16_t numTeams = *(uint16_t *)(a2 + 0x32);
    uint16_t half = numTeams >> 1;
    *(uint16_t *)(a2 + 0x50) = half;
    *(uint16_t *)(a2 + 0x52) = half;

    /* Clear more fields */
    *(uint16_t *)(a2 + 0x64) = 0;
    *(uint16_t *)(a2 + 0x54) = 0;
    *(uint16_t *)(a2 + 0x58) = 0;
    *(uint16_t *)(a2 + 0x56) = 0;
    *(uint16_t *)(a2 + 0x14A) = 0;

    /* Set current round match count from table */
    uint16_t roundIdx = *(uint16_t *)(a2 + 0x58);
    *(uint16_t *)(a2 + 0x14C) = *(uint16_t *)(a2 + 0x66 + roundIdx * 2);

    /* Check for single-team round: if halfTeams==1 and matchCount!=2, clear matchCount */
    if (*(uint16_t *)(a2 + 0x50) == 1) {
        if (*(uint16_t *)(a2 + 0x14C) != 2) {
            *(uint16_t *)(a2 + 0x14C) = 0;
        }
    }

    /* Clear more state */
    *(uint16_t *)(a2 + 0x330) = 0;
    *(uint16_t *)(a2 + 0x32E) = 0;
    *(uint16_t *)(a2 + 0x336) = 0;
    *(uint16_t *)(a2 + 0x332) = 0;

    /* Initialize 64-byte sequential team order at 0x2CE */
    {
        uint8_t *dst = a2 + 0x2CE;
        uint8_t val = 0;
        for (int16_t i = 63; i >= 0; i--)
            *dst++ = val++;
    }

    /* Initialize 32-byte zero array at 0x8A */
    {
        uint8_t val = 0;
        uint8_t *dst = a2 + 0x8A;
        for (int16_t i = 31; i >= 0; i--)
            *dst++ = val;
    }

    /* Set random phase from start month */
    a2[0x2F] = a2[0x38];
    viewRandomPhase = a2[0x2F];

    /* Count missing teams */
    *(uint16_t *)(a2 + 0x4C) = 0;
    uint8_t *orderList = a2 + 0x2CE;
    uint16_t *teamGrid = (uint16_t *)(a2 + 0x24E);

    for (int16_t i = (int16_t)numTeams - 1; i >= 0; i--) {
        uint16_t idx = (uint8_t)*orderList++;
        uint16_t teamNum = teamGrid[idx];
        uint8_t *teamPtr = _VerifyTeamLoaded(teamNum);
        if (teamPtr[4] != 1) {
            *(uint16_t *)(a2 + 0x4C) += 1;
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
        _asm_ShowDIYCupMenuWithParams(1, 1, 0);
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
    uint16_t numTeams = *(uint16_t *)(diyCompetitionStart + 50);
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
    uint16_t numTeams = *(uint16_t *)(diyCompetitionStart + 50);
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
    /* Entry 11: diyFileBufferCopy[58+18] = diyFileBufferCopy[76] */
    _Int2Ascii(*(uint16_t *)(diyFileBufferCopy + 76), (uint8_t *)menuEntries[11].fg);
    /* Entry 13: diyFileBufferCopy[58+20] = diyFileBufferCopy[78] */
    _Int2Ascii(*(uint16_t *)(diyFileBufferCopy + 78), (uint8_t *)menuEntries[13].fg);
}

/* DesignDIYTournament_UpdateSelectTeamsPrompt — format "SELECT N TEAMS" for tournament (SWOS2.s line 50192) */
__attribute__((used, externally_visible))
void _DesignDIYTournament_UpdateSelectTeamsPrompt(void)
{
    uint8_t *str = (uint8_t *)menuEntries[92].fg;
    uint16_t numTeams = *(uint16_t *)(diyFileBufferCopy + 50);
    _PrintFormatted(aSelect0Teams, str, numTeams, 0, 0, 0, 0, 0, 0, 0, 0, 0);
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
    uint8_t *str = (uint8_t *)entry->fg;
    _InputText(25, str);
}

/* DesignDIYLeagueChangePitchType — cycle pitch type (SWOS2.s line 41557) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueChangePitchType(MenuEntry *entry)
{
    uint16_t *pitchTypeField = (uint16_t *)(diyCompetitionStart + 54);
    *pitchTypeField += 1;
    if (*pitchTypeField == 7)
        *pitchTypeField = 0xFFFF;
    _CallDrawMenuItem(entry);
}

/* DesignDIYLeagueChangeStartMonth — cycle start month (SWOS2.s line 41567) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueChangeStartMonth(MenuEntry *entry)
{
    uint8_t *monthByte = &diyCompetitionStart[56]; /* DIYLEAGUE_STARTINGMONTH */
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
    uint8_t *monthByte = &diyCompetitionStart[57]; /* DIYLEAGUE_ENDINGMONTH */
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
    uint16_t *numTeams = (uint16_t *)(diyCompetitionStart + 50);
    *numTeams += 1;
    if (*numTeams >= 25)
        *numTeams = 2;
    *(uint16_t *)&entry->fg = *numTeams;  /* write number to entry's fg union */
    _CallDrawMenuItem(entry);
}

/* SetPlayEachTeamString — format "X N" string into entry (SWOS2.s line 41622) */
__attribute__((used, externally_visible))
void _SetPlayEachTeamString(MenuEntry *entry)
{
    uint16_t playEach = *(uint16_t *)(diyCompetitionStart + 94);
    uint8_t *out = (uint8_t *)entry->fg;
    _PrintFormatted(aX0, out, playEach, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* DesignDIYLeagueChangePlayEachTeam — cycle play each team 1-10 (SWOS2.s line 41611) */
__attribute__((used, externally_visible))
void _DesignDIYLeagueChangePlayEachTeam(MenuEntry *entry)
{
    uint16_t *playEach = (uint16_t *)(diyCompetitionStart + 94);
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
    *(uint16_t *)&entry->fg = *pts;
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
    uint8_t d0 = diyCompetitionStart[56];  /* STARTMONTH */
    if (d0 >= 96)
        d0 -= 96;
    diyCupStartMonthIndex = (uint16_t)d0 / 8;

    d0 = diyCompetitionStart[57];  /* ENDMONTH */
    if (d0 >= 0x60)
        d0 -= 0x60;
    diyCupEndMonthIndex = (uint16_t)d0 / 8;

    _DesignDIYCup_UpdateMinSubstitutesAndPointsStrings();
}

/* DesignDIYCup_UpdatePitchTypeOrSeasonSelectionVisibility — toggle seasonal/pitch UI (SWOS2.s line 44776) */
__attribute__((used, externally_visible))
void _DesignDIYCup_UpdatePitchTypeOrSeasonSelectionVisibility(void)
{
    diyCupPitchTypeOrSeasonSelection = *(uint16_t *)(diyCompetitionStart + 52);

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
            uint16_t bgColor = *(uint16_t *)&e->bg;
            if ((bgColor & 0xF) != 0) {
                *(uint16_t *)&e->bg = (bgColor & 0xFFF0) | 9;  /* PINK_TO_BROWN_9 */
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
        *(uint16_t *)(diyCompetitionStart + 52) = 0;
        *(uint16_t *)(diyCompetitionStart + 54) = pitchType;

        /* Grey out entry[3] */
        uint16_t *bgColor3 = (uint16_t *)&menuEntries[3].bg;
        *bgColor3 = (*bgColor3 & 0xFFF0) | 9;  /* PINK_TO_BROWN_9 */
        menuEntries[3].isDisabled = 1;

        /* Grey out entry[4] */
        uint16_t *bgColor4 = (uint16_t *)&menuEntries[4].bg;
        *bgColor4 = (*bgColor4 & 0xFFF0) | 9;
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
        uint16_t val = *(uint16_t *)(diyCompetitionStart + 0x58);
        uint16_t startIdx = val * 5 + 0x12;
        MenuEntry *e = _CalcMenuEntryAddress(startIdx);
        for (int16_t i = 4; i >= 0; i--) {
            uint16_t *bgColor = (uint16_t *)&e->bg;
            *bgColor = (*bgColor & 0xFFF0) | 0xB;
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
    uint8_t *str = (uint8_t *)entry->fg;
    _InputText(0x19, str);
}

/* DesignDIYCupOnSelect3 — toggle pitch type / season selection (SWOS2.s line 44805) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect3(MenuEntry *entry)
{
    (void)entry;
    *(uint16_t *)(diyCompetitionStart + 52) ^= 1;
    _DesignDIYCup_UpdatePitchTypeOrSeasonSelectionVisibility();
    _CallDrawMultipleItems(5, &menuEntries[3]);
}

/* DesignDIYCupOnSelect4 — cycle pitch type 0-6, wraps to 0xFFFF (SWOS2.s line 44815) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect4(MenuEntry *entry)
{
    uint16_t *pt = (uint16_t *)(diyCompetitionStart + 54);
    *pt += 1;
    if (*pt == 7)
        *pt = 0xFFFF;
    _CallDrawMenuItem(entry);
}

/* DesignDIYCupOnSelect5 — cycle start month (SWOS2.s line 44825) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect5(MenuEntry *entry)
{
    uint8_t *monthByte = &diyCompetitionStart[56];
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
    uint8_t *monthByte = &diyCompetitionStart[57];
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
    uint16_t *playEach = (uint16_t *)(diyCompetitionStart + 0x5E);
    *playEach += 1;
    if (*playEach >= 3)
        *playEach = 0;
    _DesignDIYCupOnDraw();
    _CallDrawMenuItem(entry);
}

/* DesignDIYCupOnSelect11 — cycle number of rounds 1-6 (SWOS2.s line 44869) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect11(MenuEntry *entry)
{
    (void)entry;
    uint16_t *numRounds = (uint16_t *)(diyCompetitionStart + 0x5A);
    *numRounds += 1;
    if (*numRounds >= 7)
        *numRounds = 1;
    uint16_t r = *numRounds;
    *(uint16_t *)(diyCompetitionStart + 0x32) = (uint16_t)(1 << r);
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
    *(uint16_t *)(diyCompetitionStart + 0x66 + wordOff) ^= 3;
    _DesignDIYCupOnDraw();
    _CallDrawMenuItem(entry);
    _CallDrawMultipleItems(2, &menuEntries[8]);
}

/* DesignDIYCupOnSelect21 — cycle qualifiers per round 0-2 (SWOS2.s line 44934) */
__attribute__((used, externally_visible))
void _DesignDIYCupOnSelect21(MenuEntry *entry)
{
    uint16_t wordOff = getCupRoundWordOffset(entry);
    uint16_t *field = (uint16_t *)(diyCompetitionStart + 0x72 + wordOff);
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
    uint16_t wordOff = getCupRoundWordOffset(entry);
    uint16_t *field = (uint16_t *)(diyCompetitionStart + 0x7E + wordOff);
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
    *(uint16_t *)(diyFileBufferCopy + 0x54) = stageIdx;
    uint16_t offset = stageIdx * 0x76;
    return diyFileBufferCopy + offset;
}

/* DesignDIYTournament_UpdateMonthPageIndices — compute month page indices (SWOS2.s line 49585) */
__attribute__((used, externally_visible))
void _DesignDIYTournament_UpdateMonthPageIndices(void)
{
    uint8_t d0 = diyFileBufferCopy[56];  /* STARTMONTH */
    if (d0 >= 0x60)
        d0 -= 0x60;
    diyTournamentStartMonthPage = (uint16_t)d0 / 8;

    d0 = diyFileBufferCopy[57];  /* ENDMONTH */
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
            uint16_t bgColor = *(uint16_t *)&e->bg;
            if ((bgColor & 0xF) != 0) {
                *(uint16_t *)&e->bg = (bgColor & 0xFFF0) | 9;  /* PINK_TO_BROWN_9 */
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
    diyTournamentUseSeasonalPitchEffective = *(uint16_t *)(diyFileBufferCopy + 52);

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
    if (pitchType != 0xFFFE) {  /* SEASONAL */
        *(uint16_t *)(diyFileBufferCopy + 52) = 0;
        *(uint16_t *)(diyFileBufferCopy + 54) = pitchType;

        /* Grey out entry[3] */
        uint16_t *bgColor3 = (uint16_t *)&menuEntries[3].bg;
        *bgColor3 = (*bgColor3 & 0xFFF0) | 9;
        menuEntries[3].isDisabled = 1;

        /* Grey out entry[4] */
        uint16_t *bgColor4 = (uint16_t *)&menuEntries[4].bg;
        *bgColor4 = (*bgColor4 & 0xFFF0) | 9;
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
        uint16_t val = *(uint16_t *)(diyFileBufferCopy + 0x54);
        uint16_t startIdx = val * 0xC + 0x11;
        MenuEntry *e = _CalcMenuEntryAddress(startIdx);
        for (int16_t i = 0xB; i >= 0; i--) {  /* 12 iterations */
            if (e->background == 2) {
                uint16_t bgColor = *(uint16_t *)&e->bg;
                if ((bgColor & 0xF) != 0) {
                    *(uint16_t *)&e->bg = (bgColor & 0xFFF0) | 0xB;
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
    uint8_t *str = (uint8_t *)entry->fg;
    _InputText(0x19, str);
}

/* DesignDIYTournamentOnSelect3 — toggle pitch type / season (SWOS2.s line 49704) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect3(MenuEntry *entry)
{
    (void)entry;
    *(uint16_t *)(diyFileBufferCopy + 52) ^= 1;
    _DesignDIYTournament_UpdatePitchTypeSeasonalUI();
    _CallDrawMultipleItems(5, &menuEntries[3]);
}

/* DesignDIYTournamentOnSelect4 — cycle pitch type 0-6, wraps to 0xFFFF (SWOS2.s line 49714) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect4(MenuEntry *entry)
{
    uint16_t *pt = (uint16_t *)(diyFileBufferCopy + 54);
    *pt += 1;
    if (*pt == 7)
        *pt = 0xFFFF;
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect5 — cycle start month (SWOS2.s line 49724) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect5(MenuEntry *entry)
{
    uint8_t *monthByte = &diyFileBufferCopy[56];
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
    uint8_t *monthByte = &diyFileBufferCopy[57];
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
    uint16_t *field = (uint16_t *)(diyFileBufferCopy + 0x4A);
    *field += 1;
    if (*field >= 3)
        *field = 0;
    _DesignDIYTournamentOnDraw();
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect11 — cycle min substitutes per team (SWOS2.s line 49772) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect11(MenuEntry *entry)
{
    uint16_t maxVal = *(uint16_t *)(diyFileBufferCopy + 78);  /* FIELD_3A + $14 */
    if (maxVal == 0)
        return;
    uint16_t *curVal = (uint16_t *)(diyFileBufferCopy + 76);  /* FIELD_3A + $12 */
    *curVal += 1;
    if (*curVal > maxVal)
        *curVal = 1;
    _DesignDIYTournament_UpdateTwoNumericFieldsText();
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect13 — cycle num games per team 0-5 (SWOS2.s line 49787) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect13(MenuEntry *entry)
{
    uint16_t *field14 = (uint16_t *)(diyFileBufferCopy + 78);
    uint16_t *field12 = (uint16_t *)(diyFileBufferCopy + 76);
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

/* DesignDIYTournamentOnSelect21 — toggle stage field at +0x160 (SWOS2.s line 49806) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect21(MenuEntry *entry)
{
    uint8_t *stageBlock = getTournamentStageBlock(entry);
    *(uint16_t *)(stageBlock + 0x160) ^= 1;
    _DesignDIYTournamentOnDraw();
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect24 — cycle stage qualifiers 0-2 at +0x170 (SWOS2.s line 49829) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect24(MenuEntry *entry)
{
    uint8_t *stageBlock = getTournamentStageBlock(entry);
    uint16_t *field = (uint16_t *)(stageBlock + 0x170);
    *field += 1;
    if (*field == 3)
        *field = 0;
    _DesignDIYTournamentOnDraw();
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect25 — cycle stage byes 0-2 at +0x172 (SWOS2.s line 49841) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect25(MenuEntry *entry)
{
    uint8_t *stageBlock = getTournamentStageBlock(entry);
    uint16_t *field = (uint16_t *)(stageBlock + 0x172);
    *field += 1;
    if (*field == 3)
        *field = 0;
    _DesignDIYTournamentOnDraw();
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect26 — toggle stage field at +0x168 (SWOS2.s line 49853) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect26(MenuEntry *entry)
{
    uint8_t *stageBlock = getTournamentStageBlock(entry);
    *(uint16_t *)(stageBlock + 0x168) ^= 1;
    _DesignDIYTournamentOnDraw();
    _CallDrawMenuItem(entry);
}

/* DesignDIYTournamentOnSelect28 — cycle stage count 1-4 at +0x166 (SWOS2.s line 49861) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect28(MenuEntry *entry)
{
    uint8_t *stageBlock = getTournamentStageBlock(entry);
    uint16_t *field = (uint16_t *)(stageBlock + 0x166);
    *field += 1;
    if (*field == 5)
        *field = 1;
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
extern uint32_t diyTournamentSelectedStagePtr; /* void * stored as uint32_t */
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
 * Counts stages until one has field +0x164 == 1 (terminal stage), max 6 */
__attribute__((used, externally_visible))
void _DIYTournament_RecalculateNumStages(void)
{
    uint8_t *a0 = diyFileBufferCopy;
    uint16_t d0 = 1;
    while (d0 < 6) {
        if (*(uint16_t *)(a0 + 0x164) == 1)
            break;
        a0 += 0x76;
        d0++;
    }
    *(uint16_t *)(diyFileBufferCopy + 0x52) = d0;
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
    uint32_t ptr = (uint32_t)(uint32_t)diyFileBufferCopy + (uint32_t)stageIdx * 0x76;
    diyTournamentSelectedStagePtr = ptr;
}

/* --- _DesignDIYTournament_RedrawStageItemsFromBlock (SWOS2.s line 50114) ---
 * Converts block pointer to menu entry ordinal and redraws items.
 * blk=a0, count=d1 in asm. Computes: ordinal = ((blk-diyFileBufferCopy)/0x76)*0xC + 0x11 */
__attribute__((used, externally_visible))
void _DesignDIYTournament_RedrawStageItemsFromBlock(uint8_t *blk, uint16_t count)
{
    uint32_t offset = (uint32_t)(blk - diyFileBufferCopy);
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
    uint8_t *a2 = diyFileBufferCopy;
    uint16_t numStages = *(uint16_t *)(diyFileBufferCopy + 0x52);
    int16_t scanCount = (int16_t)numStages - 1;
    int16_t found3rdPlayoff = 0;

    /* Pre-scan: find knockout stage (isGroupStage==0) with type==2 */
    for (int16_t i = scanCount; i >= 0; i--) {
        if (*(uint16_t *)(a2 + 0x15E) == 0) {
            if (*(uint16_t *)(a2 + 0x16E) == 2) {
                found3rdPlayoff = 1;
                break;
            }
        }
        a2 += 0x76;
    }

    if (!found3rdPlayoff) {
        /* Hide entries 9 and 8 */
        menuEntries[9].isInvisible = 1;
        menuEntries[8].isInvisible = 1;
    } else {
        /* Show entries 9 and 8. Note: asm used ($2A26).w which is menuEntries[9] */
        a2 = diyFileBufferCopy;
        menuEntries[9].isInvisible = 0;
        menuEntries[8].isInvisible = 0;
    }

    /* Main loop: configure entries for each of 6 stage slots */
    MenuEntry *a4 = &menuEntries[17]; /* menuEntries+$3B8 */
    a2 = diyFileBufferCopy;
    int16_t d7 = 0;

    for (d7 = 0; d7 < 6; d7++) {
        /* Check if this slot is past the last active stage */
        if (d7 == (int16_t)numStages) {
            if (*(uint16_t *)(a2 + 0xEC) == 2)
                goto fill_remaining_invisible_1;
        }

        /* Entry pair: show both */
        a4->isInvisible = 0;
        a4++;
        a4->isInvisible = 0;

        /* Conditionally hide based on stage 5 or elimination type */
        if (d7 == 5) {
            a4->isInvisible = 1;
        } else if (*(uint16_t *)(a2 + 0x162) == 2) {
            if (*(uint16_t *)(a2 + 0x15C) == 0)
                a4->isInvisible = 1;
        }

        /* Next entry: show, set content to numTeamsInGroup */
        a4++;
        a4->isInvisible = 0;
        *(uint16_t *)&a4->fg = *(uint16_t *)(a2 + 0x162);

        /* Next entry: advance */
        a4++;

        /* Check if d7 == numStages (past active stages) */
        if (d7 == (int16_t)numStages)
            goto fill_remaining_hidden;

        /* Show entry and set stage name string */
        a4->isInvisible = 0;
        {
            uint8_t *dst = (uint8_t *)(uint32_t)a4->fg;
            if (*(uint16_t *)(a2 + 0x15E) == 0) {
                /* Knockout stage: determine round name */
                const uint8_t *name;
                uint16_t val15C = *(uint16_t *)(a2 + 0x15C);
                if (val15C != 0) {
                    name = a3rdPlayOff; /* "3RD PLAY-OFF" */
                } else {
                    uint16_t val162 = *(uint16_t *)(a2 + 0x162);
                    if (val162 == 2) {
                        name = aFinal_4; /* "FINAL" */
                    } else if (val162 == 4) {
                        name = aSemiFinal; /* "SEMI-FINAL" */
                    } else if (val162 == 8) {
                        name = aQuarterFinal; /* "QUARTER-FINAL" */
                    } else {
                        name = aKnockout; /* "KNOCKOUT" */
                    }
                }
                _StringCopy(name, dst);
            } else {
                /* Group stage: "%0 GROUPS OF %1" or "1 GROUP OF %1" */
                uint16_t numGrp = *(uint16_t *)(a2 + 0x16A);
                uint16_t grpSize = *(uint16_t *)(a2 + 0x16C);
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
        if (*(uint16_t *)(a2 + 0x160) != 0) {
            a4->fg = (uint32_t)(uint32_t)aStar;  /* "*" */
        } else {
            a4->fg = (uint32_t)(uint32_t)asc_135D9A; /* "-" */
        }

        /* Next entry: qualification description */
        a4++;
        a4->isInvisible = 0;
        {
            uint8_t *dst = (uint8_t *)(uint32_t)a4->fg;
            if (*(uint16_t *)(a2 + 0x15E) == 0) {
                /* Knockout qualification text */
                uint16_t val16E = *(uint16_t *)(a2 + 0x16E);
                if (val16E != 2) {
                    uint16_t val164 = *(uint16_t *)(a2 + 0x164);
                    if (val164 == 1) {
                        _StringCopy(aMatchWinnerWin, dst); /* "(MATCH WINNER WINS TOURNAMENT)" */
                    } else {
                        _StringCopy(aMatchWinnersQu, dst); /* "(MATCH WINNERS QUALIFY)" */
                    }
                } else {
                    uint16_t val164 = *(uint16_t *)(a2 + 0x164);
                    if (val164 == 1) {
                        _StringCopy(aMatchWinnerOnA, dst); /* "(MATCH WINNER ON AGGREGATE WINS TOURNAM..." */
                    } else {
                        _StringCopy(aMatchWinnersOn, dst); /* "(MATCH WINNERS ON AGGREGATE QUALIFY)" */
                    }
                }
            } else {
                /* Group qualification text */
                uint16_t d2 = *(uint16_t *)(a2 + 0x16A);
                uint16_t d0 = *(uint16_t *)(a2 + 0x164);
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
        if (*(uint16_t *)(a2 + 0x15E) == 0) {
            /* Knockout path: show 4 entries for legs/play/goals config */
            a4->isInvisible = 0;
            {
                /* Set number value via double-indirection: fg → ptr → write */
                uint16_t **pp = (uint16_t **)(uint32_t)a4->fg;
                **pp = *(uint16_t *)(a2 + 0x16E);
            }
            a4++;
            a4->isInvisible = 0;
            {
                uint16_t **pp = (uint16_t **)(uint32_t)a4->fg;
                **pp = *(uint16_t *)(a2 + 0x170);
            }
            a4++;
            a4->isInvisible = 0;
            {
                uint16_t **pp = (uint16_t **)(uint32_t)a4->fg;
                **pp = *(uint16_t *)(a2 + 0x172);
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
                uint16_t **pp = (uint16_t **)(uint32_t)a4->fg;
                **pp = *(uint16_t *)(a2 + 0x168);
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
                uint16_t **pp = (uint16_t **)(uint32_t)a4->fg;
                **pp = *(uint16_t *)(a2 + 0x166);
            }
            a4++;
        }

        /* Advance to next stage block */
        a2 += 0x76;
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
 * Toggle aggregate flag (+0x16E) XOR 3, then redraw stage */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect23(MenuEntry *entry)
{
    uint8_t *stageBlock = getTournamentStageBlock(entry);
    *(uint16_t *)(stageBlock + 0x16E) ^= 3;
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
    uint8_t *a3 = (uint8_t *)(uint32_t)diyTournamentSelectedStagePtr;

    uint16_t d1 = *(uint16_t *)(a3 + 0x16A);
    uint16_t d2 = *(uint16_t *)(a3 + 0x16C);
    uint16_t d6 = *(uint16_t *)(a3 + 0x164);

    /* Adjust group layout for max teams */
    uint16_t d7 = diyTournamentSelectedStageIndex1Based;
    GroupLayoutValues v = adjustGroupLayoutForMaxTeams(d1, d2, d6, d7);
    *(uint16_t *)(a3 + 0x15E) = v.numGroups;
    *(uint16_t *)(a3 + 0x16A) = v.numGroups;
    *(uint16_t *)(a3 + 0x16C) = v.groupSize;

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
    uint8_t *a3 = (uint8_t *)(uint32_t)diyTournamentSelectedStagePtr;
    uint8_t *a2 = a3 - 0x76; /* previous stage */

    if (d7 == 1) {
        /* First stage: adjust numTeams at a3+0x32 */
        diyTournamentPrevValueBackup = *(uint16_t *)(a3 + 0x32);

        for (;;) {
            uint16_t ctrl = controlMask;
            if (ctrl == 0x81 || ctrl == 0x101) {
                /* Right/down: increment */
                *(uint16_t *)(a3 + 0x32) += 1;
                if (*(uint16_t *)(a3 + 0x32) >= 0x41)
                    goto restore_first;
            } else if (ctrl == 0x41 || ctrl == 0x201) {
                /* Left/up: decrement */
                *(uint16_t *)(a3 + 0x32) -= 1;
                if (*(uint16_t *)(a3 + 0x32) == 1)
                    goto restore_first;
            } else {
                return; /* no recognized input */
            }

            /* Check if value actually changed */
            if (*(uint16_t *)(a3 + 0x32) == diyTournamentPrevValueBackup)
                return;

            /* Compute new group layout */
            {
                uint16_t candidate = *(uint16_t *)(a3 + 0x32);
                uint16_t current = *(uint16_t *)(a3 + 0x164);
                GroupLayoutResult r = computeGroupLayoutForTeams(candidate, current, d7);
                if (!r.valid)
                    continue; /* try again */

                *(uint16_t *)(a3 + 0x32) = candidate;
                *(uint16_t *)(a3 + 0x162) = *(uint16_t *)(a3 + 0x32);
                *(uint16_t *)(a3 + 0x15E) = r.numGroups;
                *(uint16_t *)(a3 + 0x16A) = r.numGroups;
                *(uint16_t *)(a3 + 0x16C) = r.groupSize;
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
            *(uint16_t *)(a3 + 0x32) = diyTournamentPrevValueBackup;
            return;
        }
    }

    /* Subsequent stages (d7 > 1): adjust pass-through count at a2+0x164 */
    diyTournamentPrevValueBackup = *(uint16_t *)(a2 + 0x164);

    for (;;) {
        uint16_t ctrl = controlMask;
        if (ctrl == 0x81 || ctrl == 0x101) {
            /* Right/down: increment */
            *(uint16_t *)(a2 + 0x164) += 1;
            uint16_t newVal = *(uint16_t *)(a2 + 0x164);
            if (newVal == *(uint16_t *)(a2 + 0x162))
                goto restore_prev;
        } else if (ctrl == 0x41 || ctrl == 0x201) {
            /* Left/up: decrement */
            *(uint16_t *)(a2 + 0x164) -= 1;
            if ((int16_t)*(uint16_t *)(a2 + 0x164) < 0)
                goto restore_prev;
            if (*(uint16_t *)(a2 + 0x164) < *(uint16_t *)(a2 + 0x16A))
                goto restore_prev;
        } else {
            return; /* no recognized input */
        }

        /* Check if value actually changed */
        if (*(uint16_t *)(a2 + 0x164) == diyTournamentPrevValueBackup)
            return;

        /* Compute new group layout for next stage */
        {
            uint16_t candidate = *(uint16_t *)(a2 + 0x164);
            uint16_t current = *(uint16_t *)(a3 + 0x164);
            GroupLayoutResult r = computeGroupLayoutForTeams(candidate, current, d7);
            if (!r.valid)
                continue; /* try again */

            *(uint16_t *)(a3 + 0x15E) = r.numGroups;
            *(uint16_t *)(a3 + 0x16A) = r.numGroups;
            *(uint16_t *)(a3 + 0x16C) = r.groupSize;

            /* Adjust previous stage's group layout if it was knockout */
            if (*(uint16_t *)(a2 + 0x15E) == 0) {
                uint16_t d1a = *(uint16_t *)(a2 + 0x16A);
                uint16_t d2a = *(uint16_t *)(a2 + 0x16C);
                uint16_t d6a = *(uint16_t *)(a2 + 0x164);
                GroupLayoutValues v = adjustGroupLayoutForMaxTeams(d1a, d2a, d6a, d7);
                *(uint16_t *)(a2 + 0x15E) = v.numGroups;
                *(uint16_t *)(a2 + 0x16A) = v.numGroups;
                *(uint16_t *)(a2 + 0x16C) = v.groupSize;
            }

            *(uint16_t *)(a2 + 0x162) = *(uint16_t *)(a2 + 0x164);
            /* Note: asm has: move.w $164(a2),$162(a3) - but the code before already
             * set a3 fields. Looking more carefully, this sets a3+0x162 = a2+0x164.
             * Wait, the asm is: move.w $164(a2),$162(a3) */
        }
        /* Actually re-read: line 50046: move.w $164(a2),$162(a3) */
        *(uint16_t *)(a3 + 0x162) = *(uint16_t *)(a2 + 0x164);

        _DIYTournament_RecalculateNumStages();
        _DesignDIYTournamentOnDraw();

        {
            uint16_t redrawCount = 2;
            if (d7 != 6 && *(uint16_t *)(a3 + 0x164) == 1)
                redrawCount = 3;
            _DesignDIYTournament_RedrawStageItemsFromBlock(a2, redrawCount);
        }
        _CallDrawMultipleItems(2, &menuEntries[8]);
        return;

restore_prev:
        *(uint16_t *)(a2 + 0x164) = diyTournamentPrevValueBackup;
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
extern uint32_t diyCupMatchTeamNamePtr1;
extern uint32_t diyCupMatchTeamNamePtr2;
extern uint16_t diyCupMatchListDrawModeFlag;
extern uint32_t diyCupLastMatchTeam1Ptr;
extern uint32_t diyCupLastMatchTeam2Ptr;
extern uint16_t diyCupLastMatchScoreTeam1;
extern uint16_t diyCupLastMatchScoreTeam2;
extern uint16_t chooseTeamsResult;
extern uint16_t seasonUpdateInProgress;
extern uint16_t fire;

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
extern uint32_t rndStringTable[];     /* const char *[15] */
extern uint32_t replayStringTable[];  /* const char *[3] */

/* ASM functions still in asm */
extern void DesignDIYCup_BuildMatchListEntryStrings(void);
extern void MenuCheckControls(void);

/* C functions from other modules */
extern void _InitCareerVariables(uint16_t gameType);
extern void _CopySelectedTeamNumbers(uint16_t count, uint8_t *dest);
extern void _InitTeams(void);
extern void _GoGetTeamsForPlay(uint8_t *gameTypeStr, uint8_t *playWhatStr,
                               uint16_t numTeams, uint16_t showClub,
                               uint16_t showNational, uint16_t showOrigCustom);

/* DesignDIYCupOnDraw — draw callback for cup design menu (SWOS2.s line 44793)
 * Iterates cup rounds, configures menu entries with round names and values. */
__attribute__((used, externally_visible))
void _DesignDIYCupOnDraw(void)
{
    uint8_t *a2 = diyCompetitionStart;
    MenuEntry *entry9 = &menuEntries[9];   /* menuEntries+$1F8 */

    /* Entry 9: show and write play-each-team value via double indirection */
    entry9->isInvisible = 0;
    uint16_t *numericPtr = *(uint16_t **)(uint32_t)entry9->fg;
    *numericPtr = *(uint16_t *)(a2 + 0x5E);

    /* Entry 11: show and write numRounds */
    MenuEntry *entry11 = &menuEntries[11]; /* menuEntries+$268 */
    entry11->isInvisible = 0;
    *(uint16_t *)&entry11->fg = *(uint16_t *)(a2 + 0x5A);

    /* Entries 18+ : loop over 6 round slots (d7 = 0,2,4,6,8,10) */
    MenuEntry *a4 = &menuEntries[18];      /* menuEntries+$3F0 */
    uint16_t d7 = 0;
    uint16_t d6 = *(uint16_t *)(a2 + 0x32);
    uint16_t d5 = 0;

    while (d7 < 0x0C) {
        uint16_t roundIdx = d7 >> 1;

        if (roundIdx < *(uint16_t *)(a2 + 0x5A)) {
            /* Active round: show entries */
            a4->isInvisible = 0;
            uint16_t d1 = *(uint16_t *)(diyCompetitionStart + 58 + 0x20); /* DIY_COMPETITION_FIELD_3A+$20 */
            uint16_t d2 = *(uint16_t *)(diyCompetitionStart + 58 + 0x22); /* DIY_COMPETITION_FIELD_3A+$22 */
            const char *roundStr = _GetNextCupRoundString(roundIdx, d1, d2);
            a4->fg = (uint32_t)roundStr;
            a4++;

            /* Number of teams in this round */
            a4->isInvisible = 0;
            *(uint16_t *)&a4->fg = d6;
            a4++;

            /* Knockout type: double indirection write */
            a4->isInvisible = 0;
            numericPtr = *(uint16_t **)(uint32_t)a4->fg;
            *numericPtr = *(uint16_t *)(a2 + 0x66 + d7);
            a4++;

            /* Check if knockout type == 2 (aggregate) */
            if (*(uint16_t *)(a2 + 0x66 + d7) == 2)
                d5 = 1;

            /* Qualifiers: double indirection write */
            a4->isInvisible = 0;
            numericPtr = *(uint16_t **)(uint32_t)a4->fg;
            *numericPtr = *(uint16_t *)(a2 + 0x72 + d7);
            a4++;

            /* Byes: double indirection write */
            a4->isInvisible = 0;
            numericPtr = *(uint16_t **)(uint32_t)a4->fg;
            *numericPtr = *(uint16_t *)(a2 + 0x7E + d7);
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
    uint16_t numTeams = *(uint16_t *)(diyCompetitionStart + 50); /* DIY_COMPETITION_NUMTEAMS */
    uint8_t *teamListPtr = diyCompetitionStart + 4; /* DIY_COMPETITION_FIELD_2 + 2 */

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
    _asm_ShowDIYCupMenuWithParams(0, 0, 0);
    _DIY_CopyCompetitionStartToCompetitionFileBuffer();
    _SetExitMenuFlag();
}

/* DesignDIYCup_UpdateRoundOptionsVisibility — toggle scroll button visibility (SWOS2.s line 45931) */
__attribute__((used, externally_visible))
void _DesignDIYCup_UpdateRoundOptionsVisibility(void)
{
    uint16_t d0 = *(uint16_t *)(diyCompetitionStart + 58 + 0x18); /* DIY_COMPETITION_FIELD_3A+$18 */

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
    uint8_t *a2 = diyCompetitionStart;
    uint8_t *dst = (uint8_t *)(uint32_t)menuEntries[0].fg;

    /* Copy competition name (at offset 4) */
    dst = _StringCopy(a2 + 4, dst);
    *dst++ = ' ';

    if (*(uint16_t *)(a2 + 0x48) != 0) {
        /* Tournament continuation path: use GetCupRoundString with diyFileBufferCopy */
        uint16_t d0 = *(uint16_t *)(diyFileBufferCopy + 58 + 0x1A); /* DIY_COMPETITION_FIELD_3A+$1A = 0x54 */
        uint16_t d1 = 0;
        uint16_t d2 = *(uint16_t *)(diyFileBufferCopy + 0x52);
        uint16_t d3 = *(uint16_t *)(a2 + 0x52) * 2;
        uint16_t d4 = *(uint16_t *)(diyFileBufferCopy + 0x15A);
        const char *roundStr = _GetCupRoundString(d0, d1, d2, d3, d4);
        dst = _StringCopy((const uint8_t *)roundStr, dst);
        /* Fall through to replay/leg check */
    } else {
        /* Direct cup path */
        uint16_t d0 = *(uint16_t *)(a2 + 0x58);

        if (diyCupMatchListDrawModeFlag != 0) {
            /* Draw mode */
            d0 += 1;
            uint16_t d1 = *(uint16_t *)(a2 + 0x5A);
            uint16_t d2 = *(uint16_t *)(a2 + 0x5C);
            const char *roundStr = _GetNextCupRoundString(d0, d1, d2);
            dst = _StringCopy((const uint8_t *)roundStr, dst);
            *dst++ = ' ';
            dst = _StringCopy(aDraw, dst);
            *dst = 0;
            return;
        }

        /* Normal mode */
        uint16_t d1 = *(uint16_t *)(a2 + 0x5A);
        uint16_t d2 = *(uint16_t *)(a2 + 0x5C);
        const char *roundStr = _GetNextCupRoundString(d0, d1, d2);
        dst = _StringCopy((const uint8_t *)roundStr, dst);
        /* Fall through to replay/leg check */
    }

    /* Check for replay suffix */
    uint16_t replayCount = *(uint16_t *)(a2 + 0x330);
    if (replayCount != 0) {
        const uint8_t *replayStr;

        if (replayCount > 3) {
            /* Many replays */
            if (*(uint16_t *)(a2 + 0x52) == 1)
                replayStr = aReplay;  /* "REPLAY" */
            else
                replayStr = aReplays; /* "REPLAYS" */
        } else {
            /* Table lookup: 1-3 replays */
            uint32_t *table;
            if (*(uint16_t *)(a2 + 0x52) == 1)
                table = &rndStringTable[12]; /* rndStringTable+$30 = element 12 */
            else
                table = replayStringTable;
            replayStr = (const uint8_t *)table[replayCount - 1];
        }

        *dst++ = ' ';
        dst = _StringCopy(replayStr, dst);
        *dst = 0;
        return;
    }

    /* Check for leg suffix */
    if (*(uint16_t *)(a2 + 0x14A) == 1) {
        *dst++ = ' ';
        dst = _StringCopy(a2ndLeg, dst); /* "2ND LEG" */
    } else if (*(uint16_t *)(a2 + 0x14C) == 2) {
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
    uint8_t *base = diyCompetitionStart;
    uint16_t idx = *(uint16_t *)(base + 0x64);
    uint16_t idx2 = idx * 2;
    int8_t slot1 = (int8_t)base[0x2CE + idx2];
    int8_t slot2 = (int8_t)base[0x2CE + idx2 + 1];
    uint8_t *a1 = base + (int16_t)slot1 * 4;
    uint8_t *a2 = base + (int16_t)slot2 * 4;

    MenuEntry *e = &menuEntries[11]; /* menuEntries+$268 */

    /* Entry 11: "V" separator, visible */
    e->isInvisible = 0;
    e->fg = (uint32_t)aV;
    e++;

    /* Entry 12: team 1 name, visible */
    e->isInvisible = 0;
    uint8_t *teamName1 = (uint8_t *)((uint32_t)*(uint32_t *)(a1 + 0x344) + (uint32_t)&selTeamNameBase);
    e->fg = (uint32_t)teamName1;
    e++;

    /* Entry 13: team 2 name, visible */
    e->isInvisible = 0;
    uint8_t *teamName2 = (uint8_t *)((uint32_t)*(uint32_t *)(a2 + 0x344) + (uint32_t)&selTeamNameBase);
    e->fg = (uint32_t)teamName2;
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
        DesignDIYCup_BuildMatchListEntryStrings();
        return 1;
    }

    /* Calculate target scroll position */
    uint16_t d0 = 0;
    if (diyCupMenuArg0Copy != 0) {
        uint16_t totalMatches = *(uint16_t *)(diyCompetitionStart + 58 + 0x18); /* DIY_COMPETITION_FIELD_3A+$18 */
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
        uint16_t maxScroll = *(uint16_t *)(diyCompetitionStart + 58 + 0x18) - 6;
        if (diyCupMatchListScrollIndex == maxScroll)
            break;
        diyCupMatchListScrollIndex += 1;
        changed = 1;
    }

    if (changed != 0) {
        DesignDIYCup_BuildMatchListEntryStrings();
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
    uint8_t *team1 = (uint8_t *)(uint32_t)diyCupLastMatchTeam1Ptr;
    uint8_t *team2 = (uint8_t *)(uint32_t)diyCupLastMatchTeam2Ptr;
    uint16_t score1 = diyCupLastMatchScoreTeam1;
    uint16_t score2 = diyCupLastMatchScoreTeam2;

    MenuEntry *e = &menuEntries[11]; /* ($2A96).w = menuEntries+$268 = entry 11 */

    /* Entry 11: "-" separator */
    e->isInvisible = 0;
    e->fg = (uint32_t)aMinus;
    e++;

    /* Entry 12: team 1 name (offset 5 into team record) */
    e->fg = (uint32_t)(team1 + 5);
    e->isInvisible = 0;
    e++;

    /* Entry 13: team 2 name */
    e->fg = (uint32_t)(team2 + 5);
    e->isInvisible = 0;
    e++;

    /* Entry 14: team 1 score */
    *(uint16_t *)&e->fg = score1;
    e->isInvisible = 0;
    e++;

    /* Entry 15: team 2 score */
    *(uint16_t *)&e->fg = score2;
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
extern void DesignDIYCup_UpdateMenuEntriesVisibilityAndSelection(void);

/* DIYSetMonthsOrPitchTypeEntries — toggle pitch type / month entry visibility (SWOS2.s line 41515) */
__attribute__((used, externally_visible))
void _DIYSetMonthsOrPitchTypeEntries(void)
{
    diyPitchTypeOrSeason_1 = *(uint16_t *)(diyCompetitionStart + 52); /* PITCHTYPEORSEASON */

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
    uint8_t startVal = diyCompetitionStart[56];
    if (startVal >= 96)
        startVal -= 96;
    diyLeagueStartingMonth = (uint16_t)startVal / 8;

    /* End month: byte at offset 57 */
    uint8_t endVal = diyCompetitionStart[57];
    if (endVal >= 96)
        endVal -= 96;
    diyLeagueEndingMonth = (uint16_t)endVal / 8;

    /* Entry 13: number of teams */
    *(uint16_t *)&menuEntries[13].fg = *(uint16_t *)(diyCompetitionStart + 50);

    /* Entry 14: play-each-team string */
    _SetPlayEachTeamString(&menuEntries[14]);

    /* Entry 15: points for a win */
    *(uint16_t *)&menuEntries[15].fg = *(uint16_t *)diyPointsForAWin;

    _ConvertMinMaxSubstitutesNumbers();
}

/* DIYLeagueOKSelected — gray out entries 3-19, hide/show OK/cancel (SWOS2.s line 41450) */
__attribute__((used, externally_visible))
void _DIYLeagueOKSelected(void)
{
    MenuEntry *e = &menuEntries[3];
    for (int16_t i = 0x10; i >= 0; i--) { /* 17 iterations: entries 3..19 */
        if (e->background == 2) {
            uint16_t bgVal = *(uint16_t *)&e->bg;
            if ((bgVal & 0xF) != 0) {
                bgVal = (bgVal & 0xFFF0) + 9; /* PINK_TO_BROWN_9 */
                *(uint16_t *)&e->bg = bgVal;
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
    uint8_t *a6 = diyCompetitionStart;
    MenuEntry *e18 = &menuEntries[18];
    uint8_t *outBuf18 = (uint8_t *)(uint32_t)e18->fg;

    uint8_t compType = a6[302]; /* offset $12E */

    if (compType >= 80 && compType <= 85) {
        /* Winner/qualifier format */
        uint16_t numQualify = *(uint16_t *)(a6 + 84);
        if (numQualify == 1)
            _PrintFormatted(aWinnerQualifie, outBuf18, numQualify, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        else
            _PrintFormatted(aTop0Qualify, outBuf18, numQualify, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        e18->isInvisible = 0;
    } else {
        /* Promoted format */
        uint16_t numPromoted = *(uint16_t *)(a6 + 84);
        uint16_t numPlayoffs = *(uint16_t *)(a6 + 86);
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
    uint8_t *outBuf19 = (uint8_t *)(uint32_t)e19->fg;
    uint16_t numRelegated = *(uint16_t *)(a6 + 88);
    uint16_t numRelPlayoffs = *(uint16_t *)(a6 + 90);

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
        *(uint16_t *)(diyCompetitionStart + 52) = 0; /* pitchTypeOrSeason = 0 */
        *(uint16_t *)(diyCompetitionStart + 54) = pitchType; /* pitchType */

        /* Disable and gray out entry 8 */
        MenuEntry *e8 = &menuEntries[8];
        uint16_t bg8 = *(uint16_t *)&e8->bg;
        bg8 = (bg8 & 0xFFF0) + 9; /* PINK_TO_BROWN_9 */
        *(uint16_t *)&e8->bg = bg8;
        e8->isDisabled = 1;

        /* Disable and gray out entry 9 */
        MenuEntry *e9 = &menuEntries[9];
        uint16_t bg9 = *(uint16_t *)&e9->bg;
        bg9 = (bg9 & 0xFFF0) + 9;
        *(uint16_t *)&e9->bg = bg9;
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
    uint16_t numTeams = *(uint16_t *)(diyCompetitionStart + 50);
    uint8_t *teamListPtr = diyCompetitionStart + 4; /* FIELD_2 + 2 */

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

/* ShowDIYCupMenuWithParams — store params, show cup menu (SWOS2.s line 43290) */
__attribute__((used, externally_visible))
void _ShowDIYCupMenuWithParams(uint16_t arg0, uint16_t arg1, uint16_t arg2)
{
    writeOnlyVar01 = 1;
    diyCupMenuArg0 = arg0;
    diyCupMenuArg0Copy = arg0;
    diyCupMenuArg1 = arg1;
    diyCupMenuArg2 = arg2;

    if (seasonUpdateInProgress != 0) {
        do {
            MenuCheckControls();
        } while (fire != 0);
    }

    _ShowMenu(diyCupMenu);
    writeOnlyVar01 = 0;
}

/* DIYCupFinish — finalize cup menu display (SWOS2.s line 45807) */
__attribute__((used, externally_visible))
void _DIYCupFinish(void)
{
    uint16_t off74 = *(uint16_t *)(diyCompetitionStart + 74); /* FIELD_3A+$10 */

    if (off74 == 6 || diyCupMenuArg0Copy == 0) {
        _DesignDIYCup_HideCurrentMatchTeamEntries();
    } else if ((int16_t)diyCupMenuArg0Copy < 0) {
        _DesignDIYCup_UpdateCurrentMatchResultHeaderEntries();
    } else {
        _DesignDIYCup_UpdateCurrentMatchTeamEntryStrings();
    }

    _DesignDIYCup_UpdateRoundOptionsVisibility();
    DesignDIYCup_UpdateMenuEntriesVisibilityAndSelection();
    DesignDIYCup_BuildMatchListEntryStrings();
    _DesignDIYCup_UpdateMatchListScrollAndRebuildStrings();
    _DesignDIYCup_BuildMenuTitleString();
}

/* DIYCupInit — init cup menu state, then fall through to finish (SWOS2.s line 45770) */
__attribute__((used, externally_visible))
void _DIYCupInit(void)
{
    diyCupMatchListScrollIndex = 0;

    if (diyCupMenuArg0 != 0) {
        *(uint16_t *)(diyCompetitionStart + 74) = 0; /* FIELD_3A+$10 */
        if (*(uint16_t *)(diyCompetitionStart + 84) != 0) { /* FIELD_3A+$1A */
            *(uint16_t *)(diyCompetitionStart + 74) = 6;
            diyCupMenuArg0Copy = 0;
        }
    }

    if (*(uint16_t *)(diyCompetitionStart + 86) != 0) { /* FIELD_3A+$1C */
        if (*(uint16_t *)(diyCompetitionStart + 72) == 0) { /* FIELD_3A+$E */
            diyCupMenuArg0Copy = 0;
            *(uint16_t *)(diyCompetitionStart + 74) = 6;
        }
    }

    if (diyCupMenuArg1 != 0) {
        diyCupMenuArg0Copy = 0;
        _SetCurrentEntry(10);
    }

    *(uint16_t *)(diyCompetitionStart + 2) = 1; /* FIELD_2 */
    diyCupMatchListDrawModeFlag = 0;

    if (seasonUpdateInProgress != 0) {
        diyCupMenuArg0Copy = 0;
        _SetCurrentEntry(0x42); /* 66 */
        menuEntries[9].isInvisible = 1;
        menuEntries[10].isInvisible = 1;
    }

    /* Original asm falls through to DIYCupFinish via bra.w *+4 */
    _DIYCupFinish();
}

/* DIYCupOnSelect10 — cup select/play callback (SWOS2.s line 45845) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect10(void)
{
    uint8_t *a0 = diyCompetitionStart;
    uint16_t off72 = *(uint16_t *)(a0 + 72);  /* FIELD_3A+$E */
    uint16_t off86 = *(uint16_t *)(a0 + 86);  /* FIELD_3A+$1C */

    if (off72 != 0) {
        if (!(off72 == 2 && off86 != 0)) {
            /* off72 != 0 and not the (==2 && has replays) exit-only case */
            *(uint16_t *)(a0 + 72) = 0xFFFF;
        }
    } else {
        /* off72 == 0 */
        if (off86 != 0)
            *(uint16_t *)(a0 + 2) = 2;
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
    DesignDIYCup_BuildMatchListEntryStrings();
    _DrawMenu();
}

/* DIYCupOnSelect17 — scroll match list down (SWOS2.s line 45889) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect17(void)
{
    uint16_t d0 = diyCupMatchListScrollIndex + 6;
    uint16_t d1 = *(uint16_t *)(diyCompetitionStart + 82); /* FIELD_3A+$18 */

    if (diyCupMatchListDrawModeFlag != 0)
        d1 >>= 1;

    if (d0 >= d1)
        return;

    diyCupMatchListScrollIndex++;
    DesignDIYCup_BuildMatchListEntryStrings();
    _DrawMenu();
}

/* ==========================================================================
 * Chunk 132 — DIYLeagueChangePitchTypeOrSeason, DIYCupOnSelect1/3-8/Common,
 *             DesignDIYTournamentOnSelect89 (10 functions)
 * ========================================================================== */

extern uint16_t _asm_DIY_PlayCupMatchAndAdvance(uint16_t mode);
extern uint8_t aPlayTournament[]; /* "PLAY TOURNAMENT" */

/* DIYLeagueChangePitchTypeOrSeason — toggle pitchTypeOrSeason, redraw (SWOS2.s line 41373) */
__attribute__((used, externally_visible))
void _DIYLeagueChangePitchTypeOrSeason(void)
{
    *(uint16_t *)(diyCompetitionStart + 52) ^= 1; /* eori.w #1 */
    _DIYSetMonthsOrPitchTypeEntries();
    _CallDrawMultipleItems(5, &menuEntries[8]); /* entries 8-12 */
}

/* DIYCupOnSelectCommon — play cup match and update state (SWOS2.s line 46119)
 * Called from DIYCupOnSelect3-8 with mode parameter. */
static void DIYCupOnSelectCommonC(uint16_t mode)
{
    uint16_t result = _asm_DIY_PlayCupMatchAndAdvance(mode);
    if (result != 0)
        return;

    /* Match results already stored in globals by bridge */
    diyCupMenuArg0Copy = 0xFFFF;

    uint8_t *a0 = diyCompetitionStart;
    uint16_t off54 = *(uint16_t *)(a0 + 0x54);  /* hasReplays? */

    if (off54 == 0)
        goto state_check;

    if (*(uint16_t *)(a0 + 0x48) != 0) {  /* field_3A+$E */
        if (diyCupMenuArg0 != 0)
            goto state6;
        goto state_check_replays;
    }

    /* off48 == 0 */
state_check_replays:
    if (*(uint16_t *)(a0 + 0x14A) != 0)
        goto check_32E;
    if (*(uint16_t *)(a0 + 0x14C) != 2)
        goto check_32E;
    if (diyCupMenuArg0 != 0)
        goto state6;
    *(uint16_t *)(a0 + 0x4A) = 1;
    goto state_check;

check_32E:
    if (*(uint16_t *)(a0 + 0x32E) != 0)
        goto check_32E_nonzero;

    if (*(uint16_t *)(diyCompetitionStart + 72) != 0) {  /* FIELD_3A+$E */
        /* state 5 */
        *(uint16_t *)(a0 + 0x4A) = 5;
        if (*(uint16_t *)(a0 + 0x48) == 2)
            goto final_draw;
        goto state_check;
    }

    if (*(uint16_t *)(a0 + 0x330) != 0)
        goto state_check_finished;

    {
        uint16_t roundIdx = *(uint16_t *)(a0 + 0x58);
        if (roundIdx + 1 == *(uint16_t *)(a0 + 0x5A))
            goto state_check_finished;
    }
    goto state2;

check_32E_nonzero:
    if (*(uint16_t *)(a0 + 0x330) != 0)
        goto state3;
    if (*(uint16_t *)(diyCompetitionStart + 72) != 0)  /* FIELD_3A+$E */
        goto state3;
    {
        uint16_t roundIdx = *(uint16_t *)(a0 + 0x58);
        if (roundIdx + 1 != *(uint16_t *)(a0 + 0x5A))
            goto state2;
    }
    if (diyCupMenuArg0 != 0)
        goto state6;

state3:
    *(uint16_t *)(a0 + 0x4A) = 3;
    goto state_check;

state2:
    *(uint16_t *)(a0 + 0x4A) = 2;
    goto state_check;

state_check_finished:
    if (diyCupMenuArg0 != 0)
        goto state6;
    *(uint16_t *)(a0 + 0x4A) = 4;
    goto state_check;

state6:
    *(uint16_t *)(a0 + 0x4A) = 6;
    goto state_check;

state_check:
    if (*(uint16_t *)(a0 + 0x56) != 0)
        *(uint16_t *)(a0 + 0x4A) = 6;

final_draw:
    _DesignDIYCup_UpdateCurrentMatchResultHeaderEntries();
    _DesignDIYCup_UpdateMatchListScrollAndRebuildStrings();
    DesignDIYCup_UpdateMenuEntriesVisibilityAndSelection();
    DesignDIYCup_BuildMatchListEntryStrings();
    _DrawMenu();
}

/* DIYCupOnSelect3 — cup round select callback (SWOS2.s line 46078) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect3(MenuEntry *entry)
{
    *(uint16_t *)(diyCompetitionStart + 62) = entry->ordinal; /* FIELD_3A+4 */
    DIYCupOnSelectCommonC(2);
}

/* DIYCupOnSelect4 — cup round select callback (SWOS2.s line 46085) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect4(MenuEntry *entry)
{
    *(uint16_t *)(diyCompetitionStart + 62) = entry->ordinal;
    DIYCupOnSelectCommonC(3);
}

/* DIYCupOnSelect5 — cup round select callback (SWOS2.s line 46092) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect5(MenuEntry *entry)
{
    *(uint16_t *)(diyCompetitionStart + 62) = entry->ordinal;
    DIYCupOnSelectCommonC(0);
}

/* DIYCupOnSelect6 — cup round select callback (SWOS2.s line 46099) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect6(MenuEntry *entry)
{
    *(uint16_t *)(diyCompetitionStart + 66) = entry->ordinal; /* FIELD_3A+8 */
    DIYCupOnSelectCommonC(1);
}

/* DIYCupOnSelect7 — cup round select callback (SWOS2.s line 46106) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect7(MenuEntry *entry)
{
    *(uint16_t *)(diyCompetitionStart + 66) = entry->ordinal;
    DIYCupOnSelectCommonC(3);
}

/* DIYCupOnSelect8 — cup round select callback (SWOS2.s line 46113) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect8(MenuEntry *entry)
{
    *(uint16_t *)(diyCompetitionStart + 66) = entry->ordinal;
    DIYCupOnSelectCommonC(0);
}

/* DIYCupOnSelect1 — cup state dispatch (SWOS2.s line 45634) */
__attribute__((used, externally_visible))
void _DIYCupOnSelect1(void)
{
    uint8_t *a0 = diyCompetitionStart;
    uint16_t state;

    diyCupMatchListDrawModeFlag = 0;
    state = *(uint16_t *)(a0 + 0x4A);

    switch (state) {
    case 1:
        *(uint16_t *)(a0 + 0x4A) = 0;
        _DIY_UpdateCompetitionProgress();
        break;

    case 2:
        diyCupMatchListDrawModeFlag = 1;
        _DesignDIYCup_UpdateRoundOptionsVisibility();
        if (diyCupMenuArg0 != 0) {
            *(uint16_t *)(a0 + 0x4A) = 6;
        } else if (*(uint16_t *)(a0 + 0x32E) != 0) {
            *(uint16_t *)(a0 + 0x4A) = 3;
        } else {
            *(uint16_t *)(a0 + 0x4A) = 4;
        }
        break;

    case 3:
        *(uint16_t *)(a0 + 0x4A) = 0;
        _DIY_UpdateCompetitionProgress();
        break;

    case 4:
        *(uint16_t *)(a0 + 0x4A) = 0;
        _DIY_UpdateCompetitionProgress();
        break;

    case 5:
        *(uint16_t *)(a0 + 2) = 2;
        _SetExitMenuFlag();
        return;

    default:
        /* State >= 6 or 0: loc_134806 path */
        diyCupMenuArg0Copy = 1;
        _DesignDIYCup_UpdateMatchListAndRedrawIfNeeded();
        _DesignDIYCup_DrawCurrentMatchTeamEntries();
        /* loc_134C96 */
        DesignDIYCup_UpdateMenuEntriesVisibilityAndSelection();
        _CallDrawMultipleItems(8, &menuEntries[1]);
        return;
    }

    /* Common path for cases 1-4: loc_13481A */
    _DesignDIYCup_BuildMenuTitleString();
    _DesignDIYCup_HideCurrentMatchTeamEntries();
    diyCupMatchListScrollIndex = 0;
    DesignDIYCup_BuildMatchListEntryStrings();
    DesignDIYCup_UpdateMenuEntriesVisibilityAndSelection();
    _DrawMenu();
}

/* DesignDIYTournamentOnSelect89 — choose teams & play tournament (SWOS2.s line 48984) */
__attribute__((used, externally_visible))
void _DesignDIYTournamentOnSelect89(void)
{
    uint16_t numTeams = *(uint16_t *)(diyFileBufferCopy + 50); /* NUMTEAMS */
    uint8_t *teamListPtr = diyFileBufferCopy + 4; /* FIELD_2 + 2 */

    _GoGetTeamsForPlay(teamListPtr, aPlayTournament, numTeams, 1, 1, 1);

    if (chooseTeamsResult == 0) {
        _SetExitMenuFlag();
        return;
    }

    _InitCareerVariables(1);
    _CopySelectedTeamNumbers(numTeams, (uint8_t *)diyTournamentTeamIds);
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
extern uint32_t diyNameOfCompetitionPtr;
extern uint32_t diyTeamsBufferPtr;
extern uint16_t diyCompetitionType;
extern uint8_t competitionStatsMenu[];

__attribute__((used, externally_visible))
void _SetupDIYCompetitionStatsMenu(void) {
    uint8_t *a0 = diyCompetitionStart;
    uint16_t isCup = (*(uint16_t *)a0 == DIY_CUP);

    diyUnknownWord_2C = *(uint16_t *)(a0 + 44);
    diyNumberOfTeams = *(uint16_t *)(a0 + 50);
    if (*(uint16_t *)(a0 + 72))
        diyNumberOfTeams = *(uint16_t *)(a0 + 78);

    diyNameOfCompetitionPtr = (uint32_t)(a0 + 4);
    diyTeamsBufferPtr = (uint32_t)(a0 + (isCup ? 590 : 302));
    diyCompetitionType = isCup ? 1 : 0;

    if (*(uint16_t *)(a0 + 72))
        diyCompetitionType = 2;

    _ShowMenu(competitionStatsMenu);
}

/* ================================================================
 * Chunk 148: ChooseTeamsSetHeader, CheckNumTeamsAllowed, Bureaucracy
 * ================================================================ */

extern uint32_t chooseTeamsHeader1stPart;  /* pointer to header prefix string (or 0) */
extern uint32_t gameTypeStr;               /* pointer to game type string (or 0) */
extern uint8_t *curCountryStr;
extern uint16_t selectTeamsNoLimit;
extern uint16_t numNeededTeams;
extern uint32_t playWhatStr;               /* pointer to "play what" string */
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
extern uint16_t seasonUpdateInProgress;
extern uint32_t diyLeftTeamPtr;
extern uint32_t diyRightTeamPtr;
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
    uint8_t *a0 = diyCompetitionStart;
    int16_t field48 = *(int16_t *)&a0[0x48];   /* hasGroups */
    int16_t field60 = *(int16_t *)&a0[0x60];

    if (field48 == 0) {
        /* field48 == 0 path */
        if (field60 != 0)
            goto set_ff_and_exit;
        *(int16_t *)&a0[2] = 2;    /* competitionType area */
        if (diyTournamentViewOnly != 0) {
            /* fall through to exit */
        }
        _SetExitMenuFlag();
        return;
    }

    if (field48 == 2 && field60 == 0) {
        if (diyTournamentViewOnly != 0) {
            /* fall through */
        }
        _SetExitMenuFlag();
        return;
    }

set_ff_and_exit:
    if (field48 != 0) {
        *(int16_t *)&a0[0x48] = (int16_t)0xFFFF;
    }
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
    if (*(int16_t *)&diyCompetitionStart[0x48] != 0) {
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
    uint8_t *a1 = (uint8_t *)diyLeftTeamPtr;
    uint8_t *a2 = (uint8_t *)diyRightTeamPtr;
    uint16_t d1 = diyLeftTeamResult;
    uint16_t d2 = diyRightTeamResult;

    MenuEntry *e = &menuEntries[11];  /* 0x268/56=11 */
    e->isInvisible = 0;
    e->fg = (uint32_t)aMinus;         /* "-" */

    e++;  /* entry 12 */
    e->isInvisible = 0;
    e->fg = (uint32_t)(a1 + 5);       /* TEAMFILE_TEAMNAME = 5 */

    e++;  /* entry 13 */
    e->isInvisible = 0;
    e->fg = (uint32_t)(a2 + 5);       /* TEAMFILE_TEAMNAME = 5 */

    e++;  /* entry 14 */
    e->isInvisible = 0;
    *(uint16_t *)&e->fg = d1;         /* left score as 16-bit value in fg field */

    e++;  /* entry 15 */
    e->isInvisible = 0;
    *(uint16_t *)&e->fg = d2;         /* right score */
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
        targetLine = *(uint16_t *)&diyCompetitionStart[0x5C];
        if (diyFixtureViewMode != 1) {
            if ((int16_t)(targetLine - 1) >= 0) {
                targetLine--;
            } else {
                /* Wrap: use totalRounds-1 from field_3A+$16 = offset 0x50 */
                targetLine = *(uint16_t *)&diyCompetitionStart[0x50] - 1;
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
    if (bottom >= *(uint16_t *)&diyCompetitionStart[0x50])
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
    e->fg = (uint32_t)aV;             /* "V" */

    e++;  /* entry 12 */
    e->isInvisible = 0;
    /* a1=$2B4(team1) → pointer at offset 0x2B4, add selTeamNameBase */
    {
        uint32_t nameOff = *(uint32_t *)(team1 + 0x2B4);
        e->fg = (uint32_t)(&selTeamNameBase + nameOff);
    }

    e++;  /* entry 13 */
    e->isInvisible = 0;
    {
        uint32_t nameOff = *(uint32_t *)(team2 + 0x2B4);
        e->fg = (uint32_t)(&selTeamNameBase + nameOff);
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
    e1->fg = (uint32_t)aPlayMatch;    /* "PLAY MATCH" */
    _CallDrawMenuItem(e1);

    _CallDrawMultipleItems(5, &menuEntries[11]);  /* 0x268/56=11, draw 5 entries */
}

/* ================================================================
 * DIYLeagueTournamentNextMatch  (SWOS2.s line 42586)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIYLeagueTournamentNextMatch(void)
{
    uint8_t *a0 = diyCompetitionStart;
    int16_t field60 = *(int16_t *)&a0[0x60];

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
__attribute__((used, externally_visible))
void _DIYLeagueTournamentInit(void)
{
    currentDiyLine = 0;
    diyAllMatchesPlayed = 0;
    _DIYLeagueTournament_UpdateViewOptionsAvailability();

    if (diyMinSubstitutes == 0) {
        /* Check hasGroups (field_3A+$E = 0x48) */
        if (*(int16_t *)&diyCompetitionStart[0x48] == 0) {
            diyAllMatchesPlayed = 1;
            diyFixtureViewMode = 0;
        }
    }

    if (diyTournamentViewOnly != 0) {
        diyFixtureViewMode = 0;
        _SetCurrentEntry(10);
    }

    /* diyCompetitionStart+DIY_COMPETITION_FIELD_2 = offset 2 */
    *(uint16_t *)&diyCompetitionStart[2] = 1;

    if (seasonUpdateInProgress != 0) {
        diyFixtureViewMode = 0;
        _SetCurrentEntry(42);
        menuEntries[9].isInvisible = 1;    /* 0x1F8/56=9 */
        menuEntries[10].isInvisible = 1;   /* 0x230/56=10 */
    }
    /* Fall through (bra.w *+4 → FriendlyMenuReinit, but in context this is
     * actually a bra to the next instruction = fall-through/nop) */
}

/* ================================================================
 * Chunk 151: IsNationalTeamCompetitionOrQualification
 *
 * Scans nationalTeamCompetitionsAndQualifications1 list for a match.
 * Returns 0 if found (is national), 1 if not found.
 *
 * In asm, returns via Z flag: Z=set if found, Z=clear if not found.
 * ================================================================ */
extern uint32_t nationalTeamCompetitionsAndQualifications1[];

__attribute__((used, externally_visible))
uint16_t _IsNationalTeamCompetitionOrQualification(uint32_t key)
{
    uint32_t *p = nationalTeamCompetitionsAndQualifications1;
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
/* InitCompetitionFromRecord dispatches to InitDIY*FromRecord functions which
 * are still in asm with register-based calling (d0=index, a0=rec).
 * Must stay in asm until its callees are ported. */

/* ================================================================
 * Chunk 152 — DIY League/Tournament Match Callbacks & Visibility
 * ================================================================ */

extern uint16_t diyTournamentType;
extern uint16_t diyHasGroups;
extern uint16_t careerLoadedFlag;
extern uint16_t fireResetFlag;
extern uint16_t highlightedLeagueTeamId;

/* C-callable asm bridge — calls DIY_PlayOrSimulateLeagueFixture with d7=mode,
 * stores multi-register results to diyLeft/RightTeamPtr/Result globals.
 * Returns d0 (0 = success, non-zero = exit/cancel). */
extern uint16_t _asm_DIY_PlayOrSimulateLeagueFixture(uint16_t mode);

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
    *(uint16_t *)&diyCompetitionStart[0x3A + 4] = entry->ordinal;
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
    *(uint16_t *)&diyCompetitionStart[0x3A + 4] = entry->ordinal;
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
    *(uint16_t *)&diyCompetitionStart[0x3A + 4] = entry->ordinal;
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
    *(uint16_t *)&diyCompetitionStart[0x3A + 8] = entry->ordinal;
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
    *(uint16_t *)&diyCompetitionStart[0x3A + 8] = entry->ordinal;
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
    *(uint16_t *)&diyCompetitionStart[0x3A + 8] = entry->ordinal;
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
    uint16_t result = _asm_DIY_PlayOrSimulateLeagueFixture(action);
    if (result != 0)
        return;

    _DIYLeagueTournament_UpdateViewOptionsAvailability();
    diyFixtureViewMode = 0xFFFF;   /* -1 = show results */
    /* nullsub_15 omitted (no-op) */

    /* diyLeftTeamPtr/diyRightTeamPtr/diyLeftTeamResult/diyRightTeamResult
     * already populated by the asm bridge */

    uint16_t setAllPlayed = 0;

    if (diyHasGroups != 0) {
        /* With groups: move separator entry x to 0x97 */
        menuEntries[10].x = 0x97;  /* entry at $230/56 = 10 */
        setAllPlayed = 1;
    } else {
        uint8_t *a0 = diyCompetitionStart;
        if (diyTournamentType != 0) {
            /* Tournament: check field $1C0 */
            if (*(int16_t *)&a0[0x1C0] != 0)
                setAllPlayed = 1;
            /* else fall through to league check */
        }
        if (!setAllPlayed) {
            /* League check: if field $60 != 0 OR field $48 == 1 → not all played */
            if (*(int16_t *)&a0[0x60] == 0 && *(int16_t *)&a0[0x48] != 1)
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

    uint8_t *a1 = diyCompetitionStart;

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

    /* Not all played — check field $60 */
    if (*(int16_t *)&a1[0x60] != 0) {
        /* field $60 != 0 — check fixture view mode */
        if ((int16_t)diyFixtureViewMode <= 0)
            goto show_next_match;
        /* diyFixtureViewMode > 0 — show match view options */
        goto show_match_view;
    }

    /* field $60 == 0 — show "NEXT ROUND" */
    fireResetFlag = 1;
    menuEntries[1].fg = (uint32_t)aNextRound;
    goto show_next_button;

show_next_match:
    menuEntries[1].fg = (uint32_t)aNextMatch;

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

            _SetCurrentEntry(*(uint16_t *)&diyCompetitionStart[0x3A + 8]);
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

        _SetCurrentEntry(*(uint16_t *)&diyCompetitionStart[0x3A + 4]);
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
    writeOnlyVar01 = 1;
    diyTournamentType = type;
    diyFixtureViewMode = type;
    diyTournamentViewOnly = viewOnly;
    diyInitFocusOverride = focusOverride;
    _DIY_SortLeagueTableByStandings();

    if (seasonUpdateInProgress != 0) {
        do {
            _MenuCheckControls();
        } while (fire != 0);
    }

    _ShowMenu((uint8_t *)&diyLeagueTournamentMenu);
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
uint16_t _FindTeamInDIY(uint16_t teamNo, uint8_t *comp)
{
    diyFoundTeamIndex = 0;
    diyCompHasGroupStage = 0;

    uint16_t numTeams = *(uint16_t *)(comp + 50);
    uint16_t compType = *(uint16_t *)comp;

    if (compType == 1)    /* DIY_CUP */
        goto cup_entry;
    if (compType == 2)    /* DIY_TOURNAMENT */
        goto tournament_entry;

    /* --- LEAGUE path --- */
    /* seq: set high byte to 0xFF if comp[96] == 0 */
    diyCompHasGroupStage = (*(uint16_t *)(comp + 96) == 0) ? 0xFF00 : 0;

league_search:
    {
        uint16_t *leagueTeamIds = (uint16_t *)(comp + 302);
        int16_t d1 = -2;
        uint16_t rem = numTeams;
        while (rem-- > 0) {
            d1 += 2;
            int16_t rawIdx = *(int16_t *)(comp + 110 + d1);
            uint16_t teamIdx = (uint16_t)((uint32_t)(int32_t)rawIdx / 18u);
            if (leagueTeamIds[teamIdx] == teamNo)
                return (uint16_t)(d1 >> 1);
        }
        goto not_found;
    }

cup_entry:
    /* sne: set high byte to 0xFF if comp[86] != 0 */
    diyCompHasGroupStage = (*(uint16_t *)(comp + 86) != 0) ? 0xFF00 : 0;

cup_search:
    {
        uint16_t *cupTeamIds = (uint16_t *)(comp + 590);
        int8_t *cupIndexTable = (int8_t *)(comp + 718);
        int16_t d1 = -1;
        uint16_t rem = numTeams;
        while (rem-- > 0) {
            d1++;
            int16_t idx = (int16_t)(int8_t)cupIndexTable[d1];
            if (cupTeamIds[idx] == teamNo) {
                /* Found in cup — post-processing */
                if (*(uint16_t *)(comp + 86) != 0 && (uint16_t)d1 < 2) {
                    /* Group stage, position 0 or 1 */
                    if (teamNo == *(uint16_t *)(comp + 58))
                        return 0;
                    return 1;
                }
                goto cup_bracket;
            }
        }
        goto not_found;

    cup_bracket:
        {
            uint16_t roundVal = *(uint16_t *)(comp + 80);
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
        /* sne: set high byte to 0xFF if comp[88] != 0 */
        diyCompHasGroupStage = (*(uint16_t *)(comp + 88) != 0) ? 0xFF00 : 0;

        uint16_t *tournTeamIds = (uint16_t *)(comp + 154);
        int8_t *tournIndexTable = (int8_t *)(comp + 90);
        int16_t d1 = -1;
        uint16_t rem = numTeams;
        while (rem-- > 0) {
            d1++;
            int16_t idx = (int16_t)(int8_t)tournIndexTable[d1];
            if (tournTeamIds[idx] == teamNo) {
                /* Found in tournament */
                uint16_t stageConfig = *(uint16_t *)(comp + 84);
                uint16_t stageOffset = stageConfig * 118;
                uint8_t *stagePtr = comp + stageOffset;

                if (*(uint16_t *)(comp + 88) != 0) {
                    /* Has group stage */
                    uint16_t threshold = *(uint16_t *)(stagePtr + 0x162);
                    if ((uint16_t)d1 < threshold) {
                        /* Team is in group stage — switch to secondary competition */
                        comp += 0x420;
                        numTeams = *(uint16_t *)(comp + 50);
                        if (*(uint16_t *)(stagePtr + 0x15E) == 0)
                            goto cup_search;
                        else
                            goto league_search;
                    }
                }

                goto tournament_post;

            tournament_post:
                {
                    uint16_t threshold = *(uint16_t *)(stagePtr + 0x162);
                    if ((uint16_t)d1 < threshold) {
                        return threshold / 2 - 1;
                    }

                    diyFoundTeamIndex = 1;

                    if (*(uint16_t *)(comp + 0x15A) != 0) {
                        if ((uint16_t)d1 == 3 || (uint16_t)d1 == 4)
                            return (uint16_t)d1;
                    }

                    /* Walk through stage blocks to find which stage the team is in */
                    uint16_t pos = (uint16_t)d1;
                    uint8_t *sp = comp;
                    uint16_t stageThreshold;
                    do {
                        sp += 118;
                        stageThreshold = *(uint16_t *)(sp + 0x162);
                    } while (pos < stageThreshold);

                    uint16_t stageNumTeams = *(uint16_t *)(sp + 0xEC);

                    if (findTeamInDIY_SuppressSideEffectsFlag != 0) {
                        uint16_t rank = _GetMoreExpensiveTeamsCount(teamNo, comp);
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
    uint8_t *comp = competitionFileBuffer;
    uint16_t numTeams = *(uint16_t *)(comp + 50);  /* DIY_COMPETITION_NUMTEAMS */
    uint16_t compType = *(uint16_t *)comp;
    uint16_t *teamIdArray;
    int8_t *indexTable;

    if (compType == 0) {        /* DIY_LEAGUE */
        teamIdArray = (uint16_t *)(comp + 302);
        indexTable = (int8_t *)(comp + 238);
    } else if (compType == 1) { /* DIY_CUP */
        teamIdArray = (uint16_t *)(comp + 590);
        indexTable = (int8_t *)(comp + 718);
    } else {                    /* DIY_TOURNAMENT */
        teamIdArray = (uint16_t *)(comp + 154);
        indexTable = (int8_t *)(comp + 90);
    }

    for (uint16_t i = 0; i < numTeams; i++) {
        int16_t idx = (int16_t)(int8_t)indexTable[i];  /* sign-extend byte to word */
        uint16_t teamNo = teamIdArray[idx];
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
extern uint16_t _GetAveragePlayerPrice(uint8_t *team);

__attribute__((used, externally_visible))
uint16_t _GetMoreExpensiveTeamsCount(uint16_t teamNo, uint8_t *comp)
{
    uint16_t numTeams = *(uint16_t *)(comp + 50);  /* DIY_COMPETITION_NUMTEAMS */
    uint16_t compType = *(uint16_t *)comp;
    uint16_t *teamIds;

    if (compType == 0) {        /* DIY_LEAGUE */
        teamIds = (uint16_t *)(comp + 302);
    } else if (compType == 1) { /* DIY_CUP */
        teamIds = (uint16_t *)(comp + 590);
    } else {                    /* DIY_TOURNAMENT */
        teamIds = (uint16_t *)(comp + 154);
    }

    /* Get our team's average player price */
    uint8_t *myTeam = _VerifyTeamLoaded(teamNo);
    uint16_t myPrice = _GetAveragePlayerPrice(myTeam);

    /* Count teams with higher average price */
    uint16_t count = 0;
    for (uint16_t i = 0; i < numTeams; i++) {
        uint16_t otherId = teamIds[i];
        if (otherId == teamNo)
            continue;
        uint8_t *otherTeam = _VerifyTeamLoaded(otherId);
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
    uint8_t *a0 = diyCompetitionStart;
    uint16_t count = *(uint16_t *)(a0 + 0x1C4);
    uint8_t startVal = a0[0x38];
    uint8_t endVal = a0[0x39];
    uint8_t *out = a0 + 0x1CE;

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
