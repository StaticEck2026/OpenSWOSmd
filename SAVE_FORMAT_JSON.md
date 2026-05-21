# SWOS Save Format — JSON Migration

## Why we're moving

The legacy `.CAR/.SEA/.PRE/.DIY/.HIL/.TAC` save format is a raw dump of
`careerFileBuffer` (and friends) — every byte of the Amiga BSS layout
including absolute pointer values. That works fine on Amiga where the
binary loads at a fixed address, but on SDL every rebuild relocates the
strings/arrays and any saved pointer becomes a stale address. We've
been patching this with field-by-field translation in
`save_pointer_lut.c`, but each rebuild surfaces another pointer site we
hadn't enumerated and the testers' saves keep crashing.

JSON solves this permanently:

- No pointers on disk — comp identities become string IDs / numeric
  refs, not addresses. Same save loads on any binary, any platform.
- 64-bit migration becomes trivial. Today's 32-bit pointer fields just
  become string IDs; no struct layout to redesign.
- Schema can evolve without breaking old saves (add a field, old
  loaders ignore it; remove a field, new loaders default it).
- A human can `cat` a save and see what's wrong.

## Library — mjson

We use [cesanta/mjson](https://github.com/cesanta/mjson):
- Single ~600 LOC C file, public domain.
- No allocations — caller provides all buffers. Amiga-friendly.
- Both parser (path queries, callbacks) and writer (`mjson_printf`).

Drop `mjson.h` + `mjson.c` into `src/`. Makefile picks them up
automatically via `$(wildcard src/*.c)`.

## Build flag

`make SWOS_SAVE_FORMAT_JSON=1` enables JSON writes. Default off until
the schema is complete. The *load* path is always auto-detect — first
non-whitespace byte of `{` → JSON, else legacy. Existing saves keep
loading regardless of the flag.

Once all save types are migrated and round-trip-tested, flip the
default and (after a release cycle) delete the legacy paths.

## Filename

Same as today: `*.CAR / *.SEA / *.PRE / *.DIY / *.HIL / *.TAC`. The
load path identifies the format from the file's first byte, not its
extension. Files written by older builds keep loading until the user
re-saves, at which point they upgrade to JSON in place.

## Schema (work in progress)

Conventions:

- Top-level: `{ "schema_version": 1, "file_kind": "career"|"preset"|... , ... }`
- `lowercase_with_underscores` field names, mirroring Amiga global
  symbol names where possible.
- Integers as JSON numbers; uint32 fits exactly in IEEE 754 double.
- Pointer fields become:
  - Competition-name string ID (`"ec_qualifying_group_7"`) — when the
    field is a comp-name pointer (the seasonInformations STRPTRs).
  - Competition record reference (`{ "kind": "ec_qual", "index": 7 }`)
    — when the field is a pointer to one of the `*Copy` competition
    structs.
  - `null` when the field is unset / not applicable for the current
    career state.
- Byte arrays stay as JSON arrays of small integers. Large opaque
  blobs (team records, tactic data) are objects with named sub-fields,
  not base64 — readability matters.

### Career (.CAR) top-level

```jsonc
{
  "schema_version": 1,
  "file_kind": "career",

  // Player + career meta
  "current_season_number": 0,
  "career_team": { /* TeamFileEx */ },
  "season_informations": [
    {
      "club_no": 80,
      "club_name": "GERMANY",
      // Competition name string IDs — replaces the raw pointers at
      // legacy offsets 22/26/30.
      "primary_comp":   "ec_qualifying_group_7",
      "secondary_comp": null,
      "tertiary_comp":  null,
      "comp_1_result":  2,
      // ... rest of the 106-byte record fields ...
    }
    // up to 20 entries
  ],

  // Competition slots — see careerCompetitionSlotPtrTable[5]
  "current_competition":   { "kind": "ec_qual", "index": 7 },
  "secondary_competition": null,
  "tertiary_competition":  null,
  "quaternary_competition": null,
  "diy_competition":       null,

  // The 16-team selection cache (selTeamsPtr region)
  "num_selected_teams": 16,
  "selected_teams": [ /* 16x TeamFileEx */ ],

  // ... many more sections — see implementation TODO
}
```

### Other file types

- `.SEA / .PRE` (preset competition): `competitionFileBuffer` only —
  subset of the career schema.
- `.DIY` (DIY competition): single competition object with cup/league
  layout, no career wrapper.
- `.HIL` (highlights): replay state — sprite positions, frame indices.
- `.TAC` (tactics): trivial — 11-position formation grid with a name.

## Migration

- New writes always produce JSON (when flag is on).
- Legacy loads keep working — header detection routes them to the
  existing `_LoadCareerFile` etc. body.
- On the next save after loading a legacy file, the file is written
  back as JSON. No explicit conversion step.
- For Playaveli's `NATIONAL.CAR`: stale cross-build pointers in
  `seasonInformationsArray` can't be recovered semantically (we don't
  know what comp each past season was). Migration will write `null`
  for those — management record of past seasons shows blank comp
  lines instead of crashing. Current season's comp pointer is still
  valid because it was written this session.

## Implementation phases

1. **Foundation** — skeleton files, dispatcher wired into `file_io.c`,
   schema doc, build flag. ← *we are here*
2. **`.CAR` schema** — author write/read for each careerFileBuffer
   sub-region in `src/save_format_json_career.c`. Round-trip test by
   loading a legacy save, saving as JSON, loading JSON, comparing
   in-memory state.
3. **`.SEA / .PRE`** — re-use career sub-schema for the competition
   block.
4. **`.DIY`** — diyFileBufferCopy schema.
5. **`.HIL`** — replay schema.
6. **`.TAC`** — trivial.
7. **Cut over** — flip default flag, delete legacy paths after a
   release cycle.

## What this replaces

When fully cut over, we delete:
- `src/save_pointer_lut.c` (translate_one, _TranslateAmigaAddressesAfterLoad)
- `src/generated/amiga_address_map.c` (only used by the Amiga LUT)
- The cross-build pointer-fixup parts of `_RebuildPointersAfterLoad`
- The legacy bodies of `_SaveCareerFile`, `_LoadCareerFile`,
  `_SavePresetCompetitionFile`, `_LoadPresetFile`, `_SaveDIYFile`,
  `_LoadDIYFile`, `_DoSaveHighlights`, `_LoadHilFile`,
  `_ReadTacticsFile`, `_WriteTacticsFile`.

The dispatcher wrappers stay (renamed if you prefer).
