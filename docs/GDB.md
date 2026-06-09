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

From WSL inside the project root:

```bash
cd /mnt/c/Projects/AI/SWOS/SWOS_C
make            # produces uae/dh0/data/SWOS2 (hunk) + SWOS2.elf (gdb)
```

`SWOS2.elf` is what gdb loads for symbols. The hunk-format `SWOS2`
binary is what FS-UAE actually runs (the build emits both from one
ELF via `elf2hunk`).

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
(or `FAIL`) at the end:

1. tears down any stale FS-UAE / gdb / daemon,
2. launches FS-UAE,
3. waits for the `Breakpoint 20 triggered` SWOS2-entry line in the log,
   then settles,
4. starts the `gdb_proxy.py` daemon,
5. **health-checks** the attach (target stopped at SIGTRAP, PC == the
   SWOS2 entry `0x219b10`), and
6. if the attach came up DEGRADED (`not being run` / no registers — the
   FS-UAE stub's racy one-shot port), does a **full restart and retries**
   (up to `ATTACH_RETRIES=4`). A degraded attach is the historical
   flakiness; auto-restart is the only real recovery for the one-shot port.

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
gdb_proxy.py cmd "continue"              # resume target (returns immediately)
```

### Wait for a halt (after `continue`)

`continue` returns immediately because gdb's prompt doesn't come
back until the target stops. To **block until the target stops and
be alerted the moment a breakpoint fires**, use `waitstop`:

```bash
gdb_proxy.py cmd "continue"     # returns immediately (target running)
gdb_proxy.py waitstop 300       # BLOCKS until a breakpoint/watchpoint/signal
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

`Ctrl-C` to gdb stops the target. The proxy can't send a raw
control character through its socket request format reliably, so
the workaround is `SIGINT` on the gdb process:

```bash
GDB_PID=$(pgrep -f m68k-amiga-elf-gdb | head -1)
kill -INT $GDB_PID
sleep 1
gdb_proxy.py poll 2         # collect the "Program received signal SIGTRAP" line
```

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
