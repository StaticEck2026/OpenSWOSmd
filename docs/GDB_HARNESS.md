# SWOS gdb Test Harness

A gdb-driven harness that lets the assistant **drive the game and read its state
itself** — navigate menus, press fire, load saves, capture the screen — on
**both** the SDL build and the Amiga build (under FS-UAE), with no OS-level input
or screenshot tools. Navigation feedback comes from reading the game's internal
menu state over gdb (fast + token-light); screenshots are an orientation aid
only.

This document is **separate from the generic gdb daemon doc**
(`docs/GDB_DEBUGGING.md`). GDB_DEBUGGING.md documents the reusable transport; this documents
the SWOS automation built on top of it.

---

## Where the tools live (moved 2026-07-19)

`gdb_proxy.py` and `amiga_gdb.sh` were split out of `SWOS_C/scripts/amiga/` into
their own repo — **AmigaScope** (github.com/benbaker76/AmigaScope), checked out
locally at `/mnt/c/Projects/AI/AmigaScope`. Only the *transport* moved; the SWOS
layers (`scripts/harness_lib.py`, `scripts/swos_plugin.py`,
`scripts/swos_harness.py`) stayed here.

`harness_lib.PROXY_CANDIDATES` resolves the proxy in this order:
`$AMIGASCOPE_DIR/gdb_proxy.py` → `/mnt/c/Projects/AI/AmigaScope/gdb_proxy.py` →
the pre-split in-tree path. Set `AMIGASCOPE_DIR` if your checkout is elsewhere.
`swos_plugin.amiga_up()` derives `amiga_gdb.sh` from `dirname(proxy)`, so
pointing the proxy at the repo fixes the Amiga side too.

**Symptom of a wrong path:** `GdbHarness()` now raises `FileNotFoundError` at
construction. Before that guard existed, every `cmd()` returned python3's "can't
open file" on stderr, which parses as "no symbol / not stopped" — so a moved
repo looked exactly like a wedged target and cost a session. Docs elsewhere that
still say `scripts/amiga/gdb_proxy.py` predate the split.

---

## ⭐ READ THIS FIRST — driving a career through seasons (the proven recipe)

This is the route that works (validated 2026-06-19: a career driven season 3 → 19
+ retirement, no wedges; cold-start + fresh-career reliability re-verified
2026-07-03). Follow it exactly; the "DON'Ts" below each cost *hours* in a
debugging session — they are the only reason this got hard.

**Cold start is now automatic (fixed 2026-07-03).** From a stone-cold daemon
(ELF loaded, never `run`), ANY entry point — `where` / `prime` / `nav` / `select`
/ `load_career` / `start_fresh_career` — auto-boots to the main menu. You do NOT
need a manual `break MenuInputInjectPoint` + `run` first. Three bugs had made this
fail every session; all three are fixed in `scripts/harness_lib.py` +
`scripts/swos_plugin.py`:
  1. `is_stopped()` did `"stopped" in health()`, but the not-booted verdict is
     `"DEGRADED target not cleanly stopped"` — which CONTAINS "stopped" → false
     positive → `ensure_stopped()` thought a never-run daemon was already stopped
     and skipped the boot. Now keys on the `OK` verdict.
  2. `prime()` issued `continue`, which silently no-ops on a never-run target
     (`info inferiors` shows `<null>`). Now it detects "not booted" and `boot()`s.
  3. `set_var()` did `set variable X = v`, which FAILS ("Invalid cast") for the
     codebase's `uint16_t X[1]` stub-globals (`_fire`, `_shortFire`,
     `_menuControlsDirection`) — so every menu _fire was silently dropped and the
     drivers hung. Now retries `X[0] = v`. (This — not a "non-deterministic
     single-frame drop" — was the real dropped-fire cause.)

### The recipe (copy/paste)

```bash
# 1. BUILD FRESH and confirm it actually built (a stale binary = garbage struct reads).
make X86_64=1 SWOS_SAVE_FORMAT_JSON_VERIFY=0 SWOS_TEST_NO_INJURIES_CARDS=1 -j"$(nproc)"
echo "exit=$?"; ls -l bin/SWOS.x86_64        # exit must be 0; mtime must be NEWER than your src edits
find src \( -name '*.c' -o -name '*.h' \) -newer bin/SWOS.x86_64 | head   # MUST be empty
```

```python
# 2. Launch the daemon via Bash run_in_background:true (NOT nohup& — see DON'Ts), headless.
#    gdb_proxy.py lives in the AmigaScope repo (see "Where the tools live" below);
#    --bin MUST be absolute (a relative path makes `run` silently no-op):
#    cd bin; SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SWOS_TEST_NO_INJURIES_CARDS=1 \
#      SWOS_SAVE_FORMAT_JSON_VERIFY=0 python3 /mnt/c/Projects/AI/AmigaScope/gdb_proxy.py \
#      daemon --mode sdl --bin /abs/path/to/OpenSWOS/bin/SWOS.x86_64
# 2.5 Put the test careers where the game looks for them — the PER-USER save dir
#     (~/.swos/save on Linux/WSL), NOT bin/save (changed 2026-07-04, see DON'T #10):
#        cp bin/save/*.CAR ~/.swos/save/
# 3. Drive seasons from a LOADED career (NOT a fresh one):
from swos_plugin import SwosHarness
s = SwosHarness()
s.boot()                                  # cold boot → main menu (~10 s with TURBO, auto-set by boot())
s.load_career("MILAN.CAR")                # rc 0 (rc 1 = not in ~/.swos/save — see step 2.5)
s.enter_career()                          # → _CareerOverInit (the season-end hub)
s.run_seasons(20)                         # advances season-by-season → retirement (season 19)
```

`run_seasons` = `advance_one_season` in a loop: from `_CareerOverInit`, fire
CONTINUE CAREER → `drive_transition` (skip-latch auto-advances the result chain) →
`drive_inseason` → next season-end hub. ~17–47 s/season.

### DON'Ts (each of these wasted hours — do not repeat)

1. **DON'T drive a *fresh* career's in-season fixtures as the season engine.**
   `start_fresh_career()` is for *creating* a career; it lands at the *in-season*
   hub (`_ContestMenuInit`), not the season-end hub. Driving its fixtures
   one-by-one with blind `shortFire` (`drive_inseason` from season 0) is **not the
   tested path** — it wanders hub↔match-day↔memo. To drive seasons, use
   `run_seasons` on a **loaded** career. (To use a fresh career: `save_career()` it
   and reload, or just load `MILAN.CAR`.)
2. **DON'T read `diyCompetitionStart.*` on a brand-new career.** Until a
   competition is *active*, that struct is **uninitialised** — you'll read
   garbage (`matchesPlayedCount=1614`, `gamesPlayed=206 > 38 max`) and conclude
   there's an "overlay"/regression. There isn't: the fields are clean named
   members and the size asserts pass; on a *loaded* mid/late-season career they
   read correctly (`gamesPlayed ≤ 38`). Use `currentSeasonShowing` /
   `_current_init()` to track progress, not raw competition-struct reads.
3. **DON'T debug against a stale binary.** If `find src -newer bin/SWOS.x86_64`
   is non-empty, REBUILD first. A stale binary's gdb member offsets won't match
   the source — you'll chase phantom "struct mismatches". (`raw[&x+0xNN]` ≠
   `member` is the tell.)
4. **DON'T launch the daemon with `nohup … &` inside a normal Bash call** — it
   dies on shell exit and the next `boot()` returns garbage (`init=module`,
   everything `None`). Launch it with the Bash tool's **`run_in_background: true`**
   (a foreground `gdb_proxy.py daemon` the harness keeps alive).
5. **DON'T run two harness scripts against the daemon at once.** It multiplexes
   one gdb stdin/stdout; overlapping clients interleave traffic and **wedge** it
   (`health → DEGRADED info program hung`). Wait for one script to exit before the
   next. If wedged: force-kill the daemon + `gdb` by PID and relaunch.
6. **DON'T `load_career()` in place into an inferior that already drove a career**
   — runtime competition state is left stale. Cold-boot fresh each test.
7. **DON'T poll the output file every few seconds.** Long drives are minutes;
   use `python3 -u` + `flush=True` and wait for the background-task notification.
8. **DON'T fire a menu entry once and assume it dispatched.** `select()` /
   `start_fresh_career()` **retry the fire until the menu transitions** (onInit or
   the visible list changes); any custom drive must do the same (the season-loop
   drivers already re-assert shortFire every iteration — that's why they're
   robust). **Root cause (fixed 2026-07-03):** the fire wasn't "non-deterministically
   dropped" — it was NEVER set. `_fire`/`_shortFire` are `uint16_t[1]` stub-arrays,
   so `set variable _fire = 1` failed "Invalid cast" and `set_var` silently no-op'd
   every retry (nav still worked because direction ALSO flows through the scalar
   `_up`/`_down` globals). `set_var` now falls back to `X[0] = v`. Diagnose fire
   issues by reading `selectedEntry->controlMask` (the control bit the entry gates
   on — e.g. CAREER = `0x20` = shortFire) and confirming your `_fire[0]`/
   `_shortFire[0]` writes actually READ BACK non-zero, plus `SWOS_HARNESS_LOG=1`'s
   `dispatch-try` line (controlMask vs injected d7).
9. **DON'T blame "WSL/gdb degradation" for a slow or wedged daemon — it's almost
   always misuse or a daemon bug (diagnose, ⚠️ section below).** `boot()` sets
   TURBO, which already removes the per-frame wall-clock waits WSL could stretch,
   so a `SLOW CONT` / `DEGRADED` / `target running` mid-session is **not** "WSL
   timer starvation". Every time we've blamed WSL, the real cause was: two clients
   hitting the daemon at once, rapid kill/relaunch (stale sockets / half-dead gdb),
   `delete_breakpoints()`+`cont()` into a state that never stops, or the operation
   not being in the state you assumed. Diagnose (30 s) before ever reaching for
   `wsl --shutdown`. And never let one task's cleanup `pkill -f gdb_proxy.py` run
   while another task's daemon is live — it kills the wrong daemon; use `pkill -x`
   or a PID.
10. **DON'T assume `load_career()` reads from `bin/save`.** Since 2026-07-04 the
    game resolves saves to the **per-user OS dir** (`swos_save_dir()`:
    `~/.swos/save` on Linux/WSL, `Documents/SWOS/save` on Windows, `Application
    Support/SWOS` on macOS) — see docs/SAVE.md. `load_career('X.CAR')` looks there,
    NOT in `bin/save`, and returns **rc 1 *silently*** if the file is absent (no
    exception; you then read stale/warm in-memory state and chase a phantom bug).
    Fix: `cp bin/save/*.CAR ~/.swos/save/` before the run. Confirm the game's dir
    with `cmd 'print (char*)swos_save_dir()'`. (Also: many career state globals
    — `_diyTournamentType`, `_squadChangesAllowed`, `_fire`, … — are `uint16_t[1]`
    stub arrays, so **read them as `sym[0]`**; `print (int)sym` gives the *address*,
    not the value. This is the read-side of DON'T #8's write gotcha.)

If a season run "wedges": it's almost always #4 (dead daemon), #5 (contention),
#8 (dropped fire, no retry), or #9 (degraded/slow daemon) — not gdb instability
or a game bug. Check `health`, the daemon process, and a `SWOS_HARNESS_LOG` trail.
If `load_career` returns rc 1 or reads look stale, it's #10 (wrong save dir).

### ⚠️ "Boot hangs / harness wedges" is a DAEMON BUG — diagnose it, don't blame WSL

Every "boot is slow / harness hangs" we've chased has turned out to be a bug in
the **daemon** (`scripts/amiga/gdb_proxy.py`), NOT WSL/gdb "degradation", NOT the
game, NOT the binary. Two have been root-caused and fixed; the symptom is always
the same and the diagnosis takes ~30 s:

**FIRST, get the real state (do NOT guess "WSL").** While `boot()`/`waitstop`
appears hung, ask the daemon what's actually true:
- `gdb_proxy.py health` → if it says `OK stopped PC=…`, the inferior IS halted
  fine and only the *stop reporting* is broken (a daemon bug).
- `gdb_proxy.py cmd "info breakpoints"` → if the **hit count climbs**, the
  breakpoint is firing; the inferior is reaching the menu. So boot isn't slow —
  `waitstop` just isn't noticing the stop.
- If you need a backtrace: `kill -INT <gdb-pid>` then `cmd "thread apply all bt"`
  (the main thread sitting at `MenuInputInjectPoint` = it reached the menu).

**The two daemon bugs (both fixed):**
1. **Relative `--bin` vs gdb cwd** → `run` no-ops (`Use the "file"/"exec-file"
   command`), inferior never launches, every boot hangs. Fix: `abspath(args.bin)`.
2. **`wait_stop` consume-the-stop RACE (fixed 2026-06-28).** `boot()` does
   `cmd("run", timeout=4)` then `waitstop`. If the breakpoint fires *inside* the
   4 s window, `cmd("run")`'s prompt-drain **swallows** the `Breakpoint N,` chunk
   and flips `running=False`; the later `waitstop` then waits for a *new* stop
   that never comes → hang. It is **boot-SPEED-dependent and INVERTED**: a FAST
   boot (TURBO healthy, bp < 4 s) wedges; a SLOW boot dodges it. *That is exactly
   why slowing things down (or "WSL degradation") appears to "fix" it, and why a
   timing tweak fixes it but it recurs.* Fix: `wait_stop()` returns when the
   target is already stopped (`if not self.running: return …`). Effect: `boot()`
   150 s+/hang → ~3 s.

Red herrings, ruled out: the `endbr64;ret` 4-byte `MenuInputInjectPoint`, and
line- vs symbol-breakpoints (the harness already breaks on the **symbol**, which
resolves + fires correctly — a `.map` file would not have helped). Don't chase
those; get `health`/`info breakpoints` first.

### ⚠️ "SLOW CONT / DEGRADED / target running / None reads" mid-drive is ALSO not WSL

Same rule as boot hangs, for the OTHER recurring symptom — a `[harness] SLOW CONT`
warning, `health → DEGRADED`, `int_value` reads returning `None`, or `target
running (busy)` **in the middle of a drive**. Every time this got blamed on "WSL2
timer starvation" the real cause was one of the following. TURBO (auto-set by
`boot()`) already removed the per-frame pacing WSL could stretch, so **with TURBO
on it is not WSL** — check these IN ORDER first:

1. **Two clients at once.** A second script or one-shot probe (even `health` /
   `cmd`) hit the daemon while a background drive still owned it → interleaved gdb
   traffic wedges it (`DEGRADED info program hung`). Wait for the background script
   to EXIT before probing. (See "Daemon discipline".)
2. **Rapid kill/relaunch.** Restarting the daemon on every hiccup leaves stale
   sockets / half-dead gdb — that IS the flake. Launch once; recover in place.
3. **`delete_breakpoints()` then `cont()` into a state that never stops.** With no
   breakpoint the inferior runs the menu/idle loop forever; the cont burns its
   whole timeout (→ SLOW CONT) and `_interrupt()` (SIGINT) is a racy stop → `None`
   reads / `target running`. To sample DURING a match, keep a breakpoint at a
   per-frame function (e.g. `MoveCamera`) and read at that clean stop — do NOT
   delete-breakpoints + cont + interrupt.
4. **The operation isn't in the state you assumed.** If the "match" never actually
   started (fumbled menu nav — e.g. `ordof("PLAY")` matching "PLAYER-COACH", or the
   real PLAY entry never fired), your "in-match cont" is really spinning in a menu.
   Confirm the state (`_gameStatePl == ST_GAME_IN_PROGRESS`, or the expected `init`)
   BEFORE concluding the harness is slow.

Diagnose first (same 30 s as boot): `gdb_proxy.py health`, `cmd "info breakpoints"`
(hit count climbing?), and confirm no other client is live. `wsl --shutdown` is a
LAST resort after 1–4 are ruled out — reaching for it first is how we keep
mis-blaming WSL and losing an hour. (2026-07-07: hit all four of these in one
session — repeatedly relaunched the daemon, ran a concurrent `health` probe, and
`delete_breakpoints()`+cont into a match that never started because the menu nav
matched "PLAYER-COACH" for "PLAY" — then blamed "WSL". None of it was WSL.)

### Seeing where the game is — `SWOS_HARNESS_LOG=1` (the [harness] log)

Set `SWOS_HARNESS_LOG=1` in the inferior's env and the game writes `[harness] ...`
lines to `SWOS.log` (stderr) on menu activation, menu-entry dispatch, and the
team-picker enumeration — flushed each line. Because it's a plain file the game
writes itself, you can read `bin/SWOS.log` to see exactly where a drive got even
when the inferior is mid-operation or hung (the LAST line pinpoints the freeze) —
no gdb interrupt, no wedge. This is the fastest way to localize a nav hang. Macro
is `HLOG(...)` in `swos_platform.h`; off by default; SDL-only (no-op on Amiga).

---

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
target memory/registers. Native SDL gdb is fine. **The stock FS-UAE / bartman stub
is read-only** (it ACKs reads + breakpoints but silently drops `M`/`X`/`G`/`P`
writes — so `set var` never sticks and every injection no-ops). It was patched to
add write support — see below. Run `swos_harness.py check` to confirm writes
reach the target (it fails loudly on a read-only stub).

> **You do not have to redo this patch by hand.** It is packaged in the
> [`benbaker76/vscode-amiga-debug`](https://github.com/benbaker76/vscode-amiga-debug)
> fork of the extension, which is the build this project (and AmigaScope) assumes
> — install that instead of the marketplace one. The two are **indistinguishable
> once installed**: same `bartmanabyss.amiga-debug-<ver>` directory, same
> publisher and upstream repo URL in `package.json`. The only reliable tell is
> behavioural — reads work, writes silently do nothing. The section below is kept
> for the record, and for rebuilding the stub if you need to change it.

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

## Amiga: attach → main menu (the fast procedure)

Getting the Amiga harness to the main menu is **fast** once you know the
procedure — measured ~34 s cold-boot+attach, then **~3–5 s entry→menu** (verified
3 fresh cycles, 2026-06-28). The CAR load is the only slow part (1–3 min, the
mjson parse — unavoidable).

```python
# 1. amiga_gdb.sh up   (bash) — cold-boots FS-UAE + attaches at the SWOS2 entry.
#    It ALSO installs the loader-intro skip:
#      break ShowLoaderIntroSequence ;  commands: silent / return / continue
#    (force-returns from the slow intro so the next continue lands at the menu).
# 2. From the harness — DO NOT boot() (SDL-only: kill+run doesn't apply to the
#    FS-UAE remote; it returns None). DO NOT h.cmd("delete") — that deletes the
#    intro-skip breakpoint and the intro then plays in FULL (the slow path).
from swos_plugin import SwosHarness
s = SwosHarness(); h = s.h
h.break_at("MenuInputInjectPoint")     # ADD it; keep the intro-skip bp
h.cmd("continue", timeout=2)           # kick off; the intro auto-skips
h.waitstop(60)                         # -> SWOSMainMenuInit in ~3-5 s
```

**Why the cold boot is ~34 s and that's NOT polling/our-fault.** The attach is
**event-driven, exactly like the VSCode bartman extension**: `attach_once`
(amiga_gdb.sh) spawns FS-UAE then has the daemon's gdb `target remote` connect
**immediately** — gdb auto-retries the TCP connect and grabs the stub the instant
FS-UAE opens port 2345 at the `:SWOS2` trigger, then blocks on the handshake (no
log-scraping, no settle — that old approach is retired). The ~34 s is the emulated
A1200 **cold boot** (Kickstart + dh0 + launching SWOS2); the extension's
`target remote` blocks the same ~30 s. The only non-signal bit left is
`start_daemon`'s `sleep 1` socket-poll (≤1 s to *notice* the attach after it
happens) — tighten that if you want, but it's ~1 s, not the 34 s. To cut the boot
itself you'd need an FS-UAE savestate snapshotted at the entry (skips Kickstart).
If a *run* feels like it's "waiting", check it's not the agent polling the
background `up` task instead of waiting for its completion signal.

## Amiga `DBG=1` build (precise breakpoints)

The release Amiga build is `-Ofast`, whose reordering makes line breakpoints
imprecise and elides locals. The Makefile (`SWOS_C/Makefile`) has a **`DBG=1`**
mode that builds `-O0` (precise line breakpoints, every local/static live in
memory). Struct/ABI layout is opt-independent, so save-format cross-target tests
stay valid.

```sh
export AMIGA_BIN=/mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-*/bin/linux
export PATH=$AMIGA_BIN/opt/bin:$AMIGA_BIN:$PATH
rm -f /mnt/c/Projects/AI/SWOS/OpenSWOS/obj/* /mnt/c/Projects/AI/SWOS/OpenSWOS/out/*   # first WSL build only: stale Windows-path .d files break GNU make
make -C /mnt/c/Projects/AI/SWOS/OpenSWOS DBG=1 DUMP_ASM=0 -j$(nproc)
```

Notes: keep `-fomit-frame-pointer` even in DBG (`-fno-omit-frame-pointer` ICEs
GCC 15.1.0 in `dwarf2out_frame_debug_adjust_cfa`); `-g` still emits CFI so gdb
unwinds. `-O0` emits a real `strncmp` that `-Ofast` inlined — provided in the
`#if defined(__m68k__)` shim in `src/swos_io.c`. Release binary backed up as
`uae/artifacts/SWOS2.release.bak`.

## Usage

Bring a target up (see `docs/GDB_DEBUGGING.md` for the daemon / `amiga_gdb.sh`),
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

## Reliability & navigating by screenshot

Most "harness flakiness" is avoidable — it comes from process mistakes, not gdb
(gdb itself is stable; the inspection/read path is rock-solid). Follow this:

### Run the inferior HEADLESS (the #1 reliability fix)

Set `SDL_VIDEODRIVER=dummy` and `SDL_AUDIODRIVER=dummy` for the inferior (in the
daemon's gdb, **before `run`**):

```
cmd "set environment SDL_VIDEODRIVER dummy"
cmd "set environment SDL_AUDIODRIVER dummy"
cmd "break menu_system.c:1567" ; cmd "run" ; waitstop(200)
```

No real SDL window → no WSLg window churn → the daemon stays stable across long
runs. The game **still renders to `frontbufferBaseAltPtr`** (its 8bpp chunky
buffer — the chunky draw is the game's own code, before the SDL upload), so
`screencap` works unchanged. Booting *with* a real window and relaunching it many
times per session is what made the daemon look unreliable.

### TURBO — make the inferior CPU-bound, not wall-clock-bound (the cont-balloon fix)

`SWOS_HARNESS_TURBO=1` makes the SDL build run frames as fast as the CPU allows
instead of pacing them to a 50 Hz wall clock. **`boot()` sets it automatically**
(via `set environment SWOS_HARNESS_TURBO 1` before `run`), so you normally don't
touch it; set it by hand only if you `run` the inferior yourself.

Why it matters: the harness drives the game frame-by-frame via `continue`, so
per-frame real-time waits are pure overhead. Worse, under WSL2/gdb the inferior's
timer thread starves and those waits balloon — a `continue` that should be
sub-second stretches to **10–120 s** (reads stay fast; it's the *inferior frame*,
not command latency), the driver over-runs its `cont` timeout, and
`is_stopped()` falsely reports "stuck". TURBO removes every wall-clock wait the
starvation could balloon:

- `Platform_VBlank` skips the 50 Hz `SDL_Delay`+spin pace.
- `_RunInt3HandlerThrottled`'s 18 ms throttle drops to 0, so `currentGameTick`
  advances every call — tick-counted fades/holds/the loader-intro timeout drain
  at CPU speed and stop depending on the starved timer thread.
- the `SDL_AddTimer` interval drops from 20 ms to 1 ms (for pure-timer spins like
  the loader intro that never call `Platform_VBlank`).

Effect: a cont is bound by CPU work, not by a clock the scheduler stretches.
Validated headless — cold boot reaches `_SWOSMainMenuInit` in **~7.5 s** (vs the
old ~1–2 min); the residue is genuine RNC asset decompression, which TURBO leaves
alone. No effect on normal play (default off); SDL-only (`platform_sdl.c`
`s_harness_turbo`), nothing to back-port to the Amiga build.

A tripwire backs this up: `GdbHarness.cont()` warns on stderr (`[harness] SLOW
CONT …`) when a continue exceeds `CONT_SLOW_WARN_S` (4 s). Read it as "something is
wrong with MY usage", not "WSL is degrading": with TURBO on (which `boot()` sets)
the pacing WSL could stretch is already gone, so a persistent SLOW CONT is almost
always one of the causes in the ⚠️ "SLOW CONT … is ALSO not WSL" section above
(two clients, rapid relaunch, cont-with-no-breakpoint, wrong assumed state) —
diagnose those first. `wsl --shutdown` is a genuine last resort, not the first
move.

### Daemon discipline

- **Launch the daemon ONCE per session and leave it alone.** Do not rapid
  kill/relaunch it — that (stale sockets, half-dead gdb) is most of the "flake".
- **One client at a time — NEVER run two harness scripts against the daemon
  concurrently.** It multiplexes a single gdb stdin/stdout; two overlapping
  clients interleave their command/response traffic and **wedge gdb**
  (`health` → `DEGRADED info program hung`), which then needs a daemon restart.
  In practice: don't launch a new probe while a previous background script is
  still finishing (e.g. a slow `screencap` memory dump). Wait for it to exit
  first. If wedged, force-kill the daemon + `gdb` by PID and restart.
- **Boot** — the loader intro auto-times-out to the main menu and ignores the
  `1567` breakpoint until then. With TURBO (the default in `boot()`) this is
  ~10 s; without it ~1–2 min. Use `waitstop(200)` either way; never wrap the boot
  in a tight outer `timeout`.
- **Re-boot the inferior in the same daemon** with `kill` then `run` — breakpoints
  and `set environment` persist, so you don't pay a relaunch.
- Foreground `sleep` is blocked in this environment — poll background output, or
  use `python3 -c "import time;time.sleep(N)"` as its own step (not chained).

### Navigate by screenshot — `screencap_oriented()`

**Use screenshots to orient.** `screencap_oriented(out_png)` pins the selection
cursor BRIGHT (`menuCursorFrame=8` → `kColorTableShine[4]`=colour 7; the cursor
normally pulses `{2,2,1,1,7,7,1,1}` and is faint/invisible on some frames) and
blink-text visible, re-renders one input-free frame, then captures. Read the PNG
to see *which* screen you're on and what's displayed (team lists, names, etc.).

### The navigation-learning process

To discover an unknown menu flow, combine three reads at each screen:
1. `_current_init()` / `((Menu*)currentMenu)->onInit` — which screen.
2. `enum()` — every entry's `onSelect` symbol (tells you what each entry *does*).
3. `screencap_oriented()` — *see* it (labels, selection, lists).

Then act with `select("_SymbolName")` (jump to + fire an entry by its onSelect) or
`nav("down")`/`fire()`, and repeat. Symbols + screenshot together = you always
know where you are and where to go. For lists where every entry shares one
`onSelect` (the team picker), use `scan_text()` / `fire_text("LABEL")` instead —
see "Dynamic lists" below.

### Single-frame input pulse

`fire()` / `combo()` clear `fire`/`shortFire` after the frame, so input is a
one-frame pulse. A *held* fire mis-drives a nested `_ShowMenu` modal (it would
re-fire/exit the modal's default entry on the next frame).

### Dynamic lists: navigate by LABEL, not by onSelect

`goto("_SymbolName")` works when each entry has a distinct `onSelect`. Some
screens reuse ONE callback across many entries whose meaning is the displayed
STRING — the career **team picker** is the prime example: continents, then
countries, then league divisions, then clubs all share `_SelectTeamsCommon` /
`_SelectLeaguesCommon` / `_ChooseTeamsFriendlyCommon2`. For those, navigate by
text:

- `scan_text()` → `[(ordinal, label)]` of every visible string entry (the
  reliable way to "see" a dynamic list without a screenshot).
- `find_text("ENGLAND")` → the matching ordinal (case-insensitive; leading-
  substring fallback so `"NOTT"` finds `"NOTT'M FOREST"`).
- `fire_text("ENGLAND")` / `fire_ordinal(ord, timeout=…)` → select + single-frame
  fire (give a long timeout to the press that triggers `_InitCareer`).

Label matching also makes the path robust to **data edits that renumber
ordinals** — only the displayed names matter, which is exactly what you want when
regression-testing the data-jailbreak changes.

### Starting a fresh career (the NORMAL flow — no `_LoadLiverpool`)

`start_fresh_career()` drives a brand-new career through the real menus (verified
end-to-end from cold boot, 2026-06-19). The proven path:

```
boot()                         → _SWOSMainMenuInit          (headless, ~2 min)
  CAREER (select _CareerMenu)  → _CareerMenuInit
  CHOOSE TEAM AND START CAREER → _SelectTeamsInit  (the team picker)
    EUROPE → ENGLAND → PREMIER LEAGUE → ARSENAL  (fire_text drill-down by label)
  → _ChooseTeamAndStartCareer → _InitCareer(0x800)          (the REAL init; ~30-60s)
  → _ChairmanMemosInit         (welcome memo — dismiss with shortFire)
  → _ContestMenuInit           (the live career hub, season 0)
```

```python
from swos_plugin import SwosHarness
s = SwosHarness()
s.boot()                                  # cold start → main menu
s.start_fresh_career()                    # CREATE a career → _ContestMenuInit (Arsenal, EPL)
s.save_career("FRESHARS.CAR")             # snapshot it (reload in seconds later)
```

`start_fresh_career()` is for **creating** a career; it lands at the *in-season*
hub (`_ContestMenuInit`). To **drive seasons**, use the proven `run_seasons` path
on a career that's at the *season-end* hub (`_CareerOverInit`) — see "⭐ READ THIS
FIRST" at the top. Driving a *fresh* career's first season in-season with
`drive_inseason` is **not** the proven engine (blind `shortFire` wanders); the
reliable season driver is `run_seasons` on a loaded career (e.g. `MILAN.CAR`).

To reload a saved career fast (skip the ~7-min start): `s.boot();
s.load_career("FRESHARS.CAR"); s.enter_career()`.

Pick a different club with the `path` argument (continent, country, division,
club — all by visible label):

```python
s.start_fresh_career(path=("EUROPE", "ITALY", "SERIE A", "JUVENTUS"))
```

The auto-generated manager name + default nationality are kept (a fresh career
needs no name typed). `_LoadLiverpool` is a debug `call` shortcut (hard-codes
team 0x811, skips the country logic) — **not** used here; `start_fresh_career`
exercises the same `_InitCareer` path a human triggers.

> Note: the earlier "the `1567` breakpoint won't hold through the careerMenu
> modal" report was a **misdiagnosis** — it was display flakiness (fixed by
> running headless) plus a discarded-stop bug in the probe script. The `1567`
> breakpoint fires correctly inside every nested `_ShowMenu` modal.

---

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

3. **The player's OWN fixture — SIM IT (`teamControls = 1`).** A human-controlled
   club team (`careerTeam.team.teamControls` = 2/3) *plays* each of its ~38 league
   fixtures: "- MATCH -" → squad (`_PlayMatchMenuInit`) → `pl1Fire` →
   **the real-time in-game match loop** (`_ShowMatchSequence`, ~2+ min each,
   throttled in `clock_nanosleep`), which never returns to the inject breakpoint
   until the match clock expires. That is impractical to drive unattended for a
   league season (and it's what looked like a "wedge"). The lever: set
   `careerTeam.team.teamControls = TEAM_COMPUTER (1)` — the game then treats the
   player's fixture like an AI fixture and **auto-simulates** it on `shortFire`
   (computes a result, no squad, no in-game match). `drive_inseason(sim_mode=True)`
   (the default) sets this every step. Confirmed 2026-06-19 on a just-started
   career: a pending player fixture simmed instantly (no squad/in-game screen).
   (AI fixtures already auto-sim via the game's own `selector==0` path; sim mode
   just brings the player's fixture into the same path.)

The in-season **cup/league match-day menus self-drive on `shortFire`**:
`_DIYCupInit` / `_DIYLeagueTournamentInit` pre-select the correct entry at each
state — sim the player's fixture (in sim mode), auto-advance an AI fixture, or
**exit** when the round is done (e.g. `_DIYCupOnSelect10` at
`menuState=6 / roundComplete=1`). When the season's fixtures are exhausted
(`seasonFinished=1`), the contest hub's PLAY MATCH runs
`_SeasonFinished → careerOverMenu` (the hub).

**✅ THE PROVEN WAY TO DRIVE SEASONS — `run_seasons` on a LOADED career.** Driving
a career through seasons works via `advance_one_season` / `run_seasons`, which
operate from the **season-end hub** (`_CareerOverInit`): fire CONTINUE CAREER →
`drive_transition` (skip-latch auto-advances the result chain) → `drive_inseason`
→ next season-end hub. Start it by `load_career()`-ing a career that's at/near a
season end (e.g. `MILAN.CAR` = AC Milan season 3) + `enter_career()`. This is the
route that has always worked; use it.

**⚠️ Do NOT use a brand-new `start_fresh_career()` as the season-driver entry, and
do NOT read `diyCompetitionStart` before the league competition is active.** On a
fresh career sitting at the in-season hub, `diyCompetitionStart` is **not yet
populated** as the active league competition — reading
`diyCompetitionStart.matchesPlayedCount` / `leagueStandings` there returns
uninitialised values (the `1614` / `gamesPlayed=206` "garbage" that sent an
earlier debugging session down a rabbit hole; the field itself is a clean,
properly-untangled member at 0x1BE, NOT an overlay, and the struct size asserts
pass). Once a competition is active (a loaded mid/late-season career, or after
the season is under way) the same fields read correctly (`gamesPlayed ≤ 38`).
Also: don't `load_career()` IN PLACE into an inferior that already drove a career —
cold-boot fresh each time.

**Status of the loop (SDL) — VALIDATED on the fresh binary 2026-06-19:**

- ✅ **Full career to retirement** — `boot()` → `load_career("MILAN.CAR")` (rc 0) →
  `enter_career()` → `_CareerOverInit` (season 3, sane: gameType 4, numSel 47,
  `gamesPlayed=[34,34,34]`). `advance_one_season()` / `run_seasons()` drove
  **season 3 → 19** (17 seasons, each landing at `_CareerOverInit`, ~17–47s/season,
  no wedge); then CONTINUE CAREER at the season-19 hub triggered the retirement
  flow — `_ChairmanMemosInit` (retirement memo) → `_SWOSMainMenuInit` (career reset,
  season 0). This is the working harness: a complete career lifecycle.
- ✅ **Fresh-career START** — `boot()` + `start_fresh_career()` cold-boot →
  `_ContestMenuInit` (England/Arsenal, season 0), validated. Use this to CREATE a
  career; to DRIVE seasons, save it and re-enter (or play into a competition)
  before relying on `drive_inseason`.
- ✅ **Sim lever** — `careerTeam.team.teamControls = 1` (TEAM_COMPUTER) makes the
  player's fixtures auto-sim (the game's `selector==0` path) instead of opening
  the real-time in-game match loop (`_ShowMatchSequence`, throttled in
  `clock_nanosleep`). `drive_inseason(sim_mode=True)` sets it each step.
- ⚠️ **The squad screen needs the ENTRY, not just `pl1Fire`.** `PlayMatchSelected`
  is an entry `onSelect`: it only runs once that entry is *dispatched*, so the
  PLAY/COACH MATCH entry must be SELECTED (`SetCurrentEntry`) and the menu fire
  bit set; `pl1Fire` is the *inner* gate it then reads. Setting `pl1Fire` alone
  does nothing — the screen just sits there (60+ no-op frames).
- ⚠️ **A PLAYER-CONTROLLED fixture cannot be driven to completion headless.**
  Start one for real and the inferior pins a core in the in-game loop and never
  returns to `MenuProc`, so every `cont` burns its full timeout (observed: 30 min
  of CPU, no breakpoint). That is what the sim lever exists to avoid. The catch
  for *keycache* work: the sim lever sets `teamControls = 1`, which is the very
  condition `Career_UpdateStatsAfterMatch`'s store block tests
  (`team1->teamControls != 1 || team2->teamControls != 1`), so a simmed fixture
  cannot exercise the player-fixture path. To test that path, drive to the hub
  and CALL the writer directly on the current fixture, taking the two
  `TeamFileEx *` from `_selectedTeams[]`:
  `call (void)Career_UpdateStatsAfterMatch(&_selectedTeams[i], &_selectedTeams[j], 0, 0x0201, 0, 0, 1)`
  then diff `remainingElements` / `matchFlags` / `packedGoals` on the slot at
  `_teamSelectionKeyCache + _teamSelectionKeyCacheOffset + 10`. Do NOT reach for
  `VerifyTeamLoaded` to get those pointers — an inferior call into it hangs.

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
