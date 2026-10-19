# DOS graphics support (PC-only) — loaders that convert DOS assets → our internal format

The Amiga build is the definitive SWOS; this is an optional PC-only feature.
Gated on `_gDosGraphics` (`include/dos_mode.h`; directory in `_gDosGraphicsDir`),
set by `[graphics] style = dos` in `SWOS.ini` (`style = 2020` is the same path
reading `bin/grafs_2020`) or `--dos-graphics=on`. (Originally `[dos] graphics=true`;
the settings reader no longer knows the `[dos]` section.) When on, we **load the DOS-format graphics from
`bin/grafs_dos` and convert them into our existing internal indexed format**
(8bpp chunky), then let the *unchanged* renderer draw them. We do NOT port a
second renderer — our chunky pipeline already renders either asset set.

Spec reverse-engineered from the working reference port `swos-port-c`
(`src/vm_stubs.cpp`, `src/swos.h`, `src/amiga/game_stubs.c`). All multi-byte
fields in the DOS files are **little-endian** (read natively on x86).

## Our target format (recap)
- **Sprite** = `SpriteGraphics` record (`game_types.h`) + **8bpp chunky** pixels
  (1 byte/pixel = palette index). Our Amiga path decodes bitplanes→chunky in
  `image_cmd.c`; the DOS path decodes DOS planar→chunky the same way.
- **Pitch** = `pitchDatBuffer` tile map + **16×16 8bpp chunky tiles (256 B each)**
  (`pitch.c`). This is exactly the DOS `PITCH*.BLK` tile shape.

## DOS file formats (`bin/grafs_dos` + `bin/sprite.dat`)

| File | Format | → our buffer |
|---|---|---|
| `PITCHn.BLK` | raw N × 256-byte **16×16 chunky** tiles (already chunky — no decode) | pitch tile bank |
| `PITCHn.DAT` | array of LE `u32` offsets; `tileIndex = offset / 256` | pitch tile map |
| `CHARSET.DAT` | font/UI sprite records (header+planar); index table is the **separate `sprite.dat`** (LE u32 offsets into charset.dat) → sprites 0..226 | sprite slots 0..226 |
| `SCORE.DAT` | planar sprite `.dat`, 114 sprites | DOS idx 227.. |
| `GOAL1.DAT` | planar sprite `.dat`, 116 sprites (loaded twice) | DOS idx 947.. and 1063.. |
| `BENCH.DAT` | planar sprite `.dat`, 155 sprites | DOS idx 1179.. |
| `TEAM1.DAT`/`TEAM2.DAT`/`TEAM3.DAT` | planar player sprites, 303 each — now present in `grafs_dos`. They are kit PATTERNS (vertical stripes / coloured sleeves / horizontal stripes), not per-team sets: each team's range takes the file for its shirt type (`dos_kit_file`, 516c125) | DOS idx 341.. / 644.. |
| `FILL.256` | `.256` image: 64000 chunky px (320×200) **+ 768-byte trailing 6-bit palette** = 64768 B | menu bg / palette |

### Sprite `.dat` record = `[24-byte header][planar pixels]`, concatenated
Header field offsets (all int16 LE): `0` data-ptr (file offset, fixed up),
`4` mask/unused, `8` type(0=planes), `10` **pixWidth**, `12` **nlines**, `14`
**wquads** (width in 16px quads), `16` centerX, `18` centerY, `22` ordinal.
Pixel bytes per record = `nlines * wquads * 8`. (Matches our `SpriteGraphics`.)

Planar layout: **4 bitplanes**, per line = plane0 block, plane1, plane2, plane3,
each block `wquads*2` bytes (`lineStride = wquads*8`). Bit combine:
`p0→1, p1→2, p2→4, p3→8` (16 colours). **MSB is the leftmost pixel** (bit 7→0).

### The decode loop (our 1-byte/pixel adaptation)
Identical bit-gather to the reference, but emit **one byte per pixel** instead of
their packed 4bpp:
```c
for (y=0; y<nlines; y++)
  for (q=0; q<wquads; q++) {
    const uint8_t *line = pix + y*wquads*8;
    uint8_t pl[4][2];
    for (p=0;p<4;p++){ pl[p][0]=line[p*wquads*2 + q*2]; pl[p][1]=line[p*wquads*2 + q*2+1]; }
    for (byteIdx=0; byteIdx<2; byteIdx++)
      for (bit=7; bit>=0; bit--) {
        int v = (pl[0][byteIdx]>>bit&1) | (pl[1][byteIdx]>>bit&1)<<1
              | (pl[2][byteIdx]>>bit&1)<<2 | (pl[3][byteIdx]>>bit&1)<<3;
        out[y*(wquads*16) + q*16 + byteIdx*8 + (7-bit)] = (uint8_t)v;   /* chunky, 0..15 */
      }
  }
```
Output line stride = `wquads*16`; `SpriteGraphics.sprData` points at this buffer.

### DOS sprite-index table + the Amiga↔DOS offset
`loadAllGameSprites` order — `(file, count, dosStartIndex)`:
`charset 227@0, score 114@227, team1 303@341, team3 303@644, goal1 116@947,
goal1 116@1063, bench 155@1179` (`kEndSprite=1334`).

**`DOS_index = Amiga_index + 114`** → to store a DOS sprite at the slot our
(Amiga) sim/renderer uses, write it at **`amigaIndex = dosIndex − 114`**. This
is the "Amiga→DOS sprite translation"; applying it at load time means the
renderer needs no per-frame mapping. (e.g. Amiga team1 227–410 ↔ DOS 341–643.)

### Palette
`kDosGamePalette[768]` (256 RGB triples, 6-bit 0..63) — hardcoded array, see
`src/dos_graphics.c` (from `DosPalette.cs Game[]`). Pitch-condition overlay:
`kPitchPaletteIndices[9] = {0,7,9,78,79,80,81,106,107}` and `kPitchPalettes[7][27]`
(Frozen/Muddy/Wet/Soft/Normal/Dry/Hard) overwrite those 9 entries per pitch type
(`physType = pitchNumberAndType & 0xFF`, clamp >6→4). 6-bit→8-bit: **`v*255/63`**.

## Integration plan (loaders only; gated on `gDosGraphics`) — concrete hook points

Our internals differ structurally from the raw DOS files, so each hook adapts
the DOS data into the exact buffer our renderer already consumes:

1. **Palette** — our renderer colours come from a 256-entry RGBA palette built in
   `platform_sdl.c` (`refresh_palette_from_copper`, used by `chunky_to_rgba`).
   Hook: when `gDosGraphics`, fill that 256-entry palette from
   `DosGfx_BuildPalette(pitchType, rgb888)` instead of the Amiga copper palette.
   (Add a `Platform_SetDosPalette(const uint8_t rgb888[768])` setter, or branch
   inside `refresh_palette_from_copper`.)

2. **Pitch** — ✅ **DONE** (`pitch.c::dos_load_pitch`). `_LoadPitchMap(mapIndex)`
   branches on `gDosGraphics`: it loads `grafs_dos/PITCH{n}.BLK` (the N×256 chunky
   tile bank) **straight into `pitchChunkyBuffer`** (tile `t` at `+t*256`, matching
   the Amiga decode path), parses `PITCH{n}.DAT` with `DosGfx_ParsePitchMap`, and
   fills the per-slot side table `s_pitchFullPtrs[i] = pitchChunkyBuffer +
   tileMap[i]*256` — exactly what `_FixupAndColorPitchFile` Step 1b produces for
   the Amiga path. `_FixupAndColorPitchFile` then short-circuits for DOS (camera
   reset only; the Amiga bitplane relocate/decode and the kit-colour fan recolor
   are skipped — `_AdjustFanColor` indexes a 16-entry LUT with 0..255 DOS pixel
   values). `animated_patterns.c` (`_DrawAnimatedPatterns` /
   `_ResetAnimatedPatternsForBothTeams`) early-returns for DOS, since it walks the
   raw `pitchDatBuffer` slots (the DOS path leaves the raw DAT bytes there, not
   relocated pointers) and would stomp `s_pitchFullPtrs`.
   - **Superseded since:** `FixupAndColorPitchFile` no longer returns early for
     DOS — it skips only Steps 1/1b (relocate/decode) and runs Step 2 on, so the
     crowd's kit recolour applies to DOS pitches too (`pitchFansConvertTable` is
     256 entries on SDL; see the comment above `dosPitch` in `src/pitch.c`). And
     `animated_patterns.c` has no DOS gate any more (no `_gDosGraphics` in it);
     `AnimatedPatterns_SyncTileMirror` notes the unmapped DOS surround instead.
   - **Row offset SETTLED: 42×55 from index 0, no header-row skip.** All six
     `PITCH{n}.DAT` are exactly 2310 = 42×55 entries, every offset a clean
     multiple of 256, every tile index in-range for its BLK. A standalone render
     of the assembled grid (`/tmp/dos_pitch_render.c`) shows a coherent pitch —
     grass, boxes, centre circle, goals, crowd, hoardings — with no blank/shifted
     top row. Matches the existing `_DrawChunkyPitch` (`PITCH_PTR_START=0`, 42×55).
     The reference's "skip 42-entry header row (→42×54)" does NOT apply here.

3. **Sprites** — ✅ **DONE (players/keepers/bench)** (originally
   `color_sprites.c::_DosOverlaySprites`; now `dos_prepare_sprites()` /
   `dos_build_all_sprites()` in `src/color_sprites.c`, called from
   `ConvertColorsAndTextSprites` when `_gDosGraphics`). For each DOS `.dat` it walks the `[24-byte header][4-bitplane
   planar]` records, `DosGfx_DecodePlanarSprite`s the pixels into a persistent
   chunky buffer, fills a `SpriteGraphics` (pixWidth/nlines/wquads/centerX/centerY
   from the DOS header; **type=0, bltsize=h·64+wq·2, maskPtr=0** — same as the
   Amiga `_ProcessImageCommandStream` chunky output, NOT the DOS header's
   type/bltsize), and stores it at `amigaIndex = dosIndex − 114`. Files +
   DOS-start indices (from the reference `loadAllGameSprites`): team 1's kit file
   `303@341` (→amiga 227), team 2's kit file `303@644` (→530) — originally
   hard-coded `team1.dat`/`team3.dat`, which put every team in stripes (fixed
   516c125, above), `goal1.dat 116@947` (→833),
   `goal1.dat 116@1063` (→949), `bench.dat 155@1179` (→1065). Built once
   (file-load + decode persist); the table slots are re-pointed every call because
   the Amiga load+convert path overwrote them earlier in the same function. The
   blitter resolves `spriteGraphicsPtr[idx]` live each frame, and player
   `Sprite`s look up their graphic by index, so re-pointing redirects rendering.
   - **Per-team kit-colour recolour: DONE** (commit `2569105`). DOS player/keeper
     `.dat` sprites are 4-bitplane = 16-colour (not 256), with the kit at the SAME
     indices the Amiga uses (10 stripes, 11 shirt, 14 shorts, 15 socks + skin), so
     the stock Amiga `team1/2PlayersColorTable` conversions recolour them.
     `dos_prepare_sprites()` now drops the DOS sprites into the table BEFORE those
     conversions (they had been overlaid *after* → default kit), with a
     pristine-decode reset each match so successive remaps don't stack.
   - **Scope / still deferred:** `score.dat` (amiga 113..226 — score/result digits,
     team-name + player-name sprites at 0x8D.., partly runtime-generated) and the
     charset/fonts stay Amiga, so text/HUD keep working.
   - Verified: all four `.dat` record-walks consume their files exactly; a
     standalone decode of `team1.dat` (`/tmp/dos_sprite_render.c`) renders
     coherent player figures (kits, shadows, animation frames, working index-0
     transparency).

## Status
- Done: INI setting + `--dos-graphics`/`--dos-gameplay` flags + `_gDosGraphics`
  global (`dos_mode.h`, `main.c`, `swos_settings.c`, `platform_sdl.h`). The INI
  keys are now `[graphics] style = amiga | dos | 2020` and `[gameplay] style`
  (the original `[dos]` section is gone).
- Done: `src/dos_graphics.c` loader module (palette / planar-sprite / pitch-map).
- Done: hook **1 (palette)** — `platform_sdl.c::refresh_palette_from_copper`.
- Done: hook **2 (pitch)** — `pitch.c::dos_load_pitch` + `FixupAndColorPitchFile`
  skipping Steps 1/1b (originally a full short-circuit, and `animated_patterns.c`
  DOS gates — both since removed, see hook 2). Row offset settled (42×55 from 0).
- Done: hook **3 (sprites, players/keepers/bench)** — `color_sprites.c::
  dos_prepare_sprites` (was `_DosOverlaySprites`) overlays the two teams' kit
  files + goal1×2 + bench at amiga 227..1219.
- Done: kit PATTERN per team (`516c125`) — TEAM1/2/3.DAT chosen by shirt type.
- Done: **DOS per-team kit-colour recolour** (`2569105`) — `dos_prepare_sprites()`
  loads the DOS sprites into the table *before* the Amiga kit conversions, so both
  teams recolour correctly (pristine-decode reset per match).
- Next follow-ups (minor/deferred): **(a) score.dat** (digits/HUD numbers) and
  **(b) DOS charset/fonts** via `SPRITE.DAT` if Amiga fonts look wrong under the
  DOS palette.
