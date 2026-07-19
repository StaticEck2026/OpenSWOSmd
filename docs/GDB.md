# gdb Debugging Workflow (Amiga + SDL)

How to build, run, and debug **both** ports of SWOS from WSL2:

- **Amiga build** — remote-debugged inside FS-UAE via `m68k-amiga-elf-gdb`.
- **SDL Linux build** — native gdb session on the x86 binary.

Both modes use the same `gdb_proxy.py` script so the command interface
is identical — you start a daemon and send commands with
`gdb_proxy.py cmd "<gdb command>"`. The proxy spawns the right gdb,
attaches/launches appropriately, and listens on a Unix socket.

---

## Amiga workflow

### Prerequisites

- **WSL2 with WSLg** (`echo $DISPLAY` / `echo $WAYLAND_DISPLAY` should
  return values). FS-UAE's window renders through WSLg.
- **bartmanabyss.amiga-debug VS Code extension** installed under
  Windows. It bundles both FS-UAE and `m68k-amiga-elf-gdb`. The
  Linux binaries live at:
  - `C:\Users\<user>\.vscode\extensions\bartmanabyss.amiga-debug-1.8.2\bin\linux\fs-uae\fs-uae`
  - `C:\Users\<user>\.vscode\extensions\bartmanabyss.amiga-debug-1.8.2\bin\linux\opt\bin\m68k-amiga-elf-gdb`
  Both are accessible from WSL via `/mnt/c/Users/<user>/.vscode/...`.
- **Amiga Kickstart** at `C:\Projects\Amiga\Kickstarts\KICK31_A1200.rom`.
- **Workbench HD** at the extension's bundled `bin/dh0/` directory.

## Build the Amiga ELF

From WSL inside the project root. The m68k toolchain (`m68k-amiga-elf-gcc`,
`-as`, and `elf2hunk`) ships with the bartman VS Code extension and is **not
on PATH** by default — add it:

```bash
cd /mnt/c/Projects/AI/SWOS/SWOS_C
TCDIR=/mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-1.8.2/bin/linux
export PATH="$TCDIR/opt/bin:$TCDIR:$PATH"   # opt/bin = gcc/as, $TCDIR = elf2hunk

rm -f obj/*.d   # see note below — required when the last build was on Windows
make            # produces uae/dh0/data/SWOS2 (hunk) + SWOS2.elf (gdb)
```

`SWOS2.elf` is what gdb loads for symbols. The hunk-format `SWOS2`
binary is what FS-UAE actually runs (the build emits both from one
ELF via `elf2hunk`).

**Why `rm obj/*.d`:** the auto-generated dependency files embed the *absolute
path of the build host*. A prior **Windows** build writes `C:/...` (gcc) and
`C:\...` (asm, backslashes) into `obj/*.d`; WSL `make` then dies with
`obj/X.d:1: *** target pattern contains no '%'` (the `C:` colon looks like a
rule separator). Deleting the `.d` files sidesteps it — the `.o` files stay, so
make falls back to **mtime-based incremental** (only changed `.c`/`.s` recompile)
and regenerates `.d` with this host's paths. Don't try to `sed` the paths: the
asm `.d` uses backslashes that collide with make's line-continuation `\`.
(A Windows build self-heals the same way next time it runs.)

---

## One-time: confirm Linux FS-UAE works under WSLg

```bash
cd /mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-1.8.2/bin/linux/fs-uae
LD_LIBRARY_PATH=. ./fs-uae --help | head -3
```

If a window opens or the help text prints, you're good. WSLg
provides `$DISPLAY` automatically — no Xserver setup needed.

---

## Run + attach: the reliable one-command path (use this)

```bash
bash /mnt/c/Projects/AI/SWOS/SWOS_C/amiga_gdb.sh up
```

`amiga_gdb.sh up` does the whole dance deterministically and prints `OK`
(or `FAIL`) at the end. As of 2026-06-10 it's reliable: **4/4 cold boots
HEALTHY, ~6s each, attempt 1** (was intermittently DEGRADED). The flow now
mirrors how the VSCode bartman extension attaches:

1. tears down any stale FS-UAE / gdb / daemon,
2. launches FS-UAE **directly** (no `grep` pipe — that pipe caused SIGPIPE
   early-exits; VSCode spawns it directly too),
3. starts the `gdb_proxy.py` daemon **immediately** — no log-scraping, no
   settle. gdb's `target remote` (with a 90s connect/remote timeout) grabs
   the stub the instant it's ready, exactly like VSCode's `target remote
   localhost:2345` right after spawn,
4. **health-checks** the attach (target stopped at SIGTRAP, PC == the SWOS2
   entry `0x219b10`),
5. if DEGRADED, does a full restart and retries (up to `ATTACH_RETRIES=4`),
   and fails FAST if FS-UAE crashes on boot.
6. **skips the loader intro** for a fast boot-to-menu (~4s vs the full intro):
   after a healthy attach it installs a gdb breakpoint at `ShowLoaderIntroSequence`
   with `commands { silent; return; continue }`, so the next `continue`
   force-returns past the intro. (A gdb-set `_skipLoaderIntro` global can't be used
   on the Amiga — it is only read there, so the m68k LTO/--gc-sections drops it;
   the SDL build seeds that same global from `SWOS_HARNESS_TURBO` instead.)

**Why the 90s timeout matters (the old flakiness, root-caused against
`barto_gdbserver.cpp`):** the stub opens the LISTEN port at *startup* but only
`accept()`s + handshakes when the `:SWOS2` trigger fires (when SWOS2 actually
launches — several to tens of seconds into a cold boot). gdb TCP-connects early
and BLOCKS on the handshake until that trigger. The old 20s `remotetimeout` was
sometimes shorter than a slow boot's time-to-SWOS2, so gdb gave up first →
DEGRADED. 90s outwaits any boot. No FS-UAE change needed. (The old
`wait_for_trigger` log-scrape + `SETTLE` are retired; those env vars are inert.)

Then drive gdb as usual and check the live verdict any time:

```bash
python3 /mnt/c/Projects/AI/SWOS/SWOS_C/gdb_proxy.py cmd "info registers"
python3 /mnt/c/Projects/AI/SWOS/SWOS_C/gdb_proxy.py health    # OK / DEGRADED
bash    /mnt/c/Projects/AI/SWOS/SWOS_C/amiga_gdb.sh status     # HEALTHY / DEGRADED / DOWN
bash    /mnt/c/Projects/AI/SWOS/SWOS_C/amiga_gdb.sh down       # tear everything down
```

The daemon also prints its attach verdict to `/tmp/swos_gdb_daemon.log`
(`[daemon] attach health: OK|DEGRADED (...)`), so a bad attach is never
silent. Tunables are env vars (see the top of `amiga_gdb.sh`):
`ATTACH_RETRIES`, `BOOT_TIMEOUT`, `SETTLE`, `PORT`, `ENTRY_PC`.

### Stop notifications (hands-free `watch`)

`gdb_proxy.py watch` emits one line every time the target transitions into a
clean stop — a breakpoint, panic, signal, or the initial entry pause — so you
never have to hand-poll `health` or be told "I loaded the file". It's
mode-generic (the amiga FS-UAE remote and the sdl native inferior both report
stopped/running now) and non-draining, so the gdb output buffer stays intact
for a follow-up `cmd`/`poll`. Pipe it into any notifier:

```bash
# one line per stop, with function + file:line (Ctrl-C / kill to stop):
python3 /mnt/c/Projects/AI/SWOS/SWOS_C/gdb_proxy.py watch
#   STOP OK stopped PC=0x259ebc | setCupPickList_Europe career_season.c:1148

python3 .../gdb_proxy.py watch --once                  # exit after first stop
python3 .../gdb_proxy.py watch --interval 1 --no-frame # faster, PC only
```

The manual steps below are the underlying mechanism (kept for reference /
debugging the orchestrator itself).

## Reliability hardening (2026-06-28)

Three changes removed the failure modes that made driving — especially the Amiga
— flaky. None change the command interface; existing `cmd`/`waitstop`/`health`
calls behave the same except where noted.

1. **Mode-aware drive timeouts** (`swos_plugin.py` `drive_transition` /
   `drive_inseason`). A single cycle-exact 68020 `continue` through the
   season-transition screens runs far longer than a native SDL frame, so the old
   SDL-tuned 14s per-continue timeout falsely declared the drive "stuck" while
   the Amiga was still progressing. The per-continue timeout is now chosen by
   target — 14s/25s on SDL, **90s on Amiga** (pass `timeout=` to override).

2. **Daemon busy-guard** (`gdb_proxy.py`). The daemon tracks a running-state flag
   (set when a command resumes the target / returns no prompt; cleared the
   instant any stop signature — breakpoint / signal / `(gdb)` prompt — surfaces
   in gdb's output, including an external SIGINT, via the reader thread). Two
   effects:
   - `HEALTH` while the target runs returns `DEGRADED target running (busy)`
     **instantly** instead of blocking ~6s on `info program` (which hangs on a
     running all-stop target — the old "DEGRADED info program hung"). So the
     "DEGRADED while running is NORMAL" caveat below is now reported cleanly
     rather than as a hang, and `is_stopped()` no longer stalls.
   - **Read-probes** (`print`/`p`/`info`/`x`/`bt`/`where`/`list`/…) issued while
     the target runs are refused with `[BUSY: …]` instead of queueing inside gdb
     to race the eventual stop output (the single-socket wedge). Control / resume
     / `set` / breakpoint commands pass through (so `kill`/`continue` during a
     re-boot are never blocked). Use `waitstop` (or interrupt) to regain control,
     then probe.

3. **One-shot-port auto-recovery** (`swos_plugin.py` `amiga_up()` + `prime()`).
   On Amiga, `prime()` auto-recovers a DEFINITIVELY-dead session — no socket, no
   ping, or gdb reports the remote target gone ("not being run" / "Remote
   connection closed" / "Connection refused") — by calling the idempotent
   `amiga_gdb.sh up` (reuse-if-healthy, else a full FS-UAE restart for the burned
   one-shot port), instead of hard-failing the run. It is conservative: a
   merely-running target is LIVE and is never re-booted out from under a drive.
   `s.amiga_up(fresh=True)` forces a clean restart on demand.

## Run + attach: manual session (reference)

The order matters: **FS-UAE must be running and listening on its
debugger port before the proxy daemon starts.**

### 1. Launch FS-UAE (detached, displays a window via WSLg)

```bash
cd /mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-1.8.2/bin/linux/fs-uae && \
  setsid env LD_LIBRARY_PATH=. nohup ./fs-uae \
    --amiga_model=a1200 \
    --automatic_input_grab=0 \
    --hard_drive_0=/mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-1.8.2/bin/dh0 \
    --hard_drive_1=/mnt/c/Projects/AI/SWOS/SWOS_C/uae/dh0/data \
    --remote_debugger=20 \
    --remote_debugger_port=2345 \
    --remote_debugger_trigger=:SWOS2 \
    --ntsc_mode=0 \
    --kickstart_file=/mnt/c/Projects/Amiga/Kickstarts/KICK31_A1200.rom \
    --chip_memory=2048 \
    --fast_memory=8192 \
    > /tmp/fs-uae.log 2>&1 < /dev/null &
disown
```

Flag highlights:
- `--remote_debugger=20 --remote_debugger_port=2345` — enables the
  GDB remote stub on TCP `127.0.0.1:2345`.
- `--remote_debugger_trigger=:SWOS2` — FS-UAE pauses execution at
  the entry of any binary named `SWOS2` and waits for gdb to
  connect before proceeding. Without this, the boot would run
  past the entry point before you could set breakpoints.
- `setsid + nohup + disown + redirected stdio` — detach FS-UAE from
  the shell so it survives this terminal closing or background-task
  cleanup.

Wait 3-4 seconds, then sanity-check:

```bash
ss -ltn | grep :2345         # expect: LISTEN  127.0.0.1:2345
pgrep -af fs-uae | head -1   # expect: the running fs-uae process
```

### 2. Start the proxy daemon

```bash
setsid nohup python3 /mnt/c/Projects/AI/SWOS/SWOS_C/gdb_proxy.py daemon \
       --mode amiga --host 127.0.0.1 \
  > /tmp/swos_gdb_daemon.log 2>&1 < /dev/null &
disown
sleep 3
python3 /mnt/c/Projects/AI/SWOS/SWOS_C/gdb_proxy.py ping     # expect: pong
```

The daemon:
1. Spawns `m68k-amiga-elf-gdb -q --nx uae/dh0/data/SWOS2.elf`.
2. Issues `target remote 127.0.0.1:2345`. The target is paused at
   `_swos_c_start` / `main` (`0x00219b10`).
3. Listens on `/tmp/swos_gdb.sock`. Subsequent `gdb_proxy.py cmd …`
   calls round-trip commands over the socket.

If the daemon fails to attach, the log says so. Common causes:

| symptom | cause | fix |
|---|---|---|
| `Connection refused` | FS-UAE not running, or its debugger port closed after a previous gdb detach | restart FS-UAE — the stub is **one-shot per session** |
| daemon stays alive but `info program` hangs | gdb is at a malformed state from a prior `call` that triggered an inferior fault | restart both daemon AND FS-UAE |
| **daemon exits instantly, empty log, no `/tmp/swos_gdb.sock`** | (FIXED 2026-06 — was an argparse `dest` collision: the `daemon` subcommand's `--mode` overwrote the chosen subcommand, so `daemon_main` never ran. The subparser dest is now `cmd_name`.) If you see this again, run the daemon **in the foreground** to see the traceback: `python3 gdb_proxy.py daemon --mode amiga --host 127.0.0.1` | — |

### CRITICAL startup discipline (the one-shot port)

FS-UAE's `--remote_debugger` port is **one-shot per emulator session**.
Every gdb client connect→disconnect burns it. So:

1. Start FS-UAE **once**. Confirm `ss -ltn | grep :2345` shows LISTEN.
2. Start the daemon **once**, correctly, the first time. Do NOT do a
   throwaway foreground `timeout N python3 …daemon…` "just to test" — that
   connects and disconnects, **killing the port**, and the real daemon
   then can't attach. (This wasted a full session once.)
3. The daemon must stay **resident**. When launched as a background task,
   confirm `/tmp/swos_gdb.sock` exists and `gdb_proxy.py ping` returns
   `pong` before doing anything else. If the socket never appears, the
   daemon died — check the log / run it foreground, do NOT relaunch
   blindly (each attempt may burn the port).
4. If the port gets burned (daemon log shows attach failure, or
   `info program` says "not being run" with packet errors), there is no
   recovery except a **full restart**: `pkill -9 -x fs-uae`, confirm
   gone, relaunch FS-UAE, then the daemon. The `--remote_debugger_trigger
   =:SWOS2` makes FS-UAE pause when the SWOS2 binary launches, so launch
   the game from the Amiga Workbench AFTER the daemon is attached.

---

## Issuing gdb commands

### Send a command, get the response

```bash
python3 /mnt/c/Projects/AI/SWOS/SWOS_C/gdb_proxy.py cmd "<gdb command>"
```

`<gdb command>` is any normal CLI gdb input. The response is the
output up to gdb's next `(gdb) ` prompt, including any async events
(watchpoint trips, signals) that arrived before the command. Add
`--timeout N` for commands that may take a while (e.g. waiting for
a stop).

Frequently useful:

```bash
gdb_proxy.py cmd "info program"          # what's running, where stopped
gdb_proxy.py cmd "bt"                    # backtrace
gdb_proxy.py cmd "info registers"
gdb_proxy.py cmd "print/x someVar"
gdb_proxy.py cmd "x/16xw 0xADDR"         # hex-words dump
gdb_proxy.py cmd "break someFunc"        # bp on function entry
gdb_proxy.py cmd "break file.c:NNN"      # bp on line
gdb_proxy.py cmd "watch *(unsigned char *)0xADDR"
gdb_proxy.py cmd "info watchpoints"
gdb_proxy.py cmd "disable 1"             # turn off bp 1
gdb_proxy.py cmd "continue" --timeout 2  # resume; returns ~2s later (target left running)
```

### Wait for a halt (after `continue`)

A `cmd "continue"` does **not** return immediately — gdb's prompt
never comes back while the target runs, so the `cmd` blocks for its
whole timeout (default 10s) and then returns a `[TIMEOUT ...]` marker
followed by `Continuing.`. That marker is **expected, not an error**:
it just means the target is now running. Pass a short `--timeout` so
you get control back quickly, then `waitstop` to block until it stops:

```bash
gdb_proxy.py cmd "continue" --timeout 2  # sends continue, returns ~2s later (running)
gdb_proxy.py waitstop 300                 # BLOCKS until a breakpoint/watchpoint/signal
                                          # hits, then prints the stop info. 300s cap.
```

`waitstop` watches gdb's async output for a stop signature
(`Breakpoint N,` / `received signal` / `Watchpoint N`) and returns
the instant one appears — no polling, no `info program` hang. This
is the reliable "set breakpoint → continue → get notified when it
hits" path. A `kill -INT` on the gdb process (interrupt) also makes
it return (the target stops with SIGINT). On timeout it returns a
`[WAITSTOP timeout]` marker (target still running).

NOTE: `waitstop` blocks the single-socket daemon for its duration —
that's intended (you're waiting on one breakpoint). Don't fire other
`cmd`/`poll` calls in parallel; they queue until it returns.

`poll` is the older inactivity-based variant — returns whatever's
buffered once output quiets (~300 ms). Prefer `waitstop` for
breakpoints; use `poll` only to scrape already-buffered output:

```bash
gdb_proxy.py poll 10        # wait up to ~10 s for output to settle, then return all of it
```

(`watch` polls HEALTH/`info program`, which HANGS while an all-stop
target is running, so it can miss a hit — `waitstop` supersedes it
for breakpoint waiting.)

### Interrupt a running target

`Ctrl-C` to gdb stops the target, but the proxy **cannot** deliver it
through the socket. `gdb_proxy.py raw $'\x03'` *looks* like it works —
it prints `[sent]` — but the byte never reaches gdb's controlling TTY,
so **the target keeps running** and a following `poll` returns nothing.
**Do not trust `raw $'\x03'` to interrupt** (confirmed 2026-06-10; the
`[sent]` is a false positive).

The reliable interrupt is `SIGINT` to the gdb process, then `waitstop`:

```bash
pkill -INT -f '[m]68k-amiga-elf-gdb'   # bracket [m] so pkill can't match its own line
gdb_proxy.py waitstop 8                 # catches the stop (prefer over `poll`)
```

The halt reports as `Program received signal SIGTRAP, Trace/breakpoint
trap` (not SIGINT) at wherever the target was — usually the
`_WaitForVSync` idle loop when sitting in a menu. `pkill -INT` (pattern
form) beats `pgrep -f m68k-amiga-elf-gdb | head -1; kill -INT $PID`
because you don't capture a PID — but you MUST use the `[m]` bracket, or
pkill matches its own argv and the SIGINT misses gdb.

### Read state after a manual in-game action (no breakpoint)

To inspect a global the game wrote in response to something *you* did in
the emulator (load a save, enter a menu) — no breakpoint needed, because
the data is static once written:

```bash
gdb_proxy.py cmd "continue" --timeout 2     # let the game run
#  ... drive the emulator by hand: load the file / open the menu ...
pkill -INT -f '[m]68k-amiga-elf-gdb'        # interrupt at the idle loop
gdb_proxy.py waitstop 8
gdb_proxy.py cmd "p someGlobal.field"       # read it (symbolic if the .elf has the type)
gdb_proxy.py cmd "p ((SomeType *)0xADDR)->field"   # or cast a raw address
gdb_proxy.py cmd "continue" --timeout 2     # resume so the emulator stays live
```

**Input gotcha — mouse desync (`--automatic_input_grab=0`):** FS-UAE is launched
WITHOUT host mouse capture (so the window doesn't trap your cursor). The
side-effect: moving the mouse over / out of the FS-UAE window injects Amiga mouse
events that desync SWOS's keyboard/joystick menu input — the selector freezes and
you can't navigate. It looks like the game hung, but the game is fine; only the
input is wedged. To drive by hand reliably: click ONCE into the window to give it
focus, then use the KEYBOARD ONLY (arrow keys + Enter/fire) and don't move the
mouse. If it's already wedged there's no in-emulator recovery — `amiga_gdb.sh
restart` for a clean boot and re-do the steps.

gdb knows the full struct types from `SWOS2.elf`, so prefer symbolic
access (`p diyCompetitionStart.matchData.cup.roundFormat`) over hand-
computed offsets. A literal-address cast (`(DIYCompetitionEx *)0x565ac4`)
is the fallback when the symbol is typed as something narrower (e.g.
`competitionFileBuffer` is declared `uint16_t`); `set $c = (T *)&sym`
convenience-variable casts sometimes fail with "not a structure pointer"
— use the literal-address cast inline instead.

---

## Runtime address quirks (READ THIS)

The bartman build uses `elf2hunk` to convert the ELF into Amiga
Hunk format. The hunk loader allocates `.BSSInChip.MEMF_CHIP` into
chip RAM **at runtime**, so:

- `info address frontbufferBase` returns the ELF VMA (often
  inside .text range) — **this is not where the buffer lives**.
- `print/x &frontbufferBase` likewise returns the ELF VMA.

To get the real chip-RAM address use a C variable that holds the
pointer at runtime, e.g. `frontbufferBasePtr`, or evaluate
expressions inside a function frame after the address has been
taken at runtime. Once a buffer's runtime address is known, gdb
pretty-printing recognises it (`x/4xw 0x127094` displays the line
as `<frontbufferBaseAlt>`).

---

## Watchpoints

GDB on m68k-amiga-elf supports hardware watchpoints via the remote
stub. `watch *(unsigned char *)0xADDR` watches a single byte for
**value change** — not every write.

Two consequences:

- If a writer stores the same byte value that's already there, the
  watch does **not** fire. To force a fire on the next write of
  any value, pre-seed the byte with a sentinel:
  ```
  gdb_proxy.py cmd "set *(unsigned char *)0xADDR = 0xff"
  gdb_proxy.py cmd "watch *(unsigned char *)0xADDR"
  ```
- Hardware watchpoints can be scarce (~1–4 active at once on m68k
  targets). If gdb falls back to a software watchpoint the target
  is single-stepped — orders of magnitude slower but still
  functional.

`awatch` triggers on read OR write (slower); `rwatch` triggers on
read only.

---

## Shutting down

**Don't disconnect gdb mid-session unless you intend to also
restart FS-UAE.** The FS-UAE remote-debugger port is one-shot per
emulator session — once the gdb client disconnects, the port stops
accepting new connections until FS-UAE is restarted.

Clean teardown:

```bash
python3 /mnt/c/Projects/AI/SWOS/SWOS_C/gdb_proxy.py stop      # quits gdb, frees socket
pkill -f "fs-uae --amiga"                                     # closes FS-UAE
```

---

## Troubleshooting

- **gdb prints `warning: SECTION.foo: No such file or directory`** —
  harmless, the ELF's `.debug_*` sections reference Windows-style
  source paths (`C:/Projects/...`) which Linux gdb can't open. Use
  `set substitute-path C:/Projects/AI/SWOS/SWOS_C/ /mnt/c/Projects/AI/SWOS/SWOS_C/`
  to give it the WSL path if you need `list`/source view to work.
- **`call (void*)memcpy(...)` hangs** — gdb's "call inferior
  function" requires the target to be at a clean stop point. If
  the inferior is on a signal frame or inside an ISR context the
  call can deadlock. Restart both daemon and FS-UAE.
- **Daemon hangs on a simple `print`** — usually the previous
  command left gdb buffer in a state the proxy can't reconcile.
  Restart the daemon (and accept that FS-UAE needs restart too).
- **WSL can't reach `127.0.0.1:2345`** — only happens when running
  the *Windows* FS-UAE binary (it binds to Windows loopback, which
  WSL2 can't see). Use the bundled Linux FS-UAE binary instead, as
  documented above.
- **FS-UAE window is blank / shows `[WARN:COPY MODE]` in the title bar, no
  emulation visible** — but `monitor screenshot` / `fb_capture.py` still return a
  correct frame, so FS-UAE and the game are fine; only the *window* is dead. Cause:
  WSLg's **weston** compositor has wedged into CPU-copy mode — typically after
  repeated FS-UAE boot→kill→boot **relaunch churn** in one session exhausts/wedges
  the vGPU surface sharing (launching once and leaving it up avoids it). Fix, from
  a **Windows PowerShell** (no full `wsl --shutdown` needed):
  ```
  wsl.exe --system sh -c "pkill -9 weston"
  ```
  weston lives in the WSLg **system** distro; killing it makes WSLg respawn it, and
  the next FS-UAE launch renders normally. (Confirmed working 2026-07-17.)
- **`up` fails every attempt: "trigger never appeared in /tmp/fs-uae.log"**
  (4 retries → FAIL), and gdb "suddenly takes forever / never worked
  before." The FS-UAE build's GDBSERVER log strings changed.
  `wait_for_trigger` keys on a string in the **native** log
  (`$HOME/FS-UAE/Cache/Logs/fs-uae.log.txt`) to know the `:SWOS2` trigger
  fired. The spam-fix rebuild (remote_debugger_barto, the one that removed
  the `g_async_queue` / `SKIPPED FRAME` stderr spam) **dropped the
  `GDBSERVER: Waiting for connection...` printf**; it now emits
  `GDBSERVER: listen() succeeded on 127.0.0.1:2345` instead. `amiga_gdb.sh`
  matches both now (fixed 2026-06-09). If a future rebuild changes it again,
  `grep GDBSERVER "$HOME/FS-UAE/Cache/Logs/fs-uae.log.txt"` to see the actual
  ready marker and add it to `wait_for_trigger`.
- **"DEGRADED" while the target is RUNNING is NORMAL, not broken.**
  `amiga_gdb.sh status` / `gdb_proxy.py health` probe with `info program`,
  which **hangs while an all-stop target runs** → reported as DEGRADED. Do
  NOT interpret it as a dead session and start rebooting. After `cmd
  continue`, use `waitstop` (or `watch`) and fire **no** other
  `cmd`/`status`/`health`/`poll` until it returns — concurrent commands wedge
  the single socket (this caused a long thrash once). Only query when stopped.
- **Teardown:** `amiga_gdb.sh down`. NEVER `pkill -f gdb_proxy` — the pattern
  matches the `pkill` command's own line and kills its shell (empty exit 1).
  Use a `[g]db_proxy` / `[f]s-uae` bracket pattern if you must pgrep/pkill.
- **Building the Amiga ELF under WSL: `obj/*.d:1: target pattern contains no
  '%'`.** The dependency files carry Windows paths (`obj/X.o: C:/Projects/...`)
  from a prior Windows build; the `C:` colon breaks GNU make. Rewrite the drive
  prefix (`sed -i 's#C:/#/mnt/c/#g' obj/*.d`) — they're regenerable artifacts,
  and the build stays incremental. Toolchain lives at
  `.../bartmanabyss.amiga-debug-*/bin/linux/{opt/bin,}` (gcc/as + elf2hunk);
  put both on PATH. `make` outputs straight to `uae/dh0/data/SWOS2{,.elf}`.

---

## SDL Linux workflow

The SDL port lives in `/mnt/c/cygwin64/home/headk/SWOS_C_SDL/`. It
builds an x86 Linux ELF (`bin/SWOS`, 32-bit by default — the `-m32`
flag keeps pointer/long widths matching the Amiga struct layouts).

### Build

```bash
cd /mnt/c/cygwin64/home/headk/SWOS_C_SDL
make clean      # only when crossing between Cygwin and WSL builds
make -j4        # produces bin/SWOS (Linux ELF, debug info included)
```

Requires `libsdl2-dev:i386` (for the 32-bit build). If you've been
building in Cygwin on Windows, `make clean` is needed before the
first WSL build — Cygwin .o files reference newlib symbols
(`__getreent`, `__locale_ctype_ptr`) that won't link with glibc.

### Run + attach

The SDL workflow is simpler than Amiga: gdb directly launches the
binary, no separate emulator step.

```bash
setsid nohup python3 /mnt/c/Projects/AI/SWOS/SWOS_C/gdb_proxy.py daemon \
       --mode sdl \
  > /tmp/swos_gdb_daemon.log 2>&1 < /dev/null &
disown
sleep 2
python3 /mnt/c/Projects/AI/SWOS/SWOS_C/gdb_proxy.py ping         # expect: pong
```

By default the SDL daemon uses `/mnt/c/cygwin64/home/headk/SWOS_C_SDL/bin/SWOS`
and runs gdb with that directory as cwd (so the inferior finds its
`data/`, `grafs/`, `sound/` subdirs). Override with `--bin
/path/to/SWOS` if needed.

At this point gdb is loaded with the binary but hasn't launched it
yet. Start the SDL process under gdb control:

```bash
gdb_proxy.py cmd "start"        # halts at main(), so you can set bps first
# or
gdb_proxy.py cmd "run"          # runs to completion / signal / bp
```

Then issue commands as usual:

```bash
gdb_proxy.py cmd "break _DrawMenu"
gdb_proxy.py cmd "continue"
gdb_proxy.py poll 10            # wait for halt
gdb_proxy.py cmd "bt"
```

### Differences vs Amiga mode

| Aspect | Amiga | SDL |
|---|---|---|
| gdb binary | bartman m68k-amiga-elf-gdb | host gdb (/usr/bin/gdb) |
| Target start | FS-UAE launches the binary, gdb attaches at trigger | gdb itself launches via `run` |
| Reconnect after `quit` | requires FS-UAE restart (one-shot port) | just restart the daemon |
| Hardware watchpoints | scarce (1–4) | abundant (per-process DR0–DR3 on x86) |
| Symbol→address (BSS) | ELF VMA ≠ runtime (hunk relocation) | ELF VMA == runtime |
| Window display | WSLg from FS-UAE | WSLg directly from SWOS process |

The runtime-address quirk (ELF VMA ≠ runtime for chip-RAM symbols)
is **Amiga-only**. On SDL `print/x &frontbufferBase` and
`print/x frontbufferBasePtr` give the same address.

### Shutting down

```bash
gdb_proxy.py stop               # quits gdb; SDL inferior dies with it
```

You can re-`daemon` immediately afterward — no emulator restart
needed.

---

## File layout

```
/mnt/c/Projects/AI/SWOS/SWOS_C/                     # Amiga port
├── gdb_proxy.py                                     # the proxy script (used by both modes)
├── uae/dh0/data/
│   ├── SWOS2                # hunk binary FS-UAE runs
│   ├── SWOS2.elf            # ELF gdb loads for symbols
│   └── SWOS2.map            # linker map (adjacent-symbol lookups)
└── GDB.md                   # this file

/mnt/c/cygwin64/home/headk/SWOS_C_SDL/               # SDL port
├── bin/SWOS                 # x86 Linux ELF the SDL daemon launches
├── data/ grafs/ sound/      # runtime assets (loaded relative to bin/)
└── Makefile
```

State files used by the proxy:

```
/tmp/swos_gdb.sock        # Unix socket the daemon listens on
/tmp/swos_gdb_daemon.log  # daemon stdio (gdb banner, attach output)
/tmp/fs-uae.log           # FS-UAE stdout/stderr (Amiga mode only)
```
