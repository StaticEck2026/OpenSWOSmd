# Plan — Retire the comp-slot `SLOT_ALIAS` family + finish the `careerFileBuffer` overlay

> Status (2026-09-30): **complete** -- the "PLAN ONLY" line below is the original header. Part A (S1-S6) landed (`e3d7278`, `6a504dd`, `c2c4d0b`, `3d85078`, `6f2ab40`): the SLOT_ALIAS `.equ` family is gone (see the note in `src/swos_competition_slots.c`). Part B landed through Phase 2b (`cd5c359`, `2734186`, `ac5ad49`): `careerFileBufferBuf` is retired and `src/swos_globals_career_overlay.c` holds just the two `uint16` scalars. Kept as the record of how; `SRC_FILES`/Amiga-commit steps in §10 predate the 2026-08-12 merge.

Status: **PLAN ONLY (no code).** Written 2026-06-12 after the `.equ` untangle
sweep (groups #1–#10: offerOrdinals, englishLeagues +64-bit fix, euroCup, sprite
tables +sync-gap fix, and the whole diy\* overlay). Those were byte-identical,
low-ripple, save-neutral. The two regions below are the **save-critical** remainder
and need this deliberate, test-between-every-step approach. Execute in a fresh
session with the ability to run a full career save/load round-trip on **both**
SDL and the Amiga.

Related: `[[project_competition_slots_untangled]]`, `[[project_crosstarget_save_scope]]`,
`[[project_career_buffer_overlay]]`, `[[project_career_overlay_extraction_recipe]]`,
`docs/SAVE_DATA.md`, `docs/PRIMARY_COMP_BLOCK_PROMOTION.md`.

---

## 0. TL;DR of current state

- **Comp-slot storage is ALREADY typed.** The 5 slots are standalone
  `CompetitionSlotBuffer` instances (`primaryCompetitionBuf`,
  `competitionContextBuf`, `tertiaryCompetitionContextBuf`,
  `euroCupsCompetitionContextBuf`, `diyCompetitionContextBuf`) in
  `swos_competition_slots.c`. `CompetitionSlotBuffer = { DIYTournamentState tourn;
  uint8_t stageSnapshot[0x734]; }`.
- **What remains in Part A** = retiring the **46 `SLOT_ALIAS(...)` legacy-name
  aliases** that re-base names like `competitionFileBuffer`, `primaryCompSlotStateA`,
  `diyTeamIdListType0` onto byte offsets inside those buffers. The storage doesn't
  move; only the *names* are converted to typed `<buf>.tourn.<field>` access.
- **The `careerFileBuffer` overlay (Part B) is ~done.** 74 `.skip` pilots already
  pulled the 338-label run into standalone typed structs (careerFinances,
  careerStatusFlags, careerFinanceTail, careerTeamBuffers, careerSeasonRecord,
  …). Only **3 structural labels** remain emitted: `careerFileBufferBuf` (backing),
  `careerFileBuffer` (start), `numSelectedTeams` (end). Part B is now mostly
  *verification + eventual backing-buffer retirement*, not struct conversion.

---

## 1. Why this is byte-identical at the SAVE-FILE level (the reassuring part)

The save descriptors use:
```c
#define FX(name, sym, type)      { name, (void *)&sym, 0,   type }
#define FA(name, sym, len, type) { name, (void *)&sym, len, type }
```
i.e. they store the **runtime address** `&sym`. Because every typed field sits at
the **same address** the alias pointed at (the alias is literally
`buf + <same offset>`), rewriting `FX("primary_slot_type", competitionFileBuffer,
FLD_S16)` → `FX("primary_slot_type", primaryCompetitionBuf.tourn.header.<field>,
FLD_S16)` produces an **identical serialized byte image**. The `.CAR`/JSON output
does not change; old saves still load.

**Consequence:** the risk is **not** a format change. The risk is a **wrong field
path** in the mechanical rewrite → the descriptor points at the wrong address →
**silent save corruption**. That is the entire danger and why every increment must
be save/load-verified.

`SWOS_SAVE_FORMAT_JSON_VERIFY=1` is the primary safety net: it diffs the
JSON-derived byte image against the opaque fallback. A non-zero mismatch after an
increment = a wrong path. Keep it at **0** after every step.

---

## 2. THE key complication — dual-use field names

The `DIYCompetition` header (`tourn.header`) names its fields by their **cup**
meaning, but several slot aliases are the **league** meaning of the *same byte*:

| Alias (offset)                      | DIYCompetition field at that offset | Same byte, different meaning |
|-------------------------------------|-------------------------------------|------------------------------|
| `primaryCompWinnerTeam`   @0x3A     | `resultCode`        @0x3A           | winner-team id ⟷ result code |
| `primaryCompSquadViewParam` @0x3C   | `substitutesCount`  @0x3C           | squad-view param ⟷ sub count |
| `secondaryCompFinished`   @0x4C     | `roundIterator`     @0x4C           | finished flag ⟷ round iter   |
| `diyTeamMinDivisionIndex` @0x54     | `roundComplete`     @0x54           | min-division ⟷ round-complete|
| `diyTeamMaxDivisionOffset`@0x56     | `competitionFinished`@0x56          | max-div-offset ⟷ comp-finished|
| `leaguePromotionCount`    @0x58     | `currentRoundIndex` @0x58           | promotion count ⟷ round idx  |
| `leagueRelegationCount`   @0x5A     | `numRounds`         @0x5A           | relegation count ⟷ num rounds|
| `competitionOver`         @0x02     | `isActive`          @0x02           | over flag ⟷ isActive flag    |
| `*TrophyBonusValue`       @0x30     | `avgSkill`          @0x30 (byte!)   | trophy bonus ⟷ avg skill     |

`*CompSlotStateA/B/C`@0x3E/0x40/0x42 and `*CompetitionName`@0x04 map cleanly
(same name/meaning) — those are trivial renames. The dual-use ones are NOT clean.

**Decision required (pick one, document it):**
- **(Option D1 — recommended) Add named unions in the struct.** Give the
  dual-use offsets an anonymous `union { int16_t roundComplete; int16_t
  diyTeamMinDivisionIndex; };` inside `DIYCompetition` (or a thin
  `DIYCompetitionLeagueView`-style overlay over the header tail). Then league code
  reads `.diyTeamMinDivisionIndex` and cup code reads `.roundComplete` — both
  self-documenting, same bytes, zero ambiguity. This mirrors how
  `DIYCompetitionEx_MatchData` already unions cup/league views. **Lowest
  reader-confusion, highest clarity. Preferred.**
- **(Option D2) Reuse the cup field name everywhere** (`diyTeamMinDivisionIndex`
  → `.roundComplete`). Byte-correct but the league call sites read misleadingly.
  Only acceptable with a loud `/* league-mode: min division index */` at each site.
  Cheaper but leaves a readability debt the whole exercise is meant to remove.

Resolve D1 vs D2 **before** touching consumers; it changes the target paths.

---

## 3. Part A — the 46-alias offset map (the spec to convert against)

`CFB_NP` = 4 on 64-bit / 0 on 32-bit (DIYCompetition namePtr growth). `2*CFB_NP`
applies past the DIYCompetition base (0x60). Offsets are within each buffer.

### Slot 0 — `primaryCompetitionBuf` (legacy `competitionFileBuffer`)
```
competitionFileBuffer      @0                -> .tourn.header.competitionType (VALUE reads, int16);
                                                &primaryCompetitionBuf for the FA base/blob dump
competitionOver            @0x02             -> .tourn.header.isActive          (DUAL: "over" flag ⟷ isActive)
diyPresetNameOfCompetition @0x04             -> .tourn.header.competitionName   (clean, int8[36])
majorTrophyBonusValue      @0x30 +CFB_NP     -> .tourn.header.avgSkill          (DUAL byte: bonus ⟷ avgSkill)
numDiyTeams                @0x32 +CFB_NP     -> .tourn.header.numTeams          (int16; ~same meaning)
primaryCompWinnerTeam      @0x3A +CFB_NP     -> .tourn.header.resultCode        (DUAL — see §2)
primaryCompSquadViewParam  @0x3C +CFB_NP     -> .tourn.header.substitutesCount  (DUAL)
primaryCompSlotStateA      @0x3E +CFB_NP     -> .tourn.header.slotStateA        (clean)
primaryCompSlotStateB      @0x40 +CFB_NP     -> .tourn.header.slotStateB        (clean)
primaryCompSlotStateC      @0x42 +CFB_NP     -> .tourn.header.slotStateC        (clean)
diyTeamMinDivisionIndex    @0x54 +CFB_NP     -> .tourn.header.roundComplete     (DUAL)
diyTeamMaxDivisionOffset   @0x56 +CFB_NP     -> .tourn.header.competitionFinished (DUAL)
leaguePromotionCount       @0x58 +CFB_NP     -> .tourn.header.currentRoundIndex (DUAL)
leagueRelegationCount      @0x5A +CFB_NP     -> .tourn.header.numRounds         (DUAL)
leagueWarningsEnabled      @0x60 +2*CFB_NP   -> .tourn.<DIYCompetitionEx region, past base>
diyTeamIdListType2         @0x9A +2*CFB_NP   -> .tourn.teamIds        (offsetof teamIds==0x9A ✓)
diyTeamIdListType0         @0x12E+2*CFB_NP   -> .tourn.<teamIds/groupMap region @0x12E>
diyTeamIdListType1         @0x24E+2*CFB_NP   -> .tourn.<slots region @0x24E>
```
NB: the `diyTeamIdListType*` and `leagueWarningsEnabled` offsets land in
`DIYTournamentState.teamIds`/`groupMap`/`slots` (verify each against the
`_diytourn_*_off_check` asserts: teamIds@0x9A, groupMap@0x11A, slots@0x15A). Some
may need a named sub-field added.

### Slots 1–4 (same header pattern, per buffer)
```
slot 1 competitionContextBuf:        competitionContext@0, secondaryCompetitionName@0x04,
   minorTrophyBonusValue@0x30, secondaryCompWinnerTeam@0x3A(DUAL resultCode),
   secondaryCompSlotStateA/B/C@0x3E/40/42, secondaryCompFinished@0x4C(DUAL roundIterator)
slot 2 tertiaryCompetitionContextBuf: tertiaryCompetitionContext@0, tertiaryCompetitionName@0x04,
   leagueTitleBonusValue@0x30, tertiaryCompWinnerTeam@0x3A, tertiaryCompSlotStateA/B/C, tertiaryCompFinished@0x4C
slot 3 euroCupsCompetitionContextBuf: euroCupsCompetitionContext@0, quaternaryCompetitionName@0x04,
   majorTrophyOrCupBonusValue@0x30, quaternaryCompWinnerTeam@0x3A, quaternaryCompSlotStateA/B/C
slot 4 diyCompetitionContextBuf:      diyCompetitionContext@0, diyCompSlotStateA/B/C@0x3E/40/42
```

**Offset map is now 100% explicit** (filled from `DIYCompetition`, game_types.h
585–624). The one nuance left per-site: `competitionFileBuffer`/`competitionContext`
/etc.@0 are used **both** as a VALUE (= `competitionType`, e.g. the `FX
primary_slot_type` S16 read) **and** as a BASE ADDRESS (the `FA ...
sizeof(CompetitionSlotBuffer)` blob dump, and `(uint8_t *)competitionFileBuffer`
casts in consumers). Convert value reads → `.tourn.header.competitionType`; convert
base/address uses → `&primaryCompetitionBuf` (or `(uint8_t *)&primaryCompetitionBuf`).
Classify each of the ~54 `competitionFileBuffer` refs as value-vs-base before
rewriting — this is the single most error-prone sub-task in Part A.

---

## 4. Part A — consumer ripple

13 distinct consumer files reference the 46 names (~119 file-refs total):
`career_init.c`, `career_season.c`, `career_status.c`, `career_teams.c`,
`diy_competition.c`, `file_io.c`, `job_offers.c`, `menus.c`, `save_format_json.c`,
`statistics.c`, `swos_data_bulk_imports.c`, `transfer_market.c`, `game_types.h`.

This is the same order of magnitude as the diy\* work (which touched ~5–6) but
spread wider and including `save_format_json.c` and `file_io.c`. All are in the
sync list — confirm with the pre-flight check before each commit.

**Watch-outs learned this session:**
- **Declaration scope** (the diy #10 bite): several consumers declare
  `extern <type> <buf>` *after* early functions that use the aliases. When the
  alias extern is removed, add a file-scope `extern CompetitionSlotBuffer
  primaryCompetitionBuf;` (etc.) near the top. Build will catch any miss.
- **BE byte-pair vs host-native**: `diyTeamIdListType*` and the team-id arrays use
  `BE_READ_U16`/`BE_WRITE`/`PACK_BE` (see `[[project_cup_team_list_be_pairs]]`,
  `[[project_slot_type_native_uint16]]`). Preserve the exact macro at each site —
  only change the base symbol, never the access width/endianness.
- **`*Copy` template pointers**: some slot fields are pointer-to-`*Copy`
  (`[[project_primary_comp_block_handoff]]`). Those are re-derived on load, not
  saved as raw bytes — don't "simplify" them.

---

## 5. Part A — save-format rewrite (`save_format_json.c`)

The descriptor tables reference the alias names (competitionFileBuffer ×54, each
`*CompetitionContext` ×8, the slot-state/winner/name globals ×2). Each `FX/FA`
entry must change its `sym` arg from the alias to the typed field expression:
```
FX("primary_slot_type", competitionFileBuffer,            FLD_S16)
  -> FX("primary_slot_type", primaryCompetitionBuf.tourn.header.<field>, FLD_S16)
FA("primary_competition_context", competitionFileBuffer, sizeof(CompetitionSlotBuffer), FLD_BYTES)
  -> FA("primary_competition_context", primaryCompetitionBuf, sizeof(CompetitionSlotBuffer), FLD_BYTES)
```
`FX`/`FA` macros are unchanged (`&sym` of a field expression is still a constant
address). **The serialized bytes are identical** — verify with VERIFY=1 == 0 and a
decode-diff of a known-good `.CAR`.

The `FA(... sizeof(CompetitionSlotBuffer), FLD_BYTES)` raw-blob dumps are the
*opposite* of the roadmap goal (named fields). They can stay as-is for this pass
(byte-identical) and be promoted to typed fields **later** — do NOT bundle blob→
typed promotion into the alias retirement; that is a separate, format-affecting
change.

---

## 6. Part A — increment plan (one commit per slot, VERIFY=0 gate each)

Pre-step (its own commit): **struct prep** — resolve §2 (D1 unions or D2 naming),
fill the §3 `<field@...>` blanks, add any missing named sub-fields to
`DIYCompetition`/`DIYTournamentState`, land the `_Static_assert`s pinning every
target offset. No consumer changes yet; builds 3 arches; VERIFY=0.

Then per slot, smallest-blast-radius first:
1. **Slot 4 (diy)** — only 4 aliases (`diyCompetitionContext`, `diyCompSlotStateA/B/C`),
   all clean. Smallest, proves the pattern end-to-end incl. save descriptors.
2. **Slot 2 (tertiary)** — 8 aliases, 1 dual (`tertiaryCompFinished`).
3. **Slot 1 (secondary)** — 8 aliases, 1 dual.
4. **Slot 3 (euro cups)** — 7 aliases. Coordinate with the euro-cup national
   backup-clobber known issue (`[[project_eurocup_national_backup_clobber]]`).
5. **Slot 0 (primary)** — 18 aliases incl. all the league dual-use names AND the
   `diyTeamIdListType*` BE arrays. **Biggest, riskiest — do last**, by which point
   the pattern + test loop are proven.

Each slot commit:
- remove that slot's `SLOT_ALIAS(...)` lines from `swos_competition_slots.c`,
- convert that slot's consumer refs to typed paths,
- rewrite that slot's `FX/FA` descriptors,
- build x86_64/armhf/aarch64,
- `make SWOS_SAVE_FORMAT_JSON=1 SWOS_SAVE_FORMAT_JSON_VERIFY=1` → run → **VERIFY must print 0 mismatches**,
- save a career, reload it, play a season transition that exercises that slot,
- commit, push, sync, **wait for the user's Amiga career save/load test before the next slot.**

---

## 7. Part A — test checklist (per increment)

- [ ] 3 arches build clean (`_Static_assert`s pass = offsets still pinned).
- [ ] `SWOS_SAVE_FORMAT_JSON_VERIFY=1` mismatch count == 0 (the 14 known-benign
      string mismatches from `[[project_verify_expected_mismatches]]` aside — watch
      the count doesn't *climb*).
- [ ] New career: play ≥1 full season transition touching the slot's competition
      type (league for slot 0/2, cup/euro for slot 1/3, DIY for slot 4).
- [ ] Save → quit → reload → continue: no panic, scores/standings/finances intact.
- [ ] Decode the `.CAR` (existing decode tooling) and diff against a pre-change
      capture: **byte-identical** expected.
- [ ] Amiga: user loads the SDL-written save and plays a transition (cross-target).

---

## 8. Part B — `careerFileBuffer` overlay

Current state: the 338-label run is reduced to **3 structural labels**
(`careerFileBufferBuf`, `careerFileBuffer`, `numSelectedTeams`) + `.skip` holes
where pilots were extracted. So there is **no large struct-conversion left** here.

### 8.1 Residual raw-offset audit — DONE (2026-06-15), CLEAN

No code reads `careerFileBufferBuf + 0xNN` or `&careerFileBuffer + N` by offset.
The only direct uses left are: `careerFileBuffer` as the `uint16` TMD-team-count
field (a value, not a base), `numSelectedTeams` as a `uint16`, and the
`save_format_json.c` load-memset + VERIFY-region base. Every former field is
already a standalone typed struct. **No not-yet-extracted fields remain.**

### 8.2 Retirement gate — SATISFIED, but a prerequisite surfaced

The legacy raw-dump `.CAR` reader was retired 2026-06-10 (`file_io.c`: JSON-only,
non-JSON gracefully rejected) and `opaque_data` is gone, so **nothing loads the
buffer by offset** — the doc's removal gate is met and `careerFileBufferBuf` is
now vestigial (256 KB backing only the 2 live scalars + dead zero-holes).

**BUT** retiring it is NOT the "lower-risk" drop the earlier draft implied:
`numSelectedTeams`'s *runtime address* was the upper bound of the `.DIY`/`.PRE`
comp-slot field-selection span `field_in_range(&primaryCompetitionBuf,
&numSelectedTeams)` (used by the preset/DIY emit + load + zero). Moving it to a
standalone global can invert that span and break preset/DIY saves cross-target —
the exact forbidden address-range pattern.

### 8.3 Phase plan

- **Phase 1 — DONE (`cd5c359`, Amiga-validated):** replace the comp-slot
  address-span selection with an explicit `comp` tag on `FieldDesc`
  (`FXC`/`FAC` macros); select the 23 primary-slot + header + count fields by
  identity. Removes `field_in_range`; decouples comp-field selection from
  `numSelectedTeams`'s address. Format-neutral (same 23 keys). VERIFY: career 0,
  DIY 0; cup-stage preset shows pre-existing ~12 derived-`teamOffsetTable`/
  stage-name snapshot diffs (`[[project_tournament_type2_save]]`, benign).
  - **Follow-up (`4a5e65b`, Amiga-validated):** Phase 1 unmasked that the
    `.DIY`/`.PRE` LEAGUE standings only rode the raw blob and byte-swapped the
    int32 `teamSelOffset` cross-target. Wired `emit/load_competition_overrides`
    (the career path's endian-neutral standings re-lay) into
    `write_/load_competition_buf`. TEST.DIY now loads correct on the BE Amiga.
- **Phase 2a — DONE (`2734186` + `df0f068`):** replace the near-vacuous career
  VERIFY *region* diff (`[&careerFileBuffer, +data_size)`) with
  `verify_roundtrip_fields()` — a per-field walk of `s_career_scalars`. Strict
  coverage increase: **0 mismatches across 240 typed fields** (was zeros-vs-zeros).
  The 5 redundant whole-buffer comp-slot blobs (`sizeof(CompetitionSlotBuffer)`)
  are exempted (logged) — their authored state is the typed fields (clean); their
  residual diffs are derived caches + typed-path-recomputed bytes. VERIFY-only.
- **Phase 2b — DONE (commit pending Amiga validation):** `careerFileBuffer`
  (TMD count) + `numSelectedTeams` are now standalone `uint16` globals;
  `swos_globals_career_overlay.c` gutted to those two (the 256 KB
  `careerFileBufferBuf` + the whole `.skip` sequence deleted); `career_data_size()`
  removed; the load memset reduced to zeroing the two scalars. **The overlay is
  retired.** `numSelectedTeams` is still re-derived semantically on `.DIY`/`.PRE`
  load (`= primaryCompetitionBuf.tourn.header.numTeams`). Real BSS-layout change →
  needs Cygwin link + VERIFY=0 + a career load + Amiga cross-target test.

---

## 9. Recommended session structure

- **Session S1:** Part A struct-prep commit only (D1/D2 decision, fill the offset
  map, asserts). Get it reviewed + Amiga-built before any consumer change.
- **Sessions S2–S6:** one slot per session (diy → tertiary → secondary → euro →
  primary), each gated on the user's cross-target save test from the prior slot.
- **Session S7:** Part B residual-offset audit + (optionally) backing-buffer
  retirement, only after step-4 legacy-reader status is confirmed dead.

Do **not** compress slots into one commit — per-slot isolation is what makes a
wrong field path bisectable instead of a mystery corruption.

---

## 10. Pre-flight checklist (every commit, both parts)

- [ ] Resolve §2 naming first; never guess a dual-use path.
- [ ] Every touched file in `SRC_FILES`/`HDR_FILES`
      (`[[project_sync_list_gaps_amiga_divergence]]`) — incl. `save_format_json.c`,
      `file_io.c`, and the header if struct fields changed.
- [ ] Preserve `BE_READ`/`BE_WRITE`/`PACK_BE` at every team-id site verbatim.
- [ ] `_Static_assert` each converted offset (catches a wrong path at compile time
      on 32- AND 64-bit — the cheapest guard available).
- [ ] VERIFY=0 after; flag any climb in the mismatch count to the user immediately
      (`[[feedback_save_format_changes]]`).
- [ ] Hand the user a per-commit Amiga commit message; never auto-commit the Amiga
      tree (`[[feedback_amiga_port_no_autocommit]]`).
