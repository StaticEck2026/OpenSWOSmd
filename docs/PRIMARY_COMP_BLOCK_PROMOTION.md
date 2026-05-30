# `primary_comp_block` → typed semantic fields (hand-off)

This document tracks the work to retire the `primary_comp_block`
byte-blob field in the JSON save format and replace it with typed
semantic fields. The blob currently encodes 154 bytes of
`competitionFileBuffer` (158 on 64-bit because of `namePtr` widening)
straight as JSON byte arrays — which means cross-target saves are
broken, because the DIYCompetition struct field offsets are
host-dependent (`namePtr` is 4 vs 8 bytes).

## Where we are right now

- Same-target round-trip works on both targets (Amiga 32-bit, SDL
  64-bit). Verified end-to-end on Amiga via fresh new-career → save →
  load → CONTINUE CAREER (2026-05-30).
- Cross-target round-trip is broken. Concrete bite:
  `DIYCompetition.numTeams` is at struct offset 50 on 32-bit and offset
  54 on 64-bit. A 64-bit SDL save writes `[54, 0]` at byte-array index
  50 (the SDL byte that happens to live there); a 32-bit Amiga load
  reads those bytes into its own offset-50 `numTeams` and gets
  `BE uint16 = 13824` — runaway loop, panic 0x6006 in `_FindTeamInDIY`.
- This was diagnosed live via FS-UAE gdb on 2026-05-30. The bug is
  documented in detail at the bottom of this file under
  "diagnosis details — keep for context".
- The temporary `panic6006Snapshot` diagnostic + the
  `SWOS_DBG → KPrintF` Amiga routing experiment have been reverted.

## The job

Replace the `primary_comp_block` byte-blob entry in
`s_career_scalars[]` (`src/save_format_json.c`) with named-address
fields whose load resolves to the host's *actual* struct offset on
each target. Once done, the cross-target trap above goes away.

The blob covers `&competitionFileBuffer + 0 .. + 154` on 32-bit
(`+158` on 64-bit). Within that span the same memory is read three
different ways at runtime:

| View | Pattern | Examples |
|------|---------|----------|
| Typed struct cast | `((DIYCompetition *)&competitionFileBuffer)->X` | `numTeams`, `competitionName`, `substitutesCount`, `namePtr` |
| Named globals (per-slot state) | direct global symbol | `primaryCompWinnerTeam`, `primaryCompSquadViewParam`, `primaryCompSlotStateA/B/C` |
| Named globals (league config) | direct global symbol | `diyTeamMinDivisionIndex`, `diyTeamMaxDivisionOffset`, `leaguePromotionCount`, `leagueRelegationCount`, `leagueWarningsEnabled` |

The proposed JSON shape replaces the byte blob with three semantically
grouped objects:

```json
"primary_competition":  { /* DIYCompetition fields */ },
"primary_slot_state":   { /* per-slot UI/state globals */ },
"primary_league_config":{ /* league promotion/relegation/warnings */ }
```

Each field is emitted from its named global *address* (or struct
accessor) and loaded the same way — so on each target the bytes land
at that target's correct struct-relative offset. Cross-target loads
work because no shared byte position is assumed.

## Audit needed before code is written

This is **field discovery** — write no code yet. The goal is a
comprehensive list of every byte position in `competitionFileBuffer +
0..+153` that the runtime reads or writes, what name(s) it's read
under, and what semantic type it carries.

### Step 1 — Enumerate named globals in the region

The overlay defines the named globals inline in
`src/swos_globals_career_overlay.c`. From the existing read at
`competitionFileBuffer` (line 908+) walk down through
`leagueWarningsEnabled` (line 961). Note specifically:

- Every `.globl`/label inside the run, with its `.skip` size
- The conditional 4-byte 64-bit pad after `diyPresetNameOfCompetition`
  (re-aligns post-namePtr globals so they match the grown struct)
- The trailing 58-byte `.skip` after `leagueWarningsEnabled` — almost
  certainly hides multiple unnamed fields the runtime reads

### Step 2 — Enumerate DIYCompetition struct field reads

The runtime casts `competitionFileBuffer` to `DIYCompetition *` in
many call sites. Find every such read/write across the codebase.
`_FindTeamInDIY` is the obvious starting point but `_InitDIYTournament`,
`_DIY_CacheSelectedTeamPointers`, the chairman / season-end paths, and
the diy_competition / career_status / career_season / career_init / 
career_eurocups suite all participate.

Useful greps:
- `(DIYCompetition *)&competitionFileBuffer`
- `(DIYCompetition *)competitionFileBuffer`
- `compEx->` and `comp->` field references in `diy_competition.c`

`DIYCompetition`'s struct definition lives in `include/game_types.h`
(synced from `src/game_types.h`). Note pointer fields (`namePtr` and
similar) which can't be saved as raw bytes — they need symbolic save
+ lookup-table restore on load.

### Step 3 — Enumerate "tail" reads (offsets 96..153)

The 58-byte `.skip` after `leagueWarningsEnabled` is the highest-risk
area. Grep for any code that reads `competitionFileBuffer + N` or
`comp + N` for N in 96..153. Any unnamed sub-field found here needs
to be added either as a new overlay-named global or as a typed struct
field (and saved semantically).

### Step 4 — Build the field table

Output of the audit is a table:

| Byte offset (32-bit) | Byte offset (64-bit) | Name / accessor | Type | Notes |
|---|---|---|---|---|
| 0 | 0 | `primary_slot_type` (already FX) | int16 | covered |
| 2 | 2 | (`competitionOver` or struct field) | int16 | check |
| 4 | 4 | `competitionName` | char[36] | overlaps `diyPresetNameOfCompetition` |
| 40 | 40 | `namePtr` | uint8_t * | save as string identity |
| 48 | 52 | `majorTrophyBonusValue` | int16 | |
| 50 | 54 | `numTeams` | uint16 | the bug we just hit |
| ... | ... | ... | ... | ... |

Mark anything ambiguous so we can disambiguate before promoting.

## Implementation plan (after audit lands)

1. Add the three new top-level JSON objects (`primary_competition`,
   `primary_slot_state`, `primary_league_config`) emitted by name.
2. Keep `primary_comp_block` (byte blob) in parallel **during
   transition**. Verify mode (`SWOS_SAVE_FORMAT_JSON_VERIFY=1`)
   confirms typed path produces identical bytes to byte path.
3. Once verify is clean across a representative save set, delete the
   byte blob.
4. Handle `namePtr` symbolically via the existing string-pool /
   competition-name-table pattern (the same approach
   `SeasonInformations` uses for its 4 comp-name pointers, and the
   same approach `restore_comp_copy_text_ptrs_from_template` uses for
   *Copy template pointers).

## Referenced existing docs

These already cover relevant background — start here before doing the
audit:

- `docs/OVERLAPPED_DATA.md` — overlapping data regions in the
  careerFileBuffer overlay, including the
  `competitionFileBuffer`-cast-to-`DIYCompetition` pattern around line
  306.
- `docs/DIYCOMPETITIONEX_AUDIT.md` — DIYCompetitionEx field audit,
  references `competitionFileBuffer` overlay at line 157.
- `docs/64BIT_TRANSITION.md` — the 64-bit transition writeup with a
  `substitutesCount = 2` walk-through (around line 131) that's exactly
  the same class of bug as the `numTeams` one we just hit. Read this
  before designing the typed promotion.
- `docs/SAVE_DATA.md` — `competitionFileBuffer` overlay layout
  notes around line 150, including pointer-field inventory.

## Diagnosis details — keep for context

Save layout debt: `primary_comp_block` was sized 154 bytes (32-bit
literal). After the May 2026 `+4 namePtr gap` and `+320 SI pad`
overlay fixes, the bytes for `competitionFileBuffer` reach 158 on
64-bit. We added a conditional `#if UINTPTR_MAX` 154/158 size in
`save_format_json.c` (commit `dccc5c1`). That makes same-target
saves work; cross-target still breaks because the bytes encode each
host's struct layout.

The `numTeams` example: on 32-bit Amiga the field is at struct offset
50; on 64-bit SDL it's at offset 54 (`namePtr` widened 4→8 bytes
between the struct start and `numTeams`). A 64-bit save writes
`numTeams = 16` at byte 54 of the array, leaving byte 50 as whatever
non-numTeams memory was there. The Amiga then loads byte 50 into its
own `numTeams` field and reads a garbage value (`BE uint16 = 13824` in
the gdb session). `_FindTeamInDIY`'s loop runs 13824 iterations
through garbage and never matches `selTeamNumber`, falling to the
`panic 0x6006` "team not in competition" panic.

This pattern repeats for every struct field after `namePtr` (offset
0x28). The byte blob's bytes carry one host's layout; the other host
misreads them.

## Recent context (handoff bookmark)

Recent commits worth knowing about:

- `fad683d` — Remove `panic6006Snapshot` diagnostic globals. Done; on
  Amiga via sync_to_amiga_port.sh.
- `0ea44ea` then `606746a` — Tried routing `SWOS_DBG → KPrintF` on
  Amiga, reverted because `KPrintF` lives in `gcc8_c_support.c`
  which the Amiga Makefile filters out. Replaced with a fixed-global
  snapshot pattern (also now reverted).
- `138c3f7` — Walk-once dispatch (FLD_BYTES fast walker +
  fast_object_next + fast_array_next). Amiga career-load went from
  5+ minutes to ~71 seconds.
- `dccc5c1` — `primary_comp_block` size 154/158 + address fix
  (`&competitionFileBuffer`). The conditional size is what enables
  the same-target round-trip we have today.
- `7b162fc` — overlay pointer-growth pads (the +4 namePtr gap, +320
  SI pad, +8 per `*Copy` slot). These define the on-disk byte
  positions that the typed promotion will replace.

The next session should be able to start with this doc + the four
referenced docs.
