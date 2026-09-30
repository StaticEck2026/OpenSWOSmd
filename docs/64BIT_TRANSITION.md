# 64-bit transition plan

> **Status (2026-09-30): largely done — kept as the record of the plan.** 64-bit SDL
> builds ship: `make X86_64=1` → `bin/SWOS.x86_64`, and the macOS, aarch64 and Cygwin64
> targets are 64-bit (LP64); the default `make` is still 32-bit x86 (`-m32`). What
> actually happened differs from the plan in places: there is no `BITNESS=` variable
> (the target picks the width); the size asserts were **kept but made bitness-conditional**
> (`#if UINTPTR_MAX == 0xFFFFFFFFu` in `include/game_types.h`), not dropped; the
> `careerFileBufferBuf` overlay is **retired** (`src/swos_globals_career_overlay.c` now holds
> two plain globals); and the legacy raw `.CAR` reader is **retired** (JSON only,
> 2026-06-10). Still open from §8 / Phase 7: `swos_amiga_data_imports.c`,
> `swos_amiga_data_stubs.c`, `swos_amiga_func_stubs.c`, `swos_amiga_ptr_imports.c` still
> exist. Saves round-trip across 32/64-bit and LE/BE via the JSON format.

## Goal

The SDL port currently builds 32-bit only because the source assumes
32-bit pointers in many places — BSS overlay offsets, struct field
sizes, `uint32_t` "pointer" fields, etc. The endgame is one C source
tree that compiles cleanly for:

| Target          | Pointer size | Endianness |
|-----------------|--------------|------------|
| Amiga m68k      | 32 bit       | big        |
| SDL Linux x86   | 32 bit       | little     |
| SDL Linux x64   | 64 bit       | little     |
| SDL Windows x64 | 64 bit       | little     |
| SDL Mac arm64   | 64 bit       | little     |

i.e. **two pointer widths, two endiannesses, one source tree.**

Endianness is already handled — see `BE16_TO_HOST` /
`BE_READ_U16` / `PACK_BE_U16` in `src/endian_helpers.h` (no-ops on the
Amiga build, byteswaps on the SDL build). Pointer width is what this
doc covers.

## What stays the same

- **Endian helpers.** Big-endian wire-format reads stay as macros. The
  Amiga build is still BE, just like before.
- **JSON save format.** Already pointer-free as of the recent
  pointer-removal pass. Bare JSON strings for the four
  `seasonInformations` string slots round-trip through the runtime
  string pool. No raw addresses on disk anywhere. A career save
  produced on x64 Linux loads on m68k Amiga and vice versa as long as
  the runtime can decode the bytes — which is what the typed scalar
  and structured-field encoders already do.
- **Compile-time constants** that aren't sizes (constants like
  `STRING_POOL_SIZE = 8192`, magic numbers, etc.) — unaffected.

## What changes

### 1. Pointer-typed fields stop being `uint32_t`

The original Amiga code stored pointers as 32-bit values in BSS, and
the C port mirrored that by typing many fields as `uint32_t`. On a
64-bit build a real pointer is 8 bytes, so `uint32_t` truncates and
the program crashes the first time the field is dereferenced.

**Rule:** every field that holds a runtime address gets typed as a
real pointer (`void *`, `TeamFileEx *`, `void (*)(void)`, etc.) — or
as `uintptr_t` if the code does arithmetic on it. Never as
`uint32_t` / `int32_t`. Greppable list (incomplete) of fields that
need attention:

- `DIYCompetition.namePtr` — currently `uint32_t`, points at a
  static string. Change to `const char *`.
- `careerCompetitionSlotPtrTable[5]` — currently `uint8_t *`
  (already correct as a pointer; just verify nothing does
  `(uint32_t)entry`).
- `currentCompetitionPtr` / `secondaryCompetitionPtr` /
  `tertiaryCompetitionPtr` / `quaternaryCompetitionPtr` /
  `diyCompetitionPtr` / `diyCompetitionTablePtr` — currently
  `uint32_t`. Change to `void *` or the specific competition record
  pointer type. (These are no longer saved to disk after the JSON
  cleanup, so the type change is purely in-memory.)
- Any callback table entry typed `uint32_t` — change to a proper
  function-pointer typedef.
- `primaryCompStringOffset` / `secondaryCompStringOffset` etc. —
  currently `uint32_t`. Change to `const char *` (they're string
  pointers, despite the "offset" name).

Audit candidates:

```
grep -nE "uint32_t[[:space:]]+[a-z]+Ptr\\b" src/
grep -nE "uint32_t[[:space:]]+\\*?[a-z]+CompetitionPtr" src/
grep -nE "\\(uint32_t\\)&" src/  # casts that round-trip through u32
```

Cast convention: when interop with serialization or address
arithmetic forces an integer view of a pointer, use `uintptr_t`,
not `uint32_t`. `uintptr_t` is 4 bytes on 32-bit and 8 bytes on
64-bit, no loss.

### 2. Struct sizes change between targets

Once `DIYCompetition.namePtr` becomes `const char *`, the struct
grows by 4 bytes on 64-bit. Offsets of every subsequent field shift.
`sizeof(DIYCompetition)` is no longer 0x60 — it's 0x64 on 64-bit
builds.

Same for any struct containing a pointer:

| Struct                  | 32-bit size | 64-bit size (est) |
|-------------------------|-------------|-------------------|
| `DIYCompetition`        | 0x60        | 0x64 (+4)         |
| `DIYCompetitionEx`      | varies      | +4 per ptr field  |
| `Competition*` variants | varies      | +4 per ptr field  |
| `MarketPlayer`          | 42          | 42 (no pointers)  |
| `TeamFileEx`            | 684         | 684 (no pointers) |
| `PlayerFile`            | 38          | 38 (no pointers)  |
| `Tactics` / `TeamTactics`| 370        | 370 (no pointers) |
| `DIYTournamentSlot`     | 118         | 118 (no pointers) |

Most game structs are pointer-free — the size assertion delta only
hits the competition records and a handful of others.

### 3. The `careerFileBufferBuf` BSS overlay

The 256 KB `careerFileBufferBuf` array holds ~340 named symbols laid
out at hardcoded byte offsets (see `swos_globals_career_overlay.c`).
The offsets were computed by the auto-extractor against the original
Amiga binary's 32-bit pointer sizes. On a 64-bit build the layout
should:

- Either be **recomputed from struct definitions** so pointer-bearing
  globals get the correct local size; the `.equ` aliases then take
  their address from `offsetof()`-style expressions rather than
  hardcoded numbers.
- Or be **retired entirely** in favour of standalone globals (the
  long-term direction per `CLAUDE.md` step 3 — "Untangle the data
  overlays"). After this transition, every global is its own `.o`
  symbol at a linker-determined address; no overlay; no
  adjacent-globals reads.

Until the overlay is retired, mark every hardcoded offset constant
in `swos_globals_career_overlay.c` with a `TODO: derive from struct`
comment so we can audit them later. (**Retired** since: every label became its own typed
global; see the header of `src/swos_globals_career_overlay.c` and
docs/COMPETITION_SLOT_ALIAS_UNTANGLE.md §8.)

#### Concrete bite — `substitutesCount=2` invisible-suspension bug (May 2026)

`competitionFileBuffer` lives in this overlay and is cast to
`DIYCompetition *` at multiple read sites. On 32-bit the cast worked
because the struct's field offsets matched the overlay's named-globals
positions. On 64-bit `DIYCompetition.namePtr` grew 4 → 8 bytes,
shifting `substitutesCount` from struct offset `0x3C` → `0x40`. The
overlay's named globals stayed put, so
`((DIYCompetition *)&competitionFileBuffer)->substitutesCount` started
reading the byte that `primaryCompSlotStateB` was sitting on — which
`_InitCareer` unconditionally sets to `2`. Every league/cup match
silently got `substitutesCount = 2`, which `_PlayDIYLeague…` passed as
`playMatchExcludeFlags`, which `_IsPlayerInjured` interprets as "skip
the cards check" — so a player with 1+ red cards never tripped the
suspension lockout and `VIEW RESULT` stayed enabled. The squad rows
also stayed blue (no red-bg highlight) because they call the same
predicate.

Stopgap fix: a `#if UINTPTR_MAX != 0xFFFFFFFFu` 4-byte `.skip` after
`diyPresetNameOfCompetition` in `swos_globals_career_overlay.c`,
re-aligning the named globals with the grown struct on 64-bit only.
This is the interim before the broader audit replaces hardcoded
offsets with `offsetof()` expressions — see
`memory/project_career_overlay_nameptr_gap.md` and
`memory/project_raw_offset_64bit_shift_trap.md`. Any new struct field
that grows on 64-bit (another pointer added before a referenced field)
will hit the same class of bug at a different overlay byte until the
overlay is retired entirely.

### 4. Adjacent-globals reads need to die

A handful of routines read past the end of one BSS symbol into the
next (the `stadiumMenuLittlePlayer` / `littlePlayerSpriteGeneric`
/ `littlePlayersColorTable` triple is the classic case, plus
`goalieFacesBackingBuf` and friends). On the Amiga BSS run those
labels are contiguous; on SDL we replicate that with `.equ`
aliases into a single backing buffer.

This pattern is **not robust under bitness change**. If the struct
layout shifts because a pointer field grew, the adjacent reads land
at the wrong place. Two options:

- **Inline the read**: change the consumer to use a struct field
  access (`stadiumLittlePlayer.colorTable[i]`) instead of
  walking off the end of another global.
- **Keep the backing buffer**: but compute its size from
  `sizeof(Struct1) + sizeof(Struct2) + sizeof(Struct3)`, not
  hardcoded numbers.

Inline is better. The .equ aliases for adjacent-read patterns are
listed in `memory/project_adjacent_globals_bss.md` — work through
them before the 64-bit cutover, not after.

### 5. Size assertions

Currently the codebase has compile-time size checks like:

```c
typedef char _diycompetition_size_check[(sizeof(DIYCompetition) == 0x60) ? 1 : -1];
typedef char _teamfileex_size_check[(sizeof(TeamFileEx) == 684) ? 1 : -1];
typedef char _playerfile_size_check[(sizeof(PlayerFile) == 38) ? 1 : -1];
typedef char _diyslot_size_check[(sizeof(DIYTournamentSlot) == 0x76) ? 1 : -1];
```

These were anchored to the original Amiga binary's struct sizes
when we needed byte-exact compatibility with the original Amiga
binary's BSS and the legacy raw-byte `.CAR` format. **Both of those
constraints are gone** — note the Amiga port itself is still a
compatibility target, just not the original asm binary:

- The C source is now the spec, not `SWOS2.s`. The Amiga build is
  *our C compiled for m68k*, not the original asm binary. SWOS2.s
  is consulted only as a reference for diagnosing porting bugs.
- Saves no longer dump raw struct bytes; the JSON format
  reconstructs each field from named scalars / structured emit
  paths. Byte-for-byte struct compatibility with the original is
  no longer load-bearing — fields are looked up by name on load.

**Recommendation: drop the size assertions** for structs that
contain pointers, and for any struct whose size only mattered
because the legacy save format dumped it raw. Keep them for:

- Structs that map onto an external file format we don't control
  (RNC compressed headers, RJP1 module fields, Amiga sprite/pitch
  headers).
- Structs whose layout is constrained by hardware — palette
  entries, etc.

For the structs we drop checks on, leave a one-line comment so it's
clear the size is now *whatever the C compiler produces* and the
JSON encoder is the authority for round-trip stability:

```c
// sizeof(DIYCompetition) is target-dependent — pointer fields
// vary between 32 and 64-bit. JSON encoder handles fields by name.
typedef struct DIYCompetition { ... } DIYCompetition;
```

If a check is genuinely useful (e.g. as a sanity guard against
accidental layout drift on the same target), make it
bitness-conditional:

```c
#if UINTPTR_MAX == 0xFFFFFFFFu
typedef char _diycompetition_size_check_32[(sizeof(DIYCompetition) == 0x64) ? 1 : -1];
#else
typedef char _diycompetition_size_check_64[(sizeof(DIYCompetition) == 0x70) ? 1 : -1];
#endif
```

But honestly: just drop them. We catch field-level bugs through the
JSON round-trip, not through total-size checks.

### 6. Save format implications

Already handled. Recap of what the recent cleanup gave us:

- **No raw pointers on disk.** Every former pointer field is either
  gone (re-derived on load via `_RebuildPointersAfterLoad`) or
  emitted as a bare JSON string interpreted through the string pool.
- **No raw struct dumps on disk.** Every byte is reached via either
  a typed scalar (`FA(... FLD_BYTES)`) or a structured field
  (`emit_team_file_ex_body`, `emit_player_file_body`,
  `emit_market_player_body`, etc.).
- **Cross-bitness portable.** A 32-bit Amiga save loads on 64-bit
  Linux; a 64-bit Linux save loads on 32-bit Amiga. The wire format
  doesn't care.

The legacy raw-byte `.CAR` loader is still wired up for backward
compat (`file_io.c:_LoadCareerFile`'s non-JSON branch). On 64-bit
builds this path **does not work** — the raw image was 32-bit-only.
(**Done:** retired 2026-06-10 — `LoadCareerFile` in `src/file_io.c` accepts only JSON and
shows the `aFileSavedByDif` error menu for anything else.)
Either:

- Mark the legacy `.CAR` reader as 32-bit-only (`#if UINTPTR_MAX ==
  0xFFFFFFFFu`), so 64-bit builds reject the file with a clear
  error rather than crashing on a pointer fixup.
- Or delete it entirely once the tester has converted their saves.

### 7. Build configuration

The Makefile currently force-builds 32-bit (*as of writing; now the default `make` is
32-bit and `X86_64=1`, macOS, aarch64 and Cygwin64 build 64-bit*):

```
gcc -m32 ...
```

For the transition:

- Add a `BITNESS=32` / `BITNESS=64` (or `M=32` / `M=64`) make
  variable that selects between `-m32` and `-m64`.
- Default to `-m32` on Amiga (only option) and on the SDL build
  *until* the bitness audit is complete, then flip the SDL default
  to `-m64`.
- Keep the 32-bit SDL build green throughout — it's the bridge
  between the Amiga and the future x64 host.

### 8. Things we no longer need

Since SWOS2.s is no longer the source of truth:

- **`save_pointer_lut.c`** — the Amiga address translation table.
  Used by the legacy `.CAR` loader to remap Amiga pointer values
  into current-build pointer values. **DELETED 2026-06-11**: the
  legacy raw readers it served are all retired (JSON-only), so the
  whole file went away.
- **`generated/amiga_address_map.c`** — same dependency. The address
  map was generated from the original m68k binary's symbol table.
  **DELETED 2026-06-11** alongside `save_pointer_lut.c`.
- **`src/swos_amiga_data_imports.c` / `swos_amiga_data_stubs.c` /
  `swos_amiga_func_stubs.c` / `swos_amiga_ptr_imports.c`** — the
  scaffolding that bridged the partial C port to the rest of the
  Amiga binary during initial bring-up. Now that the C port is
  self-contained, these can collapse into normal `swos_globals.c` /
  `competition_data.c` / etc. definitions.
- **Hard-coded byte offsets in `swos_globals_career_overlay.c`** —
  see section 3. Replace with `offsetof()` / struct-relative
  derivations, or remove the overlay altogether.

These cleanups aren't *required* for 64-bit, but they remove the
last places where 32-bit Amiga assumptions are baked into the
source.

## Phased plan

The pieces above turn into 7 phases. Each phase keeps the 32-bit SDL
build green and (after Phase 1) keeps the Amiga back-port viable.
Cross-phase rule: every phase ends with the project still building
and the menus / match loop still working — no half-converted state
across PR boundaries.

### Phase 1 — `base + offset` → `struct->field`

The structural pre-req for everything else. Most of the codebase
already uses struct field access; this phase finds the holdouts and
converts them. After Phase 1, struct field offsets no longer appear
as numeric literals in `src/*.c`.

The pattern to find:

- `*(uint16_t *)(base + 0x42)` — raw cast over byte arithmetic
- `*(int16_t *)(base + N)` / `*(uint32_t *)(...)` — same with other widths
- `base[N]` where `base` is a `uint8_t *` walking a typed record
- `(SomeStruct *)(buf + N * stride)` — array indexing via byte math
- `*(uint16_t *)&struct.byteField` — adjacent-byte read (use
  `PACK_BE_U16` helper or, better, restructure the struct so the
  pair is a proper `uint16_t` field)

Conversion is mechanical per file: cast `base` to the right struct
pointer once, then access named fields. Where the struct doesn't
exist yet (or is missing a name for the byte at offset N), add the
field to `game_types.h` first. Reference offsets in code comments
during the conversion so the reviewer can verify against
`DIYCompetition.competitionType /* 0x00 */` etc.

This phase **does not** change struct layouts or add/remove fields —
field types stay exactly what they were. The point is to eliminate
hardcoded offsets, not to redesign anything. Pointer-typed fields
that are still `uint32_t` stay `uint32_t` until Phase 2.

Order within Phase 1: convert one struct family at a time, biggest
first. Suggested split (see `phase1-audit.md` produced in 1A):

- 1A — Audit: enumerate all `base + offset` sites and the struct
  each one is implicitly indexing.
- 1B — `DIYCompetition` / `DIYCompetitionEx` / `DIYTournamentSlot`
  (heaviest user, all of `diy_competition.c` plus
  `career_status.c` / `career_teams.c`)
- 1C — `TeamFile` / `TeamFileEx` / `PlayerFile` (teams.c,
  match_setup.c, transfer_market.c, edit_tactics.c)
- 1D — `Competition*` variants (competition_data.c consumers,
  career_init.c, career_season.c, career_eurocups.c)
- 1E — Smaller structs and remaining sites (MarketPlayer,
  SeasonInformations, tactics buffers, menu structs)

### Phase 2 — pointer-typed `uint32_t` fields → real pointer types

With offsets out of the way, changing a field's type is purely a
search-and-replace job: every consumer is going through
`comp->namePtr`, not `*(uint32_t *)(buf + 0x28)`.

Per the existing audit list in §1 of this doc, convert each
pointer-typed `uint32_t` field to a real pointer type (`const char *`,
`void *`, specific struct pointer, function-pointer typedef).
Consumers using `(uint32_t)ptr` for storage become bug sites — fix
them to use real pointer typing or `uintptr_t` if integer arithmetic
is genuinely needed.

After Phase 2 a 64-bit build *would* produce correctly-typed code —
struct sizes grow, but the runtime semantics are right.

### Aside: overlapped data regions

Several runtime structures re-use the same memory bytes for different
semantic views at different game phases (DIYCompetitionEx.overlap
union, matchData cup/league views, bracketScratch over
teamShuffleOrder, the league standings table writing past struct end
into audioInstrumentBank, etc.). See `docs/OVERLAPPED_DATA.md` for
the catalog.

These overlaps are **left intact** during the 64-bit transition. The
work in this doc is structure-preserving — every byte stays where
it was, every Amiga-style union or alias keeps working. Separating
overlap regions into distinct buffers is a separate post-64-bit
project with its own behavioural risk. Phase 1 *may* add named
union fields where the IDA reference has them and our port doesn't,
but that's still a typing change, not a layout change.

### Phase 3 — retire adjacent-globals patterns

The `memory/project_adjacent_globals_bss.md` list catalogues every
place where Amiga code reads past one global into the next. For
each:

- Identify the struct or struct-family the read was synthesising.
- Inline the access as a proper struct field reference.
- Delete the `.equ` alias and the merged backing buffer if no
  consumer remains.

After Phase 3 nothing in `src/*.c` does
`((Type *)&firstGlobal)[someIndex]` to reach an adjacent global.

### Phase 4 — replace hardcoded `careerFileBufferBuf + N` offsets

The 256 KB overlay in `swos_globals_career_overlay.c` aliases ~340
named symbols to hardcoded byte offsets. After Phase 1 each consumer
goes through the named symbol, so the *value* of the offset isn't
checked by the compiler — it just has to be self-consistent within
the overlay.

For Phase 4:

- Define a `CareerFileBufferLayout` struct (or several smaller
  structs) that captures the meaningful regions of the overlay.
- Replace `.equ name, careerFileBufferBuf + 12345` with
  `.equ name, careerFileBufferBuf + offsetof(CareerFileBufferLayout, name)`
  (or equivalent assembler expression).
- Alternative end state: retire the overlay entirely — each former
  alias becomes a standalone `uint8_t name[size]` global. Larger
  patch but cleaner; consider after the rest of the transition is
  proven.

### Phase 5 — drop size assertions for pointer-bearing structs

One pass through `game_types.h` and friends. Each `typedef char
_x_size_check[...]` for a struct that contains a pointer gets
removed and replaced with a one-line comment noting the size is
target-dependent. Keep checks for external file formats (RNC
headers, RJP1 module headers, palette layouts).

### Phase 6 — `BITNESS=64` build option

Makefile: add `BITNESS` variable that selects `-m32` or `-m64`,
default `-m32` until 64-bit is proven, then flip the SDL default.
Build a 64-bit SDL binary, fix the first wave of issues (often
`%lx` / `%ld` format strings, `sizeof()` assumptions in serializers,
implicit `int` ↔ `pointer` casts).

After this phase: 64-bit SDL build runs the front-end. Iteration
fixes whatever 64-bit-specific bugs appear in deeper paths
(transfer market, season transitions, etc.).

### Phase 7 — cleanup of 32-bit-Amiga scaffolding

Once 64-bit SDL is stable:

- Drop the legacy raw `.CAR` reader (32-bit-only image) or mark it
  with a clear `#if UINTPTR_MAX == 0xFFFFFFFFu` guard.
- ~~Drop `save_pointer_lut.c` and `generated/amiga_address_map.c`
  (consumed only by the legacy reader).~~ **DONE 2026-06-11.**
- Fold `swos_amiga_data_stubs.c` / `swos_amiga_func_stubs.c` /
  `swos_amiga_ptr_imports.c` content into normal modules
  (`swos_globals.c` / `competition_data.c` / per-feature files).
  These existed as bridges during initial bring-up and are no longer
  load-bearing.
- ~~Back-port the whole tree to the Amiga C port~~ — moot: the SDL and
  Amiga trees merged into this one repo on 2026-08-12, so `make -f Makefile.amiga`
  builds the same `src/` (the `BE_*` macros are no-ops on m68k).

Steps 5-7 can land in any order once Phases 1-4 are complete. Phase
6 is the milestone — first 64-bit SDL build — and the only one with
externally visible behaviour change.

## Reference: what counts as a "pointer field"

Any field whose value is the address of something in memory. This
includes:

- Plain `void *` / typed-struct pointer
- Function pointer (callback tables)
- Array pointer (`uint8_t *base = &someBuffer`)
- The "offset" misnomers: `primaryCompStringOffset` etc., which are
  string pointers despite the name
- Anything cast `(uint32_t)&foo` for storage — this is the
  smoking-gun pattern

Things that are *not* pointer fields, even if they look like
addresses:

- Sentinel values (`(CompetitionCup *)-1`, `0xFC`, etc.) — these
  are flags. Where they appear in real data, the struct field
  should be a `uint32_t` (or `intptr_t` if signed semantics matter)
  and the consumer should test for the sentinel explicitly. They
  do NOT need to become real pointers.
- BSS region offsets / sizes — these are byte counts, not
  addresses.
- Amiga register save slots (`d0..d7` / `a0..a6` if they survived
  the port) — these were 32-bit on m68k and stay 32-bit in the C
  port. Use `uint32_t`.
