# Phase 1A — `base + offset` access audit

> **Status (2026-09-30): complete — kept as the record of how.** Phases 1B–1F
> landed 2026-05-23 (`4312556` 1B/1C, `26d3e71` 1D/1E, `101e464` 1E,
> `397302b` 1F TeamSelectionKeyCache), followed by Phase 2 (type fixes) and the
> 64-bit bring-up (docs/64BIT_TRANSITION.md). The `careerFileBufferBuf` overlay
> Bucket 4 refers to is retired, and `CompetitionCup2_*` variants are now the merged
> `CompetitionTournament` (docs/STRUCT_CONSOLIDATE.md). The tally and
> `game_types.h:516` line reference are as of 2026-05-23 (the header is
> `include/game_types.h`).

Output of the Phase 1A audit: every `base + offset` access pattern
that needs converting to `struct->field` in subsequent Phase 1B-1E
work, plus a few that we should leave alone and why.

## Tally per file

Raw count of `*(u?int(8|16|32)_t *)(...)` cast-over-pointer sites
(does not distinguish struct vs. raw-byte access; the list below is
the human-categorized version):

| File                       | Sites |
|----------------------------|-------|
| `diy_competition.c`        | 52    |
| `menus.c`                  | 37    |
| `statistics.c`             | 30    |
| `career_init.c`            | 24    |
| `transfer_market.c`        | 23    |
| `sprites.c`                | 22    |
| `menu_system.c`            | 21    |
| `career_teams.c`           | 18    |
| `save_format_json.c`       | 15    |
| `asset_loader.c`           | 12    |
| `blitter.c`                | 12    |
| `teams.c`                  |  9    |
| `audio.c`                  |  8    |
| `career_season.c`          |  6    |
| `career_status.c`          |  6    |
| `match_setup.c`            |  4    |
| smaller (≤2 sites)         | ~10   |

## Categorization

The sites fall into four buckets. Phase 1B-1E only touches bucket 1.

### Bucket 1 — named struct access disguised as offsets

The struct already exists in `game_types.h` (or should be added);
each `(base + N)` corresponds to a known field. **All of these
convert** in Phase 1B-1E.

#### 1B — `DIYCompetition` / `DIYCompetitionEx` / `DIYTournamentSlot`

Files: `diy_competition.c` (the bulk), `career_status.c`,
`career_teams.c`, `statistics.c`, `menus.c` (cup tournament screens),
`save_format_json.c` (the few remaining offset literals there).

Common offsets you'll see (DIYCompetition base, defined in
`game_types.h:516`):

| Offset | Field                 |
|--------|-----------------------|
| 0x00   | `competitionType`     |
| 0x02   | `isActive`            |
| 0x04..0x27 | `competitionName[36]` |
| 0x28   | `namePtr` (uint32 — pointer; stays uint32 until Phase 2) |
| 0x2C   | `compId`              |
| 0x2D   | `slotId`              |
| 0x2E   | `field_2E`            |
| 0x30   | `avgSkill`            |
| 0x31   | `strengthTier`        |
| 0x32   | `numTeams`            |
| 0x34   | `pitchTypeOrSeason`   |
| 0x36   | `pitchType`           |
| 0x38   | `startMonth`          |
| 0x39   | `endMonth`            |
| 0x3A   | `resultCode`          |
| 0x3C   | `substitutesCount`    |
| 0x3E   | `slotStateA`          |
| 0x40   | `slotStateB`          |
| 0x42   | `slotStateC`          |
| 0x44   | `matchStarted`        |
| 0x46   | `careerTeamInvolved`  |
| 0x48   | `hasGroups`           |
| 0x4A   | `menuState`           |
| 0x4C   | `roundIterator`       |
| 0x4E   | `numTeamsInStage`     |
| 0x50   | `matchPairsPerRound`  |
| 0x52   | `totalMatchesInRound` |
| 0x54   | `roundComplete`       |
| 0x56   | `competitionFinished` |
| 0x58   | `currentRoundIndex`   |
| 0x5A   | `numRounds`           |
| 0x5C   | `subRoundCounter`     |
| 0x5E   | `playEachTeam`        |

`DIYTournamentSlot` starts at base + 0x15A (per
`memory/project_diycomp_slot_layout.md`), 6 × 118 byte slots; its
own fields are at base + 0x15A + slotIdx * 118 + slotField.

`DIYCompetitionEx` extends `DIYCompetition` at offset 0x60 with
league section state, points-for-win, etc. — see
`game_types.h`.

`save_format_json.c` already references most of these by name (the
recent slot_type / structured emit work). Holdouts there are mainly
the `primary_comp_block_padding` byte range and the per-slot context
blobs that we left as `FLD_BYTES` scalars. Those scalars don't
*read* fields by offset — they just dump bytes — so they're not in
Bucket 1 anyway.

#### 1C — `TeamFile` / `TeamFileEx` / `PlayerFile`

Files: `transfer_market.c`, `career_init.c`, `teams.c`,
`match_setup.c`, `career_teams.c` (TMD pool walk),
`edit_tactics.c`.

These structs are well-defined. Common patterns:

- `(uint8_t *)(base + N * sizeof(TeamFileEx))` — array indexing.
  Convert to `((TeamFileEx *)base) + N`.
- `*(uint16_t *)&team.byteField` — adjacent-byte read for the
  `{countryNumber, teamOrdinal}` team-id pair. Use
  `PACK_BE_U16(team.countryNumber, team.teamOrdinal)`.
- Field accesses via `*(int16_t *)(team + 0x32)` etc. Convert to
  `team->team.numTeams` or similar.

#### 1D — `Competition*` variants

Files: `career_init.c`, `career_season.c`, `career_eurocups.c`,
`career_status.c`, plus `competition_data.c` consumers.

The struct definitions live in `game_types.h`
(`CompetitionLeague`, `CompetitionCup`, `CompetitionCup2_6_2_64`,
etc.) — they're heterogeneous, each carries an `unk1[]` lookup
table at a different offset.

Common patterns:

- `comp[1]` / `comp[5]` / `comp[0xA]` byte reads of the competition
  header — these are fields like `comp->type`, `comp->nameOffset`,
  `comp->teamCount`. Use proper struct access.
- `*(uint8_t **)(comp + 5 + comp[5])` — fetch the nameSource pointer
  via the comp[5] offset. This pattern exists because the comp
  struct has variable-position `text[2]` pointers per type; the
  position is encoded in `comp[5]`. Hard to convert without a
  per-variant struct cast — likely a `comp_get_name_source(comp)`
  helper rather than direct field access.

#### 1E — Smaller structs and remaining sites

Files: `transfer_market.c` (offer entries), `career_init.c` /
`statistics.c` (seasonInformations rows), various menus.

| Pattern                  | Struct (existing or to-add) |
|--------------------------|-----------------------------|
| `offerEntry + 0/6/8/10`  | New: `JobOffer` / `TransferOffer` record |
| `seasonInfo + 0/2/22/...`| New: `SeasonInformations` struct (currently only `SAVE_SI_*` defines exist) |
| `cacheEntry[0..9]`       | New: `TeamSelectionCacheEntry` (10-byte record, see career_teams.c:1612) |
| `cup record + 5 + N`     | Per-Competition variant; helper function |

For each of these, the struct may not yet exist in
`game_types.h` — add it with field names that match what the
consumers read. Keep field types unchanged from current byte access
(byte = `uint8_t`, word = `uint16_t`, etc.); type fixes happen in
Phase 2.

### Bucket 2 — hardware-mapped register blocks

Files: `sprites.c` (Amiga blitter), `blitter.c`, `audio.c`
(Paula registers), `image_cmd.c` (some).

The Amiga has a memory-mapped chip register block at
`$DFF000` (the `custom` symbol). The C port still has these writes,
mostly as no-ops on the SDL target — see
`memory/project_amiga_blitter_noop.md`.

Patterns:

- `*(uint16_t *)((uint8_t *)a3 + 8) = PLANE_STRIDE_MINUS2`
- `*(uint16_t *)((uint8_t *)&custom->bltcon0 + N)` style

These are register writes, not struct field accesses. They're
already byte-addressed against the Amiga chip-register layout.
**Leave alone in Phase 1.** Phase 6 may need to revisit if the
SDL build's `custom` stub struct gets reshaped, but that's a
separate concern.

### Bucket 3 — dynamically-typed command streams

Files: `menus.c` (menu command tables), `menu_system.c` (menu
parser), `image_cmd.c` (image command stream), `asset_loader.c`
(asset table walker).

These walk variable-length records — `command_byte` followed by
N command-specific bytes whose interpretation depends on the
command. Byte offset arithmetic is intrinsic to the parser.

**Leave alone in Phase 1.** A structured rewrite is possible
(tagged-union decoder) but it's a different shape of work — out
of scope for the pointer-width transition.

### Bucket 4 — save format byte ranges

File: `save_format_json.c`.

The remaining `*(uint16_t *)(...)` reads here are intentional —
they're inside scalar/blob emit/load helpers that walk byte
ranges of careerFileBufferBuf to produce typed JSON output.
The work is structured already (FieldDesc tables); the byte
arithmetic is the implementation, not the data model.

**Leave alone in Phase 1.** When Phase 4 retires the
careerFileBufferBuf overlay, these helpers will end up walking
proper struct fields naturally.

## Files / structs map for 1B-1E

| Phase | Struct family                       | Primary files                                          |
|-------|--------------------------------------|--------------------------------------------------------|
| 1B    | DIYCompetition / Ex / TournamentSlot | diy_competition.c, career_teams.c, career_status.c, statistics.c, menus.c (cup screens) |
| 1C    | TeamFile / TeamFileEx / PlayerFile   | transfer_market.c, career_init.c, teams.c, match_setup.c, career_teams.c, edit_tactics.c |
| 1D    | Competition* variants                | career_init.c, career_season.c, career_eurocups.c, career_status.c |
| 1E    | Smaller structs (offers, season info, cache, menu records) | transfer_market.c, career_init.c, statistics.c, menus.c |

## Acceptance criteria for each sub-phase

- `grep -nE '\\*\\(u?int(8|16|32)_t \\*\\)\\([^)]+\\+ 0x[0-9a-fA-F]+\\)' src/<file>.c`
  shows zero matches against fields covered by the relevant struct.
- Build green (`make` and `make SWOS_SAVE_FORMAT_JSON=1`).
- No behavioural regression — front-end + a match + a save/load
  cycle still works.

## Out of scope for Phase 1

- Changing field *types* (Phase 2).
- Removing or modifying the careerFileBufferBuf overlay (Phase 4).
- Restructuring the menu/image/asset command stream parsers
  (Bucket 3 — separate refactor, not pointer-width-driven).
- Reshaping the Amiga blitter/audio register stubs (Bucket 2 —
  hardware-mapped, doesn't change with bitness).
