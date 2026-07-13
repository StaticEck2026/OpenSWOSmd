# AGA Rendering (8 bitplanes / 256 colours, mixed per-bank depth)

Status (2026-07-11): **P1 + P2 done and SDL-verified. P3 (Amiga hardware-sprite
speedup) pending** — see [AMIGA_SPRITES.md](AMIGA_SPRITES.md).

AGA mode gives the game a 256-colour display (8 bitplanes) for backgrounds and
the pitch, while keeping sprite/menu/charset banks at **4 bitplanes (16 colours)**.
The depth is **mixed per bank** and auto-detected at load, so the same decode path
handles both. The switch is `#define AGA` in `include/display.h` (now the default);
comment it out for the legacy OCS build (4 planes / 16 colours, folder `grafs/`).

This document covers the *asset + palette + decode* side. The in-match
**hardware-sprite vs bob** rendering that the 4bpp sprites feed into is documented
separately in [AMIGA_SPRITES.md](AMIGA_SPRITES.md).

---

## 1. Mixed-depth banks

The AGA graphics are produced from the DOS artwork by **SwosGfx** (`!DOS2AGA.bat`,
`-bitplanes=auto`), which picks the depth per file from the art's highest palette
index. Two geometry classes, both full fixed-dimension planar images:

| Class | Dimensions | 1-plane bytes | Banks | Depth |
|---|---|---|---|---|
| **Sprite sheet** | 320×256 (40 B/plane row) | 40×256 = **10240** | charset, menus, menus2, cjcgrafs, cjcteam1/2/3, cjcteamg, cjcbits, cjcbench, team kits | **4bpp** |
| " | " | " | **soccer_s** (ball R/S rotation) | **8bpp** |
| **Full-screen** | 352×272 (stride `PLANE_STRIDE`=44) | 44×272 = **11968** | menubg, menubg2, rainbow, disk22, olddisk1, loader* | **8bpp** (OCS: 4bpp) |

RAW banks are **RNC-compressed on disk** and decompressed at load (`LoadFileEx`,
`src/file_io.c`).

## 2. Per-bank depth auto-detection — `DetectBankPlanes`

No per-file header is needed: the RNC header already carries the decompressed size,
and `LoadFileEx` returns it via `outFileSize` (uncompressed for RNC, raw byte count
otherwise — `file_io.c:289,308`). Each decoder already fixes its per-plane row bytes
and row count, so:

```
planes = decompressedSize / (planeRowBytes × rows)      // include/display.h: DetectBankPlanes()
```

- Sprite banks: `size / (BITPLANE_ROW_BYTES × SPRITE_SHEET_ROWS)` = `size/10240` → 4 or 8.
- Full-screen: `size / (PLANE_STRIDE × SCREEN_HEIGHT)` = `size/11968` → 4 (OCS) or 8 (AGA).

Exact-division checked; falls back to compile-time `NUM_PLANES` on an unexpected size.
This mirrors SwosGfx's `-bitplanes=auto` — change the art depth and it self-adjusts.
`DetectBankPlanes` is currently **`#ifdef SWOS_SDL_BUILD`**; P3 ungate it for Amiga.

## 3. Decode sites (P1)

Three decoders were made per-bank-depth-aware (SDL branches; Amiga `#else` is the
bitplane-direct path):

| Decoder | File | Reads |
|---|---|---|
| Sprite/glyph (`blitDispatch`) | `src/image_cmd.c` | `s_bankSrcPlanes` (set per 0x7000/0x8000 load), source stride `40 × planes` |
| Menu background (`LoadOverlayToFrontbuffer`) | `src/frontend.c` | `DetectBankPlanes(size, PLANE_STRIDE, SCREEN_HEIGHT)`, stride `44 × planes` |
| Loader frames (`loaderIntroLoadFrameToFrontbufferAlt`) | `src/loader_intro.c` | same as overlay |

`s_bankSrcPlanes` (file-static in `image_cmd.c`) tracks the current sprite bank's
depth; it is reset to `NUM_PLANES` at each `ProcessImageCommandStream` entry and set
from the load size on each `0x7000`/`0x8000` opcode. On the Amiga build it stays
`NUM_PLANES` until P3 wires the detection in.

## 4. Folder redirect + fallback (P2)

`GRAFS` = `"grafs_aga/"` under AGA. But many bank filenames are hardcoded `"grafs/…"`
string literals that bypass the macro. Rather than edit ~39 strings (one file is
auto-generated), the redirect lives entirely in the **SDL loader**:

`src/resload.c` `open_case_insensitive` (`#ifdef AGA`): any `grafs/…` **or**
`grafs_aga/…` path tries **`grafs_aga/<base>` first, then `grafs/<base>`** by basename.

Consequences of this design:
- `grafs_aga/` holds **only AGA-specific assets**. Shared/unconverted assets live in
  `grafs/` and are reached by fallback.
- **The extended charset is NOT in `grafs_aga/`** — see §5.
- `disk2/socpic/tactics/winscrn.raw` have no AGA version and don't exist as OCS
  either (pre-existing) — harmless.
- `grafs_arcade/…` does not match the `grafs/` prefix (next char is `_`).

The Amiga asset backend is separate (`Resload.s`), so this redirect is SDL-only. On
the Amiga the assets must already be in `grafs_aga/` (or the same fallback added to
its backend). `bin/grafs_aga` was staged for the SDL build **without** CHARSET.RAW.

## 5. Extended charset (SWOS_Font) — always `grafs/`

`bin/grafs/CHARSET.RAW` is the **extended font** from `C:\Projects\Python\SWOS_Font`
(`CHARSET_NEW.RAW`), not the stock charset — it packs extra accented/punctuation
glyphs (À, Á, Ç, É… sprites `0xC4C4..0xC519`) into unused sheet regions. It is
**ungated / always active**: `src/font_ext_script.inc` (glyph extracts, `#include`d
in `swos_globals.c:187`) + `src/font_ext_conv.inc` (`FontExt_ApplyConv()` at
`SWOS.c:325`). It is 4bpp so it works in both OCS and AGA.

SwosGfx's `!DOS2AGA.bat` writes a **plain** `grafs_aga/CHARSET.RAW` — do **not** ship
it. Keep `grafs_aga/` charset-free so the redirect falls back to the extended
`grafs/CHARSET.RAW`. (Amiga side: same rule — the extended font must be the one the
band/bob code loads.)

## 6. 256-colour palettes (P2) — were lost, now restored

AGA writes 256 copper colours via `WriteCopperPalette` (`display.c`, 8 banks × 32).
The C globals `_menuPalette` / `_gamePalette` had been **overwritten with the OCS
16-entry values** when the data was regenerated from `Data.c` (the AGA 256-entry
variants were casualties; the blitter/copper survived).

Restored as **AGA-conditional** `uint16_t [256]` (OCS 16-entry kept in `#else`):
- `_menuPalette` — `src/swos_globals.c`
- `_gamePalette` — `src/swos_amiga_data_imports.c` (WriteCopperPitchPalette overlays
  9 pitch-condition entries via `_paletteIndices` before pushing to copper)

Values baked from SwosGfx `DOS_Palettes/{Menu,Game}.txt` via `ToAmiga12` (upper
nibble of each 8-bit channel: `((r>>4)<<8)|((g>>4)<<4)|(b>>4)`). **Regenerate** with:

```
SwosGfx -palettes -pal-color=amiga12 -pal-format=c -pal-count=256   (from DOS_Palettes)
```

## 7. SDL palette extraction (P2)

On the Amiga the copper drives the display directly; on SDL the chunky→RGBA LUT is
built by `refresh_palette_from_copper` (`src/platform_sdl.c`). Its AGA branch was a
`TODO` stub → everything rendered black. Implemented: walk 8 banks × 32 at
`COP_PAL_B{0..7}_C0_VAL_OFF` (stride 4, 12-bit `0x0RGB`, `*17` to 8-bit) → `s_palette`.

## 8. Dormant-path bring-up (P2)

`#define AGA` had **never been compiled on either tree**, so the AGA blocks were full
of stale references fixed during bring-up:
- Copper symbols: bare `copperListBuffer` / `copperZeroLong` / `loaderModeFlag` →
  `_`-prefixed (`display.c`, `fade.c`, `display.h` `COPPER_W`).
- Palette symbols: bare `menuPalette` / `gamePalette` / `paletteIndices` /
  `pitchPalettePtrs` / `pitchNumberAndType` in AGA blocks → `_`-prefixed
  (`SWOS.c`, `display.c` `WriteCopperPitchPalette`, `match_setup.c`, `highlight.c`).
- (`bitPlane4-7Base[Ptr]` were already correctly un-prefixed.)

## 9. Build & test

**SDL** (decode-correctness harness — shares the decode logic with Amiga):
```
make X86_64=1
cd bin && SDL_VIDEODRIVER=dummy SWOS_SCREENSHOT_INTERVAL=45 ./SWOS.x86_64   # → swos_shotNNN.png
```
Verified rendering: Sensible loader logo (8bpp), credits charset (4bpp), 256-colour
palette all correct. Both AGA and OCS builds exit-0.

**Amiga** (FS-UAE + gdb, Kickstart 3.1 A1200 = AGA):
```
scripts/amiga/amiga_gdb.sh up          # launch FS-UAE + attach gdb (from uae/dh0/data)
python3 scripts/amiga/gdb_proxy.py cmd "info registers"
python3 scripts/swos_harness.py where  # drive menus (works on SDL and Amiga)
```
See [HARNESS.md](HARNESS.md) for the FS-UAE write-support details.

## 10. Sync

Shared C is authored in `SWOS_C_SDL` and synced to the Amiga tree
(`C:\Projects\AI\SWOS\SWOS_C`, `sync_to_amiga_port.sh`). The P1/P2 changes are **not
yet synced**. The `resload.c` redirect and `platform_sdl.c` extraction are SDL-only
by nature (that backend isn't shared); the palettes, `DetectBankPlanes`, decode
changes, and dormant-path fixes all sync.

## 11. FMODE — 32-bit bitplane fetch for the 8-plane lores pitch (P4)

Status (2026-07-12): **root cause of the pitch "blue-line" corruption found and the
fix (FMODE) confirmed live on FS-UAE; the scroll-alignment part is still to
implement.** This also addresses the pitch's performance cost.

### The problem: 8 bitplanes in LORES cannot be fed at FMODE=0

The pitch is **8 bitplanes, lores** (`BPLCON0=0x0210`, HIRES=0). The copper never
writes `FMODE` (`$1FC`) — the template slot at `display.c:181` is a real NO-OP
(`0x01FE,0x0000`), so **FMODE=0 → 16-bit (2-byte) bitplane fetch**.

Per the AGA docs (`AGA Registers.pdf` FMODE table; `A1200FuncSpec.txt`), the AGA
8-bitplane capability *comes from* the 32-bit-wide fetch doubling the chip-RAM
bandwidth. At FMODE=0 the 16-bit fetch **cannot keep 8 bitplanes fed across a lores
line** — some high-plane fetches intermittently drop.

Symptom (diagnosed 2026-07-12, see `project_aga_pitch_render_fixes` memory): the
white pitch lines (palette **index 59** = planes 0,1,3,4,5) intermittently lost
planes **4/5**, becoming index **11**/**27** = `_gamePalette` `0x000f` = pure blue.
Proven to be a *display/scan-out* bug (stored bitplanes + copper palette were clean
on a confirmed blue frame; it survived every sprite-exclusion test → it's the pitch,
not the sprites).

### FMODE register (`$1FC`) — bitplane fetch modes

| BPAGEM (b1) | BPL32 (b0) | fetch increment | bus width |
|---|---|---|---|
| 0 | 0 | 2 bytes | **16-bit** (current / FMODE=0) |
| 0 | 1 | 4 bytes | **32-bit** (FMODE=1, BPL32) — doubles bandwidth |
| 1 | 1 | 8 bytes | 64-bit (double-CAS, needs page-mode RAM) |

Bits 2/3 (`SPR32`/`SPAGEM`) do the same for **sprites** independently.

### The fix: FMODE=1 (BPL32) for the pitch

Set `FMODE = 0x0001` and re-tune the display-data-fetch window/modulo for the wider
fetch. Values confirmed on FS-UAE (blue → white lines):

- **FMODE**: `display.c:181` template `0x01FE,0x0000` → `0x01FC,0x0001` (real write
  instead of the NO-OP).
- **DDFSTRT = 0x30**, **DDFSTOP = 0xD0** — at 32-bit fetch this gives 11 fetches × 32px
  = 352px (vs FMODE=0's 21 × 16px). `DDFSTRT` must be 16-clock aligned for 32-bit
  fetch (`0x30` ✓; the old sub-word-scroll `0x28` is **invalid** at FMODE=1).
- **Bitplane modulo = 308** (`= 352 − 44`; the fetch grabs 44 B/plane = 11×4). Set in
  `UpdatePlanePtrsAndBlitParams` (both scroll branches). NOTE: the modulo is
  razor-sharp — ±4 off = full garble.

### ⚠️ Remaining work: scroll alignment for 32-bit fetch

With the above, the lines go white but the pitch shows vertical stripes and the blue
returns at *some* scroll positions. Cause: **32-bit fetch needs the bitplane pointer
4-byte aligned**, but `UpdatePlanePtrsAndBlitParams` aligns the cameraX byte offset
to only 2 bytes (`(cameraXMod320>>3) & 0xFFFE`). At odd 2-byte scroll positions the
32-bit fetch is misaligned → stripes + the line-blue returns intermittently.

Fix = align the coarse cameraX offset to **4 bytes** (`& 0xFFFC`) and move the
remaining 0–31px fine scroll into `BPLCON1`, using AGA's **extra high-order playfield
scroll bits** (the A1200 spec's "up to 64-bit-wide scroll" — 5+ bits, vs the current
low 4). BPLCON1 lores fine-scroll is 4 bits (0–15px); a 32px fetch quantum needs the
extra bits to cover 0–31.

### When we use 32-bit vs 16-bit fetch

| Display | Planes / res | FMODE |
|---|---|---|
| In-match **pitch** (AGA) | 8bpl lores | **1 (32-bit)** — required for the fetch budget |
| Menus / intro / loaders (AGA) | 8bpp full-screen | evaluate — if 8bpl lores, same FMODE=1 need; else 0 |
| **OCS** build | 4bpl lores | **0 (16-bit)** — 4 planes fit at 16-bit, no FMODE |
| Hardware sprites (band players) | ≤16px, 4bpl attached | 16-bit (`SPR32=0`) unless sprite bandwidth needs it |

FMODE is a **global** chip register, so if the menu display is also 8bpl lores it must
be set/cleared in lock-step with the pitch (or left at FMODE=1 throughout the AGA run
if all AGA displays are 8bpl lores).

### Bonus: this fixes the pitch performance too

32-bit fetch **halves the bitplane DMA cycles** the pitch consumes, freeing the chip
bus for the blitter — so the 8-plane pitch scroll (currently the main AGA slowdown)
gets faster. One change fixes both the corruption and the speed.

### OCS

FMODE is AGA-only. The OCS build (4 bitplanes) leaves FMODE=0 (16-bit) — 4 planes fit
the lores fetch budget with headroom. Gate the FMODE write behind `#ifdef AGA`.
