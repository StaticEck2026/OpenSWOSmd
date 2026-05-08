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

### Linux

You need a 32-bit SDL2 build. The codebase is compiled with `-m32` so the
Amiga `long`/pointer struct sizes survive on a 64-bit host (this is one of
the things step 3 of the roadmap will eventually undo).

```
arch linux:
sudo pacman -S lib32-sdl2
debian/ubuntu:
sudo apt install libsdl2-2.0-0:i386
```

You'll also need 32-bit gcc + headers (`gcc-multilib` on Debian/Ubuntu;
already present in stock Arch).

Build:

```
make
./bin/SWOS
```

### Windows

Build with MSYS2 / MinGW32 (gcc 32-bit), Cygwin 32-bit, or cross-compile from
Linux with `i686-w64-mingw32-gcc`. The Makefile auto-detects the toolchain
and emits `bin/SWOS.exe`.

For Windows users we ship the SDL2 DLLs alongside the executable so no
external SDL2 install is needed — just unzip and run `SWOS.exe`.

### WSL2

Works under WSL2 with WSLg or an X server. Audio uses queue mode (not
callback) because WSL2 + PulseAudio has very uneven scheduling on the SDL
audio callback thread; the queue path is paced from the main game loop and
is stable.

### ARM 32-bit (Raspberry Pi etc.)

Cross-build to armhf from an x86_64 Debian/Ubuntu host. First enable
the foreign architecture and update the package index — Ubuntu's
default sources for `amd64` don't include armhf packages, so you
also need to point `apt` at `ports.ubuntu.com`:

```
sudo dpkg --add-architecture armhf

# Add an armhf source pointing at ports.ubuntu.com (which actually
# carries arm packages). Replace 'jammy' with your Ubuntu codename
# (jammy = 22.04, noble = 24.04). On Debian use deb.debian.org as
# normal — Debian's main mirrors cover all release architectures.
echo 'deb [arch=armhf] http://ports.ubuntu.com/ubuntu-ports/ jammy main universe' \
  | sudo tee /etc/apt/sources.list.d/ports-armhf.list

# Restrict the existing amd64 entries so apt doesn't try ports for them.
sudo sed -i -E 's,^(deb )(http),\1[arch=amd64] \2,' /etc/apt/sources.list

sudo apt update
sudo apt install gcc-arm-linux-gnueabihf pkg-config-arm-linux-gnueabihf libsdl2-dev:armhf

make ARM=1
```

The Makefile picks `arm-linux-gnueabihf-gcc` and
`arm-linux-gnueabihf-pkg-config`, builds for `armv7-a` with NEON (covers
Pi 2 / 3 / 4 / 5 in 32-bit mode), and skips the `-m32` flag because
armhf is already a 32-bit ABI. Override with `make ARM=1 CC=...` if
your distro uses a different cross-compiler prefix. Pass `SWOS_DEBUG=1`
alongside to get the diagnostic loggers compiled in.

Building **on the Pi natively** is simpler — install the regular
`build-essential libsdl2-dev` packages and run `make ARM=1`. The
ARM=1 flag still picks the right `-march=armv7-a -mfpu=neon` flags
and skips `-m32`, but uses the native gcc.

## Running

Launch the binary from anywhere — the program automatically `chdir()`s to
its own directory so `data/`, `grafs/`, `sound/`, and `save/` resolve
correctly. `stderr` is auto-redirected to `SWOS.log` next to the binary
(set `SWOS_NO_LOG=1` in the environment to disable).

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
- **Tomasoft** — `SWOS_PC` (the DOS port and disassembly), invaluable as a
  reference for the C port.
- **Richard Joseph** — RJP1 audio engine.
- **gabomdq** & contributors — SDL_GameControllerDB.

## Licence

The port code in `src/` is licensed under the same terms as the
SWOS-disassembly references it derives from. The original SWOS data files
are **not** included and remain Sensible Software's copyright — bring your
own.
