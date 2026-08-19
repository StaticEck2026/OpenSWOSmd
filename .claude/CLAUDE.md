# OpenSWOS — one C source tree, two targets (Amiga m68k + portable SDL)

## What this project is

A pure C port of the Amiga **Sensible World of Soccer**, derived from the
original `SWOS2.s`. **One** source tree builds **both** targets:

| Target | Build | Output | Notes |
|---|---|---|---|
| SDL (Linux/Windows/macOS/WASM/ARM) | `make` | `bin/SWOS*` | the day-to-day dev target |
| Amiga m68k (A1200/AGA) | `make -f Makefile.amiga` | `bin/SWOS2` | runs under FS-UAE/WinUAE or real hardware |

`src/` and `include/` are **shared**. Platform divergence is expressed with
`#ifdef SWOS_SDL_BUILD` (defined by `include/swos_platform.h`, which the SDL
build force-includes via `-include`; the m68k toolchain never defines it).
A small set of SDL-only sources is excluded from the Amiga build by name —
see `SDL_ONLY_SRCS` in `Makefile.amiga`.

> **History:** until 2026-08-12 these were two separate repos (`SWOS_C_SDL`
> and `SWOS_C`) kept in step by a one-way copy script. They are now merged.
> **There is no sync step any more — edit the file once.** If you find a doc
> or comment referring to "the Amiga port" as a separate tree, it predates
> the merge; fix it when you touch it.

## Building

Both targets build from a plain bash shell (WSL included) — the old
"must build from VSCode" restriction is gone.

```bash
# SDL, native
make                      # bin/SWOS (linux x86) / bin/SWOS.exe (cygwin)
make X86_64=1 -j$(nproc)  # bin/SWOS.x86_64
make WASM=1               # bin/SWOS.html   (needs emsdk on PATH)

# Amiga m68k — toolchain ships inside the amiga-debug VSCode extension.
# ⚠️ It MUST be the owner's fork, github.com/benbaker76/vscode-amiga-debug —
# NOT the marketplace build. Its FS-UAE has the gdb WRITE packets (M/P/G) that
# let the AmigaScope harness drive the target (inject input, set vars, call
# functions) instead of only reading it, plus a log-spam fix. Both installs share
# the SAME dir name and the SAME publisher/repo in package.json, so they are
# indistinguishable after the fact — if gdb reads fine but writes silently no-op,
# that's the stock build. See scripts/amiga/amigascope/README.md.
# NOTE the path is the WINDOWS-side .vscode (VSCode is installed on the host),
# NOT ~/.vscode in the WSL home — there is no extension dir there. Getting this
# wrong fails as `Error 127` (compiler not found), and an up-to-date tree hides
# it behind "Nothing to be done for 'all'" until something actually recompiles.
EXT=/mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-1.8.2/bin/linux
PATH="$EXT/opt/bin:$EXT:$PATH" make -f Makefile.amiga -j$(nproc)
```

⚠️ **Toggling any `-D` flag requires a `clean` first** on both targets — make
compares timestamps, not CFLAGS, so a flag change silently relinks stale objects.

### Release builds — `scripts/build_dist.sh`

Builds every target this machine can produce and packages the binaries into
`bin/SWOS.zip` (WASM excluded from the zip — it's a web deployment, and the
`.data` preload alone is ~21 MB):

```bash
scripts/build_dist.sh              # TESTER drop: all targets + bin/SWOS.zip
scripts/build_dist.sh --release    # PUBLIC build: stock injuries + cards
scripts/build_dist.sh --only windows --no-clean
scripts/build_dist.sh --help
```

**Tester builds set `SWOS_TEST_NO_INJURIES_CARDS=1` by default** — that is the
owner's standing preference (without it a tester can't hit VIEW RESULT through a
season: substitution menus and booking prompts every match). `--release` turns it
off for a public build. Keep the flag **identical across every platform in one
drop**: a macOS build with it next to a Windows build without it generates bug
reports for non-bugs (under the flag, subbing an injured player scores as a
DOWNGRADE — see the Makefile's "KNOWN DISPLAY SIDE-EFFECTS" block).

Three traps it exists to handle — **don't hand-roll a release without them**:

- **`SWOS_SAVE_FORMAT_JSON_VERIFY` defaults to ON for every target except
  WASM.** The `?= 0` line sits *inside* the Makefile's `ifdef WASM` branch, so a
  plain `make` builds in the save round-trip verifier, which makes every SAVE
  zero the saved fields, reload the file and deliberately not restore. The
  script passes `=0` (use `--verify` to opt back in).
- **The Amiga makefile keeps objects in its own `obj/` tree**, so `rm -rf build`
  leaves it stale and the build reports "Nothing to be done for 'all'". The
  script also runs `make -f Makefile.amiga clean`.
- **`Makefile.amiga` has no `SWOS_*` knobs**, so the m68k target would silently
  miss `SWOS_TEST_NO_INJURIES_CARDS`; the script passes it through that
  makefile's `EXTRA_CFLAGS` escape hatch. Confirm it landed by checking
  `obj/career_status.s` — under the flag `IsPlayerInjured` is just
  `clr.w %d0; rts`.

`bin/SWOS.exe` is a **Cygwin** binary (imports `cygwin1.dll`; testers need that
plus `SDL2.dll`). The Makefile selects the Windows target from the *host shell*
(`$(OS)` / `uname -s`), never from a flag, so from WSL the script reaches Cygwin
through Windows interop (`/mnt/c/cygwin64/bin/bash.exe -lc`). Machine paths are
overridable: `EMSDK_DIR`, `AMIGA_TOOLCHAIN`, `CYGWIN_BASH`.

### macOS — `scripts/build_macos.sh`

macOS cannot be built here (`make macos-universal` / `make macos-app` hard-fail
unless `uname -s` is Darwin, and there is no cross-compile path), so this script
drives the Mac mini over ssh and brings the bundle back:

```bash
scripts/build_macos.sh             # tester build -> bin/SWOS-macOS-app.zip
scripts/build_macos.sh --release   # public build
scripts/build_macos.sh --no-pull   # build the Mac's tree as-is
```

Defaults match `build_dist.sh` so a drop is uniform. Overrides: `MAC_HOST`
(default `bbaker@192.168.1.37`), `MAC_PATH` (`~/Programming/OpenSWOS`),
`MAC_ZIP`.

- It `git pull --ff-only`s and **prints the commit it built**, because a stale
  remote checkout is invisible otherwise — a tester once reported a fixed bug as
  still present on macOS purely because that Mac was two commits behind.
- The bundle is packed with `ditto -c -k --sequesterRsrc --keepParent`, never
  `zip -r`: `SDL2.framework` is built out of symlinks, and plain zip follows
  them, producing a bloated archive whose framework won't load. The summary
  prints the preserved-symlink count — 0 means the bundle is broken.
- **`ssh` does not preserve argv boundaries** — it joins its arguments into one
  string that the remote shell re-splits, so multi-word arguments must be
  pre-quoted (`printf %q`). Getting this wrong silently shifted a make variable
  onto `-j`.

## Long-term roadmap

1. ~~**Stabilize the SDL port.**~~ / ~~**Back-port to the Amiga C port.**~~
   Superseded: the trees are merged, so a fix lands on both targets at once.
   `SWOS2.s` is retained as a **reference only** (`data/SWOS2.s`) — the Amiga
   build runs entirely from C.
2. **Keep both targets building.** Any change to shared code must compile for
   m68k as well as SDL. The Amiga build is the stricter of the two (32-bit
   pointers, big-endian, `-nostdlib`), so it catches bitness/endian slips early.
3. **Untangle the data overlays.** The current code mirrors the Amiga's BSS
   layout faithfully — adjacent-globals reuse, careerFileBuffer-style
   overlays, `selTeamsPtr` aliased onto its own data, etc. (see
   "Adjacent-globals BSS pattern" below). Once both ports run on C, those
   overlays become **proper structs** with named fields. No more `.equ`
   aliases, no more "this label is the next label's label", no more
   adjacent-reads-into-the-following-global. Memory is structured.
4. **Replace the save format.** The current `.CAR` format is a raw memory
   dump of the careerFileBuffer overlay region — fragile against any layout
   change, and contains absolute pointers (see "Pointer-to-disk problem"
   below). Eventually replace with a structured format (likely JSON or a
   tagged binary) that serializes named fields, not memory addresses. This
   change happens *together* on both ports.

What this means for current decisions:

- **"Compatible with Amiga" means our own m68k target, not original SWOS2.s.**
  Save-file format changes we make are fine — the original Amiga binary is not
  a compatibility target. SWOS2.s is a *reference*, not a *spec*.
- **Adjacent-globals workarounds and `.equ` aliases are a transitional
  scaffold.** They exist so both targets can run today against the same C
  code. When step 3 happens, we delete every alias and replace every
  "label-into-next-label" read with a struct field access. Don't write *new*
  code that depends on adjacency — fix the call site to use a proper struct
  field instead.
- **Don't preserve format details that exist only because m68k did them.**
  Big-endian byte order in the on-disk save, raw pointer fields, the precise
  98476-byte career-header size — these are all artifacts of the Amiga
  memory dump, not deliberate format design. They're fair game to change in
  step 4. Until then we use the BE_*/PACK_BE helpers to keep the SDL build
  reading the Amiga byte order.
- **Don't introduce an SDL-only divergence without checking the m68k build.**
  There is no back-port lag any more — a shared file that only compiles on SDL
  breaks the Amiga target in the same commit. Build both before you call it done.

## Pointer-to-disk problem

The careerFileBuffer overlay (see `swos_globals_career_overlay.c`) contains
several BSS labels typed as pointers — `currentCompetitionPtr`,
`secondaryCompetitionPtr`, `careerCompetitionSlotPtrTable[5]`,
`*CompStringOffset`, `diyCompetitionTablePtr`, plus runtime-only pointers
inside `tmdFileBuffer` (`inGameTeamPtr`, `spritesTable`, etc.). Because
`_SaveCareerFile` is a raw memory dump of the overlay region, those pointer
*values* end up in the .CAR file. After a rebuild (or load on a different
machine), the linker places the destination buffers at different addresses
and the saved pointers become stale.

**Original SWOS2.s ignored this problem** — the Amiga binary loaded at a
fixed address with a stable BSS layout, so saved pointer values stayed
valid across runs of the same binary. `LoadCareerFile` does no pointer
fixups at all (`LoadCompetitionFile` is just a magic-bytes verifier). This
is one of the bugs we *don't* inherit: our Amiga C port no longer places
data at fixed addresses either, so the same fixup is needed on both ports.

**The JSON save format is live** (`src/save_format_json.c`, ~3270 lines).
Build with `make SWOS_SAVE_FORMAT_JSON=1` to write new saves in JSON via
mjson — pointer fields are emitted as semantic identities (competition id,
string-pool key, etc.) rather than raw address bytes, so saves round-trip
across rebuilds and across 32/64-bit. Load auto-detects format from the
file header so legacy `.CAR` dumps still open. `SWOS_SAVE_FORMAT_JSON_VERIFY=1`
adds a load-time diff that compares the JSON-derived byte image against the
opaque-data fallback — a zero-mismatch result means every meaningful byte
is covered by typed fields/blobs and opaque_data can be removed. Step 4
of the long-term roadmap is now mostly "delete the legacy dump path once
all save types are proven."

## Data

`bin/` is the **single** data directory, shared by both targets. The SDL binary
runs with `bin/` as its CWD; FS-UAE mounts `bin/` as **DH1:** and `cd`s into it,
so the same relative paths (`data/`, `grafs/`, `sound/`, `save/`) resolve on both.
Nothing is duplicated under `uae/` any more.

- bin/data - Amiga game data
- bin/grafs - Amiga graphics in 4 bitplane format
- bin/grafs_aga - Amiga graphics in 8 bitplane format (AGA pre-processor switches between AGA and non-AGA)
- bin/grafs_dos, bin/grafs_2020, bin/grafs_arcade - alternate asset tiers (SDL only)
- bin/sound - Amiga music modules and sound effects in RJP1 format
- bin/save - save games, shared by both targets
- bin/data_json - editable JSON that bakes into competition/menu/tactics C (SDL only)
- If a file starts with `RNC` then it's RNC compressed; `src/rnc.c` decompresses it on both targets.

## Sources

- **SWOS-RE — `C:\Projects\Amiga\SWOS-RE` (github.com/benbaker76/SWOS-RE).**
  The upstream reverse-engineering project this port grew out of: the
  documented understanding of the original Amiga 68k binary. **This is the
  authority on original intent** — when the C looks wrong, arbitrary, or you
  need to know what a magic constant/struct field/BSS label actually meant,
  check here BEFORE guessing or "fixing" it. Contains:
  - `README.md` (~77KB) — the narrative: version info, DOS-vs-Amiga history,
    formats, and how the RE was done (IDA Pro for symbols → Ghidra for m68k
    decompilation via a patched exporter plugin → Python cleanup passes).
  - `Docs/` — `Structs.txt`, `Variables.md`, `Prototypes.txt`, `Menus.txt`,
    `Sprites.txt`, plus the reloc dumps.
  - `IDAPro/`, `Ghidra/` — the actual databases.
  Its DOS-side naming derives from Zlatko Karakaš's DOS IDA database, which is
  why our symbol names often match the DOS port's.
- `data/` (in this repo) - IDA/Ghidra reference artifacts exported from
  SWOS-RE: `Data.c`, `SWOS2.s`, `SWOS2_IDA.h`, `SWOS2.h`. **Reference only —
  not compiled** (`Makefile.amiga` scans only `src/`). These are a *snapshot*;
  SWOS-RE is the live source.

  `data/SWOS2.s` (~5.4 MB, matching SWOS-RE's `Amiga/SWOS/SWOS2.s`) is **the**
  disassembly to read — every doc and audit cites it. It is not assembled.

  > A second, smaller (~1.2 MB) *sparse* `SWOS2.s` used to sit at the repo root,
  > paired with `asm/swos2_unit.s` which `include`d it. That pair was the original
  > bootstrap scaffolding — assemble the whole 68k program as one vasm unit and
  > progressively move routines out into C. Both were **deleted 2026-08-12**: the
  > port runs entirely from C, neither was in the build (`swos_asm_sources` is
  > only `asm/resload_unit.s`), and `startShim` comes from
  > `src/platform_amiga_stubs.c`. Deleting them left the Amiga binary
  > byte-for-byte identical (same sections, same 5721 symbols). Recover from git
  > history if ever needed.

### Writing new m68k assembly

There IS still a live asm path — don't resurrect the old monolith to use it:

- `asm/resload_unit.s` (vasm) `include`s `../Resload.s`. **`Resload.s` at the
  repo root is a real build input** — don't mistake it for the deleted
  `SWOS2.s` and remove it.
- `support/gcc8_a_support.s` (gas) is the toolchain support unit.

To move a routine back to assembly (e.g. an interrupt handler where C codegen
isn't tight enough): add a **new** `asm/<name>.s`, append it to
`swos_asm_sources` in `Makefile.amiga`, `xdef` the entry point, and delete or
`#ifdef` out the C version. Copy the original body from `data/SWOS2.s`. Do not
re-introduce a single all-encompassing asm unit — the per-routine split is what
lets the two targets share `src/`.
- C:\Projects\AI\SWOS\swos-port-c - Partial C++ port with the Amiga game loop working under SDL. Used as the verified game-loop reference.
- C:\Projects\GitHub\SwosGfx - SWOS graphics conversion app (C#). **`SwosGfx\AmigaTools.cs` is the authoritative reference for parsing Amiga SPRITE, PITCH (tile-map mapping + bitplane tile stream), and PALETTE data.** Consult it whenever implementing or debugging an Amiga asset decoder.
- C:\msys64\home\headk\rjp1player - RJP1 module player for the original Amiga music modules / SFX extraction.
- github.com/benbaker76/AmigaScope - the gdb/profiling harness, a submodule at `scripts/amiga/amigascope`.

## Files

- `data/Data.c` - C data structures exported from IDA Amiga SWOS
- `data/SWOS2.s` - Original Amiga SWOS 68k asm
- C:\Projects\GitHub\SwosGfx\SwosGfx\AmigaTools.cs - Routines to parse and manage Amiga SWOS sprites, pitch and palettes
- C:\Projects\GitHub\SwosGfx\SwosGfx\bin\Debug\net8.0\Amiga_Images_16 - Extracted graphics from Amiga SWOS in 8bpp BMP format

## Architecture conventions

### Graphics pipeline is 8bpp chunky end-to-end

- **Frontbuffer**: 8bpp chunky. One byte per pixel = palette index. RGBA conversion happens only at the SDL texture upload boundary (platform_sdl.c).
- **Sprites**: 8bpp chunky **after load**. The Amiga bitplane decode happens once in `_ProcessImageCommandStream` (src/image_cmd.c) — every render-time consumer (blitter, `_ConvertSpriteColors`, glyph blit, `_Text2Sprite`, etc.) reads chunky bytes, not planes.
- **Why**: eliminates per-pixel bitplane-gather at blit time, reduces `_ConvertSpriteColors` to a 1-byte LUT, and unifies the render path with the DOS-data variant so either asset set works with the same pipeline. Conversion is the responsibility of the loader, not every downstream function.
- When adding new rendering code, assume 8bpp chunky sprite data. If you need the bitplane layout, you are doing something wrong — fix the load site instead.

### Endian-swap macros are explicit for back-portability

SWOS data files (team files, RNC headers, save games, audio modules) were authored on Amiga m68k (big-endian). Fixes for the x86 LE host MUST go through `src/endian_helpers.h` rather than inline byte arithmetic or `__builtin_bswap*` calls, so the SDL changes can be cherry-picked cleanly back to the Amiga tree (where every macro is a no-op).

Use:
- `BE16_TO_HOST(x)` / `HOST_TO_BE16(x)` — 16-bit byteswap (no-op on BE host)
- `BE32_TO_HOST(x)` / `HOST_TO_BE32(x)` — 32-bit byteswap
- `BE_READ_U16(p)` / `BE_READ_U32(p)` — unaligned big-endian read from a byte pointer (use for file-format headers like RNC)
- `BE_WRITE_U16` / `BE_WRITE_U32` — unaligned BE write
- `PACK_BE_U16(hi, lo)` — combine two separate single-byte struct fields into the uint16 an m68k `move.w` would produce. Use this wherever the original asm/C did `*(uint16_t *)&struct.byteField` and the next byte is a *separate* logical field (e.g. `PACK_BE_U16(team.countryNumber, team.teamOrdinal)` for team id).

Do **not** write `*(uint16_t *)&foo.byteField`, `((uint16_t)hi << 8) | lo`, or hand-rolled byte arithmetic for endian-sensitive reads — use the macros so the intent is greppable and the Amiga build stays compatible.

### Save/load is typed JSON of NAMED fields — no addresses, no ranges, no raw dumps

The save/load format (`src/save_format_json.c`) serializes **named fields** to
JSON. This is the same cleanup as untangling the Amiga's overlapped /
adjacency-packed data (roadmap step 3): the on-disk format must be **decoupled
from the runtime memory layout** so a save survives rebuilds, 32↔64-bit, and
LE↔BE. The Amiga's memory-saving tricks (overlaid globals, "this label is the
next label's label", region dumps) are what made SWOS hard to reverse-engineer
— they must NOT leak into the save format.

Save/load must **never**:

- **Decide what to save/restore by a runtime ADDRESS RANGE.** Testing a field's
  runtime address against a `[&symA, &symB)` pointer span — e.g.
  `field_in_range(f, &competitionFileBuffer, &numSelectedTeams)` — is
  forbidden. Symbol addresses are build/target-specific: a range that is valid
  on the 64-bit SDL build is **empty or inverted** on the 32-bit Amiga when the
  two symbols' relative order flips. (Real bug, 2026-06-09: on the Amiga
  `&competitionFileBuffer` (0x565a94) sorts *above* `&numSelectedTeams`
  (0x33cb00), so the `.DIY` comp-scalars range was empty, `numSelectedTeams`
  never restored → 0 → cup `_VerifyTeamLoaded` panic.) Select fields to
  serialize by **explicit name/key**, never by address span — and never do
  "fuzzy"/approximate pointer comparisons either.
- **Emit raw binary memory dumps** of a struct/overlay region (a `FLD_BYTES` of
  a whole buffer). Promote each region to typed, named fields.
- **Emit or compare pointers / pseudo-pointer (address-derived) values.** Save a
  semantic identity (competition id, team index, string-pool key) and
  re-derive the pointer on load.

If you find yourself comparing `&globalA` to `&globalB`, gating on whether an
address falls in a span, or copying a region by address span — stop. That is
the exact fragility this format exists to remove.

**KEEP binary READ support for `.TAC`, `.HIL`, `.EDT`, and `.CFG`.** The JSON
migration applies to the big overlay-dump saves (`.CAR`/`.SEA`/`.PRE`/`.DIY`) —
there the legacy raw-dump *reader* is dead post-untangle and is being retired.
But the small fixed-layout binary records (`.TAC` tactics, `.HIL` highlights,
`.EDT` edited teams / `CUSTOMS.EDT`, `.CFG` options) are still authored/consumed
as binary (`.EDT` is a SWOS-community interop format too); their
`_Read*`/`_Load*`/`_LoadFileEx` paths are NOT legacy and must be maintained.
Don't fold them into a "retire legacy reads" cleanup. See `docs/SAVE_DATA.md`
"Format migration / read-support policy".

### NULL-deref crashes are a symptom, not a bug

When ported asm code SIGSEGVs on a pointer field (`team->inGameTeamPtr`, `team->spritesTable`, a SpriteGraphics slot, etc.), **do not add a `if (!ptr) return;` guard and move on**. The guard masks a real problem:

- On Amiga the BSS was one flat `ds.b` block. Every symbol landed somewhere mapped and adjacent globals were contiguous, so reads past the "end" of a variable silently picked up the next declared global (often intentional — see the `stadiumMenuLittlePlayer`/`littlePlayerSpriteGeneric`/`littlePlayersColorTable` triple that `MENU_CMD_SPRITE_COPY` reads as one 36-byte struct).
- On our SDL host each global is its own `.o` symbol at a linker-determined address. Reads beyond a variable's declared size hit an unrelated global — or unmapped memory — and nothing is adjacent unless we make it so.

When you see a NULL-deref crash, root-cause it:

1. **Look at the Amiga source.** If the crashing code is reading `*(type *)&sym + offset`, grep `SWOS2.s` (or the Amiga-port `Data.c`) for the label — whatever `dc.b`/`dc.w`/`dc.l` follows it is the "next global" the code expects adjacent. Merge the run into one shared backing buffer with `.equ` symbol aliases (see `stadiumMenuLittlePlayerBackingBuf` / `currentMenu` in `src/swos_globals.c` for the pattern).
2. **Look for undersized stubs.** If the crashing pointer should have been written during init, trace back — is it a `char foo[4096]` auto-stub in `swos_amiga_data_stubs.c` masking a typed definition the code expects? Replace the stub with the real typed data from `Data.c` / `SWOS2.s` (see `src/swos_data_bulk_imports.c` for the bulk-imported set).
3. **Look for int8_t-vs-int16_t traps.** A `int8_t[2N]` definition that the game reads as `int16_t[N]` decodes to different values on LE vs BE — crashes and weird behaviour both come from this. Re-encode the table with the correct type.
4. **Only after ruling the above out**, consider whether the pointer is genuinely optional in the intended design (rare — most of these fields were guaranteed non-NULL by the original Amiga layout).

Adding a NULL guard "because it crashes" hides whichever of 1/2/3 is actually wrong and lets unrelated data stay broken for longer.

### Panics are signals — never patch them out

The same rule applies to every original-asm panic, not just NULL-deref SIGSEGVs. The Amiga code panics (`_PanicHaltLoop`, `panicReasonCode = 0x...`) when an invariant has been violated — a team that was supposed to be in the cache isn't, a slot count that should be 16 is 6073, a magic value got stomped, a `findInList` walked off the end. These panics are the original game telling us the *previous* setup step left state in a shape it shouldn't be. **Trust them.**

Do not:
- Replace `_PanicHaltLoop()` with a `return` / fallback / "load it now" path to make the menu draw work.
- Add a `if (X != expected) X = sane_default;` clamp before a panic-checking site.
- Wrap a `_VerifyTeamLoaded` / `_FindTeamInDIY` / `_GetCurrentSeasonPointer` call in a fallback that loads or fabricates the data the original would have expected to already be there.
- Bump a sentinel/epsilon to silence a `magicValue != 0x12345678` check.

Do:
- **Trace upward.** What was meant to populate the missing data? Add prints there to verify it ran and wrote the expected values.
- **Compare to the asm.** Find the analogous call site in `SWOS2.s` and check it matches the C ordering, parameters, and side effects (especially registers d0-d7/a0-a6 the asm relied on as scratch state).
- **Suspect adjacency / endian / sizing first.** 90% of the panics we hit on bring-up are one of: undersized backing buffer (`teamSelectionKeyCache` was 1 byte), `int8_t[2N]` stored where the game reads `int16_t[N]`, `*(uint16_t *)p` reading a `{country, ordinal}` byte pair without `BE_READ_U16`, or a `.equ`-aliased global the SDL build forgot to alias.
- **Leave the panic in.** Once the *real* setup bug is fixed, the panic stops firing on its own. If you can't find the cause yet, leave the crash and tell the user — a hard panic on an unknown invariant is far more useful than silent corruption.

Patching out a panic to "make the screen render" is the worst kind of bring-up debt: the symptom disappears but the corrupted state propagates into save files, wage calculations, scorer tables, etc., where it surfaces hours later as something completely unrelated.

### Amiga target: CHIP RAM placement is load-bearing

Data the original kept in **Chip RAM** must stay there — anything the custom
chips DMA from (copper lists, bitplanes, the frontbuffer, audio samples) is
unreachable from Fast RAM and fails silently or corrupts the display.

- Globals that lived in `BSSInChip` in `SWOS2.s` must land in Chip RAM in C.
  Use the `SWOS_CHIP_BSS` section attribute (`include/swos_sections.h`), not a
  plain `.bss` definition.
- **`SWOS2.ld` is authoritative** for section placement and the memory map.
  Read it before adding a new module or migrating a global out of asm.
- **Validate every Amiga build against the map file** (`bin/SWOS2.map`):
  - code/data land in the expected region (Chip vs Fast)
  - `BSSInChip`-equivalent data is still in Chip RAM
  - no large unintended shifts in segment sizes — those expose hard-coded
    offset bugs elsewhere

On SDL these attributes compile to no-ops, so a Chip-RAM mistake is **invisible
on the SDL target** and only shows up on Amiga. Check the map, don't assume.

### Amiga target: code-as-data and boundary spill

`SWOS2.s` sometimes reuses a **code block as a data buffer** once it's no longer
executed (e.g. `ShowLoaderIntroSequence` is really `team1Scorers[8]`), and
sometimes indexes **past the declared end** of a label, relying on the next
label being contiguous.

Porting rule: do **not** preserve the fake code-label aliases. Move the storage
into real, correctly-typed globals in the correct section, and update the call
sites. For genuine adjacency, merge the run into one backing buffer with `.equ`
aliases — see the `stadiumMenuLittlePlayer` triple in `src/swos_globals.c`, and
"NULL-deref crashes are a symptom" above. Use `bin/SWOS2.map` to confirm whether
two objects really were contiguous.

### Required C types

Use only the fixed-width integer types: `int8_t`/`int16_t`/`int32_t` and their
`uint` counterparts. Avoid `long`, `short`, `char` (except raw byte buffers and
`char *` strings), `size_t` and `bool` unless there's a strong reason — the
widths differ between the 32-bit m68k target and the 64-bit SDL hosts, and
obscuring the 68k semantics is how endian/size bugs get in.

### Struct access — typed structs, not byte offsets

Game struct definitions live in **`include/game_types.h`**. Access fields via
`->`/`.`, declare externs with real types (`extern Sprite *lastPlayerPlayed;`,
not `extern uint32_t`). For `FixedPoint` (16.16): `.raw`, `.s.i`, `.s.frac`.

Do **not** reintroduce byte-offset macros (`RD16(ptr, SPRITE_X)`,
`*(int16_t *)(ptr + 30)`, …) — they were fully removed. When `SWOS2.s` uses a
capitalised EQU offset like `TEAMFILE_TACTICS`, look the struct up in
`data/SWOS2_IDA.h` and use the matching `game_types.h` struct. Prototypes in the
asm comments (`; prototype: void __usercall Foo(PlayerFile *player@<a0>)`) tell
you the parameter types.
