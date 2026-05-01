# DIYCompetitionEx offset audit

This is the source-of-truth investigation for refactoring the ~400 raw
`*(uintNN_t *)(p + offset)` accesses in `src/diy_competition.c` into
typed `struct->field` accesses. Before any code changes happen, every
distinct offset needs to be mapped to the actual semantic field it
addresses, cross-referenced against the original Amiga BSS layout in
`/mnt/c/Projects/AI/SWOS/SWOS_C/data/SWOS2.s` (referred to below as
`SWOS2.s`).

## TL;DR — the key discovery

The C struct `DIYCompetitionEx` in `src/game_types.h` was reverse-
engineered with educated guesses and is **substantially wrong** in its
extended fields. Key disagreements between the C struct and the
authoritative SWOS2.s BSS layout:

| Field | C struct says | SWOS2.s says | Status |
|---|---|---|---|
| `aggregateGoals[64]` | offset `0x16E..0x1ED` | the named fields `diyMatchesPlayedCount`, `diyLeagueCurrentSlot`, `diyLeagueMatchProgress`, `diyLeagueTotalMatches` live at `0x1BE..0x1C5` (inside that range) | **wrong — partially** |
| `penaltyGoals[64]` | offset `0x1EE..0x26D` | unnamed `BSS_00_2E13E` then `diyCupTeamIds` | **wrong** |
| `monthProgressTable[98]` | offset `0x27E..0x2DF` | inside `diyCupTeamIds` (`0x24E..0x2CD`) | **wrong** |
| `cupTeamIds[64]` | offset `0x2E0..0x35F` | actually `0x24E..0x2CD` | **wrong (off by 0x92)** |
| `overlap.teamStats[24]` | offset `0x360` | not a labelled region; `clearLeagueTableStats` writes per-team stats fields starting at `0x2B4` | **wrong (off by 0xAC)** |
| `competitionName[36]` | bytes `0x04..0x27` of base | C code reads `uint16_t` values at `0xA, 0xC, 0xE` inside this range, treating it as structured data — not a plain name string | **misnamed** |

## Authoritative BSS layout from SWOS2.s line 2840

This is the original Amiga `ds.b` declarations, which encode the real
on-disk and in-memory layout. Offsets are relative to
`diyCompetitionStart` (which is also `&diyCompetitionStart` at runtime).

```
Offset    Length  Label                             Notes
------    ------  ----------------------------      ----------------------------
0x0000    0x60    diyCompetitionStart               DIYCompetition base struct
0x0060    0x02    diyMinSubstitutes
0x0062    0x02    diyPointsForAWin
0x0064    0x02    diyLeagueMinSubstitutes           NOT a cup/league union
0x0066    0x02    diyLeagueMaxSubstitutes
0x0068    0x02    diyHasGroups
0x006A    0x04    BSS_00_2DF5E                      unnamed; C code reads at +0x6A and +0x6C (as 2 uint16s)
0x006E    0x02    diyTeamEntryOffsetsByIndex        single uint16, not array — but C code uses as array via stride
0x0070    0x1A    BSS_00_2DF7A                      unnamed; C reads/writes at +0x72, +0x7C, +0x7E
0x008A    0x64    diyCupRoundResults                100-byte cup bracket result array; C accesses at +0xE8, +0xEA, +0xEC
0x00EE    0x02    diyTeamShuffleIndices             single uint16; C code uses as array via stride
0x00F0    0x3E    BSS_00_2E01E                      unnamed; C reads at +0xF4
0x012E    0x1C    diyLeagueTeamIds                  14 uint16 team IDs for league mode
0x014A    0x02    diyCompetitionFormat              =1 enables league round progression
0x014C    0x02    diyRoundFormat                    =2 bypasses team grid reordering
0x014E    0x70    BSS_00_2E0AE                      LARGE unnamed run; C reads many fields here:
                                                    +0x15A, +0x15C, +0x15E, +0x160, +0x162, +0x164, +0x166, +0x168
                                                    +0x16A, +0x16C, +0x16E, +0x170, +0x172, +0x174, +0x176
0x01BE    0x02    diyMatchesPlayedCount
0x01C0    0x02    diyLeagueCurrentSlot
0x01C2    0x02    diyLeagueMatchProgress
0x01C4    0x02    diyLeagueTotalMatches
0x01C6    0x88    BSS_00_2E13E                      unnamed; C reads at +0x1C6, +0x1C8, +0x1CA, +0x1CC, +0x1D2
                                                    AND clearLeagueTableStats iterates here writing 18-byte-stride
                                                    DIYTeamStats records starting at +0x2B4
0x024E    0x80    diyCupTeamIds                     64 uint16 team IDs for cup mode (NOT 0x2E0 as C struct claims!)
0x02CE    0x40    diyTeamGridPairings               64 byte-sized grid indices
0x030E    0x2BA   diyTeamShuffleOrder               698 bytes of shuffled team byte-indices
                                                    C reads at +0x32E, +0x330, +0x332, +0x334, +0x336, +0x344
0x05C8    --      audioInstrumentBank starts        END of diyCompetition-rooted region
```

**Total `diyCompetitionStart`-rooted region: 0x5C8 bytes (1480).** The
current C struct claims 0x5E4 (1508) which is 28 bytes too long.

## Two distinct buffer types — they DO NOT share the same layout

The C code uses pointer parameters (`a0`, `a2`, `a3`, `slot`, `base`,
`comp`, `diy`) generically as `uint8_t *`, but they point at **different
buffer types** with **different field layouts**. This is the single
biggest source of confusion.

### Type 1: runtime DIY work buffer — `diyCompetitionStart`
Size: 0x5C8 bytes. Layout: as documented in the table above.

C code that operates on this buffer typically uses:
- `a2 = (uint8_t *)&diyCompetitionStart.base`
- `a2raw = (uint8_t *)&diyCompetitionStart.base`
- `diy = some_arg` (when called from a function that passes &diyCompetitionStart)

Field examples:
- `+0x32` → `numTeams` (per DIYCompetition base)
- `+0x60` → `diyMinSubstitutes`
- `+0x14A` → `diyCompetitionFormat`
- `+0x162` → unnamed slot inside `BSS_00_2E0AE` (see Type 2 below for the same offset's meaning when pointer is a slot)

### Type 2: competition slot buffer — `competitionFileBuffer` / `competitionContext` / etc.
Size: ~XXXX bytes (TBD).

C code that operates on these buffers typically uses:
- `slot = some_arg` (e.g. from `careerCompetitionSlotPtrTable[i]`)
- `slotPtr = ...`
- `a3 = ...` when `a3` represents an "asm a3 = competition slot" parameter
- Sometimes `a0` when called with a slot pointer

Same numeric offsets mean **different fields**. Examples observed:
- `slot + 0x162` → "teams in slot" (numTeams) — line 217, 644
- `slot + 0x166` → "playEachTeam" (which is at +0x5E in the runtime buffer's base struct) — line 3626
- `slot + 0x15E` → "cup flag" — lines 270, 645
- `slot + 0x160` → "seedingType" — line 3349

The slot buffer appears to embed a DIYCompetition-shaped record at
**slot + 0x130** (because `numTeams` at +0x32 of the embedded record
lands at slot + 0x162 = 0x130 + 0x32). Need to confirm this by
inspecting the actual save layout (SAVE_DATA.md hinted at compId/slotId
being the first 4 bytes of `competitionContext` — that doesn't match a
0x130 prefix and needs reconciling).

## Per-offset map (work-in-progress — verify each before refactor)

For each unique offset accessed in `diy_competition.c`, what it MEANS
when accessed via a runtime pointer (Type 1) vs a slot pointer (Type 2).
**Confidence:** `H` = SWOS2.s confirmed; `M` = inferred from C usage;
`L` = guess.

### Base struct (offsets 0x00..0x5F, applies to both buffer types)
| Offset | Field | Conf |
|---|---|---|
| 0x02 | `isActive` | H |
| 0x0A, 0x0C, 0x0E | inside `competitionName[]` — but used as structured uint16 fields (purpose unknown) | L |
| 0x2C | `compId` (byte 0x2C) + `slotId` (byte 0x2D) read as uint16 word | H |
| 0x32 | `numTeams` | H |
| 0x34 | `pitchTypeOrSeason` | H |
| 0x36 | `pitchType` | H |
| 0x48 | `hasGroups` | H |
| 0x4C | `roundIterator` | H |
| 0x4E | `numTeamsInStage` | H |
| 0x56 | `competitionFinished` | H |

### Type 1 (runtime buffer) extended fields, 0x60..0x5C7
| Offset | SWOS2.s field | C usage observation | Conf |
|---|---|---|---|
| 0x60 | `diyMinSubstitutes` | `*(a2raw + 0x60) = playEachTeam` (working copy) | H |
| 0x62 | `diyPointsForAWin` | written `2` in defaults | H |
| 0x64 | `diyLeagueMinSubstitutes` | written `3` in defaults | H |
| 0x66 | `diyLeagueMaxSubstitutes` | written `5` in defaults | H |
| 0x68 | `diyHasGroups` | cleared in defaults | H |
| 0x6A | inside `BSS_00_2DF5E` — used as `totalMatchesPerSection` | M |
| 0x6C | inside `BSS_00_2DF5E` — used as `totalMatchesPerSection` copy | M |
| 0x6E | `diyTeamEntryOffsetsByIndex` (single uint16, but stride-indexed) | H |
| 0x72, 0x7C, 0x7E | inside `BSS_00_2DF7A` — purposes vary | L |
| 0x8A..0xED | `diyCupRoundResults[100]` | byte/word indexing into cup bracket results | H |
| 0xEE | `diyTeamShuffleIndices` (single uint16, stride-indexed in code) | H |
| 0xF0..0x12D | `BSS_00_2E01E` (62 bytes unnamed) | L |
| 0x12E..0x149 | `diyLeagueTeamIds[14]` | H |
| 0x14A | `diyCompetitionFormat` | H |
| 0x14C | `diyRoundFormat` | H |
| 0x14E..0x1BD | `BSS_00_2E0AE` (112 bytes unnamed) — but Type 2 sees this region differently | L |
| 0x1BE | `diyMatchesPlayedCount` | H |
| 0x1C0 | `diyLeagueCurrentSlot` | H |
| 0x1C2 | `diyLeagueMatchProgress` | H |
| 0x1C4 | `diyLeagueTotalMatches` | H |
| 0x1C6..0x24D | `BSS_00_2E13E` (136 bytes unnamed) — clearLeagueTableStats writes 18-byte-stride records starting at +0x2B4 covering this range AND beyond into diyCupTeamIds AND diyTeamGridPairings AND audioInstrumentBank | M |
| 0x24E..0x2CD | `diyCupTeamIds[64]` | H |
| 0x2CE..0x30D | `diyTeamGridPairings[64]` | H |
| 0x30E..0x5C7 | `diyTeamShuffleOrder[698]` | H |

### Type 2 (slot buffer) extended fields
A slot buffer holds the same kind of data but with a 0x130-ish-byte
prefix in front. Same numeric offset has different meaning:

| Offset on `slot` | Meaning | Conf |
|---|---|---|
| 0x14A | `competitionFormat` (also at 0x14A in Type 1 — these match!) | M |
| 0x14C | `roundFormat` (matches Type 1) | M |
| 0x15E | "cup flag" / `competitionType` | M |
| 0x160 | `seedingType` | M |
| 0x162 | `numTeams` | M |
| 0x164 | "next slot in init" | L |
| 0x166 | `playEachTeam` (the runtime buffer's offset 0x5E shifted by 0x108) | M |

Hmm — the offsets at the **end of Type 1's range** (0x14A, 0x14C) match
Type 2's offsets for the SAME meaning, suggesting that the slot buffer
may not actually have a 0x130 prefix shifting everything. Instead, the
slot buffer might **end where the runtime buffer's first half ends**,
and the slot's offsets for `numTeams` etc. are duplicates (the slot
maintains its own copies that are written to at slot-level).

This needs hands-on investigation to resolve. **Until it's resolved, the
refactor for Type 2-pointer accesses cannot proceed safely.**

## clearLeagueTableStats — the big puzzle

```
move.w  #$3F,d0
loc_12A9BC:
    clr.w   $2B8(a2)    ; +0x2B8..+0x2C4 cleared (7 words)
    ...
    clr.w   $2C4(a2)
    adda.l  #$12,a2     ; advance pointer by 18 bytes
    dbf     d0,loc_12A9BC
```

64 iterations × 18-byte stride × 14 written bytes per row, starting at
`a2 + 0x2B8`. The "table" begins at offset 0x2B4 (the +4 inside the
write pattern accounts for skipping a 4-byte teamSelPtr field at the
start of each row).

But the table extends from 0x2B4 through `0x2B4 + 64*0x12 = 0x734` —
that's PAST `audioInstrumentBank` (at 0x5C8). Either:
- the asm has a bug (no symptom because the write happens before the
  audio bank is loaded?),
- this "league table" is genuinely 1152 bytes long and intentionally
  shares memory with audio buffers across game phases,
- the dbf count was originally meant to be smaller (per number of
  teams, not 64).

The third explanation seems most likely — the loop probably *should*
iterate `numTeams` times, and 64 is a worst-case upper bound that
happens to be safe because audioInstrumentBank isn't in use during
DIY init. Needs confirmation by reading the actual asm callsite of
`DIY_InitCompetitionDefaults` and surrounding code.

## What this means for the refactor

**Cannot proceed mechanically.** Concrete next steps before any code
changes:

1. **Resolve Type 1 vs Type 2 layout** — read the slot-buffer
   layout from `careerCompetitionSlotPtrTable[]` initialization in
   `_Career_SetCurrentCompetitionSlot` and the asm equivalent
   (`Career_SetCurrentCompetitionSlot` in SWOS2.s). Document slot
   buffer layout in this file.
2. **Resolve `BSS_00_*` regions** — each unnamed BSS run (0x6A-0x6D,
   0x70-0x89, 0xF0-0x12D, 0x14E-0x1BD, 0x1C6-0x24D) holds *something*
   that the C code accesses. Each unnamed slot needs a real name based
   on usage analysis.
3. **Resolve the `competitionName[36]` puzzle** — base offset 0xA, 0xC,
   0xE accesses inside what claims to be a string buffer. Either it's
   not a string, or it's a packed metadata-then-string layout.
4. **Resolve the `clearLeagueTableStats` overflow** — confirm whether
   the 64-iteration count is intentional and the overlap with adjacent
   buffers is expected.
5. Once the layout is locked down, **rewrite `DIYCompetitionEx`** in
   `src/game_types.h` with corrected fields. Probably also define a
   separate `DIYCompetitionSlot` struct for the Type 2 buffer.
6. **Then** convert offsets to fields in `src/diy_competition.c`, one
   function at a time, with build + test after each chunk.

## Estimated effort

- Steps 1-4 (investigation & layout doc): ~3-4 hours of careful asm
  reading.
- Step 5 (struct redesign): ~1-2 hours.
- Step 6 (conversion): ~6-10 hours across multiple commits.
- Plus: verification — save round-trip, match init, league play.

Realistic total: **3-5 focused sessions**. Worth it for the precision
the user asked for — but a chunk of dedicated time, not something to
squeeze in beside other work.
