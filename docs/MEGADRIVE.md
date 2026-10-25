# The Sega Mega Drive / Genesis target

Status: **current** (2026-10-25). The cartridge plays friendlies of the Amiga game's own
match engine at full speed on NTSC and PAL consoles, one or two players, with the
statistics board, the bench and substitutions, instant replays and PSG sound. Its art is
the converter's stand-in drawings unless it is built from the player's own SWOS data
(below). Tested in Genesis Plus GX (the headless harness); not yet on real hardware.

## Building and running

```bash
make -f Makefile.md -j$(nproc)        # dist/SWOS-MD.bin (a plain .bin: any emulator, a flash cart)
make -f Makefile.md test              # scripts/md/test_md.sh: attract mode NTSC+PAL, the menus
make -f Makefile.md harness           # build/md/harness/{mdrun, genesis_plus_gx_libretro.so}
```

Toolchain: any m68k gcc. A distro's `m68k-linux-gnu-gcc` (Debian/Ubuntu
`gcc-m68k-linux-gnu`) works -- no libc and no libgcc are linked (`src/md/md_libgcc.s` has
the 68000-safe arithmetic helpers), so its Linux userland is never used. `MD_PREFIX=m68k-elf-`
for SGDK's or marsdev's. Python 3 with Pillow for the generators.

The ROM is 384 KB with the stand-in data; the header (`src/md/md_crt0.s`) says
"SEGA MEGA DRIVE", region JUE, and `scripts/md/fix_header.py` writes the ROM end and
checksum.

## What the player sees

- **Title** (SENSIBLE WORLD OF SOCCER, PRESS START, the build stamp). After 20 s idle it
  plays a computer-v-computer match (the attract mode); any button leaves it.
- **Main menu**: FRIENDLY, WATCH A MATCH, OPTIONS, CONTROLS.
- **FRIENDLY**: per side a country, a team in it, and who plays it (PLAYER 1, PLAYER 2,
  COMPUTER), with a drawing of the team's kit; PLAY MATCH.
- **OPTIONS**: game length (3/5/7/10 minutes: the engine's `_gameLength`), pitch (seasonal
  by month, random, or frozen..hard), auto replays, sound, and on an NTSC console the game
  speed: 50 Hz (the Amiga's speed, the default) or 60 Hz.
- **The match**: the engine's, unchanged -- pre-match, both halves, the half- and full-time
  boards, replays, the bench.
- **FULL TIME** (or ABANDONED when a pad left early): the score and the statistics.

Controls in a match: d-pad runs; A, B or C are the Amiga's one fire button; START pauses;
X instant replay; Y the statistics; MODE, or START+A, leaves the match. The bench is the
Amiga's: tap a direction three times at a stoppage. A three-button pad plays the match; X,
Y and MODE need a six-button pad. Pad 1 is the Amiga's joystick port (player 1).

## How it is put together

One source tree, as for the other targets (CLAUDE.md). `Makefile.md` compiles a list of
the shared engine's files from `src/` (`ENGINE_SRCS`: the match, its data tables, the
statistics, the bench, the highlights' instant-replay ring) with `-DSWOS_MD_BUILD`, and the
platform layer in `src/md/`. Nothing outside `src/md/` is compiled for the Mega Drive only;
the shared files carry a few `#ifdef SWOS_MD_BUILD` branches (below), each a no-op for the
SDL and Amiga builds.

| file | does |
|---|---|
| `md_crt0.s`, `md.ld` | vectors, ROM header, TMSS, RAM clear, `.data` copy; the memory map |
| `md_vdp.c`, `md_hw.h` | VDP set-up, VRAM/CRAM/VSRAM writes, the VBlank DMA queue; the VRAM layout |
| `md_pad.c` | 3- and 6-button pads |
| `md_isr.c` | the VBlank interrupt, the clock, the pads into the Amiga's joystick registers, `GetKey` |
| `md_match.c` | engine boot (what `mainC` does), the FRIENDLY menu's PLAY (`InitAndPlayGame`) |
| `md_engine_glue.c`, `md_engine_vars.c`, `md_assets.c` | what the engine calls that a cartridge does its own way (menus, music, the team cache, files) |
| `md_display.c` | the camera, `DrawSprites`' cull into a list of sprites, `SortSprites` |
| `md_render.c` | the pitch on plane B, the sprites, the HUD, the palettes |
| `md_overlay.c` | what the engine draws into the frame: text, the darkened grid, boxes, faces |
| `md_fade.c` | the Amiga's fade state machine, as palette levels |
| `md_sound.c` | the engine's sound calls on the PSG |
| `md_frontend.c` | the title, menus and result screen |
| `md_debug.c`, `md_libc.c`, `md_libgcc.s` | the log ring, the panic screen, mem/str/printf, 68000 arithmetic |

Shared-file branches for `SWOS_MD_BUILD`: `include/display.h` (no AGA: the OCS 16-colour
game), `include/swos_sections.h` (no Chip RAM), `src/input.c` (the joystick registers are
globals), `src/game_loop.c` (the frame's end is `MD_FrameDone`, a pause tick `MD_WaitTick`),
`src/camera.c` (`ScrollToCurrent`), `src/animated_patterns.c` (the crowd's tiles in a
compact slot table), `src/swos_globals.c` (smaller `_pitchDatBuffer`, header-only
`_hilFileBuffer`), `src/highlight.c` (goals are not archived as highlights: the replay
ring is kept), `src/statistics.c` (`DrawGrid` darkens one area), `src/build_info.c`
(the platform name). On the SDL side the export adds `src/md_export.c` (in `Makefile.amiga`'s
`SDL_ONLY_SRCS`), its option in `src/main.c`, its call in `mainC`'s Phase 9 (`src/SWOS.c`)
and `g_grafsOcsOnly` in `src/resload.c`.

### The clock

The Amiga's match runs at 50 Hz: its VBlank interrupt swaps the buffers, moves the camera
and counts the ticks the game reads. On the Mega Drive a match frame takes most of a
VBlank, so during a match **the frames own the clock**, as SDL netplay's
`NetplayMatch_OwnsClock` does: each frame's end (`MD_FrameDone`) does the interrupt's swap,
camera and tick inline and stages the frame; the 50 Hz tick shows the oldest staged frame
(two can wait). An NTSC console drops one VBlank in six to tick at 50 Hz (`md_clock60`
ticks every VBlank: a 20% faster game). Outside a match the interrupt keeps the clock.
`md_lagTicks` counts ticks with no frame to show: the game below full speed.

Measured (`make -f Makefile.md test`, 20,000 frames of attract mode): 0.1-0.2% of ticks
late, all at kick-offs; a match frame is ~78% of an NTSC VBlank of 68000 time
(`scripts/md/bench.sh`).

### The picture

The screen shows 320x224 (NTSC) or 320x240 (PAL) of the engine's 336x272 window, centred.
The engine still decides everything against its own window, so the match plays as on the
Amiga; the picture is a crop.

- **Plane B, the pitch**: 42x55 Amiga tiles of 16x16 as 84x110 VDP cells, a 64x32-cell ring
  that a camera move rewrites a column or row at a time. Crowd tiles the animation
  toggles are four cells rewritten.
- **Sprites**: each image is pre-cut by the converter into hardware sprite pieces with their
  tiles in ROM (`MdImage`); a 512-tile VRAM cache uploads an image the first frame it shows,
  within a DMA budget a frame. 80 sprites a frame, front to back.
- **Kits**: team 1's players draw with palette line 1, team 2's with line 2, each the game
  palette through that team's colour table (the remap the Amiga applies to the pixels); the
  shirt type picks one of three kit sheets. Line 3 is the reserves' table (the bench).
  One skin per team (`_useIndividualPlayerSkinColor = 0`, an Amiga option), as a palette
  line has room for one.
- **Plane A, the overlay** (`md_overlay.c`): the HUD's text, and what the engine blits into
  the frame -- `DrawMenuText`, `DarkenRectangle`/`DrawGrid`, `FillRectangle` -- as a grid of
  cells (a character over a dark or a solid background; such tiles are made on demand into
  a small VRAM cache), and `DrawSprite16Pixels`/`DrawSpriteInCameraView` as extra sprites
  over everything. So the MATCH STATS board and the bench's menus show. The engine's font is
  8 pixels a character here, wider than the Amiga's, so the substitutes menu's names are
  cramped. A board drawn the same frame after frame is only hashed (the "static" path).

VRAM: `md_hw.h` has the map (pitch tiles 0x0000-0x7FFF, sprite tiles 0x8000-0xBFFF, planes at
0xC000/0xE000, font, sprite table, scroll table).

### RAM

64 KB holds the engine's match state, the 19,000-byte instant-replay ring, the renderer's
two staged frames and the stack (0xC00 reserved; the link fails without it). The engine's
read-only tables that are declared without `const` are placed in ROM by name
(`scripts/md/rom_tables.txt` -> `gen_romdata_ld.py` -> a linker rule). A name there that
the engine writes would be a silent bug on a cartridge, so the harness counts writes into
ROM (`rom-writes:` in every `mdrun` run) and the test fails on any.

### Sound

The engine says what to play exactly as on the Amiga (a wave id on a channel, a preset,
channel volumes). With no samples on the cartridge, `md_sound.c` makes each on the PSG from
the 50 Hz tick: the whistle (tone 0), the ball (tone 1), the crowd's voice and the chants'
hum (tone 2), the crowd's murmur, swells, chants' beat and a kick's strike (noise). No
music.

## The game data

The original game's data is not in the repo (CLAUDE.md "Data"). `scripts/md/make_md_data.py`
builds `build/md/md_data.c` -- the sprite table, the kit sheets, the pitches and the teams,
already in the VDP's format -- from one of two sources:

- **The player's own data**: run OpenSWOS Setup with the Amiga disks (the export wants the
  Amiga's 16-colour `grafs/` set), then `bin/SWOS.x86_64 --md-export=DIR` (the SDL build;
  `src/md_export.c`, SDL only: it loads the match's image script once per kit sheet, the
  six pitch files and every database team, writes them and exits), then
  `make -f Makefile.md MD_EXPORT=DIR`. The run turns off the SDL build's preference for
  `grafs_aga/` (`g_grafsOcsOnly`, `src/resload.c`), and takes the sprites before
  `ConvertColorsAndTextSprites` recolours them and the pitches before
  `FixupAndColorPitchFile` colours the crowd -- the cartridge does both with palettes.
  Format: `sprites.bin` and `kit1..3.bin` ("MDSP": u16 count, then per image u16 index,
  i16 w, h, centreX, centreY and w*h palette indices), `pitch1..6.bin` ("MDPT": u16 tile
  count, the 16x16 tiles as 256 palette indices each, the 42x55 map of tile numbers, all
  big-endian) and `teams.bin` (684-byte `TeamFileEx` records as on the Amiga disk).
  `scripts/md/fake_export.py DIR` writes this format from the stand-in art (with the
  teams over three countries), which tests the `--export` path without anyone's data.
- **Stand-in art** otherwise: footballers drawn for every pose and direction the engine's
  animation tables name, a pitch drawn to the engine's line coordinates, sixteen made-up
  teams. The ROM builds and plays without anyone's data.

## Testing

`scripts/md/mdrun.c` is a headless libretro frontend over Genesis Plus GX (pinned commit,
`scripts/md/gpgx_harness.patch` adds the ROM-write log and a PC profiler): `--frames`,
`--pal`, `--input` (lines `frame port buttons`, buttons from `UDLRABCSXYZM`, `-` releases),
`--shot`/`--shot-every`, `--log 0xFF0000` (the ROM's `md_log` ring), `--peek ADDR:N`,
`--peek-every N`, `--wav`, `--profile FRAME:out.bin` (`scripts/md/profile.py`).

Test builds: `make -f Makefile.md EXTRA_CFLAGS=-DMD_TEST_HUMAN` goes straight into a match
with pad 1 on team 1, `-DMD_TEST_DEMO` into computer-v-computer matches
(`scripts/md/bench.sh`, `framehist.sh`, `perf.sh` use that one).

## Not done

- Competitions, careers, saves, the team editor, highlights saved after a match: the
  cartridge has friendlies only (no SRAM is used yet).
- Music, and the Amiga's own samples (the PSG makes its own).
- Real hardware: everything runs in Genesis Plus GX, whose VDP timing is accurate, but
  nobody has run the ROM on a console.
