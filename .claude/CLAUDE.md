# Pure C Port of Sensible World of Soccer (SWOS) Amiga to portable SDL

## Tasks

- Copy over all SWOS_C Amiga C files to our project and get them compiling
- Modify graphics and audio routines to work with SDL instead
- Use the original Amiga data files located in bin
- Keep the input system as close to the original as possible but using SDL for our input instead

## Long-term roadmap

We have **two** SWOS C ports in flight: this SDL port (here, `SWOS_C_SDL`) and the
Amiga C port at `C:\Projects\AI\SWOS\SWOS_C`. Both are derived from the original
Amiga SWOS2.s. The end goal:

1. **Stabilize this SDL port first.** Get the full game working — match loop,
   career mode, save/load, transfer market, edit teams, all UI flows.
2. **Back-port to the Amiga C port.** Once stable here, every fix and design
   change made on the SDL side gets propagated to the Amiga C port.
   `SWOS_C/SWOS2.s` is then **retired** — the Amiga build runs entirely from C.
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

- **"Compatible with Amiga" means our Amiga C port, not original SWOS2.s.**
  Save-file format changes that we make and back-port are fine — the original
  Amiga binary is not a compatibility target. SWOS2.s is a *reference*, not a
  *spec*.
- **Adjacent-globals workarounds and `.equ` aliases are a transitional
  scaffold.** They exist so the SDL port can run today against the same C
  code the Amiga port runs. When step 3 happens, we delete every alias and
  replace every "label-into-next-label" read with a struct field access.
  Don't write *new* code that depends on adjacency — fix the call site to
  use a proper struct field instead.
- **Don't preserve format details that exist only because m68k did them.**
  Big-endian byte order in the on-disk save, raw pointer fields, the precise
  98476-byte career-header size — these are all artifacts of the Amiga
  memory dump, not deliberate format design. They're fair game to change in
  step 4. Until then we use the BE_*/PACK_BE helpers to keep the SDL build
  reading the Amiga byte order.
- **The Amiga side is not stalled while we work on SDL.** The Amiga C port
  receives the same fixes via back-port; it just lags behind in time. Don't
  introduce SDL-only divergences without a back-port plan.

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

**Until the JSON-format rewrite lands**, the right fix is post-load
re-derivation: a `_RebuildPointersAfterLoad()` hook that recomputes every
pointer field from a stable identity (competition ID → static cup struct
lookup, careerFileBuffer offsets → `&competitionFileBuffer` etc., runtime
fields zeroed) and repopulates the runtime DIY struct
(`diyCompetitionStart`) from the loaded competition slot bytes. Wire it
into `_LoadCareerFile` after the magic check passes. This keeps the .CAR
format unchanged, fixes save round-trips, and gets back-ported verbatim to
the Amiga C port.

## Data

- bin/data - Amiga game data
- bin/grafs - Amiga graphics in 4 bitplane format
- bin/grafs_aga - Amiga graphics in 8 bitplane format (We have AGA pre-processor to switch between AGA and non-AGA)
- bin/sound - Amiga music modules and sound effects in RJP1 format
- If a file starts with `RNC` then it's RNC compressed. Our Amiga C port contains a decompressor

## Sources

- C:\Projects\AI\SWOS\SWOS_C - Amiga C port. Currently runs on an Amiga it is our source for all C files for this project
- C:\Projects\AI\SWOS\swos-port-c - Partial C++ port. It has the Amiga game loop already working with SDL. We can use this verified game loop for our pure C port. It uses the DOS version graphics so we will have to modify the graphics loading routines to convert from the Amiga bitplane format instead.
- C:\Projects\GitHub\SwosGfx - My SWOS graphics conversion app written in C#. We can use this to help understand the Amiga graphics format. **`C:\Projects\GitHub\SwosGfx\SwosGfx\AmigaTools.cs` is the authoritative reference for parsing Amiga SPRITE, PITCH (tile-map mapping + bitplane tile stream), and PALETTE data.** Consult it whenever implementing or debugging an Amiga asset decoder.
- C:\msys64\home\headk\rjp1player - RJP1 module player so we can playback the original Amiga music module and extract the sound effects. It needs to be modified to work with the SDL audio system.

## Files

- C:\Projects\AI\SWOS\SWOS_C\data\Data.c - C data structures exported from IDA Amiga SWOS
- C:\Projects\AI\SWOS\SWOS_C\data\SWOS2.s - Original Amiga SWOS 68k asm
- C:\Projects\GitHub\SwosGfx\SwosGfx\AmigaTools.cs - Contains routines to parse and manage Amiga SWOS sprites, pitch and palettes
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
