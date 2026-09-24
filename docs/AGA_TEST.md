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
| **`docs/GDB_DEBUGGING.md`** | Low-level harness: boot FS-UAE, attach, `gdb_proxy.py` command interface, read/write memory, breakpoints, watchpoints, the one-shot-port discipline. |
| **`docs/AMIGA_SPRITES.md`** | Hardware-sprite vs bob design (P3): band routing, attached pairs (16-colour), `RenderSpriteBandsToCopperLists`, the plane-count storage. Read before the full-load sprite test (§5b). |
| **AGA hardware docs** `/mnt/c/Projects/Amiga/doc/aga/` | Authoritative chipset behaviour — **read these instead of guessing.** `RandyAGA.txt` ("Smooth Hardware Scrolling", "LONG burst mode" — the fine-scroll model), `AGA Registers.pdf`, `AGA.guide`, `A1200FuncSpec.txt`. |
| **Memory** (`~/.claude/.../memory/`) | `project_aga_fmode_impl`, `project_aga_fmode_leftscroll_jitter`, `project_aga_mixed_depth_rendering`, `project_aga_sprite_ddfstrt_dma_steal`, `feedback_dont_blame_wsl`. Point-in-time findings; verify against code. |

Source of record for engine behaviour: the Amiga port `/mnt/c/Projects/AI/SWOS/OpenSWOS` (data
`Data.c`, `SWOS2.s`) and SwosGfx `AmigaTools.cs` for asset formats.

---

## 2. Quick start — boot the test bed and drive it

The test bed is `src/pitch_scroll_test.c` (`PitchScrollTest`), entered instead of the menu
when built `-DPITCH_SCROLL_TEST`. It shows ONLY a scrolling pitch (no players yet — see §5b).

**Build (Amiga scroll-test binary):**
```sh
cd /mnt/c/Projects/AI/SWOS/OpenSWOS
unset OS                              # else the Makefile picks the Windows 'where' branch
BART=/mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-1.8.2/bin/linux
export PATH="$BART/opt/bin:$BART:$PATH"   # opt/bin = gcc/as; $BART = elf2hunk (BOTH needed)
rm -f obj/*.d                          # stale .d carry Windows C:/ paths -> break make
rm -f obj/<changed>.o obj/<changed>.s  # force-rebuild the files you edited (LTO)
make -f Makefile.amiga EXTRA_CFLAGS=-DPITCH_SCROLL_TEST   # -> bin/SWOS2 (+ .elf for gdb symbols)
```
The scroll-test build also **compile-time-skips the loader intro** (`#ifndef PITCH_SCROLL_TEST`
around `ShowLoaderIntroSequence` in SWOS.c) so it boots straight to the pitch.

**Boot + drive (see GDB_DEBUGGING.md for detail):**
```sh
scripts/amiga/amiga_gdb.sh up            # or: down / status / restart
P="python3 scripts/amiga/gdb_proxy.py cmd"
$P "break PitchScrollTest"; timeout 30 python3 scripts/amiga/gdb_proxy.py cmd "continue"
$P "set var g_pstAutoScrollX = 2"        # continuous scroll, bounces at edges
$P "set var g_pstFullRedraw = 0"         # 0 = REAL incremental ScrollX (what the game uses)
$P "delete"; timeout 6 python3 scripts/amiga/gdb_proxy.py cmd "continue"   # free-run
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

### 5b. Full-load test — hardware sprites + bobs in the rig (DONE — rig has them)
The rig now renders ≈22 players + ball (`pitch_scroll_test.c` `pstSetupSprites`), both the
HW-sprite band path (`g_pstForceBobs=0`) and the bob path (`g_pstForceBobs=1`). Keyboard-driven
(§ rig controls). Perf profiling via `dma_profile.py` on the BOB path works; the BAND path
stalls the cycle-exact recorder (fs-uae-side, deferred — bob path serves all measurements).

### 5c. Sprite colour loss (last-column HW sprites white/black) — RESOLVED (committed 219d788)
The rightmost HW-sprite column rendered white/black because the **plane23 band uses spr3, whose
DMA slot (0x25/0x27) is stolen** by the FMODE=1 8-plane bitplane fetch at DDFSTRT=$20
(`fetch_start=DDFSTRT+4=0x24`, ≤ spr3's slot). Two prior fixes are also in: `41aff36` BPLCON4
ESPRM/OSPRM=0 (sprites index kit palette 0-15, not 16-31), `9eb2df2` no-wrap contiguous bob draw.

**The steal itself is fundamentally unfixable at DDFSTRT** (proven — do NOT re-try): three
constraints have empty intersection — smooth per-pixel scroll needs DDFSTRT on the 16-CCK grid
{$20,$30}; clearing spr3 needs DDFSTRT ≥ $24; not overrunning refresh/disk DMA needs DDFSTOP ≤ $D8
i.e. DDFSTRT ≤ $28. `[$24,$28] ∩ {$20,$30} = ∅`. Empirically $24/$28/$28+rephase all stutter
(off-grid), $20 is smooth but steals spr3, $28→DDFSTOP=$D8 guru-crashes. FMODE=3 was also refuted.
So instead of fixing the steal, the fix **routes players off the stolen band**: on AGA,
`BuildSpriteBandAndGeneralRenderStacks` (sprites.c ~1698) skips the plane23 (spr2+3) assignment
(`#if !defined(AGA)`); those sprites fall through to plane01 (spr0+1, safe) or the general/bob
stack. display.c stays at $20 → **smooth scroll preserved AND colours correct.**

### 5d. Vertical-scroll bob flicker — RESOLVED (2026-07-19), it was a RIG artifact
**NOT a DrawSpriteClipped mirror-geometry bug** (the 2026-07-18 handoff's hypothesis was wrong —
do not re-chase it). It was a display-timing **tear** caused by the test rig itself, and the fix is
one line moved in `pitch_scroll_test.c`.

**What it actually was.** A torn frame is a *progressive top-to-bottom cutoff* (top row complete,
each lower row losing more of its right side) = the beam displaying a buffer while `DrawSprites` is
still drawing bobs into it (bobs draw top-to-bottom in sort order; the beam outruns the blitter down
the screen). The rig called `UpdatePlanePtrsAndBlitParams()` *mid-render, before* `DrawSprites` —
which points the copper's block-A plane pointers at the buffer being drawn. When a VBLANK landed in
the window between that call and `DrawSprites` finishing (~a few % of frames = the intermittent
flicker), the copper switched the live display to the half-drawn buffer → the tear. The real
`game_loop.c` never calls `UpdatePlanePtrs` between scroll and draw (per frame the **VBlank ISR**
does it, at the swap — the correct time), so **the shipped game is not affected.**

**Fix (final).** REMOVE the rig's explicit `UpdatePlanePtrsAndBlitParams()` from the loop entirely
and rely on the **VBlank ISR** (interrupts.c) to call it at the buffer swap — exactly what the real
`game_loop.c` does (it never calls it between scroll and draw). The ISR points the copper at a
COMPLETE buffer, at the swap, always on time. (An intermediate fix, commit 9c681ac, instead *moved*
the call to after `DrawSprites` — that also killed the tear, but on heavy all-bob frames the render
can run past the VBLANK, landing the copper scroll-update a frame late = extra stutter on the
DMA-bound bob path; dropping the call avoids that. Superseded.) Confirmed: a debug counter of "copper
block-A == drawn-buffer at render-end" went 122/121 → **0/251**; the post-jump repro that reliably
tore went torn-at-c01/c02 → **0/14** on both the after-DrawSprites and the ISR-only builds; owner
validated live — full-pitch scroll, HW-sprites toggled on/off mid-scroll, no flicker either way,
all-bob and mixed both render perfectly.

**Separate, PRE-EXISTING issue confirmed alongside (NOT §5d, do not conflate):** horizontal scroll
on the all-bob path stutters (pitch + sprites) at the top of the pitch, and **HW sprites remove it**.
That is the documented blitter/DMA budget limit — on tile-cross frames the scroll column blit + 22
bob blits push the blitter onto 200-240 visible display lines, contending with the 8-plane fetch
(see [[project_aga_fullload_test]] perf finding). Most visible at the top (high-contrast stand/crowd/
ads; a 1px slip on plain grass is invisible). This is exactly why players go on HW sprites — the P3
architectural work, not a quick patch. The rig's 6-per-row grid is a pathological worst case.

**Method notes that cost time (heed them):**
- The tear is intermittent (~few %/frame). A single `monitor screenshot` usually catches a *clean*
  frame — it looks like flicker only when watched live/continuously. Measure with a **run of
  consecutive frames** + an objective detector, not one shot.
- **The per-frame breakpoint used to step frames PERTURBS the race and suppresses the tear** — so
  bp-stepped capture under-reports it. Use the counters (accumulate under continuous free-run) or
  continuous `monitor screenshot`.
- `fb_capture.py` had a stale `PLANE_STRIDE=48`; the AGA build is 52 (4-byte scroll margin). Fixed
  to derive the stride live from the copper (plane1−plane0), so it self-adapts to any build.
- The bob-body detector (`scratchpad/bodycount.py` during the session): a torn frame drops the
  lower-row blue-shirt / yellow-shorts pixel counts (mirror-region rows), which is the reliable
  objective signal.

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
- **One-shot debugger port** (GDB_DEBUGGING.md): each gdb connect/disconnect burns FS-UAE's port. Boot
  once; don't throwaway-connect.
- **Don't blame WSL** (`feedback_dont_blame_wsl`): blank window / slowness is relanch churn /
  two clients / wrong state — diagnose, don't reach for `wsl --shutdown`. Real root causes hit
  this session: `pkill` instead of `amiga_gdb.sh down` (leaves the stub un-handshaked); a
  lingering `ShowLoaderIntroSequence` breakpoint trapping boot; `pgrep -f 'opt/bin/…gdb'`
  false-positive matching your OWN shell command line (phantom "stale clients" — match
  `'m68k-amiga-elf-gdb -q'` or check /proc instead). weston genuinely down is the exception —
  owner restarts it from Windows: `wsl.exe --system sh -c "pkill -9 weston"`.
- **Boot is SLOW (~40s), not hung.** `ConvertSpriteColors` (from `pstSetupSprites`) converts
  many sprites and takes ~40s under the harness. Wait it out before declaring a hang — a
  premature 14s wait looked like a "BSS hang" last session and was a red herring. (Corollary:
  adding a fresh BSS global did NOT hang boot — data can move freely, good news for the untangle.)
- **Instrument the RIGHT draw function.** Players are WIDE → `DrawSpriteClipped`, not
  `drawMaskedPlayerSprite` (pixWidth ≤ 16 only). Logging the masked function for a player bug
  yields garbage/impossible values (stale seq, negative d2).
- **A "flicker/tear" may be a DISPLAY-TIMING bug, not a draw/geometry bug — and the rig can
  MANUFACTURE it.** §5d looked like a sprite-drawing bug for two sessions; it was the rig calling
  `UpdatePlanePtrsAndBlitParams` mid-render (before `DrawSprites`), switching the copper to the
  half-drawn buffer. Tells: buffers are always CLEAN when halted; static/pinned camera is clean;
  only the LIVE beam during motion tears; the artifact is a progressive top-to-bottom cutoff
  (beam-outruns-blitter), not a fixed missing region. When halting/pinning makes a "flicker"
  vanish, suspect the DISPLAY path (copper/beam/swap timing), not the pixels in the buffer.
- **Breakpoints perturb timing bugs.** A per-frame breakpoint changed the render↔beam phase enough
  to SUPPRESS the §5d tear (0 tears while bp-stepping, tears under free-run). For any race, measure
  with in-code counters accumulated under continuous free-run, or continuous `monitor screenshot` —
  not bp-stepped capture.

---

## 7. Sync (historical)

> **Superseded (2026-08-12).** The SDL and Amiga trees were merged into one
> repo; there is no sync step and no "canonical edit site" any more — edit
> the file once and both targets pick it up. The section below is kept for
> the historical record of what was/wasn't propagated at the time.

Scroll/display/sprite `.c` are shared between the SDL tree
(`/mnt/c/Projects/AI/SWOS/OpenSWOS`, canonical edit site) and the Amiga tree
(`/mnt/c/Projects/AI/SWOS/OpenSWOS`, what FS-UAE builds). `display.h` genuinely diverges — hand-port
its 3 AGA changes. Copy this file (`docs/AGA_TEST.md`) to both trees. **Never git-commit the
Amiga tree** (owner commits it); SDL tree commits straight to main, source only.
