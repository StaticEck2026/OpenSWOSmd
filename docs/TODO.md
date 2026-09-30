# TODO — Remaining Untyped Data Structures

> Status (2026-09-30): partly done. The structs proposed below now exist in
> `include/game_types.h` — `TeamSelectionKeyCacheEntry` / `TeamSelectionKeyCacheMatchSlot`
> (§1), `Competition` / `CompLeagueHeader` / `CompetitionLeague` (§2), `DIYCompetitionEx`
> (§3), `TeamTactics` (§4) — but some raw-offset call sites listed here still use byte
> offsets (see the per-section notes). Line numbers below are from 2026-05 and have
> drifted; grep for the expression instead.

These raw byte-offset accesses remain in the codebase because the underlying data
structures have no corresponding struct definition in `game_types.h` or
`data/SWOS2_IDA.h`. Each section describes the data layout as understood from
usage context, lists every file/line that accesses it, and suggests a struct.

---

## 1. Team Selection Key Cache Entry (Career Game List)

**Context:** The `teamSelectionKeyCache` is a shared BSS buffer (~20KB) that
overlaps with `careerForeignMarketPlayers` (MarketPlayer[352]) — they are mutually
exclusive at runtime. The cache holds variable-length entries, each containing a
header and per-game fixture records.

`gameListArray` (112 bytes) is **not** an array of these records. It stores 14
pointer pairs `(cacheEntryPtr, gameRecordBasePtr)` used by the menu to display
the game list. The actual data lives inside `teamSelectionKeyCache`.

**Memory layout (SWOS2.s):**

```
teamSelectionKeyCache:          ds.b $1       ; start of shared buffer
careerJobOfferPhaseBias:        ds.b $1       ; independent byte variable (overlaps cache byte 1)
BSS_01_1699A2:                  ds.b $74C     ; unnamed cache body
careerForeignMarketPlayers:     ds.b $39C0    ; MarketPlayer[352] — overlaps cache, mutually exclusive
BSS_01_16E082:                  ds.b $D20     ; unnamed cache tail
teamSelectionKeyCacheEnd:       ds.b $2       ; 2-byte flag AND bounds-check address
```

`teamSelectionKeyCacheEnd` serves dual purpose: its **address** is the upper bound
for cache growth (`cmpa.l #teamSelectionKeyCacheEnd,a4`), and it's also a 2-byte
flag (`tst.w`, `move.w #1`, `clr.w`).

**Cache Entry Header (10 bytes):**

| Offset | Size | Name (proposed)  | Description |
|--------|------|------------------|-------------|
| 0x00   | 1    | slotType         | Competition slot (0=league, 1=cup1, 2=cup2, etc.) |
| 0x01   | 1    | field_01         | Unknown (overlaps careerJobOfferPhaseBias for first entry) |
| 0x02   | 2    | entrySize        | uint16_t stride to next cache entry |
| 0x04   | 1    | teamControl      | Team control flag |
| 0x05   | 1    | numGames         | Number of game records following |
| 0x06   | 1    | remainingGames   | Games remaining to play |
| 0x07   | 3    | padding          | Unknown |

**Game Record (12 bytes each, starting at cache entry + 0x0A):**

| Offset | Size | Name (proposed)  | Description |
|--------|------|------------------|-------------|
| 0x00   | 1    | homeAway         | 0=neutral, 1=home "(H)", 2=away "(A)" |
| 0x01   | 1    | resultFlags      | 0=not played, non-zero=match played |
| 0x02   | 2    | team1Id          | uint16_t globalTeamNumber of team 1 |
| 0x04   | 2    | team2Id          | uint16_t globalTeamNumber of team 2 |
| 0x06   | 2    | leg2Scores       | uint16_t packed (hi=team1, lo=team2) |
| 0x08   | 2    | leg1Scores       | uint16_t packed aggregate |
| 0x0A   | 2    | penaltyScores    | uint16_t packed penalty shootout scores |

**How code accesses the data:**

`gameListArray` stores pairs: `(a6=cacheEntryPtr, a4=cacheEntry + gameIdx*12)`.
- Header fields via a6: `a6[0]` (slotType), `a6[5]` (numGames)
- Game fields via a4 at offset 0x0A: `a4[0x0A]` (homeAway), `a4[0x0B]` (resultFlags),
  `*(uint16_t*)(a4+0x0C)` (team1Id), etc.
- For gameIdx=0, a4==a6 so `a4[0x0A]` = first game at cacheEntry+10.
- For gameIdx=N, a4=a6+N*12 so `a4[0x0A]` = cacheEntry+10+N*12.

**Files affected:**

- `src/menus.c:2759` — `_BuildGameListArrayFromTeamSelectionKeyCache` (builds pointer pairs)
- `src/menus.c:6268-6271` — reads pointer pairs, accesses via a6/a4
- `src/menus.c:6288-6308` — `a4[0x0B]`, `a4+0x0C`, `a4+0x0E`, `a4+0x10`, `a4+0x12`, `a4+0x14`
- `src/menus.c:6580` — `a4[0x0A]` (homeAway)
- `src/diy_competition.c:2585-2614` — `a5[0x0A]`, `a5[0x0B]`, `a5+0x0C/0x0E` (building records)
- `src/diy_competition.c:2772-2789` — `resultPtr[0x0A]`, `resultPtr[0x0B]`, `resultPtr+0x0C/0x0E`
- `src/career_status.c:3638-3641` — `team1[0x0B]`, `team1+0x10/0x12/0x14` (write result)

**Status (2026-09-30):** defined as `TeamSelectionKeyCacheEntry` (10 B) and
`TeamSelectionKeyCacheMatchSlot` (12 B) in `include/game_types.h`, and used by
`career_teams.c`, `career_status.c`, `diy_competition.c` and the stride walks in
`menus.c`. Still raw: the game-list display in `menus.c` (`a6[5]`, `a4[0x0A]`, ... via
`_gameListArray`) and the record builders in `diy_competition.c` (`a5[0x0A]`,
`a5[0x0B]`, `resultPtr[0x0A]`).

**Suggested structs (as originally proposed):**

```c
typedef struct __attribute__((packed)) GameRecord {
    uint8_t  homeAway;         /* 0x00: 0=neutral, 1=home, 2=away */
    uint8_t  resultFlags;      /* 0x01: 0=unplayed, non-zero=played */
    uint16_t team1Id;          /* 0x02 */
    uint16_t team2Id;          /* 0x04 */
    uint16_t leg2Scores;       /* 0x06: packed scores */
    uint16_t leg1Scores;       /* 0x08: packed aggregate */
    uint16_t penaltyScores;    /* 0x0A: packed penalties */
} GameRecord;                   /* 12 bytes */

typedef struct __attribute__((packed)) CacheEntryHeader {
    uint8_t  slotType;         /* 0x00 */
    uint8_t  field_01;         /* 0x01 */
    uint16_t entrySize;        /* 0x02: stride to next entry */
    uint8_t  teamControl;      /* 0x04 */
    uint8_t  numGames;         /* 0x05 */
    uint8_t  remainingGames;   /* 0x06 */
    uint8_t  padding[3];       /* 0x07-0x09 */
    /* GameRecord games[]; */  /* 0x0A: variable-length array */
} CacheEntryHeader;             /* 10 bytes + N*12 */
```

---

## 2. CompetitionRecord (Competition Definition Table Entry)

**Size:** Variable (base is 9 bytes for `Competition`, extended by division/round
data). Entries are spaced 6 bytes apart per division.

**Context:** Pointed to by `competitionsTable[countryIdx]`. Accessed in career
initialization and season transition code to find competition definitions, division
counts, and DIY offsets.

**Known fields (from IDA `Competition` struct + extensions):**

| Offset | Size | Name (proposed) | Description |
|--------|------|-----------------|-------------|
| 0x00   | 1    | compNumber      | Competition number |
| 0x01   | 1    | type            | Competition type |
| 0x02   | 1    | teamNumber      | Number of teams |
| 0x03   | 1    | beginMonth      | Start month |
| 0x04   | 1    | endMonth        | End month |
| 0x05   | 1    | unk1            | |
| 0x06   | 1    | unk2            | |
| 0x07   | 1    | unk3            | |
| 0x08   | 1    | unk4            | |
| 0x09   | 1    | divisionsCount  | (CompetitionLeague) number of divisions |
| 0x0A   | 1    | rounds          | (CompetitionLeague) rounds per season |
| 0x0B   | 1    | pointsForWin    | (CompetitionLeague) |
| 0x0C   | 1    | subsReserves    | (CompetitionLeague) substitutes allowed |

Per-division entries (6 bytes each, starting after the base record):

| Relative | Size | Name            | Description |
|----------|------|-----------------|-------------|
| +0x00    | 1    | teams           | Teams in division |
| +0x01    | 1    | promoted        | Promoted count |
| +0x02    | 1    | promotedPo      | Promotion playoff count |
| +0x03    | 1    | relegated       | Relegated count |
| +0x04    | 1    | relegatedPo     | Relegation playoff count |
| +0x05    | 1    | unk1            | |

The offset `entry[0x12]` is a relative pointer to DIY qualification data from the
entry base. `entry[0x18]` is the next division's equivalent offset.

**Files affected:**

- `src/career_init.c:828` — `entry[0x12]` (DIY link offset)
- `src/career_season.c:1292` — `a2[0x12]` (DIY link offset)
- `src/career_season.c:1301` — `a2[0x18]` (next division's DIY link offset)

**Status (2026-09-30):** `Competition`, `CompLeagueHeader` and a merged
`CompetitionLeague` are in `include/game_types.h` (docs/STRUCT_CONSOLIDATE.md §4), and
`career_season.c` reads `divisionsCount` through `CompLeagueHeader`. The per-division
DIY link bytes are still raw: `entry[0x12]` in `src/career_init.c` and `a2[0x12]` /
`a2[0x18]` in `processDiyLeagueQualificationEntry` (`src/career_season.c`).

**Note:** IDA defines `Competition`, `CompetitionLeague`, `CompetitionCup`,
`CompetitionCup2`, `Division`, and many variant structs in `SWOS2_IDA.h`. These
could be imported to `game_types.h` to replace the raw offsets, but the variable-
length nature (per-division data appended) makes a single fixed struct impractical.
A base struct + per-division array would work.

---

## 3. DIY Extended Buffer (offsets >= 0x60)

**Size:** 0x5C8 bytes total. The first 0x60 bytes are `DIY_Competition` (already
defined in `game_types.h`). The remaining 0x568 bytes hold runtime state for the
active competition: team grids, fixture tables, results, group assignments, etc.

**Context:** `diyCompetitionStart`, `diyFileBufferCopy`, and `competitionFileBuffer`
point to these buffers. The extended area is populated by `_InitDIYLeagueFromRecord`,
`_InitDIYCupFromRecord`, etc.

**Known extended fields (partial):**

| Offset | Size | Name (proposed)       | Description |
|--------|------|-----------------------|-------------|
| 0x60   | 2    | field_60              | int16_t: group/stage indicator (0=single stage) |
| 0x61   | 1    | extendedFlag          | Bit 0: extended competition flag |
| 0x66   | ?    | replayField           | Used in play path decisions |
| 0x8A   | ?    | resultsTableStart     | Start of match results area |
| 0xCA   | ?    | fixtureData           | Fixture pairing data |
| 0x11A  | ?    | groupMapStart         | Group assignment map |
| 0x14C  | 2    | field_14C             | int16_t: checked for extended behavior |
| 0x14E  | ?    | field_14E             | |
| 0x15E  | ?    | seededBracketFlag     | Non-zero triggers seeded bracket build |
| 0x162  | ?    | field_162             | |
| 0x1C0  | 2    | allMatchesPlayedFlag  | int16_t: 0=more matches, non-zero=round done |
| 0x2B4  | 4    | teamSelOffset         | uint32_t: offset into selTeamsPtr |
| 0x2CE  | ?    | teamGrid              | Team grid/ordering for fixtures |
| 0x30E  | ?    | field_30E             | |
| 0x338  | ?    | field_338             | |
| 0x344  | ?    | field_344             | |

**Status (2026-09-30): done.** `DIYCompetitionEx` in `include/game_types.h` types the
whole buffer (`minSubstitutes` 0x60, `pointsForAWin` 0x62, `matchData`,
`cupAggregateScores`, `matchesPlayedCount` 0x1BE, ...); the `a0[0x60]` / `a0[0x61]` /
`a0[0x1C0]` / `a5[0x66]` reads below are gone from `src/diy_competition.c`. See
docs/DIYCOMPETITIONEX_AUDIT.md.

**Files affected (as of 2026-05):**

- `src/diy_competition.c:1665` — `a0[0x61]` (extendedFlag)
- `src/diy_competition.c:7606,7851` — `a0[0x60]` (field_60)
- `src/diy_competition.c:7630` — `a0[0x48]` write (hasGroups, but via extended ptr)
- `src/diy_competition.c:8450` — `a0[0x1C0]` (allMatchesPlayedFlag)
- `src/diy_competition.c:8456` — `a0[0x60]`, `a0[0x48]`
- `src/diy_competition.c:8515` — `a1[0x60]`
- `src/diy_competition.c:9535` — `a5[0x66]` (replayField)
- Many more in extended buffer manipulation routines

**Note:** Defining a full `DIY_CompetitionExtended` struct for the 0x5C8-byte buffer
would require extensive reverse-engineering of the fixture/results table layouts,
which vary by competition type (league vs cup vs tournament) and team count. A
practical approach is to extend `DIY_Competition` with named fields for the most
commonly accessed offsets (0x60, 0x1C0) and leave the variable-layout areas as raw
byte access with good comments.

---

## 4. TacticsData Internal Layout

**Size:** Variable (at least 0x172 bytes based on observed access).

**Context:** Pointed to by `teamTacticsTable[tacticsIdx]`. Contains tactical
formation data, position maps, and formation parameters.

**Known fields:**

| Offset | Size | Name (proposed) | Description |
|--------|------|-----------------|-------------|
| 0x171  | 1    | posMapIndex     | int8_t: index into `tacticPosMapPtrs[]` |

The `TeamTactics` struct is partially defined in `SWOS2_IDA.h` (line 691) but does
not cover the full layout. The internal structure includes position coordinates,
priority tables, and animation data that vary by formation.

**Status (2026-09-30):** `TeamTactics` (370 B) is now fully defined in
`include/game_types.h`; offset 0x171 = 369 is its `ballOutOfPlayTactics` byte. The one
access in `src/training_match.c` still reads it raw as `tacticsData[0x171]`.

**Files affected:**

- `src/training_match.c:572` — `tacticsData[0x171]` (posMapIndex)

---

## Action Items

1. **TeamSelectionKeyCache entries** — *(structs done; the menus.c game-list and
   diy_competition.c record-builder sites still raw.)* Define `CacheEntryHeader` (10 bytes) and
   `GameRecord` (12 bytes) structs. Would clean up ~15 raw offset accesses across
   3 files. Note the shared-memory overlap with `careerForeignMarketPlayers`.

2. **CompetitionRecord** — *(structs imported; the 3 DIY-link accesses still raw.)* Import `Competition`, `CompetitionLeague`, `Division`
   from `SWOS2_IDA.h` to `game_types.h`. Would clean up 3 accesses. The variable-
   length layout is inherent to the data format.

3. **DIY Extended Buffer** — *(done: `DIYCompetitionEx`.)* Add the most-accessed fields (0x60, 0x1C0) to a
   `DIY_CompetitionRuntime` struct or extend `DIY_Competition`. Full layout
   documentation requires more RE work.

4. **TacticsData** — *(struct complete; the one access still raw.)* Single access point; low priority. Document the offset in a
   comment and move on.
