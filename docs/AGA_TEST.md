# AGA Scroll + Sprite Test Bed — single entry point

**Purpose.** One file that points at every doc, tool, and piece of state needed to work the
AGA pitch-scroll / hardware-sprite rendering on the Amiga build. Start here; follow the
pointers. Written 2026-07-18 after a long session where time was lost re-deriving things the
docs already answered. **If you are picking this up cold: read §5 (open issues) and §6
(gotchas) first — they encode what has already been ruled out so you don't re-try it.**

This is the Amiga (m68k, FS-UAE) build. SDL renders chunky and does NOT exercise any of the
copper/blitter scroll path — all of this is `#if !defined(SWOS_SDL_BUILD)` / `#ifdef AGA`.

---

## 1. Document map — what each file answers

| Doc | Read it for |
|---|---|
| **`docs/AGA.md` §11** | THE FMODE=1 pitch-scroll write-up. The 6 layered fixes (§11 "SOLVED"), the DMA-contention analysis (§11a), and the "debugging lessons paid in blood" (static-vs-live). **§11 Layer 3 is the coarse-boundary jump; Layer 4 is the leftward write.** Read before touching scroll_blit.c / the display fine-scroll. |
| **`docs/DEBUG_TOOLKIT.md`** | The headless tools + **the two FS-UAE binaries** (§ "READ THIS FIRST" — this is the trap that cost days: stock re-composites, only the modified binary retains live frames). |
| **`docs/GDB.md`** | Low-level harness: boot FS-UAE, attach, `gdb_proxy.py` command interface, read/write memory, breakpoints, watchpoints, the one-shot-port discipline. |
| **`docs/AMIGA_SPRITES.md`** | Hardware-sprite vs bob design (P3): band routing, attached pairs (16-colour), `RenderSpriteBandsToCopperLists`, the plane-count storage. Read before the full-load sprite test (§5b). |
| **AGA hardware docs** `/mnt/c/Projects/Amiga/doc/aga/` | Authoritative chipset behaviour — **read these instead of guessing.** `RandyAGA.txt` ("Smooth Hardware Scrolling", "LONG burst mode" — the fine-scroll model), `AGA Registers.pdf`, `AGA.guide`, `A1200FuncSpec.txt`. |
| **Memory** (`~/.claude/.../memory/`) | `project_aga_fmode_impl`, `project_aga_fmode_leftscroll_jitter`, `project_aga_mixed_depth_rendering`, `project_aga_sprite_ddfstrt_dma_steal`, `feedback_dont_blame_wsl`. Point-in-time findings; verify against code. |

Source of record for engine behaviour: the Amiga port `/mnt/c/Projects/AI/SWOS/SWOS_C` (data
`Data.c`, `SWOS2.s`) and SwosGfx `AmigaTools.cs` for asset formats.

---

## 2. Quick start — boot the test bed and drive it

The test bed is `src/pitch_scroll_test.c` (`PitchScrollTest`), entered instead of the menu
when built `-DPITCH_SCROLL_TEST`. It shows ONLY a scrolling pitch (no players yet — see §5b).

**Build (Amiga scroll-test binary):**
```sh
cd /mnt/c/Projects/AI/SWOS/SWOS_C
unset OS                              # else the Makefile picks the Windows 'where' branch
BART=/mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-1.8.2/bin/linux
export PATH="$BART/opt/bin:$BART:$PATH"   # opt/bin = gcc/as; $BART = elf2hunk (BOTH needed)
rm -f obj/*.d                          # stale .d carry Windows C:/ paths -> break make
rm -f obj/<changed>.o obj/<changed>.s  # force-rebuild the files you edited (LTO)
make EXTRA_CFLAGS=-DPITCH_SCROLL_TEST   # -> uae/dh0/data/SWOS2 (+ .elf for gdb symbols)
```
The scroll-test build also **compile-time-skips the loader intro** (`#ifndef PITCH_SCROLL_TEST`
around `ShowLoaderIntroSequence` in SWOS.c) so it boots straight to the pitch.

**Boot + drive (see GDB.md for detail):**
```sh
scripts/amiga/amiga_gdb.sh up            # or: down / status / restart
P="python3 scripts/amiga/amigascope/gdb_proxy.py cmd"
$P "break PitchScrollTest"; timeout 30 python3 scripts/amiga/amigascope/gdb_proxy.py cmd "continue"
$P "set var g_pstAutoScrollX = 2"        # continuous scroll, bounces at edges
$P "set var g_pstFullRedraw = 0"         # 0 = REAL incremental ScrollX (what the game uses)
$P "delete"; timeout 6 python3 scripts/amiga/amigascope/gdb_proxy.py cmd "continue"   # free-run
```
Leave it free-running for the display to stay live. A read-probe returning `[BUSY: target
running]` = free-running = good. To interrupt: `pkill -INT -f '[m]68k-amiga-elf-gdb'` then
`gdb_proxy.py waitstop`.

### `g_pst*` controls (gdb-pokeable, `pitch_scroll_test.c`)
| global | meaning |
|---|---|
| `g_pstAutoScrollX` | px/frame auto-scroll, 0=off, bounces at edges |
| `g_pstCamX / g_pstCamY` | camera target (incremental path lerps; doesn't snap to a frac) |
| `g_pstFullRedraw` | **1** = full re-fill via ScrollY (jitter-free but slow, NOT the game path); **0** = incremental ScrollX (the real path, the one that jitters) |
| `g_pstShowOverlay` | 1 = overlay of camX / cameraXMod320 / coarse / frac + live BPLCON1/DDFSTRT/DDFSTOP/modulo |
| `g_pstVBlankBlit` | 1 = do the scroll blit during vblank |
| `g_pstB1Force / g_pstDsForce / g_pstDeForce / g_pstMoForce` | force BPLCON1 / DDFSTRT / DDFSTOP / BPLxMOD; −1 = camera-computed. For sweeping the fine-scroll. |
| `g_pstPtrByteOff` | coarse plane-pointer byte nudge; 0 = camera-computed |

---

## 3. Tools (all in `scripts/amiga/`, driven through the gdb proxy)

| Tool | Use |
|---|---|
| `amiga_gdb.sh up\|down\|status\|restart` | boot/teardown the FS-UAE + gdb + daemon harness |
| `gdb_proxy.py cmd "…"` / `waitstop` / `health` | send gdb commands; read/write memory & regs |
| `dma_profile.py N` | cycle-exact DMA recorder for N frames → per-frame contention + per-blit raster spans. **Objective scroll success test: no blitter cell at vpos ≥ 36** (i.e. blitter stays in vblank, off the display). Needs the modified binary. |
| `shot.sh CAMX CAMY OUT.png` | SETTLED composited screenshot. ⚠️ **freezes + settles the camera → ERASES live transients.** Good for geometry/static; USELESS for the scroll jitter. |
| `fb_capture.py OUT.png [other]` | decode the frontbuffer (displayed, or `other`=draw buffer) to PNG. Shows what's IN the buffer, not the composited beam output. |
| `copper_disasm.py` | disassemble the live copper list (BPL ptrs, DDF/DIW windows, fine scroll, palette) |

### ⚠️ The two FS-UAE binaries (the single biggest time-sink — see DEBUG_TOOLKIT.md)
- **Stock** (bartman vscode ext, the harness default `FSUAE_DIR`): `monitor screenshot`
  **re-composites a clean settled frame** — real-time artifacts vanish.
- **Modified** `/mnt/c/Projects/Linux/fs-uae/fs-uae`: `monitor screenshot` **retains the LIVE
  frame** (`fsemu_screenshot_capture_now`), and has the `monitor profile` DMA recorder. Boot it:
  ```sh
  FSUAE_BIN=/mnt/c/Projects/Linux/fs-uae/fs-uae scripts/amiga/amiga_gdb.sh up --fresh
  ```
- **To capture a LIVE transient (tearing, the scroll jitter): use the modified binary AND
  capture WHILE the scroll is actively moving — do NOT freeze/settle the camera** (that is
  what shot.sh does wrong for this purpose). Free-run with `g_pstAutoScrollX` on, then
  `monitor screenshot`, then read `~/FS-UAE/Screenshots/*.png`.

---

## 4. WSLg display (so you can watch it live)

FS-UAE renders through WSLg. If the window goes blank / shows `[WARN:COPY MODE]` in the title
(after repeated relaunch churn wedges the weston compositor), the fix is **from a Windows
PowerShell** (weston lives in the WSLg system distro; killing it respawns it):
```
wsl.exe --system sh -c "pkill -9 weston"
```
Then re-launch. **Prevent it: boot FS-UAE ONCE and leave it up; don't boot→down→boot repeatedly.**
This is a relaunch-churn wedge, NOT a WSL bug — see `feedback_dont_blame_wsl`.

---

## 5. Open issues

### 5a. Leftward scroll JITTER (open, cause identified)
**Symptom:** scrolling LEFT, the pitch snaps a whole tile to the RIGHT every 32px. Rightward is
smooth.

**Ruled out (do not re-investigate):**
- The fine-scroll display geometry (BPLCON1 delay / DDFSTRT / the `-4` pull-back). *Static*
  shot.sh captures at frac 15/16/17/31/0 move the touchline proportionally — smooth, no jump.
  (My earlier "confirmed 41aace1 geometry" arithmetic was WRONG.)
- The deferred mirror / block-B elimination. Forcing the mirror immediate changed NOTHING and
  produced no strip — mirror timing is irrelevant to this bug.

**Cause (from AGA.md §11 Layers 2–4):** it's the **incremental `ScrollX` leftward column
write** (`ScrollXUpdateColumnsAndBlit`, scroll_blit.c). Full-redraw (`g_pstFullRedraw=1`,
disables ScrollX) is jitter-free — proof it's ScrollX. Layer 3's 32px-block write "made
*rightward* perfect"; leftward's jump was never claimed fixed. The asymmetry: ScrollX writes
the leftward column at `coarse−4` (`d5=−32`) for **all** fracs, but the display only *reads*
`coarse−4` when the pull-back is active (frac 0..16, `delay≥16`); for frac 17..31 it reads
`coarse`. Write and read diverge for half the block → the tile snap.

**Failed fix attempts (DO NOT REPEAT):** (1) slicing the deferred flush — wrong subsystem, made
it worse; (2) unconditional pull-back — turned it bidirectional; (3) immediate mirror — no
effect. All were guesses before reading the docs.

**Right instrument:** modified fs-uae + `monitor screenshot` **during active auto-scroll** (§3).
`shot.sh`'s freeze+settle re-composites the transient away — that's why it looked clean.
Confirm the write-vs-read divergence, then make the leftward write track the frac-dependent
pull-back (or otherwise reconcile the leftward `d5` lead with the read position).

### 5b. Full-load test — add hardware sprites + bobs to the rig (next task)
The rig scrolls an empty pitch; it does not prove the game's real frame budget. Add
`DrawSprites` (hardware-sprite bands via `RenderSpriteBandsToCopperLists` + bobs via
`DrawQueuedSprites`) to `PitchScrollTest`, feed it a full complement (≈22 players + ball +
keepers) to hit the game's limits, and profile with `dma_profile.py`. Goals: (1) prove 8-plane
scroll + attached sprites + bobs all fit one frame (perf), (2) the in-match validation of the
P3 sprite work (`AMIGA_SPRITES.md`) that never ran live, (3) exercise the scroll fix under real
load. Success = `dma_profile` shows no blitter cell at vpos ≥ 36 and frames hold rate.

---

## 6. Gotchas / lessons (paid for already)

- **Static ≠ live.** shot.sh / frozen captures test display+geometry and re-composite clean.
  Only LIVE incremental scroll on the modified binary shows scroll transients. "Works static,
  breaks live" → suspect the fill (`ScrollX`/`ScrollY`), not the display. (AGA.md §11 lesson c.)
- **Read the AGA hardware docs before deriving chipset behaviour.** Four fix attempts failed
  from guessing the delay/DDFSTRT/pull-back coupling that `RandyAGA.txt` + AGA.md document.
- **LTO folds locals.** `d1`, `baseOffset`, etc. read as empty/"optimized out" in gdb. Add a
  `volatile`/`used` debug global, or read the copper/buffer bytes directly.
- **Stale `obj/*.d`** carry Windows `C:/` paths from vscode builds → `make` dies with "target
  pattern contains no '%'". `rm obj/*.d` before building.
- **`elf2hunk` lives in `$BART` (bin/linux), gcc in `$BART/opt/bin`** — put BOTH on PATH or the
  make succeeds to `.elf` then fails the hunk step.
- **One-shot debugger port** (GDB.md): each gdb connect/disconnect burns FS-UAE's port. Boot
  once; don't throwaway-connect.
- **Don't blame WSL** (`feedback_dont_blame_wsl`): blank window / slowness is relanch churn /
  two clients / wrong state — diagnose, don't reach for `wsl --shutdown`.

---

## 7. Sync

Scroll/display/sprite `.c` are shared between the SDL tree
(`/mnt/c/cygwin64/home/headk/SWOS_C_SDL`, canonical edit site) and the Amiga tree
(`/mnt/c/Projects/AI/SWOS/SWOS_C`, what FS-UAE builds). `display.h` genuinely diverges — hand-port
its 3 AGA changes. Copy this file (`docs/AGA_TEST.md`) to both trees. **Never git-commit the
Amiga tree** (owner commits it); SDL tree commits straight to main, source only.
