# DIY_CompetitionEx Extended Structure Report

## Overview

`diyCompetitionStart` is a 0x5C8 (1480) byte flat buffer in BSS. The current `DIY_Competition` struct covers only the first 0x60 (96) bytes. The remaining 0x568 bytes are accessed via raw byte offsets throughout the codebase.

This document maps all fields beyond 0x60 to enable a `DIY_CompetitionEx` struct definition.

## Current DIY_Competition (0x00-0x5F) — Already Defined

See `include/game_types.h` line 511. 96 bytes, fully mapped.

## Extended Fields (0x60-0x5C7)

### Competition Settings (0x60-0x65)

| Offset | Size | Type | Suggested Name | Description |
|--------|------|------|----------------|-------------|
| 0x60 | 2 | uint16_t | `minSubstitutes` | Minimum substitutes allowed |
| 0x62 | 2 | uint16_t | `pointsForAWin` | Points awarded for a win |
| 0x64 | 2 | uint16_t | `currentMatchIndex` | Current match index within round |

Note: `diyMinSubstitutes` (0x60), `diyPointsForAWin` (0x62) are separate named BSS labels, but they're always at diyCompetitionStart+0x60/0x62.

### Round Configuration Arrays (0x66-0x89)

Three parallel arrays of 6 uint16_t entries each (one per round slot):

| Offset | Size | Type | Suggested Name | Description |
|--------|------|------|----------------|-------------|
| 0x66 | 12 | uint16_t[6] | `roundLegs[6]` | Legs per round (1=single, XOR toggled with 3) |
| 0x72 | 12 | uint16_t[6] | `roundExtraTime[6]` | Extra time setting per round (0=none, 1=yes) |
| 0x7E | 12 | uint16_t[6] | `roundPenalties[6]` | Penalties setting per round (0=none, 1=yes) |

### Match Results (0x8A-0xC9)

| Offset | Size | Type | Suggested Name | Description |
|--------|------|------|----------------|-------------|
| 0x8A | 64 | uint8_t[64] | `matchResultFlags[64]` | Per-match result flags (bit 0=team2 won, bit 1=has winner, bit 2=AET, etc.) |

### Score Tables (0xCA-0x1CD)

Three score table regions, each holding packed uint16_t goal pairs (high byte = team1, low byte = team2):

| Offset | Size | Type | Suggested Name | Description |
|--------|------|------|----------------|-------------|
| 0xCA | 132 | uint16_t[66] | `matchGoals[66]` | Main match goals (packed: team1<<8 \| team2) |
| 0x14E | 128 | uint16_t[64] | `aggregateGoals[64]` | Aggregate/first-leg goals or 90-min scores |
| 0x1CE | 128 | uint16_t[64] | `penaltyGoals[64]` | Penalty shootout goals |

### League-Specific Scheduling (0x12E-0x149)

| Offset | Size | Type | Suggested Name | Description |
|--------|------|------|----------------|-------------|
| 0x12E | 28 | uint16_t[14] | `leagueTeamIds[14]` | Team IDs for league competitions |

### Two-Leg / Format Control (0x14A-0x14D)

| Offset | Size | Type | Suggested Name | Description |
|--------|------|------|----------------|-------------|
| 0x14A | 2 | uint16_t | `twoLegSubRound` | Sub-round counter for two-leg (0=first leg, 1=return leg) |
| 0x14C | 2 | uint16_t | `roundFormatType` | Round format (0=normal, 2=two-leg) |

**IMPORTANT**: ASM uses `DIY_COMPETITION_FIELD_2 + $14A` = 0x14C for the format type. A bug existed where C code read 0x14A instead of 0x14C — now fixed.

### Cup Team IDs (0x24E-0x2CD)

| Offset | Size | Type | Suggested Name | Description |
|--------|------|------|----------------|-------------|
| 0x24E | 128 | uint16_t[64] | `cupTeamIds[64]` | Team IDs for cup bracket |

### Team Grid Pairings (0x2CE-0x30D)

| Offset | Size | Type | Suggested Name | Description |
|--------|------|------|----------------|-------------|
| 0x2CE | 64 | uint8_t[64] | `teamGrid[64]` | Team grid pairings (byte indices, pairs of home/away) |

### Team Order / Shuffle List (0x30E-0x32D)

| Offset | Size | Type | Suggested Name | Description |
|--------|------|------|----------------|-------------|
| 0x30E | 32 | uint8_t[32] | `teamOrder[32]` | Bracket ordering list for reorder operations |

### Runtime Counters (0x32E-0x343)

| Offset | Size | Type | Suggested Name | Description |
|--------|------|------|----------------|-------------|
| 0x32E | 2 | uint16_t | `unselectedTeamCount` | Count of unselected/missing teams |
| 0x330 | 2 | uint16_t | `replayFlag` | Replay match flag |
| 0x332 | 2 | uint16_t | `matchesPlayedInRound` | Matches completed counter |
| 0x334 | 2 | uint16_t | `totalMonthEntries` | Total entries in month progression table |
| 0x336 | 2 | uint16_t | `currentMonthIndex` | Current month/phase index |
| 0x338 | 12 | uint8_t[12] | `monthProgressTable[12]` | Month progression values (viewRandomPhase per phase) |

### Per-Team Pointer Cache (0x344-0x3A3)

| Offset | Size | Type | Suggested Name | Description |
|--------|------|------|----------------|-------------|
| 0x344 | 96 | uint32_t[24] | `teamPtrCache[24]` | selTeamsPtr-relative pointers to team data (4 bytes per team) |

Note: This overlaps with the league per-team stats entries (see below). The pointer cache entries at 0x344 coincide with entry[8].teamSelPtr in the per-team stats array.

### League Phase Scheduling (0x1BE-0x1CD)

| Offset | Size | Type | Suggested Name | Description |
|--------|------|------|----------------|-------------|
| 0x1BE | 2 | uint16_t | `leagueMatchesPlayed` | Total league matches played |
| 0x1C0 | 2 | uint16_t | `phaseChangeFlag` | Phase boundary reached flag (1=new phase) |
| 0x1C2 | 2 | uint16_t | `phaseIndex` | Current phase index |
| 0x1C4 | 2 | uint16_t | `matchesPerSeason` | Total matches in the season |
| 0x1C6 | 2 | uint16_t | `halfGroupCount` | Half group count for scheduling |
| 0x1C8 | 2 | uint16_t | `matchesPerPhase` | Matches per phase/half |
| 0x1CA | 2 | uint16_t | `matchInPhase` | Current match within phase |
| 0x1CC | 2 | uint16_t | `leagueRoundCounter` | League round counter |

### Per-Team League Statistics (stride 0x12 = 18 bytes, starting at 0x2B4)

Teams are indexed as `base + teamIdx * 18 + fieldOffset`, starting from offset 0x2B4. With up to ~24 teams, this spans 0x2B4 to ~0x420.

Each team entry (18 bytes):

| Entry Offset | Size | Type | Suggested Name | Description |
|-------------|------|------|----------------|-------------|
| +0 | 4 | uint32_t | `teamSelPtr` | selTeamsPtr-relative pointer to team data |
| +4 | 2 | uint16_t | `gamesPlayed` | Games played |
| +6 | 2 | uint16_t | `wins` | Wins |
| +8 | 2 | uint16_t | `draws` | Draws |
| +10 | 2 | uint16_t | `losses` | Losses |
| +12 | 2 | uint16_t | `goalsFor` | Goals scored |
| +14 | 2 | uint16_t | `goalsAgainst` | Goals conceded |
| +16 | 2 | uint16_t | `points` | Total points |

This should be a separate struct (e.g., `DIY_TeamStats`) embedded as an array.

## Overlapping Regions

The buffer has significant aliasing:

1. **teamPtrCache[0] at 0x344** = per-team stats entry[8].teamSelPtr (offset 0x2B4 + 8*18 = 0x344)
2. **Score tables** (0xCA, 0x14E, 0x1CE) are used differently for league vs cup modes
3. **teamGrid (0x2CE)** and **teamOrder (0x30E)** overlap with per-team stats for teams 0-7

The safest approach for the struct definition is to use a union for overlapping regions.

## Suggested DIY_CompetitionEx Struct

```c
typedef struct __attribute__((packed)) DIY_TeamStats {
    uint32_t teamSelPtr;       /* +0: selTeamsPtr-relative pointer */
    uint16_t gamesPlayed;      /* +4 */
    uint16_t wins;             /* +6 */
    uint16_t draws;            /* +8 */
    uint16_t losses;           /* +10 */
    uint16_t goalsFor;         /* +12 */
    uint16_t goalsAgainst;     /* +14 */
    uint16_t points;           /* +16 */
} DIY_TeamStats; /* 18 bytes */

typedef struct __attribute__((packed)) DIY_CompetitionEx {
    DIY_Competition base;              /* 0x00-0x5F: base header (96 bytes) */

    /* Competition settings */
    uint16_t minSubstitutes;           /* 0x60 */
    uint16_t pointsForAWin;            /* 0x62 */
    uint16_t currentMatchIndex;        /* 0x64 */

    /* Round configuration (6 rounds max) */
    uint16_t roundLegs[6];             /* 0x66: legs per round */
    uint16_t roundExtraTime[6];        /* 0x72: extra time per round */
    uint16_t roundPenalties[6];        /* 0x7E: penalties per round */

    /* Match results */
    uint8_t  matchResultFlags[64];     /* 0x8A */
    uint16_t matchGoals[66];           /* 0xCA: packed goals (team1<<8|team2) */

    /* League team IDs */
    uint16_t leagueTeamIds[14];        /* 0x12E */

    /* Two-leg control */
    uint16_t twoLegSubRound;           /* 0x14A */
    uint16_t roundFormatType;          /* 0x14C: 0=normal, 2=two-leg */

    /* Aggregate/first-leg scores */
    uint16_t aggregateGoals[64];       /* 0x14E */

    /* Penalty scores */
    uint16_t penaltyGoals[64];         /* 0x1CE */

    /* League scheduling */
    uint16_t leagueMatchesPlayed;      /* 0x1BE -- NOTE: overlaps with aggregateGoals[] tail */
    uint16_t phaseChangeFlag;          /* 0x1C0 */
    uint16_t phaseIndex;               /* 0x1C2 */
    uint16_t matchesPerSeason;         /* 0x1C4 */
    uint16_t halfGroupCount;           /* 0x1C6 */
    uint16_t matchesPerPhase;          /* 0x1C8 */
    uint16_t matchInPhase;             /* 0x1CA */
    uint16_t leagueRoundCounter;       /* 0x1CC */
    uint8_t  monthProgressTable[98];   /* 0x1CE -- NOTE: overlaps with penaltyGoals[] */

    /* Cup team IDs */
    uint16_t cupTeamIds[64];           /* 0x24E */

    /* Per-team stats (league standings) */
    DIY_TeamStats teamStats[24];       /* 0x2B4 (24*18 = 432 bytes, extends to 0x464) */

    /* Team grid pairings -- NOTE: overlaps with teamStats[] */
    uint8_t  teamGrid[64];            /* 0x2CE */

    /* Team order list */
    uint8_t  teamOrder[32];            /* 0x30E */

    /* Runtime counters */
    uint16_t unselectedTeamCount;      /* 0x32E */
    uint16_t replayFlag;               /* 0x330 */
    uint16_t matchesPlayedInRound;     /* 0x332 */
    uint16_t totalMonthEntries;        /* 0x334 */
    uint16_t currentMonthIndex;        /* 0x336 */
    uint8_t  cupMonthTable[12];        /* 0x338 */

    /* Team pointer cache */
    uint32_t teamPtrCache[24];         /* 0x344 -- NOTE: overlaps with teamStats[8+] */

} DIY_CompetitionEx;
```

**WARNING**: Due to overlapping regions, a flat struct won't work directly. The recommended approach is:
- Define the struct with the NON-overlapping fields
- Use unions for overlapping regions (league scheduling vs penalty goals at 0x1CE, teamStats vs teamGrid at 0x2CE, teamPtrCache vs teamStats[8+] at 0x344)
- OR keep certain regions as raw byte arrays and provide accessor functions

## Functions That Access Extended Fields

### Primary (src/diy_competition.c) — ~200+ accesses
Nearly every function in this file uses extended fields via raw offsets.

### Secondary files:
| File | Fields Accessed |
|------|----------------|
| career_status.c | 0x1C0, 0x54, 0x2C, 0x12E, phaseChangeFlag |
| career_teams.c | 0x1C0, 0x1C4, 0x1CE, 0x2B4-0x2C4, 0x2CE, 0x338, 0x344 |
| career_eurocups.c | 0x12E, 0x24E, 0x2CE |
| career_init.c | 0x2C (via DIY_Competition) |
| match_sim.c | 0x2B8 (via per-team stats) |
| menus.c | 0x2C (via DIY_Competition) |
| menu_data.c | offset 4 (competition name in base struct) |

## Recommendation

1. Define `DIY_TeamStats` as a standalone 18-byte struct
2. Define `DIY_CompetitionEx` covering 0x00-0x5C7 with unions for overlapping regions
3. Migrate `diy_competition.c` raw offset accesses to struct member access incrementally
4. Start with the non-overlapping, frequently-accessed fields first
5. Keep the IDA database in sync with struct changes
