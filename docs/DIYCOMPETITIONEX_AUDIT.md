# DIYCompetitionEx offset audit

> Status (2026-09-30): **complete** -- the refactor this audit drove has landed: `DIYCompetitionEx` in `include/game_types.h` was rewritten to the SWOS2.s layout (with `DIYTournamentSlot`, `DIYTeamStats`, the `bracketScratch` union, and later the trailing `leagueStandings[64]`), and the raw offsets in `src/diy_competition.c` became typed fields (`7134415`, `4d6f653`, `44e6f81`). Kept as the record of the investigation.

This is the source-of-truth investigation for refactoring the ~400 raw
`*(uintNN_t *)(p + offset)` accesses in `src/diy_competition.c` into
typed `struct->field` accesses. Before any code changes happen, every
distinct offset needs to be mapped to the actual semantic field it
addresses, cross-referenced against the original Amiga BSS layout in
`/mnt/c/Projects/AI/SWOS/OpenSWOS/data/SWOS2.s` (referred to below as
`SWOS2.s`).

## TL;DR — the key discovery

The C struct `DIYCompetitionEx` in `include/game_types.h` was reverse-
engineered with educated guesses and is **substantially wrong** in its
extended fields. Furthermore, the audit's first pass misframed the "Type 2
slot pointer" — it is *not* one of the 5 `careerCompetitionSlotPtrTable[]`
buffers. It is an **embedded array of 0x76-byte tournament-slot records
inside `diyTournamentTeamIds`**, addressed via base + slotIdx*0x76 +
field-offset arithmetic.

Key disagreements between the C struct and the authoritative SWOS2.s BSS
layout:

| Field | C struct says | SWOS2.s says | Status |
|---|---|---|---|
| `aggregateGoals[64]` | offset `0x16E..0x1ED` | named fields `diyMatchesPlayedCount`, `diyLeagueCurrentSlot`, `diyLeagueMatchProgress`, `diyLeagueTotalMatches` live at `0x1BE..0x1C5` (inside that range) | **wrong** |
| `penaltyGoals[64]` | offset `0x1EE..0x26D` | unnamed `BSS_00_2E13E` then `diyCupTeamIds` | **wrong** |
| `monthProgressTable[98]` | offset `0x27E..0x2DF` | inside `diyCupTeamIds` (`0x24E..0x2CD`) | **wrong** |
| `cupTeamIds[64]` | offset `0x2E0..0x35F` | actually `0x24E..0x2CD` | **wrong (off by 0x92)** |
| `overlap.teamStats[24]` | offset `0x360` | not a labelled region; `clearLeagueTableStats` writes per-team stats fields starting at `0x2B4` | **wrong (off by 0xAC)** |
| `competitionName[36]` | bytes `0x04..0x27` | confirmed: a plain string buffer; the C reads at `+0xA/+0xC/+0xE` were misread — they live in different structures (fixture cache and competition init table), not the DIYCompetition struct | **misnamed in audit only — struct is correct** |

## Authoritative BSS layout from SWOS2.s line 2840

This is the original Amiga `ds.b` declarations, which encode the real
on-disk and in-memory layout. Offsets are relative to
`diyCompetitionStart` (which is also `&diyCompetitionStart` at runtime).

```
Offset    Length  Label                             Notes
------    ------  ----------------------------      ----------------------------
0x0000    0x60    diyCompetitionStart               DIYCompetition base struct
0x0060    0x02    diyMinSubstitutes                 *also written = playEachTeam in _DIY_RecomputeCompetitionDerivedFields*
0x0062    0x02    diyPointsForAWin
0x0064    0x02    diyLeagueMinSubstitutes
0x0066    0x02    diyLeagueMaxSubstitutes
0x0068    0x02    diyHasGroups
0x006A    0x04    BSS_00_2DF5E                      ⇒ totalMatchesPerSection { resetValue@+0x6A, liveCounter@+0x6C }
0x006E    0x02    diyTeamEntryOffsetsByIndex        single uint16, but stride-indexed as [64] in init helpers
0x0070    0x1A    BSS_00_2DF7A                      ⇒ cupRoundLegs/cupRoundExtraTime/cupRoundPenalties packed regions
                                                    +0x72  cup roundExtraTime[6] (uint16[6]) — confirmed init writes "1" here
                                                    +0x7C  ?
                                                    +0x7E  cup roundPenalties[6] start (uint16[6]) — confirmed init writes "1" here
0x008A    0x64    diyCupRoundResults                100-byte cup bracket result array; C accesses at +0xE8, +0xEA, +0xEC
0x00EE    0x02    diyTeamShuffleIndices             single uint16 label; stride-indexed as [64] in init helpers
0x00F0    0x3E    BSS_00_2E01E                      ⇒ unknown; C reads at +0xF4 only (sparse)
0x012E    0x1C    diyLeagueTeamIds                  14 uint16 team IDs for league mode
0x014A    0x02    diyCompetitionFormat              =1 enables league round progression (also cleared to 0 in cup init at line 1533)
0x014C    0x02    diyRoundFormat                    =2 bypasses team grid reordering
0x014E    0x70    BSS_00_2E0AE                      ⇒ contains 6×24-byte tournament slot records (shared with diyFileBufferCopy view)
                                                    these are the "slot" record fields seen in C as +0x15A..+0x172 from a base
0x01BE    0x02    diyMatchesPlayedCount             also written 0 in _DIY_RecomputeCompetitionDerivedFields
0x01C0    0x02    diyLeagueCurrentSlot              also used as "section progression" flag (1 on phase boundary)
0x01C2    0x02    diyLeagueMatchProgress            also used as phase index counter
0x01C4    0x02    diyLeagueTotalMatches             also written matchesPerSeason in derived-fields recompute
0x01C6    0x88    BSS_00_2E13E                      ⇒ league section state:
                                                    +0x1C6  halfGroupCount   (uint16)
                                                    +0x1C8  matchesPerHalf   (uint16)
                                                    +0x1CA  halfGroupsCounter (uint16)
                                                    +0x1CC  sectionMatchesProgress (uint16)
                                                    +0x1CE  phaseStartMonths[??] (uint8 array)
0x024E    0x80    diyCupTeamIds                     64 uint16 team IDs for cup mode (NOT 0x2E0 as C struct claims!)
0x02CE    0x40    diyTeamGridPairings               64 byte-sized grid indices
0x030E    0x2BA   diyTeamShuffleOrder               698 bytes of shuffled team byte-indices
                                                    ⇒ also contains league bracket-stats fields:
                                                    +0x32E, +0x330, +0x332, +0x334, +0x336, +0x344
0x05C8    0x16C   audioInstrumentBank               !! shared with league-table-stats (see clearLeagueTableStats below)
0x0734    --      end of audioInstrumentBank        END of diyCompetition-rooted region for DIY purposes
```

**Total `diyCompetitionStart`-rooted region: 0x5C8 bytes (1480) until
audioInstrumentBank, then 0x734 (1844) including the audioInstrumentBank
overlay used by `clearLeagueTableStats`.** The current C struct claims
0x5E4 (1508) which is wrong on both ends.

## Two distinct buffer types

The C code uses pointer parameters (`a0`, `a2`, `a3`, `slot`, `base`,
`comp`, `diy`) generically as `uint8_t *`, but they point at **different
buffer types** with **different field layouts**.

### Type 1: runtime DIY work buffer — `diyCompetitionStart`
Size: 0x5C8 bytes (1480), or 0x734 (1844) including audioInstrumentBank
overlay. Layout: as documented in the table above.

C code that operates on this buffer typically uses:
- `a2 = (uint8_t *)&diyCompetitionStart.base`
- `a4 = (uint8_t *)&diyCompetitionStart`
- `a5 = (uint8_t *)&diyCompetitionStart`
- `diy = some_arg` (when called from a function that passes &diyCompetitionStart)

Field examples:
- `+0x32` → `numTeams` (per DIYCompetition base)
- `+0x60` → `diyMinSubstitutes` (or playEachTeam copy)
- `+0x14A` → `diyCompetitionFormat`
- `+0x162` → unnamed slot inside `BSS_00_2E0AE`

### Type 2: tournament slot record array — embedded in `diyTournamentTeamIds`
Size of each record: 0x76 bytes (118). Up to 6 records.
**This is NOT a `careerCompetitionSlotPtrTable[]` buffer!**

C code that operates on these uses:
- `slot = bufCopyRaw + slotIdx * 0x76` where `bufCopyRaw = (uint8_t *)diyFileBufferCopy`
- `slotPtr = ...` (iterator with `slotPtr += 0x76`)
- `a3 = a2 + slotIdx * 118` where `a2 = diyFileBufferCopy`

Each "slot" pointer is computed by adding `slotIdx * 0x76` to the
**`diyFileBufferCopy` base** — so e.g. `slot + 0x162` reads
`diyFileBufferCopy + slotIdx*0x76 + 0x162`. For slotIdx=0 this lands
inside `diyTournamentTeamIds` (which begins at offset 0x9A from
`diyFileBufferCopy`); for higher slotIdx values it walks further into
`diyTournamentTeamIds`.

The pattern was confirmed by reading:
- `_InitDIYTournament` (line 605): `slot = a2raw; ... slot += 0x76;` for 6 iterations
- `_DIY_LoadCurrentCompetitionSlot` (line 3561): `a3 += slotIdx * 118;`
- `_DIYTournament_BuildMatchesPerTeamProgressionTable` (line 730): iterates with `slotPtr += 0x76`
- `_DIYTournament_RecalculateMatchesPerTeam` (line 786): same iteration

#### Slot-record field layout (24-byte field range + tail)

| Offset (from slot) | Field | Conf |
|---|---|---|
| +0x15A | (cleared to 0 in init; purpose unknown) | M |
| +0x15C | "doubled-count" flag (cup-bracket count doubled when set) | M |
| +0x15E | cup-flag: nonzero=cup, 0=league | H |
| +0x160 | seedingType (cleared to 0 in init) | M |
| +0x162 | numTeams in slot | H |
| +0x164 | "next slot" / division factor | M |
| +0x166 | playEachTeam multiplier (=1 in init) | H |
| +0x168 | constant 2 in init | L |
| +0x16A | groups count | H |
| +0x16C | teamsPerGroup | H |
| +0x16E | format flag: 1=normal, 2=skip-special | M |
| +0x170 | constant 1 in init | L |
| +0x172 | format flag: 1=halve-matches | M |
| +0x174 | computed `matchesPerTeamThisSlot` | H |
| +0x176..+0x1CF | progression table (uint8 array, ~90 bytes max) | M |

The 0x76-byte stride leaves 0x76 - 0x18 = 0x5E = 94 bytes after the
0x18-byte field range — the leading +0x15A..+0x172 range, plus
+0x174 (matchesPerSlot), plus the +0x176 progression table. That
accounts for the full 118-byte stride.

### Type 3 (does NOT exist as a useful concept): `careerCompetitionSlotPtrTable[]`
The 5 entries at `careerCompetitionSlotPtrTable[0..4]` point at:
- `competitionFileBuffer` (careerFileBufferBuf+93308)
- `competitionContext` (careerFileBufferBuf+63340)
- `tertiaryCompetitionContext` (careerFileBufferBuf+66240)
- `euroCupsCompetitionContext` (careerFileBufferBuf+68424)
- `diyCompetitionContext` (careerFileBufferBuf+67332)

Each is a thin label (2-62 bytes wide in SWOS2.s) at the start of a
larger run of named globals. They are used by `_Career_*` code, not by
`diy_competition.c` slot-pointer arithmetic. These are competition
record buffers, not DIY tournament slot records.

## Per-offset map (Type 1, `diyCompetitionStart` base)

For each unique offset accessed in `diy_competition.c`, what it MEANS
when accessed via a runtime pointer to `diyCompetitionStart`.
**Confidence:** `H` = SWOS2.s confirmed; `M` = inferred from C usage;
`L` = guess.

### Base struct (offsets 0x00..0x5F)
| Offset | Field | Conf |
|---|---|---|
| 0x02 | `isActive` | H |
| 0x04..0x27 | `competitionName[36]` (plain string buffer; written by `_StringCopy`) | H |
| 0x28..0x2B | `namePtr` (raw 68k ptr to string) | H |
| 0x2C | `compId` (byte 0x2C) + `slotId` (byte 0x2D) read as uint16 word | H |
| 0x2E | `field_2E` (team configuration byte) | H |
| 0x2F | `viewRandomPhase` (current month/phase) | H |
| 0x30 | `avgSkill` | H |
| 0x31 | `strengthTier` | H |
| 0x32 | `numTeams` | H |
| 0x34 | `pitchTypeOrSeason` | H |
| 0x36 | `pitchType` | H |
| 0x38..0x39 | `startMonth`, `endMonth` | H |
| 0x3A..0x5D | `field_3A` sub-fields (resultCode, substitutesCount, slotState{A,B,C}, matchStarted, careerTeamInvolved, hasGroups, menuState, roundIterator, numTeamsInStage, matchPairsPerRound, totalMatchesInRound, roundComplete, competitionFinished, currentRoundIndex, numRounds, subRoundCounter) | H |
| 0x5E | `playEachTeam` | H |

### Extended fields (offsets 0x60..0x5C7)
| Offset | SWOS2.s region | Semantic field | Conf |
|---|---|---|---|
| 0x60 | `diyMinSubstitutes` | also: section-progression counter (decremented each section) | H |
| 0x62 | `diyPointsForAWin` | written `2` in defaults | H |
| 0x64 | `diyLeagueMinSubstitutes` | written `3` in defaults | H |
| 0x66..0x71 | `diyLeagueMaxSubstitutes` + `diyHasGroups` + `BSS_00_2DF5E` | also: cup `roundFormats[6]` (uint16[6] starting at +0x66) | M |
| 0x6A | inside `BSS_00_2DF5E` | `totalMatchesPerSection` (reset value) | H |
| 0x6C | inside `BSS_00_2DF5E` | `totalMatchesPerSection` (live counter, decrements) | H |
| 0x6E | `diyTeamEntryOffsetsByIndex` | uint16 base; stride-indexed as 64-element array elsewhere | H |
| 0x72 | inside `BSS_00_2DF7A` | cup `roundExtraTime[6]` start (uint16[6]) | M |
| 0x7C, 0x7E | inside `BSS_00_2DF7A` | cup `roundPenalties[6]` start (uint16[6]) | M |
| 0x8A..0xED | `diyCupRoundResults[100]` | cup bracket result bytes | H |
| 0xEE | `diyTeamShuffleIndices` | uint8 base; stride-indexed as 64-element byte array | H |
| 0xF0..0x12D | `BSS_00_2E01E` (62 bytes) | unknown — only +0xF4 accessed sparsely | L |
| 0x12E..0x149 | `diyLeagueTeamIds[14]` | uint16[14] | H |
| 0x14A | `diyCompetitionFormat` | also cleared to 0 by cup-init path | H |
| 0x14C | `diyRoundFormat` | sometimes loaded from `roundFormats[curRound]` (off+0x66+roundIdx*2) | H |
| 0x14E..0x1BD | `BSS_00_2E0AE` (112 bytes) | shared 6×24-byte tournament slot records (Type 2 view) | M |
| 0x1BE | `diyMatchesPlayedCount` | matches played counter | H |
| 0x1C0 | `diyLeagueCurrentSlot` | also: section-progression flag (=1 at boundary) | H |
| 0x1C2 | `diyLeagueMatchProgress` | also: phase index counter | H |
| 0x1C4 | `diyLeagueTotalMatches` | also: matchesPerSeason | H |
| 0x1C6 | `BSS_00_2E13E` start | `halfGroupCount` (uint16) | H |
| 0x1C8 | inside `BSS_00_2E13E` | `matchesPerHalf` (uint16) | H |
| 0x1CA | inside `BSS_00_2E13E` | `halfGroupsCounter` (uint16, ==0x1C8 triggers phase boundary) | H |
| 0x1CC | inside `BSS_00_2E13E` | `sectionMatchesProgress` (uint16) | H |
| 0x1CE+phaseIdx | inside `BSS_00_2E13E` | `phaseStartMonths[??]` (uint8 array) | M |
| 0x1D2 | inside `BSS_00_2E13E` | unknown | L |
| 0x24E..0x2CD | `diyCupTeamIds[64]` | uint16[64] cup team IDs | H |
| 0x2CE..0x30D | `diyTeamGridPairings[64]` | uint8[64] pairing indices | H |
| 0x30E..0x5C7 | `diyTeamShuffleOrder[698]` | uint8[698] shuffled team byte-indices | H |
| 0x32E, 0x330, 0x332, 0x334, 0x336, 0x344 | inside `diyTeamShuffleOrder` | bracket-state fields (e.g. matchCount, totalMatches) | M |
| 0x2B4..0x733 | spans `diyCupTeamIds` + `diyTeamGridPairings` + `diyTeamShuffleOrder` + `audioInstrumentBank` | **64-row × 18-byte league table** (see below) | H |

### League table overlay (clearLeagueTableStats)

`clearLeagueTableStats` writes a 64-row × 18-byte league standings table
starting at `base + 0x2B4` (where `base = &diyCompetitionStart`). Each
row is 18 bytes: 4-byte `teamSelPtr` (NOT cleared) + 14 bytes (7 uint16
stats fields cleared at +0x2B8..+0x2C4 within the row).

End of table: `0x2B4 + 64*18 = 0x2B4 + 1152 = 0x734` — exactly the end
of `audioInstrumentBank` (`0x5C8 + 0x16C = 0x734`).

**This is intentional shared-memory overlay:**
- League mode uses these 1152 bytes as the per-team standings table.
- Cup mode uses the same memory for `diyCupTeamIds` + `diyTeamGridPairings`
  + part of `diyTeamShuffleOrder`.
- The audio bank lives there too, but isn't loaded during DIY init —
  the writes happen in a phase where audio memory is reusable.

The 64-iteration count is the worst-case team max, hardcoded; it's not
a bug. The audit's earlier suspicion that "the dbf count was originally
meant to be smaller (per number of teams, not 64)" was wrong.

For our SDL port, we need either:
1. A **`DIYLeagueTable[64]` typed array overlapping the cup/grid/shuffle/audio
   regions in the runtime DIY buffer** (preserving the Amiga behaviour), or
2. A **separate per-team-stats array** allocated independently, with the
   league/cup paths refactored to use the appropriate one.

Option 2 is cleaner but requires identifying every league-table read site
and rewriting it. Option 1 is closer to a mechanical refactor.

## Slot-record (Type 2) per-offset map

For each offset accessed via a `slot = bufCopyRaw + slotIdx*0x76`
pointer, where `bufCopyRaw = (uint8_t *)diyFileBufferCopy`:

| Offset (from slot) | Meaning | Conf |
|---|---|---|
| +0x15A | (cleared to 0 in init; purpose unknown) | L |
| +0x15C | "double cup-bracket count" flag | M |
| +0x15E | cup-flag: nonzero=cup, 0=league | H |
| +0x160 | seedingType (cleared to 0 in init) | M |
| +0x162 | numTeams | H |
| +0x164 | "next slot" / division factor | M |
| +0x166 | playEachTeam multiplier | H |
| +0x168 | constant 2 in init | L |
| +0x16A | groups count | H |
| +0x16C | teamsPerGroup | H |
| +0x16E | format flag (1=normal, 2=skip) | M |
| +0x170 | constant 1 in init | L |
| +0x172 | format flag (1=halve) | M |
| +0x174 | matchesPerTeamThisSlot (computed) | H |
| +0x176..+0x1CF | progression month bytes (uint8 array) | M |

## Adjacent-globals layout (diyFileBufferCopy)

`diyFileBufferCopy` occupies offset 0..0x5F (96 bytes). It is followed
on Amiga by:
- `BSS_00_2DB6A:ds.b $3A` — 58 bytes unnamed (offset 0x60..0x99)
- `diyTournamentTeamIds:ds.b $386` — 902 bytes (offset 0x9A..0x41F)

This means a Type 2 slot pointer at `diyFileBufferCopy + 0` accesses
field +0x15A → byte 0xC0 of `diyTournamentTeamIds` (since 0x15A - 0x9A
= 0xC0). For higher slot indices, +0x15A walks deeper into the array.

The 6 tournament-slot records together span 6 × 0x76 = 0x2C4 = 708 bytes,
fitting comfortably within `diyTournamentTeamIds`'s 902-byte span.

## What about the `competitionName[36]` "puzzle"?

Earlier audit speculated that `+0xA, +0xC, +0xE` reads inside the name
buffer indicated structured data. **Resolved as a misread:** those reads
happen on different pointers, not on a DIYCompetition base.

- Line 2622-2623 (`*(uint16_t *)(a5 + 0x0C) = teamId1`): `a5 = resultPtr`,
  a fixture-cache record (12-byte stride; +0x0C/+0x0E hold team IDs).
- Line 2803-2804 (`BE_WRITE_U16(resultPtr + 0x0C, ...)`): same fixture
  cache record.
- Line 8050 (`*(uint16_t *)(a5 + 0xA)`): `a5 = table` (a competition init
  table parameter), not a DIYCompetition struct.

So `competitionName[36]` is genuinely a plain ASCII string buffer at
offsets 0x04..0x27 of `DIYCompetition`. The struct field name is correct;
no unpacking is needed.

## What this means for the refactor

**Investigation is complete; we can now redesign the struct.** Concrete
next steps:

1. **Rewrite `DIYCompetitionEx`** in `include/game_types.h`:
   - Total size = `0x734` (1844) bytes including the audioInstrumentBank-
     overlay region used by `clearLeagueTableStats` (or: redesign so the
     league table is a separate global and `DIYCompetitionEx` is `0x5C8` /
     1480 bytes).
   - Replace `aggregateGoals[64]` and `penaltyGoals[64]` with proper
     named fields for the 0x14E..0x1BD `BSS_00_2E0AE` region (which is
     mainly the embedded slot-record array, NOT cup goals).
   - Replace `monthProgressTable[98]`, `cupTeamIds[64]`, and
     `overlap.teamStats[24]` with the correct field placements.
   - Add named struct fields for `BSS_00_2DF5E` (totalMatchesPerSection
     + counter), `BSS_00_2DF7A` (cup roundExtraTime[6]/roundPenalties[6]),
     `BSS_00_2E13E` (halfGroupCount, matchesPerHalf, halfGroupsCounter,
     sectionMatchesProgress, phaseStartMonths).

2. **Define a `DIYTournamentSlot` struct** (24 bytes for the field range,
   or 118 bytes including the progression tail) and replace the slot-
   pointer arithmetic in `_InitDIYTournament` /
   `_DIY_LoadCurrentCompetitionSlot` /
   `_DIYTournament_BuildMatchesPerTeamProgressionTable` /
   `_DIYTournament_RecalculateMatchesPerTeam` with array indexing.

3. **Define a `DIYLeagueTableRow` struct** (18 bytes: 4-byte teamSelPtr +
   7 × uint16 stats) and decide whether to keep it overlaid (Amiga-style)
   or split it out into a 64-element global array.

4. **Convert offsets to fields in `src/diy_competition.c`**, one function
   at a time, with build + test (career save round-trip + match init +
   league play) after each chunk.

## Estimated effort

- Step 1 (struct redesign): ~2-3 hours.
- Step 2 (DIYTournamentSlot conversion): ~1-2 hours, ~6 functions.
- Step 3 (DIYLeagueTableRow design + migration): ~2-3 hours.
- Step 4 (per-function offset → field conversion): ~6-10 hours across
  multiple commits, ~30-40 functions.
- Plus: verification — save round-trip, match init, league play.

Realistic total: **3-5 focused sessions**. The investigation phase is
done; refactor work is now mostly mechanical with a clear target layout.

## Open questions

These remain unresolved and need attention during the refactor:

1. **`BSS_00_2E01E`** (62 bytes at offset 0xF0..0x12D): only `+0xF4` is
   accessed (sparsely). What are the other 60 bytes for? Likely cup-
   bracket scratch space.
2. **+0x1D2** inside `BSS_00_2E13E`: accessed but purpose unknown.
3. **+0x158, +0x16A, +0x16C** in Type 1 view (not Type 2 slot view) —
   read at line 274-275 in `_DIY_SortAndShuffleTeamsByValue`'s isCup
   branch when slot is `&diyCompetitionStart` rather than a tournament
   slot. Since these aliases land in the slot-record region, are they
   reading slot 0's record or a different cup-state field?
4. The exact total size of each `BSS_00_2E0AE` slot record: 24-byte
   field range is well-mapped, but +0x176..+0x1CF (90 bytes of
   progression month bytes) needs validation against actual usage.
