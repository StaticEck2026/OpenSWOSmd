# DOS graphics support (PC-only) — loaders that convert DOS assets → our internal format

The Amiga build is the definitive SWOS; this is an optional PC-only feature.
Gated on `gDosGraphics` (`dos_mode.h`), set by `[dos] graphics=true` in `SWOS.ini`
or `--dos-graphics=on`. When on, we **load the DOS-format graphics from
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
| `team1.dat`/`team3.dat` | planar player sprites, 303 each — **ABSENT from grafs_dos**; needed for players | DOS idx 341.. / 644.. |
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

## Integration plan (loaders only; gated on `gDosGraphics`)
1. **Palette** — when DOS, upload `kDosGamePalette` (+ pitch overlay) via our
   palette path instead of the Amiga palette.
2. **Pitch** (`pitch.c` load site) — parse `grafs_dos/PITCH{n}.BLK`→tile bank and
   `PITCH{n}.DAT`→our tile map. ⚠ Convention: the DOS `.dat` is 2310 = **42×55**;
   the reference skips the first 42 (a header row) → 42×54 grid. Our memory note
   says our map is 42×55 from index 0. **Reconcile at wire-up** — match whichever
   our `pitchDatBuffer`/renderer expects (likely no skip; verify against a frame).
3. **Sprites** (`asset_loader.c` grafs load) — for each DOS `.dat`, walk the
   `[header][planar]` records, run the decode above into chunky, build a
   `SpriteGraphics`, and store at `amigaIndex = dosIndex − 114`. charset uses the
   separate `sprite.dat` offset table.

## Status
- Done: `[dos]` INI section + `--dos-graphics`/`--dos-gameplay` flags + `gDosGraphics`
  global (`dos_mode.h`, `main.c`, `swos_settings.c`, `platform_sdl.h`).
- Next: `src/dos_graphics.c` (this spec) + the 3 hook points above.
- Player sprites (`team1/team3.dat`) are not yet in `bin/grafs_dos` — first
  renderable win is the **pitch** (+ charset/score/goal/bench); wire players when
  those DATs are added.
