# Amiga Headless Debugging Toolkit

A set of **terminal-only** tools for inspecting a *running* Amiga game under FS-UAE —
no IDE, no GUI. They read live emulator state (memory, copper, DMA, the composited
frame) through the gdb harness, so an agent (Claude Code) or a human at a shell can see
exactly what the hardware is doing on any given frame.

This document covers the **inspection** layer. Two sibling docs cover the rest:

- **`GDB.md`** — the low-level gdb workflow (boot FS-UAE, attach, `gdb_proxy.py`, read
  memory, breakpoints, watchpoints).
- **`HARNESS.md`** — the higher-level *control* layer (drive the game: load careers,
  open menus, step seasons).

All tools live in **`scripts/amiga/`** and talk to the target via `gdb_proxy.py`, so a
healthy attached session (`amiga_gdb.sh up --fresh`) is the only prerequisite.

> These tools are game-agnostic. Nothing here is SWOS-specific except the default
> symbol names (`_copperListBuffer`, the pitch display window) — pass explicit
> addresses to point them at any Amiga program.

---

## The two FS-UAE binaries (READ THIS FIRST)

There are two FS-UAE builds, both derived from bartman's `amiga-debug` fork. **Which one
you boot changes what you can capture.** Point the harness at one with `FSUAE_BIN`:

| capability | stock (vscode ext) | modified (`/mnt/c/Projects/Linux/fs-uae`) |
|---|---|---|
| accurate AGA display | ✅ | ✅ |
| `monitor screenshot` | **re-composites** a clean, settled frame | **retains the LIVE frame** (`fsemu_screenshot_capture_now`) |
| catches real-time artifacts (tearing, DMA glitches, mid-blit) | ❌ (they vanish in the re-composite) | ✅ |
| cycle-exact DMA recorder (`monitor profile`) | ❌ | ✅ |

Rule of thumb:

- **Static / settled** state → either binary; the stock one is the default.
- **Anything that only exists while the machine runs** (scroll tearing, blitter-vs-beam
  contention, a flickering column) → you **must** use the modified binary, because a halt
  or a re-composite erases the artifact. Boot it with:

```sh
FSUAE_BIN=/mnt/c/Projects/Linux/fs-uae/fs-uae ./amiga_gdb.sh up --fresh
```

> Lesson paid for in days: an earlier note claimed the modified binary "renders AGA as
> rainbow garbage, unusable." That was wrong/outdated. It renders fine, and its live
> capture is the *only* way to see real-time display artifacts.

---

## Tools

### `dma_profile.py` — cycle-exact DMA contention & blit analysis

Drives the modified binary's `monitor profile`, which records **which DMA channel owned
every colour-clock of every scanline** for N frames (a `dma_rec[227×313]` grid), then
parses it. Answers the questions screenshots can't: *is the blitter stealing the beam's
fetch cycles? where, exactly? which blit?*

```sh
# capture N frames, print per-frame contention summary + per-blit raster spans
dma_profile.py 10

# parse an existing capture
dma_profile.py parse  out.prof     # contention summary
dma_profile.py blits  out.prof     # one line per blit: raster span, size, dest, overrun
```

Contention summary flags **blitter DMA landing on display lines** (a blit running while
the bitplanes are being fetched — the thing that corrupts the picture):

```
  frame 3: bitplane_cells=23936 blitter_cells=11008 conflict_cells=0 steal_evt=2652
    display lines (bitplane DMA): vpos 36..307 (272 lines)
    blitter active:            vpos 4..94 (91 lines)
    >>> CONTENTION: blitter runs on 59 DISPLAY lines (blitter + bitplane same scanline)
```

`blits` reconstructs each blit from the DMA stream and flags overruns — this is what
localises the offending blit:

```
  frame 3: 23 blit(s)
    L  4->L  5   16x64  px  chan=AD   D=$181564
    ...
    L 33->L 52   16x1024px  chan=AD   D=$188164  <<< OVERRUNS DISPLAY (blitter on display lines 36..52)
    L 77->L 94   16x1024px  chan=AD   D=$121e64  <<< OVERRUNS DISPLAY (blitter on display lines 77..94)
```

Record format (`sizeofDmaRec=58`, `DMARECORD_*` channels, `DmaEvents` bits) and the
`GetBlits` reconstruction are ported from the `vscode-amiga-debug` plugin
(`src/backend/profile.ts`, `src/client/dma.ts`).

### `copper_disasm.py` — copper-list disassembler

Reads a copper list straight out of chip RAM and disassembles it (`MOVE` / `WAIT` /
`SKIP`) with custom-register labels and **decoded values** — the copper equivalent of
`disassemble`. The fastest way to confirm the display setup (plane count, FMODE,
fetch/display windows, fine scroll, palette).

```sh
copper_disasm.py                    # &_copperListBuffer (default)
copper_disasm.py 0x1218f0 1316      # explicit address + length
copper_disasm.py --sym _copperListBuffer
```

```
00121934: 0100 0210   MOVE  BPLCON0   = $0210  ; BPU=8 lores
00121938: 0102 8822   MOVE  BPLCON1   = $8822  ; PF1H=34 PF2H=2 (fine scroll)
00121940: 01FC 0001   MOVE  FMODE     = $0001  ; BPL32=1 SPR=0 (2x/4x fetch)
00121950: 0092 0030   MOVE  DDFSTRT   = $0030  ; hpos=96
00121958: 008E 2471   MOVE  DIWSTRT   = $2471  ; V=36 H=113
```

Ported from `vscode-amiga-debug/src/client/copperDisassembler.ts`.

### `fb_capture.py` — frontbuffer decoder

Decodes the **displayed** frontbuffer straight from chip RAM (via the copper block-A
plane-0 pointer) into a PNG, applying the game palette. Fast and halt-only — it shows the
*buffer contents*, i.e. what the game drew, independent of the live beam. Use it to prove
whether an artifact is *in the buffer* (a draw bug) or *only in the live fetch* (a
real-time/DMA bug — those decode clean here).

```sh
fb_capture.py out.png            # displayed buffer
fb_capture.py out.png other      # the OTHER (draw) double-buffer, to spot a swap mismatch
```

### `shot.sh` — settled composited screenshot

Sets a camera position, lets the frame settle, and grabs the real composited FS-UAE frame
(includes hardware sprites, which a bitplane decode misses). Ground truth for *static*
appearance.

```sh
shot.sh CAMX CAMY out.png
```

---

## Case study: how these tools root-caused the AGA pitch-scroll column

A moving garbage **column** appeared during horizontal pitch scroll (AGA 8-plane lores +
FMODE=1). Two days of screenshot-guessing got nowhere; the tools cracked it in one
session. The chain of evidence — a template for any "it only happens while it runs" bug:

1. **`fb_capture.py` (buffer decode) → clean.** Every halt showed the displayed *and* the
   draw buffer byte-clean at the artifact location. → the column is **not in the buffer**.
2. **Stock `monitor screenshot` (frozen/re-composite) → clean.** A frozen frame never
   showed it. → it is **real-time**: it exists only while the machine runs, and any halt
   or re-composite erases it. (This is *why* it was invisible for two days.)
3. **Modified binary `capture_now` (live frame) → caught it.** Booting the modified
   FS-UAE and grabbing the retained live frame during active scroll finally showed the
   column — a partially-blitted region the beam displayed mid-write.
4. **`dma_profile.py` (cycle-exact DMA) → proved the mechanism.** 7 of 10 frames had zero
   blitter DMA (clean hardware scroll). The tile-cross frames showed the blitter active
   `vpos 4..94`, running on **59 display lines** with 2652 CPU/blitter-steal events. →
   the scroll blit **overruns vertical-blank into the visible display** and starves the
   8-plane fetch (which needs ~every DMA slot).
5. **`dma_profile.py blits` (per-blit spans) → localised it.** The small `16×128`
   column-tile blits fit in blanking; the **large `16×1024` mirror-copy blits** (the
   2x-tall block-B-elimination maintenance) are the ones that overrun. → the fix target is
   the mirror maintenance, not the column fill.
6. **`copper_disasm.py` → confirmed the display config** (BPU=8 lores, FMODE=1, DDF/DIW)
   was correct throughout, ruling out a display-setup cause.

Result: root cause proven **cycle-exact** (not guessed), with the exact offending blits
identified — and an *objective* success test for the eventual fix: `dma_profile.py` must
show **no blitter cell at vpos ≥ 36**. Full write-up in `AGA.md` §11a.

Lesson: when an artifact vanishes on halt, stop screenshotting the buffer and reach for
the **live capture + DMA profiler**. `fb_capture`/frozen-shot *clean* + `capture_now`
*dirty* is the signature of a real-time DMA/timing bug.

---

## How they work / extending the toolkit

- Every tool shells out to `gdb_proxy.py cmd "..."` and parses the reply — the same
  channel `GDB.md` documents. Memory is read with `dump binary memory`; register/struct
  values with `printf`.
- `dma_profile.py` uses the modified binary's `monitor profile <N> "" <out>` command,
  which enables the WinUAE cycle-exact DMA recorder for N frames and writes a binary
  dump. The DMA section is located by its `(sizeofDmaRec=58, count=227×313)` marker and
  decoded per the plugin's `profile.ts` layout; channels/events per `dma.ts`.
- To add a tool, follow the pattern: read the state you need via the proxy, decode it,
  print something a terminal (and an agent) can reason about. Keep raster/coordinate
  conventions consistent (`vpos 0..312`, `hpos 0..226`; the SWOS pitch display window is
  `vpos 36..307`).

See also: `GDB.md` (gdb harness), `HARNESS.md` (driving the game), `AGA.md` (the AGA
rendering pipeline these tools were built to debug).
