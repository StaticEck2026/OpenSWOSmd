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

Status (**2026-07-14 — SOLVED**): **The full pitch scrolls clean in BOTH axes, both
directions, corner to corner, on FS-UAE** — as long as the camera stays at **camX ≥ 17**
(the extreme-left 16px is an unavoidable FMODE edge, see "The camX ≥ 17 clamp" below).
The complete, working solution is documented in **§11·SOLVED** immediately below; the
older subsections (§11a "DMA contention TODO", the SNAP fine-scroll table, "no buffer
widen needed", "empty column is off-screen headroom") describe **earlier, since-superseded
theories** and are kept only as a debugging cautionary trail — read §11·SOLVED for the
truth. Not committed at time of writing (owner commits).

### 11·SOLVED — the complete FMODE=1 pitch scroll (2026-07-14)

The end state after a ~2-day debugging saga. Six distinct problems, fixed in layers.
Files: `include/display.h`, `src/display.c` (`UpdatePlanePtrsAndBlitParams`),
`src/scroll_blit.c` (`ScrollXUpdateColumnsAndBlit`, `ScrollYUpdateRowsAndBlit`). All
`#if defined(AGA) && !defined(SWOS_SDL_BUILD)`-gated; the SDL path early-returns and the
OCS path is the faithful 16px original, both untouched.

**The final buffer geometry — a per-plane VERGE MARGIN.**
`PLANE_STRIDE = 52` on AGA (was 44): each plane row is `[4-byte verge margin][48-byte
DATA]`. `SCROLL_LEFT_MARGIN_BYTES = 4`. `ROW_STRIDE = 8×52 = 416`; `SCROLL_X_WRAP =
PLANE_STRIDE×8 = 416`. The pitch DATA lives at byte `SCROLL_LEFT_MARGIN_BYTES..` — the
display coarse (`display.c`), and both fills (`ScrollX` dest, `ScrollY` `xByteOff`) all
add `+SCROLL_LEFT_MARGIN_BYTES`, so DATA is at byte 4 and the FMODE `-4` pull-back reads
the margin (byte 0) at the pitch's extreme-left edge. `d6`/`d7` in `ScrollY` track the
**48-byte DATA width** (`FMODE_FETCH_BYTES/2−1` words, modulo `ROW_STRIDE−FMODE_FETCH_BYTES`),
NOT the 52-byte row — the fill writes DATA and skips the margin.

**Layer 1 — the "empty far-left column" (the owner's 2-day complaint).** A flat
background-colour-0 column at the far left during scroll. Root cause: the display fetched
only **11 longs = 352px** while the ScrollY fill writes 384px+; the H7 fine-scroll delay
then showed pre-fetch pipeline pixels (colour 0). Fix: fetch **12 longs = 384px**
(`FMODE_FETCH_BYTES = 48`), `DDFSTRT 0x30→0x20`, and pull the plane pointer back one long
(`baseOffset -= 4`) so the extra LEFT long lands under the fine-scroll delay. (Cracked by
`RandyAGA.txt` "LONG burst mode": a 32-bit fetch reads the first long early and you must
fetch one EXTRA long to cover the scroll delay.)

**Layer 2 — coarse-boundary smoothness (per-pixel, no snap).** The static fine scroll is
`delay = 32 − (cameraXMod320 & 0x1F)` encoded into BPLCON1 H6/H7 (`lo|lo<<4|h6<<10|
h6<<14|h7<<11|h7<<15`), paired with the `-4` pointer pull-back **only when `delay ≥ 16`**
(frac 0..16). That `-4` is what cancels the coarse-pointer step at each 32px boundary,
making frac 31→0, 0→1 and 16→17 all per-pixel smooth (verified: touchline moves a uniform
−2 screen-px per camX-px straight through coarse boundaries). This REPLACES the old SNAP.

**Layer 3 — the coarse-boundary JUMP during *incremental* scroll.** Static full-redraws
were smooth but live incremental scroll snapped back a tile every 32px. Root cause: the
FMODE display fetches on a **32px (4-byte) grid**, but `ScrollX` wrote incoming columns on
a **16px (2-byte) grid** (`WORD_ALIGN_EVEN_MASK`), leaving the other half of each 32px
block stale — the display read the unwritten half. `ScrollY` already fixed this (its
`fmodeExtraTiles` snaps the fill to the 32px coarse and pulls in the left tile); `ScrollX`
did not. Fix: on each tile-cross `ScrollX` now writes the **full 32px block — BOTH 16px
tile columns** (the block's left tile + the leading one) at the 32px-aligned dest
(`scrollXBlitColumn` helper called twice; deferred 2-word mirror/mask). This was the big
one — it made rightward scroll perfect.

**Layer 4 — the leftward white strip.** Rightward writes its incoming column at the RIGHT
leading edge (`d5 = +SCROLL_RIGHT_EDGE_X = 335`); leftward wrote at the visible edge
(`d5 = 0`) while the display reads 32px further left (the `-4` margin) → that band was
never filled → a white strip during leftward scroll. Fix: leftward writes at the LEFT
leading edge, `d5 = −32` (`coarse−4`), edge-guarded (`_cameraX.s.i >= 32`) so it never
reads pitch-x < 0 — symmetric to the rightward +335 lead.

**Layer 5 — the extreme-left "jump back to the start" (~1 tile in).** The first coarse
block (camX 0..16) can't use the `-4` smoothing because `coarse−4` reads 32px LEFT of the
pitch's left touchline, where there is no pitch. Measured: a +62px backward lurch exactly
at frac 16→17. Fix: the **verge margin** (geometry above) fills those bytes with the
repeated leftmost pitch column (tile column 0), so `-4` has valid data everywhere and the
first block scrolls smoothly too. Filled per-row inside the `ScrollY` CPU copy loop
(cheap, incremental, no blitter) with a parallel `a1_verge` tile-0 pointer; mirrored into
the 2×-tall block-B half.

**Layer 6 — the mid-pitch dark column during Y scroll (the last bug).** Filling the
margin broke a DIFFERENT thing: the FMODE display's per-plane 48-byte fetch **OVER-READS
into the *next* plane's first 4 bytes** — that overlap IS the rolling-ring mechanism (see
"OCS rolling circular-buffer" below), and it must hold rolling over-write content, not
tile-0. Stamping the margin there at coarse > 0 corrupted the over-read → a dark vertical
column mid-pitch. Fix: **gate the margin write to `cameraXMod320 < 32` (coarse = 0)** —
the ONLY place the margin is ever read, and (crucially) the one place the over-read stops
exactly at the margin boundary without touching it. Everywhere else those bytes stay
rolling content, untouched. This was the subtle one: the "margin" and the "over-read
region" are the SAME physical bytes serving two roles, reconciled only by the coarse=0
gate.

**The camX ≥ 17 clamp.** The verge margin is a *repeated* leftmost column (grass), not
scrolling pitch — there is genuinely no content left of pitch-x 0. It is the visible
leftmost only when `coarse = 0` AND the fine delay is active, i.e. camX **0..16**. So
clamp the camera to **camX ≥ 17** and the entire visible window is fully-smooth scrolling
pitch; below that the far-left ~16px shows the static verge stand-in. In real play the
camera almost never pins to the touchline, so this is a non-issue.

**Debugging lessons paid for in blood.** (a) Static full-redraw (`shot.sh`) tests the
display+geometry; only LIVE incremental scroll exercises `ScrollX`, and only the owner's
real display (or the modified live-capture fs-uae) shows real-time transients — a frozen
capture re-composites clean. (b) A per-feature x-position sweep across a boundary
(`measure_touchline.py`, −2px/camX-px linear = smooth) beats eyeballing. (c) When a fix
"works static but breaks live", suspect the fill (`ScrollX`/`ScrollY`), not the display.
(d) The over-read overlap means any byte you add near a plane boundary is read twice, for
two purposes — check both.

### 11a. The scroll COLUMN: real-time blitter-vs-display DMA contention (2026-07-13)

The moving column seen during active horizontal scroll is **not** in the frontbuffer
and is **not** a display-config bug — the static fine-scroll path renders clean at every
fractional offset. It is a **real-time DMA-timing artifact**: it only exists while the
emulator runs and vanishes the instant the CPU halts (blitter stops), which is why every
frozen / re-composited screenshot looked clean and it took days to even capture it.

**How it was found** (full method + example output in `DEBUG_TOOLKIT.md` §"Case study"):
`fb_capture.py` decoded the buffer *clean* and a frozen screenshot was *clean*, but the
modified fs-uae binary's live `capture_now` caught the column → it's real-time, not buffer
content. `dma_profile.py` then proved the DMA contention and `dma_profile.py blits`
localised the exact overrunning blits. This is the signature to remember: **buffer/frozen
clean + live-capture dirty = a real-time DMA/timing bug** — reach for the DMA profiler,
not more screenshots.

**Proven cycle-exact** with the new `scripts/amiga/dma_profile.py` (drives the fs-uae
`monitor profile` DMA recorder and parses the `dma_rec[227×313]` per-cycle grid):

- 7 of every ~8 frames have **zero blitter DMA** → clean pure-hardware scroll (coarse
  pointer + BPLCON1 only).
- On a **tile-cross** frame the scroll blit fires. `dma_profile.py blits` lists each
  blit with its raster span; the small `16×128` column-tile blits fit in vertical blank
  (vpos 4..33), but the large **`16×1024` MIRROR-copy blits** (the 2x-tall block-B-
  elimination maintenance, §12) run **L33→L94 — i.e. on display lines 36..94**,
  concurrently with the 8-plane bitplane fetch. 2652 CPU/blitter-steal events that frame.
- 8-plane lores fetch consumes almost all chip-RAM DMA bandwidth during the visible
  display; only ~32–40 scanlines of blanking exist before the display starts, but the
  per-cross blit work needs ~90 scanlines. So it **overruns blanking into the top ~59
  display lines** → starves/steals the fetch there → the garbage column.

**Not BLITPRI** — setting/clearing `DMACON` BLITPRI (`DMACON_BLT_ON` in `waitBlit`) just
swaps failure modes (priority → starves the beam; no-priority → the slow blit isn't
finished when the buffer is swapped in). Left at the faithful `0x8400`.

**Fix direction (TODO):** the blitter must never run while the display fetches. Since
most frames are blit-idle, build the upcoming column — and its mirror — incrementally
across those idle frames' blanking periods (a few tiles/slices per frame), so the
tile-cross frame does no large blit. The 2x-tall mirror maintenance is the biggest
offender and is the first thing to make incremental (or reconsider block-B elimination
vs. scroll cost). Verify with `dma_profile.py`: success = **no blitter cell at vpos ≥ 36**.

### 11b. Headless AGA debugging toolkit (fs-uae gdb harness)

> **Full reference: `DEBUG_TOOLKIT.md`** (tool-by-tool usage, example output, the
> two-binaries table, and the scroll-column case study). Summary below.

Reusable terminal tools that read live emulator state through the gdb proxy — no
IDE, no GUI. (Screenshot/DMA tools need the **modified** fs-uae at
`/mnt/c/Projects/Linux/fs-uae/fs-uae`, which has a synchronous live-frame
`fsemu_screenshot_capture_now` and the cycle-exact DMA recorder; boot it with
`FSUAE_BIN=/mnt/c/Projects/Linux/fs-uae/fs-uae ./amiga_gdb.sh up --fresh`. The stock
vscode/bartman binary's `monitor screenshot` RE-COMPOSITES a clean settled frame and so
cannot capture real-time artifacts.)

- **`dma_profile.py [N]`** — capture N frames of cycle-exact DMA and report, per frame,
  blitter-vs-bitplane contention (blitter cells landing on display lines), DMA conflicts,
  and CPU/blitter-steal events. `dma_profile.py blits <file>` lists every blit with its
  raster start→end line, size, channels, and dest — flagging any that overrun the display
  window. Format + logic ported from `vscode-amiga-debug` (`profile.ts` `sizeofDmaRec=58`;
  `dma.ts` `GetBlits`).
- **`copper_disasm.py [addr [bytes]]`** — disassemble a copper list straight from memory
  (`MOVE`/`WAIT`/`SKIP`) with custom-register labels and decoded values (BPLCON0 plane
  count, FMODE, DDF/DIW positions, fine-scroll, colours). Ported from
  `vscode-amiga-debug/src/client/copperDisassembler.ts`. Defaults to `_copperListBuffer`.
- **`fb_capture.py [out.png]`** — decode the displayed frontbuffer straight from chip RAM
  via the copper block-A plane-0 pointer (fast, halt-only; shows buffer content, not the
  real-time beam).
- **`shot.sh CAMX CAMY OUT`** — settle + composited screenshot at a camera position.

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

### Scroll alignment for 32-bit fetch (implemented 2026-07-13)

**32-bit fetch forces a 32px-aligned bitplane pointer.** Verified live on FS-UAE:
nudging the pointer by a single 16px word (2 bytes) at FMODE=1 **scrambles the whole
display** (broken lines, fragmented circles) — the hardware ignores the low address
bit. So the coarse cameraX offset must be `& 0xFFFC` (4 bytes / 32px), and the
residual 0–31px fine scroll must be done entirely in `BPLCON1`.

**BPLCON1 fine scroll (AGA lores, per `RandyAGA.txt`).** The 8-bit scroll code
`PF1H0..H7` (35 ns / ¼-lores-px units) maps to *non-contiguous* BPLCON1 bits:

| scroll bit | lores value | PF1 bit | PF2 bit |
|---|---|---|---|
| H2 | 1 px  | 0  | 4  |
| H3 | 2 px  | 1  | 5  |
| H4 | 4 px  | 2  | 6  |
| H5 | 8 px  | 3  | 7  |
| **H6** | **16 px** | **10** | **14** |
| **H7** | **32 px** | **11** | **15** |

So a lores delay `D` (0–47) encodes as
`lo=D&0xF; h6=(D>>4)&1; h7=(D>>5)&1;`
`BPLCON1 = lo|(lo<<4)|(h6<<10)|(h6<<14)|(h7<<11)|(h7<<15)`. The doc explicitly
warns: **smooth scrolling with 2×/4× fetch mode requires these extended bits** —
you do NOT move `DDFSTRT` per fraction; the wide fetch already supplies a full 32px
block, so the fine scroll is BPLCON1-only with a *constant* `DDFSTRT/DDFSTOP`.

**The displayed pixel model** (calibrated on FS-UAE): with a 32px-aligned pointer at
`coarse` and delay `D`, screen x=0 shows `pointer + (32 − D)` for `D∈[16,31]` (a clean
**leftward** shift that reveals fetched right-edge pixels). We hold
`DDFSTRT=0x30 / DDFSTOP=0xD0 / modulo=308` constant and **SNAP** the fine fraction to
`min(frac,16)`, so only the clean H6 regime is ever used:

| `frac = cameraXMod320 & 0x1F` | pointer | delay `D` | notes |
|---|---|---|---|
| 0 | `coarse` | 0 | 32px-aligned |
| 1..16 | `coarse` | `32 − frac` (16..31) | H6 bit; smooth |
| 17..31 | `coarse` | 16 (**snapped** to frac=16) | steppy-but-clean; no H7, no pointer nudge |

`UpdatePlanePtrsAndBlitParams` computes `d1 = cameraXMod320 & 0x1F; if (d1>16) d1=16;`
then `delay = 32−d1` → `BPLCON1 = lo|(lo<<4)|(h6<<10)|(h6<<14)` with `lo=delay&0xF,
h6=(delay>>4)&1`. **This replaces the old "left strip" bug fix** (which used a
`DDFSTRT=0x20` early fetch + positive BPLCON1 delay whose left-revealed pixels came
from the FMODE pipeline-fill region → a full-height speckled strip). The snap trades
away sub-16px smoothness in the upper half of each 32px block for a guaranteed-clean
display; the "true smooth" alternative (H7 + a +32px pointer nudge) is documented above
but is currently OFF — see the rolling-buffer note below for why it fought the blit.

### The OCS rolling circular-buffer scroll model (the system AGA must copy)

This is the crux the user asked us to nail down. **The frontbuffer line is a rolling
circular buffer in X**, and every over-read/over-write "past the plane row" is
*intentional* — reads and writes roll together and stay paired. Getting this wrong
(clamping a dest into the 44-byte plane row) is what produced the "wrong column"
corruption; getting it right is a *faithful* translation of `SWOS2.s`, no clamps.

Geometry (`display.h`, OCS values in parens):
- One frontbuffer line = `ROW_STRIDE_BYTES = NUM_PLANES × PLANE_STRIDE` = **352 B**
  AGA (8×44) / **176 B** OCS (4×44). Interleaved: line `L`, plane `p`, byte `b` lives
  at `L·ROW_STRIDE + p·PLANE_STRIDE + b`.
- The X buffer is **circular at `SCROLL_X_WRAP = 352px = 44 B`** (one plane row wide).
  `cameraXMod320 ∈ [0,352)` (see `SetCameraCoordinates` / `MoveView`: `+1..wrap 352`).
- `coarse_byte = (cameraXMod320 >> 3) & align` (OCS `&0xFFFE`/2B; AGA FMODE `&0xFFFC`/4B).

How a scroll frame works (`SWOS2.s` `ScrollXUpdateColumnsAndBlit`, ported 1:1):
- On crossing a tile boundary, blit **one 16px tile column** of all `NUM_PLANES` planes.
- **Dest byte is the ROLLING leading edge, NOT clamped:** move-right uses
  `dest = (cameraXMod320 + SCROLL_RIGHT_EDGE_X(335)) >> 3 & align` → **byte 40..84**,
  deliberately *past* the 44-byte plane row. The blit steps dest by `PLANE_STRIDE`
  per word (`BLTDMOD = PLANE_STRIDE−2` → `+2+42 = +44`), writing plane `p` at
  `dest + p·PLANE_STRIDE`.
- The **display** (`UpdatePlanePtrsAndBlitParams`) reads plane `p` from
  `base + coarse_byte + p·PLANE_STRIDE`, fetching 42–44 B/plane, which **over-reads
  past the plane row** for any `coarse_byte > 2`.
- **Why it's coherent:** blit-write and display-read use the *same* `PLANE_STRIDE`
  stride, and `coarse_byte` (read) and the leading edge (write) advance in lock-step
  (`335px` apart in a 352px circular buffer). So each over-read lands exactly on the
  matching over-write. The physical "plane p+1 region" doesn't strictly hold plane
  p+1 — it holds whatever the roll placed there, and both sides agree. **The whole
  176/352-byte line is one rolling ring; "planes" are just `PLANE_STRIDE`-spaced taps
  into it.**

The AGA port replicates this **unchanged** (8 planes, `PLANE_STRIDE=44`,
`ROW_STRIDE=352`, `SCROLL_X_WRAP=352`). AGA touches ONLY the display registers — the
FMODE 4-byte coarse alignment + the extended BPLCON1 fine scroll (snap table above).
The blit (`ScrollX`/`ScrollY`) is byte-for-byte the OCS algorithm; **it must not be
"corrected" for the over-range — that range is the ring.**

### The bug this session (fixed 2026-07-13): a dest clamp broke the ring

Symptom: after a full redraw the pitch was clean, but any scroll position with
`coarse_byte > 2` showed a large field of green/white speckle over the right ~⅔ of the
screen. Root cause: `ScrollXUpdateColumnsAndBlit` had grown a
`destXByteOff %= PLANE_STRIDE` clamp (added while chasing an earlier artifact). That
forced the rolling leading edge back into `[0,44)`, so the blit wrote the incoming
column to the *wrong* physical byte while the display kept over-reading the ring →
plane-shifted garbage. **Fix: delete the clamp; restore the faithful
`dest = ((cameraXMod320 + d5) >> 3) & WORD_ALIGN_EVEN_MASK` (un-clamped).** `ScrollY`'s
row write was already un-clamped (faithful), so it was untouched. Verified on FS-UAE
via `scripts/amiga/fb_capture.py`: clean at coarse 24 (full redraw) and clean through
a full incremental scroll sweep, both directions, both double-buffers.

**The "empty/corrupt left column" is off-screen headroom, not a bug.** During
incremental scroll a ~1–15px speckle column appears at the extreme left of the *raw
352px fetch* — this is the ring **seam** (where the leading-edge write meets the
trailing edge). But the real display is only 336px: the DIW crops the leftmost
`snapped_frac` (≤16px) of the fetch as fine-scroll headroom, and the seam lives inside
that crop. `fb_capture.py` renders the DIW-cropped **visible** window by default
(`min(camXMod&0x1F,16)` px skipped) and it is clean; pass `full` to see the whole fetch
incl. the off-screen seam. **So NO buffer widen is needed** — the earlier "widen
352→384" plan (reverted) was treating the off-screen seam as if it were visible.

### Why OCS "just works" and AGA needs the FMODE display tweaks only

OCS runs **FMODE=0** (16-bit / 16px word fetch), which accepts **16px-aligned (2-byte)
pointers**: a 16px coarse quantum + a 0–15px BPLCON1 delay + a one-word-early
`DDFSTRT=0x28` prime. The *rolling ring above is identical* on OCS — nothing about the
buffer changes. AGA only needs FMODE=1 for the 336px 8-bitplane DMA budget, which forces
the 32px coarse quantum and the extended/snapped BPLCON1 fine scroll. **The scroll
buffer, the ScrollX/ScrollY blit, and the circular over-read are shared verbatim
between OCS and AGA; FMODE changes the display-register side only.**

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
