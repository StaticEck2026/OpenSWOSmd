# bracketScratch / league-standings overlay untangle (roadmap step 3)

> Status (2026-09-30): **complete** -- Stages 1-3 landed (`44e6f81`, `8ccda75`, `1776ac6`, `bbe2b2b`), Stage 4 was moot, and Stage 5 (back-port) no longer applies because the trees merged on 2026-08-12. Kept as the record of how.

Status: **STAGE 1 LANDED (2026-06-13).** The league standings are now the real
trailing `DIYCompetitionEx.leagueStandings[64]` field (offset 0x5C8 / 0x5D0),
clear of the cup scratch — they no longer overlay `cupTeamIds[51]`. Builds pass
on all 3 arch widths (x86_64 / AArch64 0xA50, ARM 0xA48; size_check +
`_diystagesnapshot_len_check` static asserts validate the layout per target) and
the game boots clean. **The rebuild band-aids are STILL in place as a safety net
(Stages 3-5 pending).** Save format changed (slot blob grew) → old `.CAR/.SEA/
.PRE/.DIY` saves are incompatible; play fresh careers.

**STAGE 3 DONE (2026-06-16).** The three selTeams-index-staleness rebuild
band-aids are **removed**; the one genuine re-sync is **kept**. All validated by
self-driven gdb/WSLg runs (`SWOS_DEBUG=1 SWOS_TEST_NO_INJURIES_CARDS=1`) with a
non-writing regression check at each removed site (run the rebuild, diff
teamOffsetTable + roundIterator, restore both so debug == production, log if
either WOULD change):

| rebuild | site | verdict |
|---|---|---|
| `8c660f4` | `_DIY_RecordCupBracketProgress` (hybrid choke point) | REMOVED `8ccda75` — probe 6/6, re-test 10/10 clean |
| `d16ad5b` | `career_status.c` season-keycache fill | REMOVED `1776ac6` — probe 14/14 (incl roundIterator) |
| cup-menu | `career_teams.c` `_Career_OpenCompetitionSlotMenuAndApplyChanges` | REMOVED `bbe2b2b` — re-test 8/8 clean |
| `93b8cfd` | `_DIYCupFinish` | **KEPT** `bbe2b2b` — re-test caught it STILL load-bearing |

**Root finding:** all three removed rebuilds guarded the **selTeams-index
staleness** ("the careerFileBuffer overlay reorders selTeams between a cup slot
store and load") — and **Part B retired that overlay**, so the staleness is gone.
**`93b8cfd` is different:** it re-derives teamOffsetTable to match the *current*
cup at finish time (a secondary cup finishing during next-season processing; the
check observed 31/64 entries differ) — a real re-sync, not a staleness artefact,
so it stays. The legit cache builds (`_DIY_InitCupBracketState` 1655,
`_DIY_RebuildCompetitionAndTeamSelectionCaches`) were never band-aids and are
untouched. `269deff`'s play_match_menu.c rebuild was already gone; `37d4bab` is a
64-bit copy-*size* fix, not a rebuild (stays).

**=> Stage 4 (re-apply stable-id) is MOOT.** Its sole purpose was to kill the
selTeams-index staleness; Part B already did, proven by the three clean removals.
(Stable-id wouldn't remove `93b8cfd` either — that's a context re-sync, not
staleness.) The reverted stable-id reconstruction is therefore not worth doing.

**STAGE 2 DONE (2026-06-18).** Save round-trip + cross-target verify of the
Stage-1 `leagueStandings[64]` relocation, via the harness
(`scripts/decode_save.py --standings`):
- **Gate A — SDL round-trip:** load `AUTO_S03.CAR` (AC Milan, full Serie A
  standings) → re-save → standings **byte-identical** to the original.
- **Gate B — cross-target SDL↔Amiga:** the Amiga build (FS-UAE) loaded the
  SDL-written `AUTO_S03.CAR` (rc=0) and re-saved it; the Amiga-written standings
  are **IDENTICAL** to the SDL original. The relocated layout round-trips both
  ways. (Used the now-working Amiga inferior-call harness.)
Stages 1–4 done/moot; Stage 2 done. Only **Stage 5** remains: back-port — the
Amiga tree is synced; user commits there manually.

## 1. Root cause (precise)

Within each `DIYCompetitionEx` block (`game_types.h`, the typed view of
`diyCompetitionStart` / each competition-slot buffer), the league standings
physically overlap the cup scratch:

| Region                         | Offset span      |
|--------------------------------|------------------|
| `cupTeamIds[64]`               | 0x24E .. 0x2CD   |
| `teamGridPairings[64]`         | 0x2CE .. 0x30D   |
| `teamShuffleOrder`/`bracketScratch` (incl `teamOffsetTable[64]`@0x344) | 0x30E .. 0x5C7 |
| **league standings** (`clearLeagueTableStats`, 64×18B) | **0x2B4 .. 0x733** |

The standings span overlaps the `cupTeamIds` tail (idx 51–63), **all** of
`teamGridPairings`, and **all** of `bracketScratch`. This is the Amiga's
"a competition is *either* a cup or a league, so reuse the bytes" trick.

**Where it bites: hybrid (type-2) competitions** have *both* cup and league
sub-slots in one block. Processing a **league sub-slot** writes standings that
**zero the cup sub-slot's `teamGridPairings` and `teamOffsetTable`** (slots
62/63 observed as 0; run-2's overflow was `compId=2`). The current rebuilds
(`_DIY_BuildTeamSelectionPointerCache` at `career_teams.c:1812`,
`career_status.c:4402`, the fill choke point `8c660f4`, and the auto-sim path)
re-derive `teamOffsetTable` from `cupTeamIds` to paper over the clobber.

## 2. Why the obvious fix (move the cup scratch) is the WRONG cut

`cupTeamIds@0x24E` and `teamGridPairings@0x2CE` are read via **raw byte
offsets** in many places — `career_eurocups.c:376`, `diy_competition.c:3924 /
4166 / 4347 / 8220`, `menus.c:6375`, `swos_data_bulk_imports.c:58` — and the
**save format hard-codes `cupTeamIds@0x24E` / `teamGridPairings@0x2CE`**
(`save_format_json.c:2826, 3209, 3669`). Moving them is a wide, fragile blast
radius.

## 3. Recommended cut: relocate the STANDINGS, not the cup scratch

The standings are reached **only** through the `DIY_STANDINGS_OFFSET` macro
(~28 sites, all `base + DIY_STANDINGS_OFFSET`). Change that one constant from
`0x2B4` to a region **past** the cup scratch (struct ends at 0x5C8; the backing
buffer is 4096B), e.g. `0x5C8` (or 16-byte aligned `0x5D0`). Then:

- standings occupy `0x5C8 .. 0xA48` — clear of `cupTeamIds` / `teamGridPairings`
  / `bracketScratch`, so a hybrid's league sub-slot can no longer clobber the
  cup sub-slot;
- all ~28 standings accessors move automatically (they use the macro);
- the cup scratch and its raw offsets / save-format offsets are **untouched**;
- the standings still overlap `audioInstrumentBank`'s range (past the struct) —
  that overlap is harmless (audio not loaded during DIY) and is preserved.

### Affected sites (recommended cut)

1. **`DIY_STANDINGS_OFFSET`** definition (grep for it — single `#define`).
   Change `0x2B4` → new offset. Update the struct doc-comment in
   `game_types.h:856`.
2. **Slot store/load copy size** — `_Career_StoreCompetitionSlotStateToBuffer` /
   `_Career_LoadCompetitionSlotStateFromBuffer` (`career_teams.c:1298/1348`).
   The copy must now span `0x00 .. (newStandingsOffset + 1152)`; today it is
   ~1844B (0x734). Verify the per-slot copy lengths (1844/1092/2900) and the
   slot buffer sizes (`competitionFileBuffer`, `competitionContext`,
   `tertiaryCompetitionContext`, `euroCupsCompetitionContext`) are ≥ the new end
   (~0xA48). Enlarge buffers if needed.
3. **Save format** — standings rows are emitted via `DIY_STANDINGS_OFFSET` /
   `diy_standings_row` (`diy_competition.c:562`, `save_format_json.c` standings
   loop). Confirm they follow the macro (auto-move). The comp-slot blob copy
   bounds in `save_format_json.c` may key off the old 0x2B4/0x734 span — audit
   and update. **This changes the on-disk slot-blob layout → old saves break;
   flag to the user (regenerate test saves).**
4. **`clearLeagueTableStats`** (`diy_competition.c:400`) — uses the macro; verify.

### Alternative cut (if standings can't be cleanly moved)

Move `bracketScratch` + `teamGridPairings` to the 0x734+ slack (keep
`cupTeamIds` since its raw offset is load-bearing in the save). Higher risk:
`teamGridPairings@0x2CE` raw accessors must all move too.

## 4. After the untangle (the payoff)

With the standings no longer clobbering the cup scratch:

1. **Remove the rebuild band-aids**: the choke-point rebuild in
   `_DIY_RecordCupBracketProgress` (revert `8c660f4`), the auto-sim-path rebuild,
   and the gated ones at `career_teams.c:1812` / `career_status.c:4402`. Verify
   `teamOffsetTable` stays valid through hybrid league→cup transitions with NO
   rebuild (re-instrument the 62/63 check).
2. **Re-apply the stable-team-id representation** (reverted 2026-06-13): make
   `teamOffsetTable`/`teamSelOffset` store `PACK_BE_U16(country,ordinal)` (−1 =
   careerTeam) resolved via `_VerifyTeamLoaded`, instead of a volatile selTeams
   index. Core change is ~2 functions in `diy_competition.c`
   (`diy_resolve_team`, `diy_team_to_identity`) + 2 raw-index writes in
   `save_format_json.c` (the `teamSelOffset = (int32_t)i` sites). This was only
   blocked by the overlay clobber (strict resolver panicked on clobbered 0
   slots); once the clobber is gone it lands cleanly and removes the index
   staleness + makes saves cross-target robust.

## 5. Staged execution + test gates

Do these as separate commits, each verified before the next:

1. **Relocate standings** (macro + slot copy + buffers). ✅ DONE 2026-06-13.
   Promoted to the named `leagueStandings[64]` field instead of relocating the
   offset constant — `DIY_STANDINGS_OFFSET`/`diy_standings_row`/`diy_ovr_row`
   are offsetof/field-derived so all ~30 accessors moved automatically. Length
   dependents tied to `sizeof(DIYCompetitionEx)`: the type-0 store/load loops,
   the four full-league `copyWords` (Save/RestoreDIYLeague +
   _DIY_Save/RestoreCompetitionFileBufferFromDIYCompetition), `DIY_CFB_STANDINGS_LEN`,
   and `stageSnapshot[]` (`DIY_STAGE_SNAPSHOT_LEN`, locked == sizeof by static
   assert). The `0x221` cup copies stay (cups have no standings). Builds + boot
   verified. **Runtime TODO (tester):** fresh English career through a full
   season incl. a hybrid (euro) cup, no panic; ideally re-instrument the 62/63
   check to confirm `teamOffsetTable` is no longer clobbered (rebuilds still in
   place as a safety net).
2. **Save-format migration**: confirm a save round-trips with the new layout;
   cross-target SDL↔Amiga. Old saves incompatible — tell the user.
3. **Remove the rebuilds** one at a time, re-testing the hybrid each time.
4. **Re-apply stable-id**; full season + cross-target save test.
5. **Back-port** every step to the Amiga C port (the overlay + macro exist
   there too; the Amiga hybrid hits the same clobber). User commits the Amiga
   side manually.

## 6. Constraint

Current guidance (memory: *"Overlapped data regions — do NOT separate during
64-bit transition"*) preserves these overlays. This is a **deliberate
exception** for one runtime-scratch collision that causes real bugs. The
standings-relocation cut keeps the per-competition block a single contiguous
unit (just larger), so it stays compatible with the slot-buffer / save-as-unit
model — it only changes one offset constant and the copy/buffer sizes.

## 7. Separate, unrelated bug (already FIXED — do not conflate)

Season-transition panic **0x4459** in `_DIY_SelectCompetitionSlot4FromCurrentDivision`
— FIXED 2026-06-13 (`f6bed18`): typed `englishLeaguesBlock` walk replacing the
self-relative byte offset. Kept here only as a pointer; not part of this plan.
