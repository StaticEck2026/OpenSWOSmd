# Save-format trim candidates

Fields currently serialized by the JSON save (`src/save_format_json.c`) that are
**redundant at runtime** — reset, derived, or recomputed during competition
setup so the saved value is overwritten before it is ever read — yet are kept in
the save **on purpose**.

## Policy (why they're still here)

While the JSON save migration (roadmap step 4) is **incomplete**, we KEEP
redundant fields as a safety margin: the legacy raw-dump path and the typed JSON
path coexist, recompute sites can move, and a field could start being read
before setup. A saved value that is already correct cross-target costs almost
nothing and removes a whole class of "we trimmed it and then something needed
it" regressions.

**Do not trim anything on this list until the JSON format is the *sole* source
of truth** (legacy `.CAR` raw-dump path retired, `opaque_data` gone for every
save type). At that point, work through this list, re-verifying each field
before removing it.

History: the cup scalars below were trimmed once (`cf5ad7e`) and deliberately
restored (`a62b1b2`, 2026-06-10) under this policy.

## Before trimming any field — re-verify

A field is trimmable only if, on the **load/resume** path, its value is
unconditionally overwritten before any read. Confirm with **either**:

1. **Static:** find a setup/recompute function on the load path that writes the
   field unconditionally (constant or derived from an already-restored field).
   Cite the write site; make sure it actually runs on *resume*, not only on
   *fresh creation* (e.g. `_DIY_RecomputeCompetitionDerivedFields` resets league
   progress to 0 — fine on creation, wrong on resume, so league-progress fields
   are NOT trimmable; see "Keep" below).
2. **Live (preferred):** cross-target gdb read. Load a save whose field holds a
   distinctive non-zero value, read the field on the other-endian target; if it
   reads back as 0 / the derived value regardless of what was saved, it is
   recomputed and safe to trim. (See `docs/GDB.md` for the attach + read flow.)

A field read only as a `!= 0` zero-check is *cross-target-safe* but that alone
does **not** make it trimmable — trimming requires the value to be genuinely
unneeded.

## Candidates

### Cup round-config + bracket-state scalars — `DIYCompetitionEx`, `competitionType == 1`

All eight are reset/derived at cup setup in `src/diy_competition.c`. Verified
live on the Amiga 2026-06-10 via `TEST.DIY` (a DIY CUP saved on the LE/SDL
build): saved `1/30/7` read back as `0`/derived on the BE target.

| field | parent offset | JSON key | why redundant (recompute site) |
|---|---|---|---|
| `matchData.cup.currentMatchIndex` | 0x64  | `cup_current_match_index` | reset 0 at setup (`diy_competition.c` 1268/2757/2787/2835/4445) |
| `matchData.cup.competitionFormat` | 0x14A | `cup_competition_format`  | reset 0 (1673/2772/2833/4449) |
| `matchData.cup.roundFormat`       | 0x14C | `cup_round_format`        | derived `= roundLegs[currentRoundIndex]` (1677/2760/4453) |
| `bracketScratch.secondLegCounter` | 0x32E | `cup_second_leg_counter`  | reset 0 (1688) |
| `bracketScratch.groupSectionFlag` | 0x330 | `cup_group_section_flag`  | reset 0 (1687) |
| `bracketScratch.matchCount`       | 0x332 | `cup_match_count`         | reset 0 (1690/4466) |
| `bracketScratch.totalMatches`     | 0x334 | `cup_total_matches`       | derived from `numTeams` (`_DIY_RecomputeCompetitionDerivedFields` 1755; cup `playEachTeam==0 → 0`) |
| `bracketScratch.curMonthIdx`      | 0x336 | `cup_cur_month_idx`       | reset 0 (1689/4465) |

Emitted/loaded in `emit_comp_slot` / `load_comp_slot` (`src/save_format_json.c`).

## Keep — genuinely persisted, never trim

These look adjacent to the candidates but carry real state the game does **not**
recompute on resume:

- **Cup `roundLegs` / `roundExtraTime` / `roundPenalties[6]`** (parent 0x66/0x72/
  0x7E) — design data; `roundLegs` is the source `roundFormat` derives from, so
  dropping it reinstates the single-leg-collapse bug.
- **League-progress run** (`matchesPlayedCount` 0x1BE, `leagueCurrentSlot` 0x1C0,
  `leagueMatchProgress` 0x1C2, `leagueTotalMatches` 0x1C4, `leagueSectionState`
  0x1C6–0x1CC) — incremented during league play; `_DIY_RecomputeCompetitionDerivedFields`
  would reset progress to 0, so it must NOT run on resume → these must persist.
- **Team-id arrays** (`leagueTeamIds`/`cupTeamIds`, BE byte-pairs),
  **`teamEntryOffsets`** (standings order), **`teamOffsetTable`** (symbolic
  per-team offsets), **tournament slot records** (13 uint16/slot), and the
  **base header scalars** — all carry persisted state.

## Related

- Cross-target uint16 audit + the four "is it safe" principles: memory
  `project_compslot_matchdata_uint16_crosstarget`.
- Save-format design rules: `.claude/CLAUDE.md` ("Save/load is typed JSON of
  NAMED fields").
