/* diy_competition.c — DIY competition/tournament management, player pricing.
 *
 * Chunk 60: 16 functions ported from SWOS2.s.
 */

#include "diy_competition.h"
#include "game_types.h"
#include "string_utils.h"
#include "random.h"
#include "panic.h"

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

/* ASM globals for validation */
extern volatile uint16_t panicReasonCode;
extern uint16_t teamsLoaded;
extern uint8_t teamFileBuffer[];

/* ASM data tables */
extern uint32_t cupRoundStringTable[];
extern uint8_t diyLeagueSeasonStateTable[];
extern uint8_t diyLeagueScratchBuffer[];
extern uint8_t competitionFileBuffer[];

/* ASM globals for cup bracket */
extern uint16_t diyCupTeamIds[];

/* ---- C bridges to asm functions ---- */
extern uint8_t *_VerifyTeamLoaded(uint16_t teamNum);
extern void _CDIY_RecomputeCompetitionDerivedProgressionTable(void);

/* ---- ASM functions still in asm (called via bridge) ---- */
extern void _asm_ShowDiyLeaguesTournamentMenu(uint16_t d0, uint16_t d1, uint16_t d2);
extern void _asm_ShowDIYCupMenuWithParams(uint16_t d0, uint16_t d1, uint16_t d2);
extern void _asm_DIY_LoadCurrentCompetitionSlot(void);
extern void _asm_Career_AutoAdvanceInternationalCompetitionFixtures(void);

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
extern void _CDIY_BuildSeededBracketAndUpdateScores(uint8_t *a2, uint8_t *a3,
                                                     uint8_t *a4, uint8_t *a5);
extern void _CDIY_CopyTeamGridFromCompToDIY(uint8_t *a2, uint8_t *a3,
                                             uint8_t *a4, uint8_t *a5);

__attribute__((used, externally_visible))
void _DIY_SyncCompetitionTeamGridOrBuildBracket(void)
{
    uint8_t *a2 = (uint8_t *)(DIY_Competition *)diyFileBufferCopy;
    uint16_t offset = (uint16_t)((DIY_Competition *)diyFileBufferCopy)->currentRound;
    uint8_t *a3 = a2 + (uint32_t)offset * 0x76;
    uint8_t *a4 = (uint8_t *)(DIY_Competition *)diyCompetitionStart;
    uint8_t *a5 = (uint8_t *)(DIY_Competition *)diyCompetitionStart;

    if (*(uint16_t *)(a3 + 0x15E) != 0) {  /* extended */
        _CDIY_BuildSeededBracketAndUpdateScores(a2, a3, a4, a5);
    } else {
        _CDIY_CopyTeamGridFromCompToDIY(a2, a3, a4, a5);
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
    _asm_ShowDiyLeaguesTournamentMenu(editA, editB, editC);

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
