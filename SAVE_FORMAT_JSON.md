# SWOS Save Format — JSON Migration Roadmap

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
"some_pointer": { "str": "DIV 1" }                    // string by content (planned)
```

Symbol-name encoding goes through `g_amiga_addr_map` in
`src/generated/amiga_address_map.c`. String-content encoding (next
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
- [ ] Promote `currentCompetitionPtr` + `careerCompetitionSlotPtrTable[5]`
- [ ] Promote `careerCupPickList[5]`
- [ ] Promote `*CompStringOffset` family (4 fields)
- [ ] Promote `diyCompetitionTablePtr`
- [ ] Promote the `*Copy` competition records (cup record
      nameSource pointers inside each)
- [ ] Promote `selTeamsPtr` — 16 × `TeamFileEx` records (684 bytes
      each)
- [ ] Promote career meta (currentSeasonNumber, careerTeam,
      careerTeam stats etc.)
- [ ] Promote finance state (overdraftStreak, balances, weekly
      costs, etc.)
- [ ] Promote `tmdFileBuffer` (transfer market data — large block
      at the very start of careerFileBuffer)
- [ ] Promote misc career flags/state (currentSeasonShowing,
      youreSacked, retired, ...)
- [ ] Verify opaque_data shrinks to empty / can be removed
- [ ] Round-trip integration test (load legacy `.CAR` → save JSON →
      reload JSON → diff in-memory state, expect zero deltas
      outside the unresolvable-pointer fields)

### `.SEA / .PRE` (preset competition)
- [ ] Skeleton write/load behind dispatcher in `file_io.c`
- [ ] `competitionFileBuffer` opaque blob
- [ ] Re-use career sub-schema for the comp records

### `.DIY` (DIY competition)
- [ ] Skeleton write/load
- [ ] `diyFileBufferCopy` opaque blob
- [ ] Promote teamGrid / matchData / standings

### `.HIL` (highlights / replay)
- [ ] Skeleton write/load
- [ ] Replay frame state schema

### `.TAC` (tactics)
- [ ] Trivial — 11 position slots + name

### Cleanup (post-migration)
- [ ] Default `SWOS_SAVE_FORMAT_JSON=1` after one release cycle of
      dual-format support
- [ ] Delete legacy bodies of `_SaveCareerFile`, `_LoadCareerFile`,
      `_SavePresetCompetitionFile`, `_LoadPresetFile`,
      `_SaveDIYFile`, `_LoadDIYFile`, `_DoSaveHighlights`,
      `_LoadHilFile`, `_ReadTacticsFile`, `_WriteTacticsFile`
- [ ] Delete `src/save_pointer_lut.c`
- [ ] Decide whether `src/generated/amiga_address_map.c` still has a
      role (Amiga ↔ SDL save sharing? cross-port testing?)
- [ ] Delete the cross-build pointer-fixup parts of
      `_RebuildPointersAfterLoad`

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

- `make SWOS_SAVE_FORMAT_JSON=1` writes JSON, default `make` writes
  legacy. Load is auto-detect either way.
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

Last update: 2026-05-20. Next session pick-up: promote
`currentCompetitionPtr` + `careerCompetitionSlotPtrTable[5]` to
semantic JSON. These point at *Copy competition records inside
`competitionFileBuffer`, so the LUT already covers them — should be
a straightforward addition.
