# SWOS gdb Test Harness

A gdb-driven harness that lets the assistant **drive the game and read its state
itself** — navigate menus, press fire, load saves, capture the screen — on
**both** the SDL build and the Amiga build (under FS-UAE), with no OS-level input
or screenshot tools. Navigation feedback comes from reading the game's internal
menu state over gdb (fast + token-light); screenshots are an orientation aid
only.

This document is **separate from the generic gdb daemon doc**
(`SWOS_C/docs/GDB.md`). GDB.md documents the reusable transport; this documents
the SWOS automation built on top of it.

## Architecture (layered; the SWOS bits are a plugin)

```
gdb_proxy.py        generic transport — one persistent gdb session, cmd /
 (SWOS_C tree)       waitstop / health / watch; --mode amiga (FS-UAE remote)
                     or --mode sdl (native x86). Reusable, no game knowledge.
        |
harness_lib.py      generic harness primitives — read/write vars, breakpoints,
 (scripts/)          input injection (break+set+continue), framebuffer capture.
                     Reusable across projects; no game knowledge.
        |
swos_plugin.py      THE SWOS PLUGIN — Menu struct accessors, the injection point
 (scripts/)          + direction encoding, framebuffer geometry, save loading.
                     Swap THIS file to drive a different game.
        |
swos_harness.py     thin CLI (where / enum / nav / fire / combo / goto / select
 (scripts/)          / load / cap / check).
```

The seam is deliberate: `gdb_proxy.py` + `harness_lib.py` are game-agnostic and
can be lifted to another project; everything SWOS-specific is in
`swos_plugin.py`.

## Why one mechanism drives both targets

The menu system (`src/menu_system.c`) is shared C between the SDL and Amiga
builds. The harness injects input at a single line in `_MenuProc`
(`menu_system.c:1567`, right after `_MenuCheckControls` sets the input globals
and before the `d7`/onSelect dispatch and the nav read). Setting the input
globals there replicates real input for the whole frame — so the **same
injection works on both targets**, no per-target input code.

`_MenuCheckControls` re-derives the input globals from (idle) hardware every
frame, so an injected value is automatically "released" on the next frame — a
single set = a single key tap, no explicit release needed.

Direction encoding (`menuControlsDirection`): `up=0, right=2, down=4, left=6`
(verified against `nextEntryDirectionOffsets` + entry neighbor links). `fire=1`
presses the current entry. For value-cyclers (e.g. nationality → OTHER) use a
combo, e.g. `combo fire left`.

## Prerequisite: a write-capable gdb stub

Input injection, inferior calls, and filename staging all require gdb to **write**
target memory/registers. Native SDL gdb is fine. **The FS-UAE / bartman stub was
read-only** (it ACKs reads + breakpoints but silently drops `M`/`X`/`G`/`P`
writes — so `set var` never stuck and every injection no-op'd). It was patched to
add write support — see below. Run `swos_harness.py check` to confirm writes
reach the target (it fails loudly on a read-only stub).

### FS-UAE write support (the patch)

Source: `C:\Projects\Linux\fs-uae` (the bartman fork; has
`src/barto_gdbserver.cpp`). Added to `barto_gdbserver.cpp`:

- `set_register(int reg, uint32_t value)` — inverse of `get_register`
  (D0-7=`m68k_dreg`, A0-7=`m68k_areg`, SR=`regs.sr`+`MakeFromSR()`,
  PC=`m68k_setpc`). Values arrive MSB-first hex, same as `hex32` emits.
- Dispatch handlers after the `m` (read) case:
  - `M addr,len:hex` → memory write via `get_mem_bank(adr).bput()` (guarded by
    `debug_safe_addr`).
  - `P n=val` → one register; `G <144 hex>` → all 18 registers.
- `X` (binary write) is intentionally left unimplemented: gdb's auto-probe
  disables it and falls back to `M`, so writes work. **Do not** `set remote
  X-packet on` — the stub won't answer `X` and gdb hangs.

Build (WSL-native — the bartman extension ships Linux binaries):

```sh
make -C /mnt/c/Projects/Linux/fs-uae -j$(nproc)            # incremental relink OK
cp /mnt/c/Projects/Linux/fs-uae/fs-uae \
   /mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-*/bin/linux/fs-uae/fs-uae
```

(The original is backed up as `fs-uae.orig.bak`. The patch is purely additive —
safe to keep permanently. The bartman dir stays the cwd for its support files.)

### Inferior CALLS on the FS-UAE stub (fixed — needed for load_career on Amiga)

gdb inferior function calls (`call func()` / `print func()`, used by
`load_career` / `save_career` / `enter_career`) originally **free-ran** the
inferior on the FS-UAE stub instead of returning. **Fixed** by a one-line
additive patch to `barto_gdbserver.cpp`, alongside the write support above.

**The whole fix is `fill_prefetch()` in `set_register(PC)`.** The A1200 runs a
cycle-exact 68020 whose instruction stream comes from the prefetch pipe /
i-cache (`regs.irc` / `regs.prefetch020[]` / `caches020`), not directly from
`regs.pc_p`. `m68k_setpc` (the `P11` packet gdb sends to point PC at the target
function) updated `regs.pc` but did **not** rebuild the prefetch — so a resumed
inferior call executed the **stale prefetched opcode** from where the inferior
had stopped, faulting at the new PC. `fill_prefetch()` after `m68k_setpc` rebuilds
it. With that, the call runs correctly and **FS-UAE's existing `bpnodes[]`
PC-compare scan catches the call-dummy return on its own**: gdb's `Z0` at the
return address (the ELF entry point) makes a `bpnodes[]` entry, and the scan
(`debug.cpp` ~7315) — re-armed after every `vCont;c` (`debug.cpp` ~7508) — stops
there. No trap-opcode patching is needed.

> **History (why this is one line, not the original ~120):** the first version of
> this fix ALSO wrote real `ILLEGAL` (`0x4AFC`) opcodes at breakpoints + shadowed
> `m`-reads + a custom IllegalError→swbreak path, believing the `bpnodes` scan
> couldn't catch the dummy-return. That belief was wrong — the scan DID work once
> the prefetch was rebuilt; the opcode machinery was both unnecessary AND the
> cause of an intermittent SIGILL (`Illegal 0008 at 0x20F3B0`) on **repeated**
> calls, where the opcode save/restore round-trip gdb does around each call
> desynced and left a corrupted word. The hardening pass deleted all of it
> (10 insertions, 117 deletions vs baseline = just `fill_prefetch`), which made
> repeated inferior calls robust. Keep it simple: don't reintroduce opcode
> patching.

The change is additive (the `bpnodes` scan is untouched, so source breakpoints
and menu-driving are unaffected). Build + deploy as in "FS-UAE write support".
**Verified:** a 5-load loop (`AUTO_S03/05/08/12/19`) all return `rc=0` with
correct `gameType=4`/`season`, no SIGILL; single calls return; source breakpoints
still fire. `rc` is reliable on the Amiga now (no "verify by data" caveat).

> **Long-call timeout (important):** the inferior call of `SaveJson_LoadCareer` /
> `SaveJson_WriteCareer` parses/emits JSON via mjson — a few seconds on SDL but
> **1-3 minutes on the emulated Amiga**. The proxy `cmd` must use a long
> `--timeout` (the plugin uses `CALL_TIMEOUT=300`) or it disconnects mid-call and
> wedges the gdb client. The daemon returns as soon as the call completes, so a
> high ceiling is safe on both targets. Stage the filename as ONE bulk write
> (`set {char[N]}scratch = "..."`, via `_stage_filename`), not per-byte
> `write_bytes` — the latter returned `rc=None` on the Amiga.
>
> **Cross-target result (2026-06-18):** all 17 deep-run saves (`AUTO_S03`–`S19`,
> SDL-written) load correctly on the Amiga build — `season`/`gameType`/`numSel`
> match per-save across the full 16-season career (5 re-confirmed by `rc=0` after
> the hardening; all 17 by the field check).

## Amiga `DBG=1` build (precise breakpoints)

The release Amiga build is `-Ofast`, whose reordering makes line breakpoints
imprecise and elides locals. The Makefile (`SWOS_C/Makefile`) has a **`DBG=1`**
mode that builds `-O0` (precise line breakpoints, every local/static live in
memory). Struct/ABI layout is opt-independent, so save-format cross-target tests
stay valid.

```sh
export AMIGA_BIN=/mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-*/bin/linux
export PATH=$AMIGA_BIN/opt/bin:$AMIGA_BIN:$PATH
rm -f /mnt/c/Projects/AI/SWOS/SWOS_C/obj/* /mnt/c/Projects/AI/SWOS/SWOS_C/out/*   # first WSL build only: stale Windows-path .d files break GNU make
make -C /mnt/c/Projects/AI/SWOS/SWOS_C DBG=1 DUMP_ASM=0 -j$(nproc)
```

Notes: keep `-fomit-frame-pointer` even in DBG (`-fno-omit-frame-pointer` ICEs
GCC 15.1.0 in `dwarf2out_frame_debug_adjust_cfa`); `-g` still emits CFI so gdb
unwinds. `-O0` emits a real `strncmp` that `-Ofast` inlined — provided in the
`#if defined(__m68k__)` shim in `src/swos_io.c`. Release binary backed up as
`uae/dh0/data/SWOS2.release.bak`.

## Usage

Bring a target up (see `SWOS_C/docs/GDB.md` for the daemon / `amiga_gdb.sh`),
get it to a menu, then:

```sh
python3 scripts/swos_harness.py check            # verify gdb writes reach target
python3 scripts/swos_harness.py where            # ordinal + onSelect symbol + count
python3 scripts/swos_harness.py enum             # every entry's onSelect symbol
python3 scripts/swos_harness.py nav down         # one step (up/down/left/right)
python3 scripts/swos_harness.py fire             # press the current entry
python3 scripts/swos_harness.py combo fire left  # hold controls one frame
python3 scripts/swos_harness.py goto _CareerMenu # move selection by action symbol
python3 scripts/swos_harness.py select _CareerMenu  # goto + fire
python3 scripts/swos_harness.py load HYBRID_CC.CAR  # load a career via the game's loader
python3 scripts/swos_harness.py cap /tmp/s.png   # screenshot (SDL only)
```

`nav`/`fire`/`combo` auto-`prime` (set the inject breakpoint and stop on it).
`goto`/`select` jump directly via `_SetCurrentEntry` (a memory write).

## Save loading (no UI navigation needed)

`load NAME.CAR` stages the filename into a scratch global (`menuSongData`, via
per-byte writes — no inferior `malloc`) and calls the game's own
`SaveJson_LoadCareer` natively. This is the clean way to set up a cross-target
test: load a save, then read the restored state read-only and compare to the SDL
side (or `scripts/decode_save.py`).

**Pass a BASENAME, not a `save/`-prefixed path.** The game's `resolve_save_path`
(`swos_io.c`) strips a `drive:` prefix then **unconditionally prepends `save/`**.
So `load_career("save/X.CAR")` opens `save/save/X.CAR`, which **fails** —
`SaveJson_LoadCareer` returns non-zero (`0` == success) and the load **silently
no-ops**. That failure is easy to miss because a warm in-memory career masks it
(the harness keeps driving the *previous* career). `load_career()` /
`save_career()` now strip a leading `save/` defensively, but pass
`"MILAN.CAR"`, not `"save/MILAN.CAR"`. Always check the return is `0` and that a
sentinel field actually changed (e.g. `game_type` == 4 for career,
`current_season_showing`, `num_selected_teams`).

`load_career()` only loads **data**. From a cold boot the active menu is still
`_SWOSMainMenuInit`; call **`enter_career()`** afterwards — it runs the game's
`_StartContest()` (with the inject breakpoint set so the modal `_ShowMenu`
call breaks *inside* the now-active contest menu, the correct nested context).
`save_career("X.CAR")` writes via `SaveJson_WriteCareer` (same basename rule).

## Screen capture

`cap OUT.png` writes a PNG of the live display on **either target** (orientation
aid; drive via menu state for precision). The plugin auto-detects the target:

- **SDL** — 8bpp chunky framebuffer (`frontbufferBaseAltPtr`, 336×272) + the
  `s_palette` RGBA table. (`scripts/swos_screencap.py` is the standalone SDL
  version.)
- **Amiga** — interleaved bitplanes decoded planar→chunky→RGB. Handles **AGA**
  (8 planes / 256 colours) and **OCS** (4 planes / 16 colours), auto-detected via
  the `bitPlane4Base` symbol (AGA-only). Layout (`src/include/display.h`): pixel
  (x,y) bit p is at `base + y*PLANE_STRIDE*NUM_PLANES + p*PLANE_STRIDE + x/8`,
  bit `7-(x%8)`, `PLANE_STRIDE=44`. The displayed base is the flag-selected
  frontbuffer (`usePlaneOffsetFlag`→mask, `useAltBaseFlag`→Alt, else base).
  Palette is `gamePalette[]` (big-endian uint16, 12-bit `0x0RGB`; nibble×0x11 →
  8-bit). Camera offsets are assumed 0 (menus); in-match scrolling isn't decoded.

## Season loop (driving a career through seasons)

> ### ⚠️ REQUIRED build flags for season runs
>
> An unattended season run needs a binary built with **both** of these, or it
> will stick or corrupt state:
>
> - **`SWOS_TEST_NO_INJURIES_CARDS=1`** — disables injuries/suspensions/cards so
>   no player ever goes **red**. Without it, a carded/injured player turns the
>   squad row red and the game **locks PLAY MATCH / VIEW RESULT until you swap
>   that player out**. The driver does not do squad swaps, so it gets **stuck on
>   the squad screen** the first time a red player appears (`_PlayMatchMenuInit`
>   with no fireable result button). With the flag, VIEW RESULT is always
>   available and the fixture sims on `pl1Fire`.
> - **`SWOS_SAVE_FORMAT_JSON_VERIFY=0`** — RELEASE (verify off). The default
>   build has VERIFY **on**, and VERIFY **reloads the career into the live
>   globals on every save** to diff it — which leaves live state half-restored,
>   so **save-then-continue crashes**. The driver saves at each season hub and
>   keeps playing, so it must not be a VERIFY build. (Verify saves separately:
>   load each file in a VERIFY build, or use `decode_save.py`.)
>
> Build it: `make X86_64=1 SWOS_SAVE_FORMAT_JSON_VERIFY=0 SWOS_TEST_NO_INJURIES_CARDS=1`
> Confirm: `nm bin/SWOS.x86_64 | grep -c verify_roundtrip` → `0`.
>
> **Cold-boot setup** (fresh daemon): `break menu_system.c:1567`, `run`, then let
> the loader-intro auto-time-out to `_SWOSMainMenuInit` (~1–2 min; it ignores the
> 1567 breakpoint until the main menu). Then `load_career("NAME.CAR")` +
> `enter_career()` (runs `_StartContest` to enter the career UI — a plain data
> load does NOT enter it). Filenames are **basenames** — see "Save loading".

`seasons N` advances a loaded career up to `N` seasons unattended. The career
moves through three kinds of screen, each taking a **different** input — and
getting this wrong is what once looked like a "gdb wedge". It was not: gdb and
the multithreaded SDL inferior are stable (80+ menu-steps and a full transition
run with `health=OK` throughout, threads untouched). The wedge was the *driver*
free-running into a screen that waits for input.

**Per-screen input model** (`scripts/swos_plugin.py`, `drive_transition` /
`drive_inseason`):

1. **Season transition** (`seasonUpdateInProgress != 0`). The cup/league result
   displays are auto-advance menus: `_DIYCupInit` pre-selects entry-66 with
   `controlMask == 0xFFFF` (see `_Career_AutoAdvanceCupMatches`). Set
   `s_seasonSkipLatched = 1` and the whole chain drains itself; the latch
   persists until `seasonUpdateInProgress` returns to 0. **Do not move the
   selection** — `_SetCurrentEntry` off entry-66 makes the modal stick (that was
   the original bug). The only keypress the chain needs is at the **chairman
   memo** (`_ChairmanMemosInit`, a plain modal) — `shortFire` on its Continue
   button. A conditional breakpoint
   (`break menu_system.c:1567 if ((Menu*)currentMenu)->onInit == _ChairmanMemosInit`)
   catches *only* memo frames; a code breakpoint at the season increment
   (`career_init.c:974`, `_InitializeNewSeason`) ends the drive. Skip-latch +
   conditional-memo-bp = no blind free-run into an input-waiting screen, so no
   wedge.

   The **old** driver free-ran straight to the increment line, which hangs
   forever at the chairman memo. That hang — misread as gdb/thread instability —
   is the entire reason this approach exists. The interrupt fallback uses
   `kill -INT` on the gdb process (stdin Ctrl-C does not work through the daemon
   pipe — no tty); it is only a safety net for an *unanticipated* waiter.

2. **In-season menus** (memo / contest hub / match-day list, `sUIP == 0`):
   plain `shortFire` on the default selection.

3. **Pre-match squad screen** (`_PlayMatchMenuInit`): `_PlayMatchSelected` gates
   on the **raw joystick fire** `pl1Fire`, *not* the menu `shortFire`. Set
   `pl1Fire = 1` to play/sim the fixture (CPU-vs-CPU → instant result).

The in-season **cup/league match-day menus self-drive on `shortFire`**:
`_DIYCupInit` / `_DIYLeagueTournamentInit` pre-select the correct entry at each
state — play the human's fixture, auto-advance an AI fixture, or **exit** when
the round is done (e.g. `_DIYCupOnSelect10` at `menuState=6 / roundComplete=1`).
When the season's fixtures are exhausted (`seasonFinished=1`), the contest hub's
PLAY MATCH runs `_SeasonFinished → careerOverMenu` (the hub). The driver's
stuck-detector is keyed on `(screen, selected-ordinal)` so the legitimate
match-day↔squad bounce and cup-round cycling don't false-trigger.

**Status of the loop — validated end-to-end on SDL:**

- **Transition** — season 3→4 via the skip-latch path, `health=OK`, zero wedges.
- **In-season** — a full season driven to the season-end hub in 208 steps /
  ~174s, no stuck, no wedge.
- **Full cycle** — `advance_one_season` (hub → CONTINUE → transition →
  in-season → next hub) ran season 4→5 in ~274s, landing back at
  `_CareerOverInit`, `health=OK`, threads untouched.

A full season is a few minutes (≈0.9s/step over a few hundred steps), so a
multi-season `seasons N` run is minutes-to-tens-of-minutes — intended to run
unattended.

## Reusing for another project

1. Keep `gdb_proxy.py` and `harness_lib.py` as-is.
2. Write a new `<game>_plugin.py` mirroring `swos_plugin.py`: the input
   injection point + encoding for that game's UI, its menu/state accessors, and
   framebuffer geometry.
3. If the target is an emulator with a read-only gdb stub, patch it for writes
   (as done for FS-UAE) or confirm writes another way (`check`).

## Status

- Validated on the **Amiga** (FS-UAE, patched stub, `-O0` DBG build): writes,
  menu state read, multi-step navigation (`nav down`/`up`, action symbols
  resolved each step), and **screen capture** (OCS decode verified pixel-correct
  against the live main menu). The AGA capture path shares the same parameterized
  decode (planes=8/colors=256, palette format confirmed against
  `_WriteCopperPalette`'s AGA branch); runtime verification against an AGA build
  is pending because the **AGA game build itself isn't fully implemented yet** —
  not a harness limitation.
- The injection is shared C, so it applies to **SDL** identically (native gdb
  writes always work); re-validate SDL after pointing the daemon at a runnable
  Linux ELF.
- **Season transition driving** validated on SDL: a career advanced 3→4 via the
  skip-latch path with `health=OK` throughout and no threading change. The
  in-season match-day stepper is best-effort (see "Season loop"). The old
  "free-run to the season increment" path is retired — it hung at the chairman
  memo, which was mistaken for gdb/multithread instability and is *not*.
