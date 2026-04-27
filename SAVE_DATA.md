# SWOS save file formats

This document describes the on-disk layout of every save file the game
writes, with particular attention to fields that contain absolute
pointers — these need rebuilding on load (see `_RebuildPointersAfterLoad`)
and are the things that have to be replaced when we eventually move to a
structured (JSON / YAML / tagged binary) save format.

All multi-byte integers are **big-endian** on disk (Amiga m68k convention).
The SDL build uses the `BE_*` / `PACK_BE_U16` helpers in
`src/endian_helpers.h` to read them; the macros are no-ops on a BE host so
the same code compiles for the Amiga port.

---

## Inventory

| Extension | Buffer | Save fn | Load fn | Format |
|---|---|---|---|---|
| `.CAR` / `.SEA` | `careerFileBuffer` (256 KB overlay) | `_SaveCareerFile` | `_LoadCareerFile` | Career save — header + selected-teams body |
| `.PRE` | `competitionFileBuffer` (overlay at +93308) | `_SavePresetCompetitionFile` | `_LoadPresetFile` | Preset competition save |
| `.DIY` | `competitionFileBuffer` (overlay at +93308) | `_SaveDIYFile` | `_LoadDIYFile` | DIY competition save (same layout as preset) |
| `.TAC` | `editTacticsCurrentTactics` | `_WriteTacticsFile` | `_ReadTacticsFile` | Single 370-byte tactics record |
| `.HIL` | `hilFileBuffer` | `_DoSaveHighlights` | `_LoadHilFile` | Match highlights ring — header + N goal frames |
| `.CFG` (options) | small fixed buffer | `_SaveOptions` | `_LoadOptions` | Player options (game length, music, etc.) |

---

## .CAR / .SEA — career save

The career save is a **raw memory dump** of the careerFileBuffer overlay
region (see `src/swos_globals_career_overlay.c` for the full label table).
On Amiga BSS this region was one contiguous `ds.b` block; on SDL we
restore that adjacency by aliasing every label into one
`careerFileBufferBuf[256*1024]`.

**File size formula:**
```
size = (&numSelectedTeams - &careerFileBuffer)        [= 98476]
     + numSelectedTeams * 684                          [team-data tail]
     + 2                                               [trailing word]
```

For a typical 32-team career: `98476 + 32*684 + 2 = 120366` bytes.

### Header region (offsets 0 .. 98477)

The full label list lives in `swos_globals_career_overlay.c` — what
follows is a summary by section. Offsets are relative to the start of the
file (which equals `&careerFileBuffer`).

#### 0 .. 54721 — `tmdFileBuffer` (54720 bytes)
Career-team pool. Holds up to ~80 TeamFile records plus auxiliary tables.
Format-internal — see `_LoadTeamPool` for the layout.

**Pointer fields inside tmdFileBuffer (runtime-only; should be zeroed
before save):** none currently identified, but each TeamFileEx slot
contains `inGameTeamPtr` (offset 10), `teamStatsPtr` (14), `spritesTable`
(20), `shotChanceTable` (24), `controlledPlayer` (32), `passToPlayerPtr`
(36), `passingKickingPlayer` (104), `lastHeadingTacklingPlayer` (72) —
none of these should ever survive a round-trip. They are reset by
`_InitializeInGameTeamsAndStartGame` when a match begins.

#### 54722 .. 56189 — Career counters and finance (1468 bytes)
Single-field globals: `currentSeasonShowing`, `stayingInOldClub`,
`overdraft*`, `youreSacked`, `retired`, `newBalance`, `old_balance`,
`playerPurchases / playerSales / generalRunningCosts / playerWagesBill /
gateReceipts / startingBalance(+Copy) / competitionBonuses /
sensisoftPlcSponsorship / weeklyRunningCosts / weeklyPlayerWages /
totalTeamValue`, plus the per-player `weeklyWageByPlayerIndex[128]` and
`playerFormModifiers[32]` tables, board-confidence history, qualifying
flags, `gamesPlayed / numWins / numDrawn / numLost / goalsScored /
goalsTaken`, contract negotiation state, transfer/job/national-team
counters and tables.

All scalar fields. **No pointers.**

#### 56190 .. 57481 — `careerTeam` (1292 bytes)
The user's team. TeamFile (76 bytes) + 32 × PlayerFile (38 bytes) — the
"extended" 32-slot layout that holds bench players too. On Amiga the
declared type is `TeamFileEx` (16 players) but pointer arithmetic reaches
slot 31.

**Runtime pointers inside (must be zeroed before save):** same set as
tmdFileBuffer slots above.

#### 57482 .. 61609 — `careerNumPlayers`, `seasonInformationsArray`, `diyLeagueSeasonStateTable` and friends
Mostly counters and tables. **No pointers.**

#### 61610 .. 62901 — `currentTeam` (1292 bytes)
Working slot for the AI opponent during a match. Same shape as
careerTeam. Same runtime-pointer caveat.

#### 62902 .. 63319 — Per-team / per-match state
`playerPositions`, `numPlayers`, `currentTeamNumber`,
`currentTeamSelectedPlayerPosition`, `trainingMatch*`, `lastMatch*`,
`currentTactics`, `careerLastMatchIndexByCompetitionSlot`,
`seasonFinished`, `careerFinished`. All scalars and small tables.

#### 63320 .. 63339 — `careerCompetitionSlotPtrTable[5]` ⚠ POINTERS
**This is a 5-entry array of `uint8_t *` pointers**, populated by
`_InitNewSeason` (career_init.c:1165) to point at:

```
[0] -> &competitionFileBuffer       (careerFileBufferBuf + 93308)
[1] -> &competitionContext          (careerFileBufferBuf + 63340)
[2] -> &tertiaryCompetitionContext  (careerFileBufferBuf + 66240)
[3] -> &euroCupsCompetitionContext  (careerFileBufferBuf + 68424)
[4] -> &diyCompetitionContext       (careerFileBufferBuf + 67332)
```

Saved values are absolute addresses. **Always rebuild on load** — these
five entries can be re-derived directly from the current run's
`careerFileBufferBuf` base.

#### 63340 .. 67331 — `competitionContext` + `secondaryCompetition*` (3992 bytes)
Holds the full DIYCompetition struct for slot 1 (secondary cup): name,
string offset, winner, slot states. The first 4 bytes are the struct
header `(compId, slotId)` packed BE.

⚠ **`secondaryCompStringOffset` (offset +63380, 4 bytes) is currently a
raw pointer** into the strings table (`uint32_t`). On Amiga it was
typed as a "string offset" but is actually used as an absolute address.
**Rebuild on load.**

#### 67332 .. 68423 — `diyCompetitionContext` (1092 bytes)
DIYCompetition struct for the user's DIY tournament slot. Same shape as
competitionContext.

#### 68424 .. 71323 — `euroCupsCompetitionContext` + quaternary state
DIYCompetition for slot 3 (Euro cup). ⚠ `quaternaryCompStringOffset`
(offset +68464) — same as secondaryCompStringOffset — rebuild on load.

#### 71324 .. 73291 — Competition copies, team-budget tables
33 `*Copy` competition struct snapshots
(`euroCupCopy / cupWinnersCupCopy / uefaCupCopy / …`), each storing the
runtime state of a static cup struct. The byte layout is whatever the
matching `CompetitionCup_*` struct encodes — internal pointer fields
inside *those* structs (slot pointers, table pointers) are runtime-only.

⚠ Several of the copy structs end with 4-byte trailers that look
pointer-shaped (`b8c85c56` / `44c95c56` / `…`). These need confirming
field-by-field whether they're real pointers or coincidental data.

#### 73292 .. 93305 — `teamSelectionKeyCache` workspace (~20 KB)
Variable-length cache of competition slot keys. Overlaps
`careerForeignMarketPlayers` (Amiga adjacency reuse — both at the same
bytes, used at different times). No pointers within the cache itself.

#### 93308 .. 98457 — `competitionFileBuffer` overlay (5150 bytes)
Slot 0 of the competition pointer table. Same DIYCompetition shape as
competitionContext, plus DIY-tournament fields:
`diyPresetNameOfCompetition`, `numDiyTeams`, `primaryCompWinnerTeam`,
the three `diyTeamIdListType[012]` arrays, etc.

⚠ `primaryCompStringOffset` (offset +93348) — same pattern as the other
`*CompStringOffset` fields. **Rebuild on load.**

#### 98458 .. 98477 — Save header + global config + RNG state
- `saveFileVersion` (2 bytes) — must be `0x0012`
- `saveFileMagic1..3` (12 bytes) — `"CJ0611143807"`
- `saveFileAmigaType` (4 bytes) — must be `0x00100000` (Amiga binary's
  j_start entry-point address; **this is a vestigial field** that the
  validator still checks)
- `numSelectedTeams` (2 bytes) — body team count

Plus `gameLength`, `autoReplays`, `menuMusic`, `autoSaveHighlights`,
`allPlayerTeamsEqual`, `pitchType`, `seed2 / randXorIndex2 / randXorKey2 /
newRandomSeedCopy / randomSeed`, and the 6 user tactics
(`teamTacticsUSER[A-F]`, 370 bytes each).

**No pointers here.**

### Body region (offsets 98478 .. EOF)

`numSelectedTeams * 684` bytes of TeamFile records, one per selected team.
The Amiga BSS overlays the named labels `selTeamsPtr` (= start),
`selTeamIsComputerFlag` (+4), `selTeamNameBase` (+5),
`selTeamImportField` (+25), `selTeamSocksColor` (+30),
`selTeamPlayerIndices` (+60, 1308 bytes), `editTeamsBuffer` (+1368,
43778 bytes) onto this region — they are **field-name labels into the
first TeamFile entry plus working buffers that re-use the array memory**,
not separate variables.

Each 684-byte TeamFile slot has the same internal pointer fields
(`inGameTeamPtr`, `spritesTable`, etc.) as `careerTeam` — must be zeroed
before save / runtime-rebuilt after load.

### Pointer-field summary for .CAR

| Offset | Field | Type | Rebuild source |
|---|---|---|---|
| 63320 | `careerCompetitionSlotPtrTable[0]` | `uint8_t *` | `&competitionFileBuffer` |
| 63324 | `careerCompetitionSlotPtrTable[1]` | `uint8_t *` | `&competitionContext` |
| 63328 | `careerCompetitionSlotPtrTable[2]` | `uint8_t *` | `&tertiaryCompetitionContext` |
| 63332 | `careerCompetitionSlotPtrTable[3]` | `uint8_t *` | `&euroCupsCompetitionContext` |
| 63336 | `careerCompetitionSlotPtrTable[4]` | `uint8_t *` | `&diyCompetitionContext` |
| 63380 | `secondaryCompStringOffset` | `uint32_t` (ptr) | string-table lookup by competition ID |
| 68464 | `quaternaryCompStringOffset` | `uint32_t` (ptr) | string-table lookup by competition ID |
| 93348 | `primaryCompStringOffset` | `uint32_t` (ptr) | string-table lookup by competition ID |
| various | tertiaryCompStringOffset | `uint32_t` (ptr) | string-table lookup |
| various inside copy structs | trailing 4-byte slots | TBD | needs per-struct audit |

**Plus runtime-only fields that should be zeroed before save** (or after
load): everything in any TeamFile slot at field offsets
`{10, 14, 20, 24, 32, 36, 72, 104}` — these are pointers to in-game
runtime structures and should never appear in a save file.

### Runtime structures NOT in the save

Several runtime structs are populated by the game code from the saved
data, but are not themselves part of the file. Loading a .CAR does not
restore these — they're rebuilt on demand:

- `diyCompetitionStart` (separate 4 KB BSS buffer outside the careerFileBuffer
  overlay). Rebuilt by `_DIY_LoadCurrentCompetitionSlot` from
  `diyFileBufferCopy` (which itself comes from `competitionFileBuffer`
  via `_Career_LoadCompetitionSlotStateFromBuffer`).
  **VIEW SQUADS reads from this struct.** If we don't repopulate it
  post-load, `_StatsViewSquads` reads zeros and panics on `_VerifyTeamLoaded(0)`.
- `topTeamData / bottomTeamData` (TeamGeneralInfo runtime structs).
  Rebuilt by `_InitializeInGameTeamsAndStartGame` at match start.
- `diyFileBufferCopy / diyCompetitionStart` workspace buffers.
- All sprite tables (`team1SpritesTable / team2SpritesTable / allSpritesArray`).

---

## .PRE / .DIY — preset / DIY competition save

Same byte layout as the .CAR header region, but rooted at
`competitionFileBuffer` (the slot-0 buffer inside the careerFileBuffer
overlay). Holds a single DIYCompetition struct plus the team list for
that competition.

**File size:**
```
size = (&numSelectedTeams - &competitionFileBuffer) + numDiyTeams * 684 + 2
```

(`&numSelectedTeams - &competitionFileBuffer` = `98476 - 93308` = 5168
bytes of header, then numDiyTeams × 684 of team data, then 2.)

**Pointer fields:** same `*StringOffset` issue as the parent .CAR.
careerCompetitionSlotPtrTable is *not* in this file — it's only in .CAR.

---

## .TAC — single tactics record

A flat 370-byte dump of one `TeamTactics` struct. No pointers, no header.

`TeamTactics` layout: 9-byte name + 9 × 35-byte
`TacticsPositionEntry[5][7]` arrays + 10-byte zone weights + 1-byte
defender-marking flag = 370 bytes total.

---

## .HIL — match highlights

Variable-size record describing one match's worth of replay frames.

```
size = hilNumGoals * 19000 + 2254
```

`hilFileBuffer` layout (from `hilStartOfGoalsBuffer` / `inGameHilBuffer`
adjacent BSS labels — see `src/swos_globals.c`):
- 2254 bytes of header (`hilStartOfGoalsBuffer` region) — match
  metadata, scoreboard, frame-count tables.
- N × 19000 bytes of replay frames (one per goal).

**Pointer fields:** TBD — needs a dedicated audit when we add highlight
save round-trips. Likely candidates: any `Sprite *` arrays inside the
frame format.

---

## .CFG — options

`_SaveOptions / _LoadOptions` write/read a small fixed-size buffer
holding `gameLength`, `autoReplays`, `menuMusic`, `autoSaveHighlights`,
`allPlayerTeamsEqual`, `pitchType`. All scalars. **No pointers.**

---

## Migration plan to structured save format

When we move to JSON / YAML / tagged binary (per CLAUDE.md long-term
roadmap step 4), every pointer field in this document becomes a
**symbolic reference** in the new format:

- `careerCompetitionSlotPtrTable[N]` → omitted entirely (purely runtime;
  always re-derived from the slot identity).
- `*CompStringOffset` → `{ "stringId": "<symbolic name>" }`.
- All runtime pointers inside TeamFile / TeamGeneralInfo /
  hilFileBuffer → omitted.

Scalar / counter / table fields ported field-by-field. The file becomes
human-inspectable and the format stays valid across binary rebuilds and
across architectures (Amiga m68k ↔ SDL x86 ↔ future ports) by
construction.

Until that lands, `_RebuildPointersAfterLoad()` is the bridge: every
pointer field listed above gets recomputed from a stable identity right
after the raw load completes.
