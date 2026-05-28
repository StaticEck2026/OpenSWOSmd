# SWOS — Pure C / SDL2 Port

A portable, pure C port of *Sensible World of Soccer* (Amiga, 1994), running
on top of SDL2 instead of Amiga hardware. The original 68k assembly has been
translated to C; the OCS/AGA chipset, Paula audio, and joystick I/O are
emulated by thin SDL2 shims.

This is one of two SWOS ports in flight by the same author:

- **`SWOS_C_SDL`** *(this repo)* — runs everywhere SDL2 runs (Windows, Linux,
  WSL2, macOS, etc).
- **`SWOS_C`** — same C codebase targeting real Amiga hardware. Fixes made
  here get back-ported there. Eventually `SWOS2.s` (the original 68k asm)
  will be retired and the Amiga build will run entirely from C.

## Goals

1. **Stabilise the SDL port first.** Get the full game working — match loop,
   career mode, save/load, transfer market, edit teams, all UI flows.
2. **Back-port to the Amiga C port** so both builds run from one C codebase.
3. **Untangle the Amiga BSS overlays.** The current code mirrors the Amiga's
   memory layout faithfully (adjacent-globals, `careerFileBuffer` overlays,
   `selTeamsPtr` aliased onto its own data). Once both ports run on C, those
   overlays become proper structs with named fields.
4. **Replace the save format.** The current `.CAR` format is a raw memory
   dump of the career-buffer overlay region — fragile and contains absolute
   pointers. Eventually replace with a structured format (JSON or tagged
   binary) that serialises named fields, not memory addresses.

## Status

Working:

- Full front-end navigation: NEW CAREER, CHOOSE TEAM, chairman memo flow,
  view competitions, management record, scorers, transfer market.
- Save / load `.CAR` career files (round-trips clean — magic-checked, layout
  preserved via shared backing buffer + `.equ` aliases).
- Friendly matches, career matches: pitch renders + scrolls, players animate,
  ball physics, AI, set-pieces.
- Crowd chants, in-match SFX (whistle, kicks, throw-ins).
- Substitutions (triple-tap or Q/W hotkey), tactics editing.
- 1-player keyboard, 1-player gamepad, 2-player gamepad.

Known issues (open):

- Player sprites: occasional palette / shape glitches.
- Ball sprite: sometimes invisible at edge of camera.
- Brief flicker (1–2 frames) on some menu transitions (e.g., post-match
  RESULT, lineup-swap).
- Amiga-side: hasn't been re-synced with recent SDL changes yet.

This is a personal project under active development; expect rough edges.

## You need the original Amiga data files

This repo contains the engine, **not the game data.** SWOS is still under
copyright and the data files (graphics, sound modules, team files,
fixtures) are not redistributed.

You need a legitimate copy of *Sensible World of Soccer* for Amiga (e.g., the
ADF/HDF you dumped from your own floppy or hard-disk install). Place the
following directories from the Amiga distribution into `bin/`:

```
bin/
├── data/       (TEAM.000…TEAM.0FE, POOLPLYR.DAT, EUROCUP.TMD, …)
├── grafs/      (4-bitplane OCS sprites)
├── grafs_aga/  (8-bitplane AGA sprites)
└── sound/      (HERO.SNG, MENU.SNG, SFX.SNG/IN1/IN2, …)
```

`save/` is created on first save.

## Building

The Makefile auto-detects the host and produces a binary named per the
target architecture. Pass an arch flag for cross builds.

| Target                                 | Command                | Output                |
|----------------------------------------|------------------------|-----------------------|
| Linux x86 (default, 32-bit)            | `make`                 | `bin/SWOS`            |
| Linux ARM64 (aarch64) — Pi 4/5 etc.    | `make ARM64=1`         | `bin/SWOS.aarch64`    |
| Linux ARM32 (armhf, armv7-a + NEON)    | `make ARM=1`           | `bin/SWOS.armhf`      |
| Windows                                | `make` on MSYS2/Cygwin | `bin/SWOS.exe`        |
| macOS (native, single-arch)            | `make`                 | `bin/SWOS.macos`      |
| macOS universal binary (x86_64 + arm64)| `make macos-universal` | `bin/SWOS.macos`      |

Common flags:
- `make clean` first if you've switched between archs in the same checkout.
- `make SWOS_DEBUG=1` adds the diagnostic-logging build (autoskip / sfx).
- `make -j4` (or higher) parallelises the compile.

### Linux x86 (32-bit, default)

Default target. Compiles with `-m32` so the Amiga `long`/pointer struct
sizes match the original 68k layout on a 64-bit host. Needs the 32-bit
SDL2 + multilib gcc:

```
# Debian / Ubuntu
sudo apt install gcc-multilib libsdl2-2.0-0:i386 libsdl2-dev:i386

# Arch
sudo pacman -S lib32-sdl2
```

```
make
./bin/SWOS
```

### Linux ARM64 (Raspberry Pi 4 / 5, aarch64 server, etc.)

```
# Native on the Pi (Raspberry Pi OS 64-bit)
sudo apt install build-essential libsdl2-dev
make ARM64=1
./bin/SWOS.aarch64
```

Cross-building from an x86_64 host is a bit more involved — enable the
foreign architecture and install both the cross-compiler and arm64 SDL2:

```
sudo dpkg --add-architecture arm64
sudo apt update
sudo apt install gcc-aarch64-linux-gnu libsdl2-dev:arm64

make ARM64=1
```

The Makefile picks `aarch64-linux-gnu-gcc` and reads SDL2 from the
matching pkgconfig directory. ARM64 is LP64 — no `-m32`.

### Linux ARM32 (armhf, Raspberry Pi 2 / 3, or Pi 4/5 in 32-bit mode)

Native on the Pi:

```
sudo apt install build-essential libsdl2-dev
make ARM=1
./bin/SWOS.armhf
```

Cross-build from an x86_64 Debian/Ubuntu host needs the armhf foreign
arch enabled. Ubuntu's default sources don't carry armhf packages, so
you also have to point apt at `ports.ubuntu.com` (Debian users: skip,
`deb.debian.org` has all archs):

```
sudo dpkg --add-architecture armhf

# Restrict your existing amd64 sources so apt doesn't try ports for them.
sudo sed -i -E 's,^(deb )(http),\1[arch=amd64] \2,' /etc/apt/sources.list

# Add armhf sources. You need ALL FOUR pockets — main universe +
# -updates + -security + -backports — otherwise libsdl2-dev:armhf
# pulls in an old libc6:armhf and refuses to install. Replace 'jammy'
# with your Ubuntu codename (`lsb_release -cs`; jammy = 22.04, noble = 24.04).
CODENAME=$(lsb_release -cs)
sudo tee /etc/apt/sources.list.d/ports-armhf.list <<EOF
deb [arch=armhf] http://ports.ubuntu.com/ubuntu-ports/ $CODENAME main universe
deb [arch=armhf] http://ports.ubuntu.com/ubuntu-ports/ $CODENAME-updates main universe
deb [arch=armhf] http://ports.ubuntu.com/ubuntu-ports/ $CODENAME-security main universe
deb [arch=armhf] http://ports.ubuntu.com/ubuntu-ports/ $CODENAME-backports main universe
EOF

sudo apt update
sudo apt install gcc-arm-linux-gnueabihf libsdl2-dev:armhf

make ARM=1
```

Compiles for `armv7-a` with NEON (covers Pi 2 / 3 / 4 / 5 in 32-bit
mode). armhf is already ILP32 so no `-m32`. Override the toolchain
prefix with `make ARM=1 CC=...` if your distro uses a different one.

### Windows

Build with MSYS2 / MinGW32 (gcc 32-bit), Cygwin 32-bit, or cross-compile
from Linux with `i686-w64-mingw32-gcc`. The Makefile auto-detects the
toolchain and emits `bin/SWOS.exe`.

For end users we ship the SDL2 DLL alongside the executable so no
external SDL2 install is needed — just unzip and run `SWOS.exe`.

### WSL2

The default `make` builds a Linux x86 binary that runs under WSL2 with
WSLg (or an X server). Audio uses queue mode (not callback) because
WSL2 + PulseAudio has uneven scheduling on the SDL audio callback
thread; the queue path is paced from the main game loop and is stable.

### macOS

Builds a Mach-O binary using clang / `cc`. Supports both Intel
(`x86_64`) and Apple Silicon (`arm64`) natively, and can produce a
universal binary that runs on both. Default deployment target is
macOS 11 (Big Sur).

```
# Single-arch native build
brew install sdl2 pkg-config
make
./bin/SWOS.macos

# Universal binary (x86_64 + arm64)
# Requires SDL2.framework from libsdl.org (universal, drop into /Library/Frameworks/)
make macos-universal
```

Full details — SDL2 framework setup, deployment-target overrides,
.app bundle layout, troubleshooting — are in
[`docs/MACOS_BUILD.md`](docs/MACOS_BUILD.md).

## Running

Launch the binary from anywhere — the program automatically `chdir()`s to
its own directory so `data/`, `grafs/`, `sound/`, and `save/` resolve
correctly. `stderr` is auto-redirected to `SWOS.log` next to the binary
(set `SWOS_NO_LOG=1` in the environment to disable).

### Settings file (`SWOS.ini`)

Display + audio settings persist between runs in `SWOS.ini`, written
next to the binary. The file is created on first clean shutdown if it
doesn't exist; missing keys fall back to the hard-coded defaults.

Resolution order at launch is: **defaults → SWOS.ini → command-line
flags** — so a flag always wins over the file, but a value you only
set in the file (or via runtime keys like `+`/`-` and Alt+Enter) is
remembered.

```ini
[video]
renderer=accel        ; software | accel
vsync=off             ; on | off
fullscreen=off        ; off | desktop | exclusive
scale=3               ; 1..4 (windowed only)

[audio]
mode=mono             ; mono | stereo
```

### Command-line options

```
-f, --fullscreen          Shorthand for --fullscreen=desktop. Off by
                          default. Toggle at runtime with Alt+Enter.
--fullscreen=<desktop|exclusive|off>
                          Fullscreen mode at startup. Off by default.
                          'desktop' = borderless letterbox over the
                          current desktop, no resolution change. Fast
                          to toggle; on Windows XP the taskbar can
                          occasionally show through this borderless
                          window.
                          'exclusive' = true fullscreen with a display-
                          mode change. Always covers the taskbar (the
                          XP fix). Slower toggle, may flicker on
                          multi-monitor rigs.
--scale=<1|2|3|4>         Windowed pixel scale at startup. Default: 3.
                          Adjust at runtime with the + and - keys.
--renderer=<software|accel>
                          Choose the SDL renderer. Default: accel.
                          'accel' uses the platform's hardware backend
                          (D3D / OpenGL / Metal). 'software' is a CPU-only
                          fallback for hosts where the hardware path
                          misbehaves.
--vsync=<on|off>          Sync frame presents to monitor vblank.
                          Default: off. Only meaningful with
                          --renderer=accel. Turn on if you see screen
                          tearing; the manual 50 Hz game-tick pace
                          runs as a backstop regardless.
--audio=<mono|stereo>     Audio output mode. Default: mono.
                          The Amiga's 4 audio channels are hard-panned
                          (1,2 → left, 0,3 → right) by Paula, which
                          sounds lopsided on most modern speakers /
                          single-speaker laptops. 'stereo' restores
                          the original Amiga panning if you'd rather
                          hear it that way (e.g. on headphones).
-h, --help                Show usage and exit.
```

#### Choosing a render mode

SWOS' game logic runs at a fixed 50 Hz (the original PAL VBlank rate).
How smooth that looks on a given monitor depends on whether each frame
present aligns with the display's refresh:

| Monitor refresh | Suggested flags                              | Result                                                                                       |
|-----------------|----------------------------------------------|----------------------------------------------------------------------------------------------|
| 50 Hz           | `--vsync=on`                                 | Perfect 1:1 pacing. Each game frame lands on a vblank.                                       |
| 60 Hz / 75 Hz   | default (`--vsync=off`) or `--vsync=on` if you see tearing | Manual 50 Hz pace gates the game; the 50 → 60 pulldown is inherent.              |
| 144 Hz / 240 Hz | default                                      | Same as above; the manual pace fills the slack between vblanks.                              |
| Buggy hardware backend | `--renderer=software`                  | Drops to the CPU path. No monitor sync, manual pace only.                                    |

## Controls

### Menus

| Action       | Keyboard           | Gamepad (any pad)    |
|--------------|--------------------|----------------------|
| Navigate     | Arrow keys         | D-pad / left stick   |
| Select       | Right Ctrl / Space / Left Ctrl / Enter | A or B button |
| Quit         | Esc (only at the top-level main menu) or window close (✕) | — |

### In-match (Player 1)

| Action       | Keyboard           | Gamepad 1            |
|--------------|--------------------|----------------------|
| Move         | Arrow keys         | D-pad / left stick   |
| Kick / shoot / pass / tackle | Right Ctrl / Space / Left Ctrl / Enter | A or B |
| Call substitution menu | Triple-tap any direction OR `Q` (away) / `W` (home) | Triple-tap |
| Pause / unpause | `P`              | —                    |

### In-match (Player 2)

Player 2 is **gamepad-only** (the keyboard stays single-player so the
arrow keys don't get hijacked when a P2 pad is plugged in but unused).

| Action       | Gamepad 2          |
|--------------|--------------------|
| Move         | D-pad / left stick |
| Kick / etc.  | A or B             |

Two pads connected at startup map to P1 / P2 in enumeration order. Pads can
be hot-plugged; new pads fill the lowest empty slot.

### Function keys

| Key          | Action                                                           |
|--------------|------------------------------------------------------------------|
| Alt + Enter  | Toggle fullscreen / windowed (uses the launch `--fullscreen` flavour) |
| `+` / `-`    | Increase / decrease windowed scale (1× → 2× → 3× → 4×). Ignored while fullscreen. |
| F9 / F11 / F12 / PrintScreen | Save screenshot to `bin/swos_shotNNN.png`            |

### Gamepad not detected?

SDL only drives gamepads it has a mapping for. If your pad isn't in SDL's
built-in DB (Xbox, PS4/5, Switch Pro, most Logitechs are covered), download
[`gamecontrollerdb.txt`](https://github.com/gabomdq/SDL_GameControllerDB)
and drop it next to the binary — it'll be picked up at startup.
`SWOS.log` lists every detected joystick and whether SDL has a mapping for
it.

## Project layout

```
src/                         — port source
    SWOS.c                   — (start of the Amiga C entry tree)
    main.c                   — SDL2 entry point
    platform_sdl.c           — window, render, vblank pacing
    audio_sdl.c              — SDL2 audio backend
    rjp1.c                   — RJP1 (Richard Joseph Player) module decoder
    input_sdl.c              — keyboard + gamepad → joy0Dat / joy1Dat / CIAA
    input.c                  — Amiga-side joystick decode (unchanged)
    game_loop.c              — main match loop
    menu_system.c            — packed-menu interpreter
    menus.c, menu_data.c     — every menu, every callback
    transfer_market.c        — buy / sell / offers
    career_*.c               — career mode (init, season, teams, eurocups)
    diy_competition.c        — custom league/cup runner
    pitch.c, scroll_blit.c   — pitch tile rendering and scrolling
    image_cmd.c              — Amiga sprite/image command stream decoder
    swos_globals*.c          — shared backing buffers + `.equ` aliases for
                               adjacent-globals BSS runs that the original
                               Amiga code reads as one struct
bin/                         — runtime location for assets + binary
    data/, grafs/, sound/    — original Amiga data (NOT in this repo)
    save/                    — career save files (.CAR)
    SWOS / SWOS.exe          — the built binary
.claude/CLAUDE.md            — engineering conventions for contributors
```

## Architecture notes

A few non-obvious conventions ([`.claude/CLAUDE.md`](.claude/CLAUDE.md) has
the long version):

- **Graphics: 8bpp chunky end-to-end.** The Amiga 4/8-bitplane decode runs
  once at sprite load time, not per blit.
- **Endian helpers (`src/endian_helpers.h`).** Use `BE_READ_U16` /
  `PACK_BE_U16` / `BE32_TO_HOST` etc. for any read of Amiga-authored
  data — they're no-ops on Amiga BE, byte-swap on x86 LE, and grep-able.
- **Adjacent-globals BSS.** The Amiga relies on neighbouring globals being
  contiguous (e.g., `MENU_CMD_SPRITE_COPY` reads three labels as one
  36-byte struct). On SDL the linker scatters them, so we restore the
  layout via shared backing buffers + `.equ` aliases. This is a temporary
  scaffold (step 3 of the roadmap).
- **Don't patch out panics.** `_PanicHaltLoop` / panic-reason codes mean an
  invariant was violated — the previous setup step left state corrupt.
  Trace upward and fix the cause; don't add a `if (!ptr) return;` to make
  the menu draw work.

## Credits

- **Sensible Software** — original SWOS (1994).
- **Ben Baker** ([github.com/benbaker76](https://github.com/benbaker76)) —
  reverse-engineered the SWOS Amiga 68k assembly that this C port is built
  from, and author of the Amiga C port (`SWOS_C`) and the SDL C port
  (`SWOS_C_SDL`).
- **Zlatko Karakaš** ([github.com/zlatkok](https://github.com/zlatkok/)) —
  reverse-engineered the original DOS version.
- **Richard Joseph** — RJP1 audio engine.
- **gabomdq** & contributors — SDL_GameControllerDB.
