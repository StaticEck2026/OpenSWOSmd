# Arcade mode (OpenSWOS Arcade)

A dedicated **arcade build/mode** of the SDL port: no front-end menus, no career,
no save/load UI. The cabinet boots straight into an **attract loop**, takes
**coins/credits**, plays short **first-goal-wins** matches, and exposes an
operator **service menu** (Midway/NBA-Hangtime style). The normal Amiga game is
the definitive product; this is a separate, stripped, coin-op presentation layer
on top of the same match engine.

Gated behind an arcade flag (proposed `gArcadeMode`, set like `gDosGraphics` from
`[arcade] enabled=true` / `--arcade`). When off, nothing here is reachable and the
normal build is unchanged.

> Source spec: `D:\Projects\SWOS\OpenSWOS\Arcade\ARCADE_MODE.md`.
> Service-menu visual reference: NBA Hangtime
> (`D:\Projects\Arcade\nba-hangtime\MAME\snap\nbahangt\*.png`).

## 1. Game rules (arcade match)

- **45 seconds** play time. **First goal wins** (golden goal — match ends the
  instant anyone scores).
- **Draw → immediate penalty shootout** (sudden death), no extra time.
- **Winner stays on.** A waiting player 2 must **pay/insert a credit ("play a
  quarter")** to challenge; on a win they take the seat.
- Short, repeatable, coin-friendly sessions. No tactics screens, no subs UI, no
  half-time — straight kickoff → result → next.

## 2. Attract mode

When idle (no credits, no game in progress) the cabinet runs an **attract loop**:

- Plays back **special highlight files** — `.HIL` reels recorded for this purpose.
  Source sets live in `D:\Projects\SWOS\DOS\save\*.zip`:
  - `2020 widescreen HIL files in DOS mode 70FPS.zip` ← primary (matches the
    arcade's 70 FPS DOS gameplay, see `docs/DOS_GAMEPLAY.md`)
  - `2020 widescreen HIL files in AMIGA mode 50FPS.zip`
  - `original DOS HIL files.zip`
  - (matching `.RPL` replay sets exist alongside)
- The loop cycles: title/logo → attract highlight(s) → "INSERT COIN" / high-score
  or how-to-play card → repeat.
- **Any coin/credit interrupts** the attract loop and drops into team select /
  kickoff.
- Reuses the existing highlight playback path (`highlight.c` / `_PlayInstantReplayLoop`
  / `.HIL` loader) — the arcade just feeds it the curated reels and disables the
  normal "press fire to exit to menu" so it free-runs.
- Pick the `.HIL` set by gameplay mode: 70 FPS DOS reels when DOS gameplay is on,
  50 FPS Amiga reels otherwise (a reel recorded at one rate plays back wrong at
  the other — see DOS_GAMEPLAY.md timing notes).

## 3. Service menu (operator menu)

Triggered by a **service button** (F2 in MAME; a dedicated cabinet input on real
hardware). Pauses the game/attract loop and shows the operator UI. Style is
deliberately the classic **Midway service menu** (NBA Hangtime), not the SWOS
menu look.

### 3.1 Visual style (from the NBA Hangtime reference)

- **Black background.**
- **Bordered boxes** drawn with the SVCFONT border/box glyphs — a title box at
  top, a left-hand **menu list** box, and right-hand **description** + **values**
  boxes.
- **Multi-colour text**, colour-by-role:
  - green = screen title (e.g. "GAME ADJUSTMENT")
  - white/grey = subtitle / static labels
  - red = instructions ("SELECT WITH ANY STICK — PRESS A BUTTON TO MODIFY")
  - blue = description text (in its own bordered box)
  - orange/yellow = values
- A scrollable **menu list** with a **selection bar**: the current row is
  highlighted and its label **flashes/cycles through colours** (see 3.4).

### 3.2 Font — `bin/grafs_arcade/SVCFONT.bmp`

- 320×256, 8bpp paletted; **palette embedded in the BMP** and identical to the
  arcade **Service palette** (16 entries) now in `dos_graphics.c`
  (`DosGfx_BuildServicePalette`, == `DosPalette.Service`). First entry is the
  transparent/background black; entries 1..15 are the text/UI colours
  (grey, white, red, orange, yellow, green, blue, …).
- Two glyph sizes on the sheet (a **large** WIMP-style face and a **small**
  variant), full ASCII (upper/lower/digits/punct) plus special glyphs:
  - a **hollow box / border** glyph (panel frames)
  - a **logo** glyph (bottom-right roundel)
  - `Sp` (space) and `END`/`RUB` editing glyphs
- **Selection bar**: a **solid white 8×8** glyph drawn behind/around the selected
  row (distinct from the hollow border glyph).
- **Variable-width glyphs.** Use a glyph LUT in the format of
  `D:\Projects\Arcade\nba-hangtime\FONT_EXPORT\combined_320x256.h`:
  ```c
  typedef struct {
      unsigned char  ch;     /* ASCII code, 0 if none */
      unsigned short x, y;   /* top-left of glyph in the sheet */
      unsigned char  w, h;   /* glyph size in pixels */
      signed char    yoff;   /* glyph top offset from text baseline-top */
  } FontGlyph;
  ```
  Generate one LUT per face (e.g. `SVCFONT_LARGE_glyphs[]`, `SVCFONT_SMALL_glyphs[]`)
  from the SVCFONT sheet. Text layout advances by per-glyph `w` (+1px tracking),
  positions each glyph by `yoff` from the row's baseline-top.

### 3.3 Rendering pipeline

- The service menu is its own full-screen 8bpp chunky surface drawn to the
  frontbuffer, like the rest of the renderer.
- When the service menu is up, set the active 256-entry SDL palette's first 16
  entries from `DosGfx_BuildServicePalette()` (rest can be zero) — analogous to
  the DOS palette hook in `platform_sdl.c::refresh_palette_from_copper`, gated on
  "service menu active" rather than `playGame`.
- Blit glyphs by copying their `w×h` rect from the decoded SVCFONT chunky bitmap
  to the target X/Y, skipping palette-index-0 (transparent). A `_DrawSvcText(x,y,
  colour, str)` helper walks the string through the glyph LUT.
- Colour is applied by **remapping the glyph's ink index to the chosen Service
  palette index** at blit time (1-byte LUT, same idea as `_ConvertSpriteColors`)
  — the sheet stores glyphs in one ink colour; the draw recolours per call.

### 3.4 Selection: flashing multi-colour text + selection bar

- The selected menu row draws its **selection bar** first: tile the solid-white
  8×8 glyph across the row width (or draw a filled rect in palette white).
- The selected row's **text flashes through the Service palette colours** —
  cycle the ink colour each frame (or every N frames) through a small colour list
  (e.g. white→yellow→orange→red→… ) keyed off `currentGameTick`, so it visibly
  pulses. Non-selected rows draw static (white/grey).
- Up/down moves the selection; a button "modifies"/enters the highlighted item.

### 3.5 Service-menu contents (initial)

Model on NBA Hangtime's "GAME ADJUSTMENT" screen; start minimal and grow:

- **Diagnostics / boot screen**: build/version string, CMOS/settings status,
  serial number, "PRESS ANY BUTTON TO CONTINUE" (the 0000.png boot card).
- **Game adjustment**: free play on/off, credits-per-coin / coin chute
  multipliers, attract-mode sound on/off, difficulty, match-time (default 45s),
  win bonus.
- **Bookkeeping**: coins in, games played, play-time totals.
- **Input test, sound test, screen test** (standard operator diagnostics).
- **Reset to factory settings.**
- Settings persist to an operator config file (own file, not the player
  `SWOS.ini`).

## 4. Pitches (arcade-exclusive art)

Seven themed pitches (art already ripped to `bin/grafs_arcade/pitch-*.png`):
Beach, Park, Yard, Skate Park, Prison, City, Stadium. These slot into the DOS
pitch pipeline (`docs/DOS_GRAPHICS.md`) as additional pitch sets — selectable per
match (random or rotating in attract). (Conversion of the PNGs into the
`PITCH*.BLK`/`.DAT` tile-bank format is a separate art-pipeline task.)

## 5. Hardware target (real cabinet)

From the spec — for context, not needed for the SDL build:
- SBC: Luckfox CM5 IO board / Raspberry Pi CM4 / Pi 5 (2GB). The ARM Linux build
  (`bin/SWOS.aarch64`) is the deployment target.
- Coin acceptor: SparkFun programmable 6-coin acceptor (GPIO → credit input).
- Card reader: Stripe Terminal M2 (cashless credit).
- Inputs map to the existing joystick/fire handling; coin/service lines are extra
  GPIO inputs.

## 6. Implementation phasing (proposed)

1. **`gArcadeMode` flag** + `[arcade]`/`--arcade` wiring (mirror the `[dos]`
   plumbing in `main.c`/`swos_settings.c`/`dos_mode.h`).
2. **SVCFONT loader**: decode `SVCFONT.bmp` → chunky bitmap + generate/embed the
   glyph LUT(s); `_DrawSvcText` + service palette hook.
3. **Service menu shell**: bordered-box layout, list + selection bar + flashing
   selection, navigation, the boot/diagnostic card. Hook the service button.
4. **Attract loop**: free-running `.HIL` playback (curated reels) + INSERT-COIN
   card; coin interrupts → match.
5. **Arcade match rules**: 45s golden-goal, draw→penalties, winner-stays-on,
   credit-to-join. (Trim the match flow: no tactics/subs/half-time.)
6. **Operator config persistence** + bookkeeping counters.
7. **Arcade pitches** art pipeline + per-match selection.
8. **Cabinet I/O** (coin/service GPIO) on the ARM target.

## Open questions
- Exact service-menu item set / defaults (start from NBA Hangtime, prune to SWOS).
- Whether the selection "flash" is a fixed colour cycle or palette-rotate.
- `.HIL` attract reels: which curated set ships, and the rotation order.
- Two-player join/seat-swap flow specifics on the cabinet.
