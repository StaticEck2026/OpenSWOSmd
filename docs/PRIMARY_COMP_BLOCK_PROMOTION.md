# `primary_comp_block` → typed semantic fields (hand-off)

This document tracks the work to retire the `primary_comp_block`
byte-blob field in the JSON save format and replace it with typed
semantic fields. The blob currently encodes 154 bytes of
`competitionFileBuffer` (158 on 64-bit because of `namePtr` widening)
straight as JSON byte arrays — which means cross-target saves are
broken, because the DIYCompetition struct field offsets are
host-dependent (`namePtr` is 4 vs 8 bytes).

## Status

- **Cross-target scalar fix LANDED (2026-05-31).** The single
  `primary_comp_block` byte blob in `s_career_scalars[]`
  (`src/save_format_json.c`) was split at the `namePtr` boundary into
  four entries — `primary_competition_over` (scalar),
  `primary_competition_name` (string), `primary_name_ptr` (raw bytes,
  host-width), and `primary_comp_tail` (a **110-byte pointer-free blob
  anchored AFTER namePtr**, identical length on both hosts). Each piece
  now lands at its host's actual struct offset on load, so the
  `numTeams`/0x6006 class of cross-target bug is gone. Compile-time
  `typedef char[...]` invariants assert the tail anchor + length on both
  arches (`offsetof(compId)+110 == 154/158`, namePtr at 0x28, tail
  contiguous with namePtr). Builds clean `-m32` and `-m64`.
- **`namePtr` is intentionally still saved as raw bytes** (the
  `primary_name_ptr` entry) — same pointer-to-disk behaviour as the old
  blob (works same-binary, stale cross-rebuild). Making it symbolic is
  the remaining follow-up; see the strengthened constraints below.
- **No `opaque_data` fallback exists** (the format is fully semantic —
  `save_format_json.c:4/15`), so `primary_comp_block` was the *sole*
  carrier of the competition header. **Old JSON saves are now
  incompatible**: the retired `primary_comp_block` key is ignored on
  load and the new keys are absent, so the competition header loads as
  all-zero. Regenerate test saves.

## Where we are right now

- Same-target round-trip works on both targets (Amiga 32-bit, SDL
  64-bit). Verified end-to-end on Amiga via fresh new-career → save →
  load → CONTINUE CAREER (2026-05-30).
- Cross-target round-trip was broken before the 2026-05-31 split.
  Concrete bite:
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
- **Field-discovery audit is complete (2026-05-30)**, grounded in the
  SWOS2.s BSS layout (source of truth). See "Audit results" below for
  the authoritative field table and five documentation/overlay mistakes
  that were corrected.

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

> **Superseded by the audit:** these three "views" are the **same
> bytes** under different names, not three independent regions (see
> Audit results → "the two name schemes are the SAME bytes"). The
> grouping above is fine as cosmetic JSON structure, but each byte must
> be emitted exactly once. The recommended emission keys off the
> `DIYCompetition` struct accessor — see "Implementation plan" below.

## Audit results (2026-05-30) — SWOS2.s authoritative

Field discovery is **done**. The authoritative source is the SWOS2.s
BSS section (`C:\Projects\AI\SWOS\SWOS_C\data\SWOS2.s`, label
`competitionFileBuffer:` at line 129264). That `ds.b` run is reproduced
below verbatim and cross-referenced against the `DIYCompetition` /
`DIYCompetitionEx` struct views and every runtime read/write found in
`src/`.

### Authoritative SWOS2.s BSS layout (competitionFileBuffer run)

```
competitionFileBuffer        ds.b $2     ; 0x00  competitionType
competitionOver              ds.b $2     ; 0x02  isActive / "1 = Over"
diyPresetNameOfCompetition   ds.b $24    ; 0x04  competitionName[36]  (NOTE: 0x24 = 36, not 44)
primaryCompStringOffset      ds.b $4     ; 0x28  *** the name pointer (== struct namePtr) ***
BSS_01_16E0B4                ds.b $4     ; 0x2C  compId, slotId, field_2E, viewRandomPhase
majorTrophyBonusValue        ds.b $1     ; 0x30  avgSkill (BYTE — read via move.b)
BSS_01_16E0B6                ds.b $1     ; 0x31  strengthTier
numDiyTeams                  ds.b $2     ; 0x32  numTeams
BSS_01_16E0BE                ds.b $6     ; 0x34  pitchTypeOrSeason, pitchType, startMonth, endMonth
primaryCompWinnerTeam        ds.b $2     ; 0x3A  resultCode
primaryCompSquadViewParam    ds.b $2     ; 0x3C  substitutesCount
primaryCompSlotStateA        ds.b $2     ; 0x3E  slotStateA
primaryCompSlotStateB        ds.b $2     ; 0x40  slotStateB
primaryCompSlotStateC        ds.b $2     ; 0x42  slotStateC
BSS_01_16E0D8                ds.b $10    ; 0x44  matchStarted, careerTeamInvolved, hasGroups,
                                         ;       menuState, roundIterator, numTeamsInStage,
                                         ;       matchPairsPerRound, totalMatchesInRound
diyTeamMinDivisionIndex      ds.b $2     ; 0x54  roundComplete
diyTeamMaxDivisionOffset     ds.b $2     ; 0x56  competitionFinished
leaguePromotionCount         ds.b $2     ; 0x58  currentRoundIndex
leagueRelegationCount        ds.b $2     ; 0x5A  numRounds
BSS_01_16E0E4                ds.b $4     ; 0x5C  subRoundCounter, playEachTeam
leagueWarningsEnabled        ds.b $2     ; 0x60  minSubstitutes (DIYCompetitionEx)
BSS_01_16E11E                ds.b $38    ; 0x62  pointsForAWin (0x62) + matchData head (0x64..)
diyTeamIdListType2           ds.b $94    ; 0x9A  end of the competition-header region
```

**Key insight — the two "name schemes" are the SAME bytes.** Every
named global in this run (`primaryCompWinnerTeam`, `primaryCompSlotState*`,
`leaguePromotionCount`, `leagueWarningsEnabled`, …) is just an alias for
a `DIYCompetition`/`DIYCompetitionEx` struct field at the same offset.
`competitionFileBuffer` is cast to `(DIYCompetitionEx *)` all over the
codebase (career_status.c:1730/2008/2582, menus.c:6366,
diy_competition.c:9733; menus.c:6359 even comments "competitionFileBuffer
mirrors diyCompetitionStart's struct view"). The named global is the name
used when the buffer holds the *primary career slot*; the struct field is
the name used when it's the *active competition* — one physical byte,
two code paths.

### Authoritative field table

| 32-bit off | 64-bit off | SWOS2.s label | DIYCompetition(Ex) field | type | save handling |
|---|---|---|---|---|---|
| 0x00 | 0x00 | competitionFileBuffer | base.competitionType | int16 | `primary_slot_type` (already FX) |
| 0x02 | 0x02 | competitionOver | base.isActive | int16 | scalar |
| 0x04 | 0x04 | diyPresetNameOfCompetition | base.competitionName[36] | char[36] | string / bytes |
| 0x28 | 0x28 | **primaryCompStringOffset** | base.namePtr | ptr (4→8) | **SYMBOLIC** (see below) |
| 0x2C | 0x30 | BSS_01_16E0B4 | compId/slotId/field_2E/viewRandomPhase | 4×uint8 | scalars |
| 0x30 | 0x34 | majorTrophyBonusValue | base.avgSkill | **uint8** (move.b) | scalar |
| 0x31 | 0x35 | BSS_01_16E0B6 | base.strengthTier | uint8 | scalar |
| 0x32 | 0x36 | numDiyTeams | base.numTeams | int16 | scalar ← **the bug we hit** |
| 0x34 | 0x38 | BSS_01_16E0BE | pitchTypeOrSeason/pitchType/startMonth/endMonth | int16+2×int8 | scalars |
| 0x3A | 0x3E | primaryCompWinnerTeam | base.resultCode | int16 | scalar |
| 0x3C | 0x40 | primaryCompSquadViewParam | base.substitutesCount | int16 | scalar |
| 0x3E | 0x42 | primaryCompSlotStateA | base.slotStateA | int16 | scalar |
| 0x40 | 0x44 | primaryCompSlotStateB | base.slotStateB | int16 | scalar |
| 0x42 | 0x46 | primaryCompSlotStateC | base.slotStateC | int16 | scalar |
| 0x44 | 0x48 | BSS_01_16E0D8 (16B) | matchStarted…totalMatchesInRound (8×int16) | int16[8] | scalars |
| 0x54 | 0x58 | diyTeamMinDivisionIndex | base.roundComplete | int16 | scalar |
| 0x56 | 0x5A | diyTeamMaxDivisionOffset | base.competitionFinished | int16 | scalar |
| 0x58 | 0x5C | leaguePromotionCount | base.currentRoundIndex | int16 | scalar |
| 0x5A | 0x5E | leagueRelegationCount | base.numRounds | int16 | scalar |
| 0x5C | 0x60 | BSS_01_16E0E4 | subRoundCounter/playEachTeam | 2×int16 | scalars |
| 0x60 | 0x64 | leagueWarningsEnabled | Ex.minSubstitutes | uint16 | scalar |
| 0x62 | 0x66 | BSS_01_16E11E (56B) | Ex.pointsForAWin + matchData head | bytes | pointer-free blob |
| 0x9A | 0xA2 | diyTeamIdListType2 | (matchData tail) | — | saved separately |

64-bit offsets for everything at/after `BSS_01_16E0B4` are +4 because
`namePtr` widens 4→8 (the overlay's `#if UINTPTR_MAX` `.skip 4` pad).
The matchData region (`0x62`+) is **pointer-free and target-identical
internally** — only `namePtr` shifts.

### Corrections applied to this doc / overlay (mistakes found)

1. **`diyPresetNameOfCompetition` is 36 bytes (`$24`), not 44.** Our
   overlay (`swos_globals_career_overlay.c:914-916`) uses `.skip 44`,
   silently merging `diyPresetNameOfCompetition(36)` +
   `primaryCompStringOffset(4)` + `BSS_01_16E0B4(4)` into one label. The
   *byte total is correct* (and the 64-bit pad lands right), so this is a
   naming defect, not a layout bug — but it hides `primaryCompStringOffset`.
2. **`namePtr` == SWOS2.s `primaryCompStringOffset` — split is
   intentional, but the read sites were buggy.** In SWOS2.s this is one
   field at `comp+0x28`, copied into `SeasonInformations.leagueStrPtr`
   (SWOS2.s:61199). Our port deliberately split the four `*CompStringOffset`
   labels out of the overlay into standalone native-width pointers
   (`swos_data_typed_stubs.c:845-848`) so they can be saved symbolically
   (pointer-to-disk handling) — the stale "MOVED to overlay" comments that
   suggested otherwise have been corrected. **The real defect** (found and
   fixed 2026-05-30): `career_init.c` populated
   `SeasonInformations.leagueStrPtr`/`strPtr1/2/3` from those standalone
   globals, which are *only ever written `= 0`* — so the season-recap
   competition-name lines (`statistics.c:2184-2190`) rendered blank. The
   fix reads the buffer `namePtr` directly
   (`((DIYCompetition *)careerCompetitionSlotPtrTable[slot])->namePtr`),
   mirroring SWOS2.s. The standalone `*CompStringOffset` globals are now
   vestigial.
3. **`majorTrophyBonusValue` is a byte (`$1`), not int16.** SWOS2.s reads
   it with `move.b` (lines 50243/50263/50270/50347/50411). The draft
   table above previously guessed int16.
4. **The "3-object split" in the original plan double-names one
   partition.** `primary_slot_state` and `primary_league_config` fields
   ARE `DIYCompetition` struct fields (resultCode, substitutesCount,
   slotStateA/B/C, roundComplete, competitionFinished, currentRoundIndex,
   numRounds, minSubstitutes). They are disjoint *by offset* so a 3-object
   split is viable as a partition, but each byte must be emitted **once** —
   do not emit both `primary_competition.numTeams` and a separate
   `numDiyTeams`; they are the same field.
5. **`primary_comp_block` over-runs by 2 bytes.** `diyTeamIdListType2`
   starts at `0x9A` = 154. The current blob is 154 bytes from offset 2 →
   `[2,156)`, overlapping `diy_team_id_list_type_2`'s first 2 bytes
   (which is separately saved). Clean sizes are **152 / 156**, not
   154 / 158. Benign today (same bytes, load order resolves) but flag it.

### namePtr symbolic-save constraints (Step 4 follow-up)

`namePtr` is assigned in `diy_competition.c:8869` from one of:
- `divisionStringTable[d7]` (static table) when `compHdr->unk1 == 0`
- `*(uint8_t **)(table + 5 + nameOffset + (d7<<3))` (custom template)
- `aCup_0` / `aLeague_1` defaults (diy_competition.c:8919/9062)

and the *same string is also copied into* `competitionName[36]` at +4
(`_StringCopy(nameSource, a1)`). Three constraints on symbolic save:

- **Pointer-identity comparison.** `namePtr` is compared `== aFriendly`
  (diy_competition.c:2930/2943) and similar (3223/3229). Re-pointing it to
  a string-pool copy on load would break those identity checks if they
  run on a loaded primary-slot competition. Prefer restoring to the
  *original* static-table address (table+index identity) rather than a
  pooled duplicate — i.e. save a table-id + index, not just the bytes.
- **Address-RANGE comparison (found 2026-05-31, not in the original
  audit).** `career_status.c:3774/3786/3798` tests
  `namePtr >= &aEuropeanTour && namePtr <= &aOceaniaTour` to decide
  whether the active league is a "Tour" competition (drives
  `boardConfidenceMod7/8/9`). This is reached via
  `diyCompetitionStart.base.namePtr`, which is populated FROM the primary
  slot buffer by `_Career_LoadCompetitionSlotStateFromBuffer` (Step 2 of
  `_RebuildPointersAfterLoad`). A string-pool restore would fall outside
  the `aEuropeanTour..aOceaniaTour` static range and silently misclassify
  tour competitions. **This definitively rules out the string-pool
  approach** and confirms the table-id+index restore is required — the
  restored pointer must land back inside the original static-string
  address range, not a copy.
- **Feeds SeasonInformations.** `primaryCompStringOffset`(=namePtr) is
  copied into `SeasonInformations.leagueStrPtr` (career_init.c:1059;
  SWOS2.s:61199). The four `*CompStringOffset` globals are already
  zeroed-on-load and re-derived (`_Rebuild_CompStringOffsets`,
  career_init.c:1492) — so namePtr's symbolic restore should slot into
  that same re-derivation path rather than a parallel mechanism.

The existing symbolic patterns to reuse: `emit_str_pool_field` /
`load_str_pool_field` (save_format_json.c, used by `SeasonInformations`
comp-name pointers) and `restore_comp_copy_text_ptrs_from_template`
(for `*Copy` text pointers). namePtr most closely matches the
template-restore pattern given the identity-comparison constraint.

## Implementation plan (audit complete — recommended approach)

The audit changed the recommended shape. Because the named globals and
the `DIYCompetition` fields are **the same bytes** (see Audit results),
the cleanest cross-target-safe promotion is to emit the region via the
`DIYCompetition` struct (numbers auto-land at each host's offset), with
two carve-outs: the `namePtr` pointer and the pointer-free matchData
tail. The 3-object JSON grouping is still fine *cosmetically* — but it
must be a single partition emitted once, not three overlapping views.

1. ~~Resolve the `primaryCompStringOffset` vs `namePtr` storage
   question.~~ **Done (2026-05-30):** intentional split; the buggy
   `leagueStrPtr` population was fixed to read the buffer `namePtr`
   directly. See mistake #2 above.
2. ~~Replace `primary_comp_block` with field emission split at the
   namePtr boundary.~~ **Done (2026-05-31):** `competition_over`
   (scalar), `competition_name` (string), `primary_name_ptr` (raw bytes,
   host-width — namePtr unchanged for now), `primary_comp_tail`
   (pointer-free blob, **anchored after namePtr** via
   `offsetof(DIYCompetition, compId)`, 110 bytes on both hosts). Chose
   the "one pointer-free blob anchored after namePtr" variant for the
   tail — the per-field-scalar form was equivalent but more entries; the
   anchored blob is cross-target-safe with no `#if` size and the
   compile-time invariants guard it.
3. ~~Trim / drop the blob.~~ **Done (2026-05-31):** the
   `primary_comp_block` blob is **deleted** (not kept in parallel —
   see below). The four entries cover exactly `[0x00 .. headerEnd)`.
4. ~~Keep the blob in parallel during transition.~~ **Skipped
   deliberately:** keeping the raw blob in parallel would conflict with
   any future symbolic-namePtr emission (both targeting the same bytes,
   load-order-dependent). Verification is via
   `SWOS_SAVE_FORMAT_JSON_VERIFY=1` directly: zero-region-reload-diff on
   the live state. Same-binary mismatch count should be UNCHANGED from
   before the split (the same bytes are saved/loaded, just under 4 keys),
   i.e. only the pre-existing ~14 expected mismatches.
5. **REMAINING — make `namePtr` symbolic** (table-id + index restore;
   string-pool ruled out by the range-check constraint above). This is
   the only part of the cross-target story still using raw pointer bytes.
   Until then `primary_name_ptr` is stale across rebuilds, exactly as the
   old blob was — a pre-existing condition, not a new regression.

Optional but recommended cleanup: give the overlay
(`swos_globals_career_overlay.c`) the real SWOS2.s sub-labels
(`primaryCompStringOffset`, the `BSS_01_*` runs) instead of the merged
`.skip 44` / `.skip 8` / `.skip 18` / `.skip 58` lumps, so the named
globals match SWOS2.s one-for-one. This is a naming refactor only (byte
layout is already correct) and removes the ambiguity behind mistake #1.

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
