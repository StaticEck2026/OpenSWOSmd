# SWOS Save Format — JSON Migration Roadmap

> **Status (2026-09-30): complete — kept as the record of how.** Every save kind
> is structured JSON (`src/save_format_json.c`, `SAVE_JSON_SCHEMA_VERSION` 3, a
> mismatch is refused). `.CAR`/`.SEA`/`.PRE`/`.DIY` are JSON-only (legacy raw
> readers/writers deleted 2026-06-10/11); `opaque_data`, the `blobs` object, the
> `{sym}/{str}/{raw}` pointer wrapper, `save_pointer_lut.c` and
> `src/generated/amiga_address_map.c` are all gone. `.HIL`/`.TAC` still read
> binary too (policy: `docs/SAVE_DATA.md`). Still raw-byte fields: the five
> `*_competition_context` slot buffers ride as `FLD_BYTES` with the typed
> `comp_slots` serializer overriding the cross-target-sensitive parts on load.
> Functions named `_Foo` below are now `Foo` (e.g. `RebuildPointersAfterLoad`).

Living roadmap and progress log for the JSON save-format migration.
Update this file as each phase completes so we can resume any time.

## Why we're moving (recap)

Legacy `.CAR/.SEA/.PRE/.DIY/.HIL/.TAC` is a raw dump of `careerFileBuffer`
and friends — every byte of the Amiga BSS layout including absolute
pointer values. On Amiga that worked because the binary loaded at a
fixed address. On SDL every rebuild relocates the strings/arrays so
saved pointers become stale and crash on dereference. We fixed
individual offenders for months in `save_pointer_lut.c`; each new
field surfaced a fresh crash. JSON removes the problem at the format
level: pointers are stored as stable string IDs, not addresses.

## Library choice

[cesanta/mjson](https://github.com/cesanta/mjson) — single ~600 LOC
public-domain file, no allocations on the hot path (output goes
through a caller-provided write callback), parser + writer in one.
Drop `src/mjson.{c,h}`.

## Build flag

`make SWOS_SAVE_FORMAT_JSON=1` enables JSON writes. Loads always
auto-detect — `{` as first non-whitespace byte → JSON, else legacy.
*(Now: on by default; `=0` only switches `.HIL`/`.TAC` writes back to binary —
the big saves are JSON regardless and refuse a non-JSON file.)*

## Top-level format

```jsonc
{
  "schema_version": 1,
  "file_kind": "career",
  "num_selected_teams": 11,

  // Catch-all blob for fields not yet promoted to semantic JSON.
  // Shrinks as each sub-region is promoted. Round-trip stays safe
  // because semantic fields apply ON TOP of the decoded blob.
  "opaque_data": "<base64 of legacy raw region>",

  // Each promoted sub-region appears as a sibling field with named
  // sub-objects and stable-identity pointer fields.
  "season_informations": [ /* 20 entries */ ]
}
```

### Pointer encoding (stable across rebuilds)

```jsonc
"some_pointer": null                                  // unset
"some_pointer": { "sym": "europeanChampionships2" }   // base address of symbol
"some_pointer": { "sym": "europeanChampionships2", "off": 16 }  // mid-symbol
"some_pointer": { "str": "DIV 1" }                    // string by content
"some_pointer": { "raw": 4294967295 }                 // sentinel value (-1, -2, 0xFC, ...)
```

*(Retired: this wrapper and `g_amiga_addr_map` are gone; the only
pointer-shaped fields left are the seasonInformations comp-name strings,
emitted as a bare JSON string.)* Symbol-name encoding went through
`g_amiga_addr_map` in `src/generated/amiga_address_map.c`. String-content encoding (next
phase) covers comp-name string pointers that the LUT doesn't index.

## Progress checklist

### Foundation
- [x] `SAVE_FORMAT_JSON.md` design doc (this file)
- [x] `SWOS_SAVE_FORMAT_JSON` build flag in Makefile
- [x] Drop mjson into `src/`
- [x] `src/save_format_json.{c,h}` skeleton with API stubs
- [x] `file_io.c` header-detect dispatcher (`{` → JSON, else legacy)
- [x] Symbol resolver helpers (`sym_from_ptr`, `ptr_from_sym`) on top
      of `g_amiga_addr_map`

### `.CAR` (career)
- [x] Opaque round-trip — entire careerFileBuffer save region as
      base64 `opaque_data`. Round-trip byte-perfect.
- [x] Address-based scalar table — FieldDesc entries take `&symbol`
      rather than a literal offset. 64-bit-stable for standalone
      overlay globals; offsets are constant expressions (relocatable
      addresses) so the table remains a `static const`. Same machinery
      applies to future struct-field promotions via `offsetof`.
- [x] `season_informations` promoted to full semantic — all 18
      fields per record × 20 records. Four STRPTR pointers use
      symbol-name encoding.
- [x] **String-symbol resolution** — pointers that aren't in
      `g_amiga_addr_map` but look like valid in-process addresses
      get encoded as `{ "str": "<content>" }`. On load the string
      is copied into a private `s_string_pool` and the slot gets
      the pool address. Works for the ~114 comp-name strings
      (`aDiv1`, `aFACup`, `aBundesliga`, etc.) without expanding
      the LUT generator. Stale cross-build pointers fail
      `looks_readable` and emit as `null`.
- [x] Promote `currentCompetitionPtr` + `careerCompetitionSlotPtrTable[5]`
- [x] Promote `careerCupPickList[5]`
- [x] Promote `*CompStringOffset` family (4 fields)
- [x] Promote `diyCompetitionTablePtr`
- [x] Promote `*Copy` cup-record `nameSource` pointers (one per
      careerContests entry, ~30 fields)
- [x] Promote `selTeamsPtr` — variable-length array of `TeamFileEx`
      records sized by `numSelectedTeams`. Uses `offsetof()` for
      struct field positions so 64-bit migration just works.
- [x] Promote `careerTeam` and `currentTeam` TeamFileEx records
      with full per-field semantic JSON (14 team header fields +
      16 player records of 14 fields each).
- [x] Promote finance state (overdraftStreak, balances, weekly
      costs, etc.) via the address-based scalar table.
- [x] Named blob region for remaining unmodeled byte ranges —
      tmdFileBuffer, careerForeignMarketPlayers, editTeamsBuffer,
      editTeamsSaveVarsArea, the comp contexts, teamSelectionKeyCache,
      diy team-id lists, teamTacticsUSER A-F, the *Copy slot region.
      Each gets a named entry under `blobs` with a base64 payload.
      Future promotions replace each blob with a typed encoder.
- [x] Promote individual `*Copy` slot bodies — superseded: the
      `comp_copy_region` blob is gone; the `*Copy` records are rebuilt
      from their static templates on load
      (`reconstruct_comp_copy_region_from_templates`, schema v2) with
      only the dynamic bytes saved.
- [x] Promote tmdFileBuffer (transfer market data) — `tmd_teams`.
- [~] Promote competition contexts (the slot state buffers) — partly:
      still `FLD_BYTES` (`competition_context` etc.), with the typed
      `comp_slots` / `comp_slot_stage` serializers layered on top.
- [x] Verify byte coverage (`SWOS_SAVE_FORMAT_JSON_VERIFY=1` ran
      with `0 mismatches across 120366 bytes`) and remove
      `opaque_data` from the writer. Loader still decodes it for
      backwards-compat with saves written during the transition.
- [x] Promote `tmdFileBuffer` (transfer market data — large block
      at the very start of careerFileBuffer) — `tmd_teams`
- [x] Promote misc career flags/state (currentSeasonShowing,
      youreSacked, retired, ...) — typed scalars in `s_career_scalars`
- [x] Verify opaque_data shrinks to empty / can be removed — removed
- [ ] Round-trip integration test (load legacy `.CAR` → save JSON →
      reload JSON → diff in-memory state, expect zero deltas
      outside the unresolvable-pointer fields)

### `.SEA / .PRE` (preset competition)
- [x] JSON write/load wired into `_LoadPresetFile` / `_SavePresetCompetitionFile`.
      Re-uses career scalar table + blob table filtered by address
      range `[&competitionFileBuffer, &numSelectedTeams+2)`. DIY
      teams (`numDiyTeams` × TeamFileEx) emitted as `diy_teams[]`.
- [x] Verify byte coverage — `[json-verify-preset] 0 mismatches
      across 17482 bytes`.

### `.DIY` (DIY competition)
- [x] JSON write/load wired into `_LoadDIYFile` / `_SaveDIYFile`.
      Shares all machinery with `.SEA` — only `file_kind` differs
      (`"diy"` vs `"preset"`).
- [x] Verify byte coverage — `[json-verify-diy] 0 mismatches
      across 6538 bytes`.

### `.HIL` (highlights / replay)
- [x] JSON write/load wired into `_LoadHilFile` / `_DoSaveHighlights`.
      Saves header bytes + numGoals × 19000-byte scenes as one
      base64 blob, plus semantic `num_goals`, `game_name`,
      `game_round` for inspection. JSON dispatch happens BEFORE the
      legacy BE-swap dance so the JSON writer captures host-native
      u16 header values.
- [x] Verify byte coverage — `[json-verify-highlights] 0 mismatches
      across 40254 bytes`.
- [ ] Promote individual goal scenes to typed encoders later.

### `.TAC` (tactics)
- [x] JSON write/load wired into `_ReadTacticsFile` /
      `_WriteTacticsFile`. 370-byte TeamTactics fully semantic
      (name, 10 × 35-byte position arrays, 10-byte unk_table,
      ball_out_of_play_tactics).
- [x] Verify byte coverage — `[json-verify-tactics] 0 mismatches
      across 370 bytes`.

### Cleanup (post-migration)
- [x] Default `SWOS_SAVE_FORMAT_JSON=1` — already the Makefile default
      (`ifneq ($(SWOS_SAVE_FORMAT_JSON),0)`). With the big-save legacy raw
      paths deleted, the flag now only governs `.HIL`/`.TAC` *write* format
      (binary read is always compiled); see the refreshed Makefile comment.
      Not removing the toggle — it's the `.HIL`/`.TAC` binary-write opt-out.
- [x] Delete legacy bodies of the big overlay-dump saves —
      `_SaveCareerFile`/`_LoadCareerFile` (f1a5146),
      `_LoadPresetFile`/`_LoadDIYFile` (081c843),
      `_SavePresetCompetitionFile`/`_SaveDIYFile` (this change, JSON-only);
      `_SaveCompetitionFile` + the save-header magic externs went with them.
      **NOT** `_DoSaveHighlights`/`_LoadHilFile`/`_ReadTacticsFile`/
      `_WriteTacticsFile` — `.HIL`/`.TAC` KEEP binary read/write per the
      read-support policy (docs/SAVE_DATA.md).
- [x] Delete `src/save_pointer_lut.c` (done 2026-06-11 — the legacy
      raw readers that used it are all retired)
- [x] Decide whether `src/generated/amiga_address_map.c` still has a
      role — no; deleted 2026-06-11. JSON re-derives pointers from
      semantic identities, so no address LUT is needed.
- [x] Delete the cross-build pointer-fixup parts of
      `_RebuildPointersAfterLoad` — done by removing `save_pointer_lut.c` /
      `_TranslateAmigaAddressesAfterLoad` (903df34). `_RebuildPointersAfterLoad`
      itself is the KEEP path: it re-derives every pointer from a stable JSON
      IDENTITY (competition ID, buffer index), never an Amiga absolute address,
      so there were no legacy-fixup branches left inside it to remove.

## Open design decisions

- **String-symbol encoding** — `{ "str": "DIV 1" }` (content-based,
  rebuild-stable) vs extending `g_amiga_addr_map` to index `.rodata`
  strings. Leaning content-based: smaller LUT, robust to string
  reordering by the linker, debuggable.
- **What happens to `currentCompetitionPtr` & friends when they
  point into `competitionFileBuffer`** — these are pointers into a
  buffer that itself contains the comp record. After the buffer is
  base64-decoded back into memory, the symbol resolver needs to
  know about `competitionFileBuffer + N`. Already in
  `g_amiga_addr_map`, should resolve once string-symbol gap is
  closed.
- **64-bit migration** — when we eventually port to 64-bit, the
  on-disk format stays unchanged (numbers are ints, pointers are
  string IDs). Internal struct layouts change but the JSON schema
  is the contract.

## Testing notes

- ~~`make SWOS_SAVE_FORMAT_JSON=1` writes JSON, default `make` writes
  legacy.~~ Default `make` writes JSON (see Status above).
- `scripts/decode_save.py FILE.CAR` (`--standings` for league/group tables)
  decodes a save with team names resolved.
- Verify a written `.CAR` with `python3 -m json.tool FILE.CAR`.
- Decode a specific field path with mjson's own test helpers, or
  with jq: `jq '.season_informations[0]' FILE.CAR`.
- Diffing two saves: `diff <(jq -S . a.CAR) <(jq -S . b.CAR)`.

## Resuming work

Pick the next unchecked box in the appropriate section. The mechanics
are:

1. Identify the byte range in the legacy save covered by the new
   semantic representation (e.g. `seasonInformationsArray` is at
   offset 57488, length 2120 bytes).
2. Add an `emit_*` function that walks the in-memory state and
   `mjson_printf`s the JSON.
3. Add a `load_*` function that pulls each field via
   `mjson_get_*` and writes to the same byte range in memory,
   AFTER the opaque_data has been decoded.
4. Wire both into `SaveJson_WriteCareer` / `SaveJson_LoadCareer`.
5. Update this file's checklist.

Last update: 2026-05-21. **All five save types verified at 0
mismatches** — JSON byte-coverage is complete for `.CAR`
(120366 B), `.SEA/.PRE` (17482 B), `.DIY` (6538 B), `.HIL`
(40254 B), `.TAC` (370 B). Format migration is done; remaining
work is internal cleanup. Next session pick-up: promote
individual blob regions to typed encoders — best ROI:
comp_copy_region (each *Copy gets a proper
CompetitionCup/Cup2/League encoder using offsetof()) and
tmdFileBuffer (transfer-market schema design). Then begin
the post-migration cleanup checklist (default the build flag
on, delete legacy writers, drop save_pointer_lut.c).
